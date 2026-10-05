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
};
Totals g_t;
Totals g_tPrev;
uint32_t g_prevMFrame = 0;
int32_t g_prevSync = 0;
uint32_t g_prevPresents = 0;
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
    if (!g_w.valid || g_w.broken || g_w.nextSub != 7 || (g_w.any60 && g_w.any30)) {
        return;
    }
    uint32_t renders = g_renderId - g_w.renderId;
    uint32_t rendersA = static_cast<uint32_t>(g_t.rendersA - g_w.rendersA);
    uint32_t dm = MFrame() - g_w.mFrame;
    if (dm != 6) {
        return; // paused, frozen or stalled tick: no exact invariant
    }
    bool sixty = g_w.any60;
    ++g_t.tickChecks;
    uint32_t expectRenders = sixty ? 12 : 6;
    if (renders != expectRenders || rendersA != 6) {
        ++g_t.tickErrors;
        if (g_t.tickErrors <= 20 || g_t.tickErrors % 1000 == 0) {
            Log("ERROR tick invariant (%s): renders=%u (expected %u) A-renders=%u (expected 6) dm_frame=%u [error #%llu]",
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
    uint32_t mFrame = MFrame();
    int32_t sync = Read<int32_t>(kSyncAccumulator);
    uint32_t presents = g_siteRun[sites::PRESENT];
    double renders = static_cast<double>(g_t.renders - g_tPrev.renders);
    double frac60 = renders > 0 ? static_cast<double>(g_t.renders60 - g_tPrev.renders60) / renders : 0.0;
    char line[512];
    std::snprintf(line, sizeof(line),
                  "%.1f,%.3f,%.2f,%.2f,%.2f,%u,%u,%.3f,%.3f,%.2f,%.1f,%.2f,%u,%llu,%llu,%llu,%u,%u,%u\r\n",
                  static_cast<double>(now - g_start) / g_freq, frac60, renders / dt,
                  static_cast<double>(g_t.rendersA - g_tPrev.rendersA) / dt, (presents - g_prevPresents) / dt,
                  g_pacerStats.lateSkips - g_prevPacer.lateSkips, g_pacerStats.forcedSkips - g_prevPacer.forcedSkips,
                  static_cast<double>(g_t.logicTicks - g_tPrev.logicTicks) / dt,
                  static_cast<double>(g_t.logicCalls - g_tPrev.logicCalls) / dt, (mFrame - g_prevMFrame) / dt,
                  (sync - g_prevSync) / dt,
                  static_cast<double>(g_pacerStats.debtTicks - g_prevPacer.debtTicks) * 1000.0 / g_freq,
                  g_pacerStats.gaps - g_prevPacer.gaps, static_cast<unsigned long long>(g_t.seedOutside - g_tPrev.seedOutside),
                  static_cast<unsigned long long>(g_t.tickChecks - g_tPrev.tickChecks),
                  static_cast<unsigned long long>(g_t.tickErrors - g_tPrev.tickErrors), g_c5Stats.replays,
                  g_cameraStats.bSwaps, g_cameraStats.aSwaps);
    CsvLine(line);
    g_tPrev = g_t;
    g_prevMFrame = mFrame;
    g_prevSync = sync;
    g_prevPresents = presents;
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
                    "cam_b_swaps,cam_a_swaps\r\n");
        }
    }
}

void TelemetryOnPreRender()
{
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
        g_prevMFrame = MFrame();
        g_prevSync = Read<int32_t>(kSyncAccumulator);
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

using LogicUpdateFn = uint32_t(__thiscall*)(uint8_t* logic, int sub);

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
    return result;
}
