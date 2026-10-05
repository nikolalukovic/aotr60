// 60-mode frame pacing (PLAN §1.7): a QPC deadline per main-loop iteration, two iterations per stock frame time,
// gap handling without catch-up bursts, Present-skip for late B-renders and the automatic fallback to 30 FPS.
// 30 mode never comes here: the stock limiter runs unchanged.

#include "pacer.h"
#include "pacer_policy.h"
#include "split_present.h"

#include "config.h"
#include "log.h"
#include "runtime.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

using namespace game;

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

namespace {

Config g_cfg;
int64_t g_freq = 0;
int64_t g_deadline = 0;    // QPC time the current iteration was released (0 = none: next run is a gap)
int64_t g_interval = 0;    // interval used for the current iteration
bool g_halfToggle = false;
int64_t g_halfPair = 0;     // half the stock frame time (Present spacing target)
int64_t g_owed = 0;         // time owed after late releases, repaid on later on-time iterations
int64_t g_releaseTime = 0;  // QPC when the current iteration was released
int64_t g_lastPresent = 0;  // QPC of the last 60-mode Present submission
HANDLE g_timer = nullptr;
bool g_timerTried = false;

// Fallback evaluation over fixed windows of 60-mode time.
constexpr int64_t kWindowSeconds = 5;
int64_t g_windowStart = 0;
int64_t g_windowDebt = 0;
uint32_t g_windowBRenders = 0;
uint32_t g_windowLateSkips = 0;
uint32_t g_windowIterations = 0;
uint32_t g_windowLateIterations = 0;   // released more than a quarter interval late
uint32_t g_windowOnTimeIterations = 0; // released on schedule (waited)
int64_t g_presentCall = 0;             // QPC when the current Present was submitted
uint32_t g_presentsSinceBlock = 1000;  // Presents since one blocked (vsync queue full)
int64_t g_fallbackUntil = 0;
FallbackPolicy g_policy;

// Split present support.
int64_t g_pairTicks = 0;
int64_t g_borrow = 0;      // the coming Y iteration was released this much early (added back to the next deadline)
int64_t g_lastA = 0;       // QPC of the last A-render Present
bool g_iterWasA = false;   // the iteration that is running (or just ended) is a 60-mode A-render iteration
int g_c0Sub = 0;           // stepper value at its C0
int64_t g_aPre[7] = {};    // release -> A Present, per stepper value (EWMA 1/8)
int64_t g_bEst = 0;        // release -> B Present (EWMA 1/8)
int64_t g_tailEst = 0;     // logic end -> pacer entry of a Y iteration, drain wait excluded (EWMA 1/8)

int64_t Now()
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

void EnsureTimer()
{
    if (g_timerTried) {
        return;
    }
    g_timerTried = true;
    g_timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!g_timer) {
        g_timer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
        Log("pacer: high-resolution timer unavailable, using a normal waitable timer");
    }
}

void WaitUntil(int64_t deadline)
{
    EnsureTimer();
    for (;;) {
        int64_t remaining = deadline - Now();
        if (remaining <= 0) {
            return;
        }
        int64_t remainingUs = remaining * 1000000 / g_freq;
        if (remainingUs > 2000) {
            if (g_timer) {
                LARGE_INTEGER due;
                due.QuadPart = -(remainingUs - 1000) * 10; // relative, 100 ns units; wake ~1 ms early
                if (SetWaitableTimerEx(g_timer, &due, 0, nullptr, nullptr, nullptr, 0)) {
                    WaitForSingleObject(g_timer, 100);
                    continue;
                }
            }
            Sleep(1);
        }
        else {
            YieldProcessor();
        }
    }
}

