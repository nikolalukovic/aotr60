// 60-mode frame pacing (PLAN §1.7): a QPC deadline per main-loop iteration, two iterations per stock frame time,
// gap handling without catch-up bursts, Present-skip for late B-renders and the automatic fallback to 30 FPS.
// 30 mode never comes here: the stock limiter runs unchanged.

#include "pacer.h"

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
HANDLE g_timer = nullptr;
bool g_timerTried = false;

// Fallback evaluation over fixed windows of 60-mode time.
constexpr int64_t kWindowSeconds = 5;
int64_t g_windowStart = 0;
int64_t g_windowDebt = 0;
uint32_t g_windowBRenders = 0;
uint32_t g_windowLateSkips = 0;
int64_t g_fallbackUntil = 0;
int g_fallbackCount = 0;

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
    double debtRatio = static_cast<double>(g_windowDebt) / static_cast<double>(length);
    double skipRatio = g_windowBRenders ? static_cast<double>(g_windowLateSkips) / g_windowBRenders : 0.0;
    g_pacerStats.lastDebtRatio = debtRatio;
    g_pacerStats.lastSkipRatio = skipRatio;
    if (debtRatio > 0.003 || skipRatio > 0.10) {
        int backoff = 30 << (g_fallbackCount < 4 ? g_fallbackCount : 4); // 30 s .. 8 min
        ++g_fallbackCount;
        g_fallbackUntil = now + backoff * g_freq;
        ++g_pacerStats.fallbacks;
        Log("pacer: falling back to 30 FPS for %d s (lost time %.2f%%, late Present skips %.1f%% over %llds)", backoff,
            debtRatio * 100.0, skipRatio * 100.0, static_cast<long long>(kWindowSeconds));
    }
    g_windowStart = now;
    g_windowDebt = 0;
    g_windowBRenders = 0;
    g_windowLateSkips = 0;
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

    int64_t pairTicks = g_cfg.pacing == Pacing::Nominal ? g_freq / 30 : StockFrameMs(engine) * g_freq / 1000;
    g_halfToggle = !g_halfToggle;
    int64_t half = pairTicks / 2;
    g_interval = g_halfToggle ? half : pairTicks - half;

    bool gap = g_deadline == 0 || g_pacerRanRid != g_renderId - 1 || g_gapDevLost || g_gapReset;
    g_gapDevLost = 0;
    g_gapReset = 0;
    if (gap) {
        g_deadline = now;
        ++g_pacerStats.gaps;
    }
    else {
        g_deadline += g_interval;
        int64_t late = now - g_deadline;
        int64_t clamp = g_interval * 5 / 4;
        if (late > g_freq / 4) {
            // Alt-tab, minimised window, debugger: a gap, not lost game time.
            g_deadline = now;
            ++g_pacerStats.gaps;
        }
        else if (late > clamp) {
            g_windowDebt += late - clamp;
            g_pacerStats.debtTicks += late - clamp;
            g_deadline = now - clamp;
        }
        else if (late < 0) {
            WaitUntil(g_deadline);
        }
    }
    g_pacerRanRid = g_renderId;
    ++g_pacerStats.iterations;
    EvaluateWindow(now);

    // The stock limiter's statistics (0x63A1C4..0x63A1D5).
    DWORD exitMs = timeGetTime();
    uint32_t last = Read<uint32_t>(0xDE4318);
    Write<uint32_t>(0xDE4314, entryMs - last);
    uint32_t waited = exitMs - entryMs;
    Write<uint32_t>(0xDE4310, waited);
    Write<uint32_t>(0xDE430C, Read<uint32_t>(0xDE430C) + waited);
    return exitMs;
}

extern "C" int __cdecl AotR60_PresentSkip()
{
    if (!OnMainThread() || !g_inB) {
        return 0;
    }
    if (g_mDrawRid == g_renderId - 1 && g_mDrawAM > 1) {
        ++g_pacerStats.forcedSkips; // camera time multiplier: present only the A-render
        return 1;
    }
    ++g_windowBRenders;
    if (g_pacerRanRid == g_renderId - 1 && g_deadline != 0 && Now() - g_deadline >= g_interval) {
        ++g_windowLateSkips;
        ++g_pacerStats.lateSkips;
        return 1;
    }
    return 0;
}

int64_t PacerFrequency()
{
    return g_freq;
}
