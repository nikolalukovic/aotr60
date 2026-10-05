// Telemetry (PLAN §3 phase 1, §4): per-tick invariants, rates, logic-RNG guard and per-site call counts.
// Telemetry=0 keeps only the cheap counters and anomaly reports; 1 adds the invariant checks, the 10 s rate CSV
// and per-site summaries.

#include "camera_math.h"
#include "config.h"
#include "log.h"
#include "pacer.h"
#include "paths.h"
#include "runtime.h"

#include <cstdio>
#include <cstring>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

using namespace game;

namespace {

Config g_cfg;
HANDLE g_csv = INVALID_HANDLE_VALUE;
HANDLE g_trace = INVALID_HANDLE_VALUE; // Telemetry=2: one line per logic call
int64_t g_freq = 1;
int64_t g_start = 0;
int64_t g_lastReport = 0;
int64_t g_lastSummary = 0;

struct Totals {
    uint64_t renders = 0;
    uint64_t rendersA = 0;
    uint64_t renders60 = 0;
    uint64_t logicCalls = 0;
    uint64_t logicTicks = 0;
    uint64_t seedOutside = 0;   // logic RNG advanced between two logic calls (client-side use)
    uint64_t seedOutside60 = 0;
    uint64_t tickChecks = 0;
    uint64_t tickErrors = 0;
    uint64_t mFrames = 0;      // m_frame advances, accumulated per render (resets excluded)
    int64_t syncMs = 0;        // W3D sync accumulator advances, accumulated per render
    uint64_t clockResets = 0;  // renders where m_frame or the sync clock went backwards (new game, load, exit)
};
Totals g_t;
Totals g_tPrev;
uint32_t g_lastMFrame = 0;
int32_t g_lastSync = 0;
bool g_clockValid = false;
uint32_t g_prevPresents = 0;
uint32_t g_prevTmFF = 0;
PacerStats g_prevPacer;
uint32_t g_prevAnomaly = 0;
uint32_t g_prevUnknown = 0;
uint32_t g_prevVt188 = 0;

// Logic RNG guard.
uint32_t g_seedAtExit = 0;
bool g_seedValid = false;

// Current tick window (between two GameLogic::update(1) calls).
struct TickWindow {
    bool valid = false;
    bool broken = false;
    bool any60 = false;
    bool any30 = false;
    bool stalled = false; // a tick attempt failed in this window (pause, frozen logic, stalled network)
    uint32_t renderId = 0;
    uint64_t rendersA = 0;
    uint32_t mFrame = 0;
    int nextSub = 2;
    uint32_t siteRun[sites::kSiteCount];
    uint32_t siteSkip[sites::kSiteCount];
};
TickWindow g_w;

// Per-site averages per tick, separately for 30 and 60 mode.
struct SiteSums {
    uint64_t ticks = 0;
    uint64_t run[sites::kSiteCount] = {};
    uint64_t skip[sites::kSiteCount] = {};
};
SiteSums g_sums[2];

int64_t Now()
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

void CsvLine(const char* text)
{
    if (g_csv == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written;
    WriteFile(g_csv, text, static_cast<DWORD>(std::strlen(text)), &written, nullptr);
}

uint32_t MFrame()
{
    uint8_t* gc = Ptr(kTheGameClient);
    return gc ? Field<uint32_t>(gc, 0x10) : 0;
}

void StartTickWindow()
{
    g_w.valid = true;
    g_w.broken = false;
    g_w.any60 = false;
    g_w.any30 = false;
    g_w.stalled = false;
    g_w.renderId = g_renderId;
    g_w.rendersA = g_t.rendersA;
    g_w.mFrame = MFrame();
    g_w.nextSub = 2;
    for (uint32_t i = 0; i < sites::kSiteCount; ++i) {
        g_w.siteRun[i] = g_siteRun[i];
        g_w.siteSkip[i] = g_siteSkip[i];
    }
}

void CloseTickWindow()
{
    if (!g_w.valid || g_w.broken || g_w.stalled || g_w.nextSub != 7 || (g_w.any60 && g_w.any30)) {
        return; // reset, mode change, pause/stall or an incomplete sub sequence: no exact invariant
    }
    uint32_t renders = g_renderId - g_w.renderId;
    uint32_t rendersA = static_cast<uint32_t>(g_t.rendersA - g_w.rendersA);
    uint32_t dm = MFrame() - g_w.mFrame;
    bool sixty = g_w.any60;
    ++g_t.tickChecks;
    uint32_t expectRenders = sixty ? 12 : 6;
    if (dm != 6 || renders != expectRenders || rendersA != 6) {
        ++g_t.tickErrors;
        if (g_t.tickErrors <= 20 || g_t.tickErrors % 1000 == 0) {
            Log("ERROR tick invariant (%s): renders=%u (expected %u) A-renders=%u (expected 6) dm_frame=%u (expected 6) "
                "[error #%llu]",
                sixty ? "60" : "30", renders, expectRenders, rendersA, dm,
                static_cast<unsigned long long>(g_t.tickErrors));
        }
        return;
    }
    SiteSums& s = g_sums[sixty ? 1 : 0];
    ++s.ticks;
    for (uint32_t i = 0; i < sites::kSiteCount; ++i) {
        s.run[i] += g_siteRun[i] - g_w.siteRun[i];
        s.skip[i] += g_siteSkip[i] - g_w.siteSkip[i];
    }
}

void LogSiteSummary()
{
    if (!g_sums[0].ticks && !g_sums[1].ticks) {
        return;
    }
    Log("sites: calls per logic tick (30 mode: %llu ticks, 60 mode: %llu ticks) - run/skip",
        static_cast<unsigned long long>(g_sums[0].ticks), static_cast<unsigned long long>(g_sums[1].ticks));
    for (uint32_t i = 0; i < sites::kSiteCount; ++i) {
        if (!g_sums[0].run[i] && !g_sums[0].skip[i] && !g_sums[1].run[i] && !g_sums[1].skip[i]) {
            continue;
        }
        double r30 = g_sums[0].ticks ? static_cast<double>(g_sums[0].run[i]) / g_sums[0].ticks : 0.0;
        double k30 = g_sums[0].ticks ? static_cast<double>(g_sums[0].skip[i]) / g_sums[0].ticks : 0.0;
        double r60 = g_sums[1].ticks ? static_cast<double>(g_sums[1].run[i]) / g_sums[1].ticks : 0.0;
        double k60 = g_sums[1].ticks ? static_cast<double>(g_sums[1].skip[i]) / g_sums[1].ticks : 0.0;
        Log("  %-24s 30: %8.2f/%-8.2f 60: %8.2f/%-8.2f", sites::kSites[i].id, r30, k30, r60, k60);
    }
}

void CheckAnomalies()
{
    if (g_anomaly != g_prevAnomaly) {
        Log("WARN anomaly bits 0x%X (Palantir caller 0x%08X, InGameUI caller 0x%08X)", g_anomaly, g_palBadRet,
            g_iguiBadRet);
        g_prevAnomaly = g_anomaly;
    }
    if (g_unknownPath != g_prevUnknown) {
        Log("WARN unexpected call path bits 0x%X (display vt+0x188 last caller 0x%08X) - 60 FPS stops", g_unknownPath,
            g_vt188LastRet);
        g_prevUnknown = g_unknownPath;
    }
    if (g_tmFFin60 != g_prevTmFF) {
        Log("ERROR fast-forward path reached in 60 mode %u times (expected never)", g_tmFFin60);
        g_prevTmFF = g_tmFFin60;
    }
    if (g_vt188Calls != g_prevVt188 && g_prevVt188 == 0) {
        Log("info: display vt+0x188 called (first caller 0x%08X)", g_vt188LastRet);
    }
    g_prevVt188 = g_vt188Calls;
}

void Report(int64_t now)
{
    double dt = static_cast<double>(now - g_lastReport) / static_cast<double>(g_freq);
    if (dt <= 0.0) {
        return;
    }
    uint32_t presents = g_siteRun[sites::PRESENT];
    double renders = static_cast<double>(g_t.renders - g_tPrev.renders);
    double frac60 = renders > 0 ? static_cast<double>(g_t.renders60 - g_tPrev.renders60) / renders : 0.0;
    char line[512];
    std::snprintf(line, sizeof(line),
                  "%.1f,%.3f,%.2f,%.2f,%.2f,%u,%u,%.3f,%.3f,%.2f,%.1f,%.2f,%u,%llu,%llu,%llu,%u,%u,%u,%llu,%u,%u,%u\r\n",
                  static_cast<double>(now - g_start) / g_freq, frac60, renders / dt,
                  static_cast<double>(g_t.rendersA - g_tPrev.rendersA) / dt, (presents - g_prevPresents) / dt,
                  g_pacerStats.lateSkips - g_prevPacer.lateSkips, g_pacerStats.forcedSkips - g_prevPacer.forcedSkips,
                  static_cast<double>(g_t.logicTicks - g_tPrev.logicTicks) / dt,
                  static_cast<double>(g_t.logicCalls - g_tPrev.logicCalls) / dt,
                  static_cast<double>(g_t.mFrames - g_tPrev.mFrames) / dt,
                  static_cast<double>(g_t.syncMs - g_tPrev.syncMs) / dt,
                  static_cast<double>(g_pacerStats.debtTicks - g_prevPacer.debtTicks) * 1000.0 / g_freq,
                  g_pacerStats.gaps - g_prevPacer.gaps, static_cast<unsigned long long>(g_t.seedOutside - g_tPrev.seedOutside),
                  static_cast<unsigned long long>(g_t.tickChecks - g_tPrev.tickChecks),
                  static_cast<unsigned long long>(g_t.tickErrors - g_tPrev.tickErrors), g_c5Stats.replays,
                  g_cameraStats.bSwaps, g_cameraStats.aSwaps,
                  static_cast<unsigned long long>(g_t.clockResets - g_tPrev.clockResets), g_cameraStats.aCuts,
                  g_cameraStats.aShake, g_cameraStats.aNoHistory);
    // Pacing detail (ms): mean release->Present for A and B, Present spacing extremes, Present waits, late releases.
    const PacerStats& p = g_pacerStats;
    const PacerStats& q = g_prevPacer;
    auto ms = [](int64_t ticks) { return static_cast<double>(ticks) * 1000.0 / static_cast<double>(g_freq); };
    uint32_t na = p.relPresentCountA - q.relPresentCountA;
    uint32_t nb = p.relPresentCountB - q.relPresentCountB;
    char extra[256];
    uint32_t iters = p.iterations - q.iterations;
    double onTimePct = iters ? 100.0 * (p.onTimeIterations - q.onTimeIterations) / iters : 0.0;
    std::snprintf(extra, sizeof(extra), ",%.2f,%.2f,%.2f,%.2f,%.1f,%u,%.1f,%u,%.1f\r\n",
                  na ? ms(p.relPresentTicksA - q.relPresentTicksA) / na : 0.0,
                  nb ? ms(p.relPresentTicksB - q.relPresentTicksB) / nb : 0.0, ms(p.presentSpacingMin),
                  ms(p.presentSpacingMax), ms(p.presentWaitTicks - q.presentWaitTicks), p.lateReleases - q.lateReleases,
                  ms(p.lateReleaseTicks - q.lateReleaseTicks), g_cameraStats.aShakerFar, onTimePct);
    size_t len = std::strlen(line);
    if (len >= 2) {
        line[len - 2] = 0; // drop "\r\n" and append the pacing columns
    }
    strncat_s(line, sizeof(line), extra, _TRUNCATE);
    CsvLine(line);
    g_tPrev = g_t;
    g_prevPresents = presents;
    g_pacerStats.presentSpacingMin = 0;
    g_pacerStats.presentSpacingMax = 0;
    g_prevPacer = g_pacerStats;
    g_lastReport = now;
}

} // namespace

void TelemetryInit(const Config& cfg)
{
    g_cfg = cfg;
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    g_freq = f.QuadPart;
    if (cfg.telemetry >= 1) {
        std::wstring dir = DataDirectory();
        if (!dir.empty()) {
            std::wstring path = dir + L"\\aotr60_rates.csv";
            g_csv = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
            CsvLine("t_s,frac60,renders_s,a_renders_s,presents_s,late_skips,forced_skips,logic_ticks_s,logic_calls_s,"
                    "mframe_s,sync_ms_s,debt_ms,gaps,seed_outside_logic,tick_checks,tick_errors,c5_replays,"
                    "cam_b_swaps,cam_a_swaps,clock_resets,cam_a_cuts,cam_a_shake,cam_a_nohist,"
                    "a_rel_present_ms,b_rel_present_ms,present_spacing_min_ms,present_spacing_max_ms,present_wait_ms,"
                    "late_releases,late_release_ms,cam_shaker_far,on_time_pct\r\n");
            if (cfg.telemetry >= 2) {
                std::wstring tpath = dir + L"\\aotr60_trace.txt";
                g_trace = CreateFileW(tpath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                                      FILE_ATTRIBUTE_NORMAL, nullptr);
                const char header[] = "# logic_frame sub seed crc(sub 1 only) mode(30/60)\r\n";
                DWORD written;
                WriteFile(g_trace, header, sizeof(header) - 1, &written, nullptr);
            }
        }
    }
}

void TelemetryOnPreRender(int stepperS)
{
    // A failed tick attempt (pause, frozen logic) leaves the stepper at s > 6.
    if (stepperS > 6) {
        g_w.stalled = true;
    }
    // Clocks accumulated per render so game resets do not corrupt the rates.
    uint32_t mf = MFrame();
    int32_t sync = Read<int32_t>(kSyncAccumulator);
    if (g_clockValid) {
        int64_t dmf = static_cast<int64_t>(mf) - g_lastMFrame;
        int64_t dsync = static_cast<int64_t>(sync) - g_lastSync;
        if (dmf >= 0 && dmf <= 64 && dsync >= 0) {
            g_t.mFrames += static_cast<uint64_t>(dmf);
            g_t.syncMs += dsync;
        }
        else {
            ++g_t.clockResets;
        }
    }
    g_lastMFrame = mf;
    g_lastSync = sync;
    g_clockValid = true;

    ++g_t.renders;
    if (g_uiTick) {
        ++g_t.rendersA;
    }
    if (g_m60) {
        ++g_t.renders60;
        g_w.any60 = true;
    }
    else {
        g_w.any30 = true;
    }
    if ((g_renderId & 63) != 0) {
        return;
    }
    int64_t now = Now();
    if (g_start == 0) {
        g_start = g_lastReport = g_lastSummary = now;
        g_tPrev = g_t;
        g_prevPresents = g_siteRun[sites::PRESENT];
        g_prevPacer = g_pacerStats;
        return;
    }
    CheckAnomalies();
    if (g_cfg.telemetry >= 1 && now - g_lastReport >= 10 * g_freq) {
        Report(now);
    }
    if (g_cfg.telemetry >= 1 && now - g_lastSummary >= 120 * g_freq) {
        LogSiteSummary();
        Log("telemetry: %llu tick checks, %llu errors; seed changes outside logic: %llu (%llu in 60 mode); "
            "C5 replays %u suppressed %u drops %u; camera B-swaps %u no-record %u A-swaps %u guard-ends %u",
            static_cast<unsigned long long>(g_t.tickChecks), static_cast<unsigned long long>(g_t.tickErrors),
            static_cast<unsigned long long>(g_t.seedOutside), static_cast<unsigned long long>(g_t.seedOutside60),
            g_c5Stats.replays, g_c5Stats.suppressed, g_c5Stats.drops, g_cameraStats.bSwaps, g_cameraStats.bNoRecord,
            g_cameraStats.aSwaps, g_cameraStats.guardEnds);
        g_lastSummary = now;
    }
}

void TelemetryOnModeChange(bool on, const char* reason)
{
    Log("mode: %s (%s) at render %u, m_frame %u", on ? "60 FPS on" : "60 FPS off", reason, g_renderId, MFrame());
    g_w.broken = true;
}

void TelemetryOnReset()
{
    g_w.broken = true;
}

using LogicUpdateFn = uint32_t(__thiscall*)(uint8_t* logic, int sub);
using GetCrcFn = uint32_t(__thiscall*)(uint8_t* logic, int deepCrcFile);

// Telemetry=2 determinism trace. After each logic tick (sub 1) the engine's own multiplayer sync checksum
// GameLogic::getCRC 0x625886 is taken, exactly as GameLogic::update does for network games (0x62E7E4..0x62E7F6:
// [0xDE87C7]=1, getCRC(0), [0xDE87C7]=0). It is read-only; it only runs at Telemetry=2.
void TraceLogicCall(uint8_t* logic, int sub)
{
    uint32_t frame = Field<uint32_t>(logic, 0x40);
    uint32_t crc = 0;
    if (sub == 1) {
        uint8_t saved = Read<uint8_t>(0xDE87C7);
        Write<uint8_t>(0xDE87C7, 1);
        crc = reinterpret_cast<GetCrcFn>(0x625886)(logic, 0);
        Write<uint8_t>(0xDE87C7, saved);
    }
    char line[96];
    int n = std::snprintf(line, sizeof(line), "%u %d %08X %08X %d\r\n", frame, sub, Read<uint32_t>(kLogicRngSeed), crc,
                          g_m60 ? 60 : 30);
    DWORD written;
    WriteFile(g_trace, line, static_cast<DWORD>(n), &written, nullptr);
}

extern "C" uint32_t __fastcall LogicUpdateWrapper(uint8_t* logic, void*, int sub)
{
    uint32_t seed = Read<uint32_t>(kLogicRngSeed);
    if (g_seedValid && seed != g_seedAtExit) {
        ++g_t.seedOutside;
        if (g_m60) {
            ++g_t.seedOutside60;
        }
    }
    ++g_t.logicCalls;
    if (sub == 1) {
        ++g_t.logicTicks;
        if (g_cfg.telemetry >= 1) {
            CloseTickWindow();
            StartTickWindow();
        }
    }
    else if (g_w.valid) {
        if (sub == g_w.nextSub) {
            ++g_w.nextSub;
        }
        else {
            g_w.broken = true;
        }
    }
    uint32_t result = reinterpret_cast<LogicUpdateFn>(kFnLogicUpdate)(logic, sub);
    g_seedAtExit = Read<uint32_t>(kLogicRngSeed);
    g_seedValid = true;
    if (g_trace != INVALID_HANDLE_VALUE) {
        TraceLogicCall(logic, sub);
    }
    return result;
}