void EvaluateWindow(int64_t now)
{
    if (!g_cfg.fallback) {
        return;
    }
    if (g_windowStart == 0) {
        g_windowStart = now;
        return;
    }
    int64_t length = now - g_windowStart;
    if (length < kWindowSeconds * g_freq) {
        return;
    }
    PacerWindow w;
    w.lostRatio = static_cast<double>(g_windowDebt) / static_cast<double>(length);
    w.lateRatio = g_windowIterations ? static_cast<double>(g_windowLateIterations) / g_windowIterations : 0.0;
    w.skipRatio = g_windowBRenders ? static_cast<double>(g_windowLateSkips) / g_windowBRenders : 0.0;
    w.onTimeRatio = g_windowIterations ? static_cast<double>(g_windowOnTimeIterations) / g_windowIterations : 1.0;
    g_pacerStats.lastDebtRatio = w.lostRatio;
    g_pacerStats.lastSkipRatio = w.skipRatio;
    if (g_policy.AddWindow(w)) {
        int backoff = g_policy.NextBackoffSeconds();
        g_fallbackUntil = now + backoff * g_freq;
        ++g_pacerStats.fallbacks;
        Log("pacer: falling back to 30 FPS for %d s (over the last %llds: lost time %.2f%%, late iterations %.1f%%, "
            "on-time iterations %.1f%%, late Present skips %.1f%%)", backoff, static_cast<long long>(kWindowSeconds),
            w.lostRatio * 100.0, w.lateRatio * 100.0, w.onTimeRatio * 100.0, w.skipRatio * 100.0);
    }
    g_windowStart = now;
    g_windowDebt = 0;
    g_windowBRenders = 0;
    g_windowLateSkips = 0;
    g_windowIterations = 0;
    g_windowLateIterations = 0;
    g_windowOnTimeIterations = 0;
}

} // namespace

PacerStats g_pacerStats;

void PacerInit(const Config& cfg)
{
    g_cfg = cfg;
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    g_freq = f.QuadPart;
}

bool PacerFallbackActive()
{
    return g_fallbackUntil != 0 && Now() < g_fallbackUntil;
}

void PacerOnModeChange(bool)
{
    g_deadline = 0;
    g_windowStart = 0;
    g_windowDebt = 0;
    g_windowBRenders = 0;
    g_windowLateSkips = 0;
    g_windowIterations = 0;
    g_windowLateIterations = 0;
    g_windowOnTimeIterations = 0;
    g_presentCall = 0;
    g_presentsSinceBlock = 1000;
    g_policy.Reset();
    g_owed = 0;
    g_releaseTime = 0;
    g_lastPresent = 0;
    g_borrow = 0;
    g_lastA = 0;
}

int PacerFallbackSecondsLeft()
{
    if (!PacerFallbackActive()) {
        return 0;
    }
    return static_cast<int>((g_fallbackUntil - Now()) / g_freq) + 1;
}

// Stock frame time P from the same inputs as the stock limiter (0x63A19C..0x63A1AD): fps limit * net speed
// multiplier, single precision, truncated.
int StockFrameMs(const uint8_t* engine)
{
    int fps = Field<int32_t>(engine, 0xC);
    float mult = Read<float>(kNetSpeedMultiplier);
    float product = static_cast<float>(fps) * mult;
    if (!(product > 0.0f)) {
        return 33;
    }
    int p = static_cast<int>(1000.0f / product);
    return p < 2 ? 2 : p;
}

