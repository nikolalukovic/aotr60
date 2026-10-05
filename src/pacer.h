#pragma once
#include <cstdint>

struct PacerStats {
    uint32_t iterations = 0;
    uint32_t gaps = 0;
    uint32_t lateSkips = 0;
    uint32_t forcedSkips = 0;
    uint32_t fallbacks = 0;
    int64_t debtTicks = 0;
    double lastDebtRatio = 0.0;
    double lastSkipRatio = 0.0;
    // release -> Present submission, per render kind (QPC ticks; sums and counts)
    int64_t relPresentTicksA = 0;
    int64_t relPresentTicksB = 0;
    uint32_t relPresentCountA = 0;
    uint32_t relPresentCountB = 0;
    // Present spacing extremes since the telemetry last reset them (QPC ticks)
    int64_t presentSpacingMin = 0;
    int64_t presentSpacingMax = 0;
    int64_t presentWaitTicks = 0;
    uint32_t lateReleases = 0;
    uint32_t onTimeIterations = 0;
    int64_t lateReleaseTicks = 0;
    int64_t pacerWaitTicks = 0;   // time spent waiting for iteration deadlines (cumulative)
    int64_t presentCallTicks = 0; // time spent inside Present (cumulative)
    int64_t owedTicks = 0;        // time currently owed after late releases
    uint32_t presentsOver25 = 0;  // Present-to-Present intervals over 25 ms (paced), cumulative
    uint32_t presentsOver34 = 0;  // ... over 34 ms: the visible hold metric
};
extern PacerStats g_pacerStats;

int64_t PacerFrequency();
int StockFrameMs(const uint8_t* engine);

// For split present (split_present.cpp), main thread.
int64_t PacerNow();
void PacerWaitUntil(int64_t qpc);
int64_t PacerNextXDeadline();                  // expected release of the next X iteration (0 if unknown)
void PacerOnDeferredPresent(int64_t callTime, int64_t returnTime, bool paced);
void PacerOnC0(bool aRender, int stepperS);    // the iteration that starts now