extern "C" uint32_t __cdecl AotR60_Pacer(uint8_t* engine)
{
    DWORD entryMs = timeGetTime();
    int64_t now = Now();

    // Y iteration tail (logic end -> here, without the split-present drain wait).
    int64_t drainWait = SpTakeDrainWait();
    if (g_m60 && !g_iterWasA && g_pacerRanRid == g_renderId - 1 && SpLogicEndTime() > g_releaseTime) {
        int64_t tail = now - SpLogicEndTime() - drainWait;
        if (tail >= 0 && tail < g_freq / 20) {
            g_tailEst += (tail - g_tailEst) / 8;
        }
    }

    int64_t pairTicks = g_cfg.pacing == Pacing::Nominal ? g_freq / 30 : StockFrameMs(engine) * g_freq / 1000;
    g_pairTicks = pairTicks;
    g_halfToggle = !g_halfToggle;
    int64_t half = pairTicks / 2;
    g_interval = g_halfToggle ? half : pairTicks - half;

    g_halfPair = half;

    bool gap = g_deadline == 0 || g_pacerRanRid != g_renderId - 1 || g_gapDevLost || g_gapReset;
    g_gapDevLost = 0;
    g_gapReset = 0;
    if (gap) {
        g_deadline = now;
        g_owed = 0;
        g_borrow = 0;
        ++g_pacerStats.gaps;
        SplitPresentCooldown();
    }
    else {
        g_deadline += g_interval + g_borrow; // an early-released Y does not move the pair schedule
        g_borrow = 0;
        int64_t late = now - g_deadline;
        int64_t clamp = g_interval * 3; // lateness carried and caught up later; beyond this it is lost time
        if (late > g_freq / 4) {
            // Alt-tab, minimised window, debugger: a gap, not lost game time.
            g_deadline = now;
            g_owed = 0;
            ++g_pacerStats.gaps;
            SplitPresentCooldown();
        }
        else if (late > 0) {
            // Released late (a heavy logic tick, a hitch). Start now instead of compressing the following frames
            // to hold the absolute schedule, and repay the time gradually on later on-time iterations, so long-run
            // speed stays exact while the presented frames keep an even spacing.
            ++g_pacerStats.lateReleases;
            g_pacerStats.lateReleaseTicks += late;
            if (late > g_interval / 4) {
                ++g_windowLateIterations;
            }
            int64_t carried = late;
            if (late > clamp) {
                g_windowDebt += late - clamp;
                g_pacerStats.debtTicks += late - clamp;
                carried = clamp;
            }
            g_owed += carried;
            int64_t cap = 6 * g_interval;
            if (g_owed > cap) {
                g_windowDebt += g_owed - cap; // more than two frames behind: count it as lost time
                g_pacerStats.debtTicks += g_owed - cap;
                g_owed = cap;
            }
            g_deadline = now;
        }
        else {
            ++g_windowOnTimeIterations;
            ++g_pacerStats.onTimeIterations;
            int64_t pay = g_interval / 8;
            if (g_cfg.repayProportional && g_owed / 6 > pay) {
                pay = g_owed / 6; // repay faster when more is owed, so owed time does not pile up to the cap
            }
            if (pay > g_owed) {
                pay = g_owed;
            }
            if (pay > -late) {
                pay = -late;
            }
            g_deadline -= pay;
            g_owed -= pay;
            // Split present, early release: a Y whose logic step is predicted to overrun starts early by the
            // overrun (its B frame is still shown at its target time by split present).
            if (g_iterWasA && g_m60 && SplitPresentEarlyRelease() &&
                SpDeferLikely(Read<uint32_t>(0xDD302C) == 0x80000000u || g_presentsSinceBlock >= 2)) {
                int64_t logic = SpPredictNextLogic();
                if (logic >= 0) {
                    int64_t over = g_bEst + logic + g_tailEst - g_interval;
                    int64_t slack = g_deadline - now;
                    int64_t shift = over < slack ? over : slack;
                    if (shift > 0) {
                        g_deadline -= shift;
                        g_borrow = shift;
                        ++g_spStats.earlyReleases;
                        g_spStats.earlyTicks += shift;
                    }
                }
            }
            int64_t waitStart = Now();
            WaitUntil(g_deadline);
            g_pacerStats.pacerWaitTicks += Now() - waitStart;
        }
    }
    g_pacerStats.owedTicks = g_owed;
    g_pacerRanRid = g_renderId;
    ++g_pacerStats.iterations;
    ++g_windowIterations;
    EvaluateWindow(now);
    g_releaseTime = Now();

    // The stock limiter's statistics (0x63A1C4..0x63A1D5).
    DWORD exitMs = timeGetTime();
    uint32_t last = Read<uint32_t>(0xDE4318);
    Write<uint32_t>(0xDE4314, entryMs - last);
    uint32_t waited = exitMs - entryMs;
    Write<uint32_t>(0xDE4310, waited);
    Write<uint32_t>(0xDE430C, Read<uint32_t>(0xDE430C) + waited);
    return exitMs;
}

namespace {

// Present submission spacing for the telemetry (only for iterations the pacer released).
void RecordPresentSpacing(int64_t now, bool paced)
{
    if (paced && g_lastPresent != 0) {
        int64_t spacing = now - g_lastPresent;
        if (spacing > g_freq / 40) {
            ++g_pacerStats.presentsOver25;
            if (spacing > g_freq * 34 / 1000) {
                ++g_pacerStats.presentsOver34;
            }
        }
        if (spacing > g_pacerStats.presentSpacingMax) {
            g_pacerStats.presentSpacingMax = spacing;
        }
        if (g_pacerStats.presentSpacingMin == 0 || spacing < g_pacerStats.presentSpacingMin) {
            g_pacerStats.presentSpacingMin = spacing;
        }
    }
    g_lastPresent = now;
}

int64_t Ewma(int64_t avg, int64_t sample)
{
    if (avg == 0) {
        return sample;
    }
    if (sample > 3 * avg) {
        sample = 3 * avg; // clip single spikes
    }
    return avg + (sample - avg) / 8;
}

// Split present target for this B-render's Present: the old spacing time, or later, the middle between the last
// A Present and the predicted next A Present when a long logic step follows.
int64_t SplitTarget(int64_t now)
{
    int64_t half = g_halfPair;
    int64_t target = now;
    if (g_owed == 0 && g_lastPresent != 0) {
        target = g_lastPresent + half - g_freq / 4000;
        int64_t latest = g_releaseTime + g_borrow + half * 6 / 10;
        if (target > latest) {
            target = latest;
        }
    }
    int64_t logic = SpPredictNextLogic();
    uint8_t* ge = Ptr(kTheGameEngine);
    if (logic >= 0 && g_lastA != 0 && ge) {
        int s = Field<int32_t>(ge, 0x34);
        int next = (s >= 1 && s <= 5) ? s + 1 : 1;
        int64_t aPre = g_aPre[next] > 0 ? g_aPre[next] : g_freq * 9 / 1000;
        int64_t end = now + logic + g_tailEst;
        int64_t deadline = PacerNextXDeadline();
        int64_t nextA = (end > deadline ? end : deadline) + aPre;
        int64_t middle = g_lastA + (nextA - g_lastA) / 2;
        if (middle > target) {
            target = middle;
        }
    }
    if (target > now + g_freq * 60 / 1000) {
        target = now + g_freq * 60 / 1000;
    }
    return target;
}

} // namespace

// Called right before every 60-mode Present (A and B). Returns 0 to present, 1 to skip this Present, 2 when split
// present has taken it over (deferred: the stock code continues as if presented).
extern "C" int __cdecl AotR60_PresentSkip()
{
    if (!OnMainThread()) {
        if (SpPending()) {
            AotR60_SpCancel(kCancelOffMain);
        }
        return 0;
    }
    if (SpPending()) {
        SpCancelBeforePresent(); // a second Present while one is pending (should not happen)
    }
    bool bRender = g_inB != 0;
    if (bRender && g_mDrawRid == g_renderId - 1 && g_mDrawAM > 1) {
        ++g_pacerStats.forcedSkips; // camera time multiplier: present only the A-render
        return 1;
    }
    int64_t now = Now();
    // Released by the pacer (not the stock unlimited path, e.g. during a camera time multiplier).
    bool paced = g_releaseTime != 0 && g_pacerRanRid == g_renderId - 1;
    if (paced) {
        int64_t sinceRelease = now - g_releaseTime;
        if (sinceRelease >= 0 && sinceRelease < g_freq) {
            (bRender ? g_pacerStats.relPresentTicksB : g_pacerStats.relPresentTicksA) += sinceRelease;
            ++(bRender ? g_pacerStats.relPresentCountB : g_pacerStats.relPresentCountA);
            if (bRender) {
                g_bEst = Ewma(g_bEst, sinceRelease);
            }
            else if (g_c0Sub >= 0 && g_c0Sub <= 6) {
                g_aPre[g_c0Sub] = Ewma(g_aPre[g_c0Sub], sinceRelease);
            }
        }
    }
    bool immediate = Read<uint32_t>(0xDD302C) == 0x80000000u; // D3DPRESENT_INTERVAL_IMMEDIATE: no vsync
    if (bRender) {
        ++g_windowBRenders;
    }
    if (!immediate) {
        // FIFO (vsync): drop a B Present that is already a frame behind the absolute schedule (including time
        // still owed from re-anchored late releases) - but only while the swap queue is actually full (a recent
        // Present blocked). This absorbs the 60.6 vs 60 Hz deficit of a 60 Hz display and keeps the exact stock
        // speed; with room in the queue the shortened intervals repay owed time by themselves.
        if (bRender && g_presentsSinceBlock < 2 && g_pacerRanRid == g_renderId - 1 && g_deadline != 0 &&
            now - (g_deadline + g_borrow - g_owed) >= g_interval) {
            ++g_windowLateSkips;
            ++g_pacerStats.lateSkips;
            return 1;
        }
    }
    if (bRender && SplitPresentInstalled()) {
        bool queueRoom = immediate || g_presentsSinceBlock >= 2; // a deferred Present must not block in logic
        if (SpTryDefer(now, SplitTarget(now), paced, queueRoom)) {
            return 2;
        }
        if (g_borrow > 0) {
            ++g_spStats.borrowedNotDeferred;
        }
    }
    // The A-render does all A-only work before its Present and the B-render almost none, so space the Presents
    // evenly (half a stock frame apart). Without vsync they reach the screen when submitted; with vsync on a
    // display of at least twice the render rate the queue never fills and spacing decides the vblank cadence.
    // Only while the schedule is clean (nothing owed), so the wait never takes time the logic step needs. The
    // small slack lets the phase drift earlier when the A-render gets cheaper; the cap keeps time for the logic.
    if (g_cfg.presentPacing && paced && g_owed == 0 && g_lastPresent != 0 && g_halfPair > 0) {
        int64_t target = g_lastPresent + g_halfPair - g_freq / 4000;
        int64_t latest = g_releaseTime + g_borrow + g_halfPair * 6 / 10;
        if (!bRender && SplitPresentActive() && g_lastA != 0 && g_pairTicks > 0 &&
            target > g_lastA + g_pairTicks - g_freq / 2000) {
            target = g_lastA + g_pairTicks - g_freq / 2000; // a late split-present B never delays the next A
        }
        if (target > latest) {
            target = latest;
        }
        if (target > now) {
            WaitUntil(target);
            g_pacerStats.presentWaitTicks += target - now;
            now = Now();
        }
    }
    RecordPresentSpacing(now, paced);
    g_presentCall = now;
    if (!bRender) {
        g_lastA = now;
    }
    return 0;
}

// Right after every 60-mode Present returns: note whether it blocked (vsync queue full).
extern "C" void __cdecl AotR60_PresentDone()
{
    if (!OnMainThread() || g_presentCall == 0) {
        return;
    }
    int64_t took = Now() - g_presentCall;
    g_presentCall = 0;
    g_pacerStats.presentCallTicks += took;
    if (took > g_freq / 1000) {
        g_presentsSinceBlock = 0;
    }
    else if (g_presentsSinceBlock < 1000) {
        ++g_presentsSinceBlock;
    }
}

int64_t PacerFrequency()
{
    return g_freq;
}

int64_t PacerNow()
{
    return Now();
}

void PacerWaitUntil(int64_t qpc)
{
    WaitUntil(qpc);
}

int64_t PacerNextXDeadline()
{
    if (g_deadline == 0 || g_pairTicks == 0) {
        return 0;
    }
    int64_t next = g_pairTicks - g_interval;
    int64_t pay = next / 8;
    if (g_cfg.repayProportional && g_owed / 6 > pay) {
        pay = g_owed / 6;
    }
    if (pay > g_owed) {
        pay = g_owed;
    }
    return g_deadline + next + g_borrow - pay;
}

void PacerOnDeferredPresent(int64_t callTime, int64_t returnTime, bool paced)
{
    RecordPresentSpacing(callTime, paced);
    g_pacerStats.presentCallTicks += returnTime - callTime;
    if (returnTime - callTime > g_freq / 1000) {
        g_presentsSinceBlock = 0;
    }
    else if (g_presentsSinceBlock < 1000) {
        ++g_presentsSinceBlock;
    }
}

void PacerOnC0(bool aRender, int stepperS)
{
    g_iterWasA = aRender;
    g_c0Sub = (stepperS >= 1 && stepperS <= 6) ? stepperS : 0;
}
