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
};
extern PacerStats g_pacerStats;

int64_t PacerFrequency();
int StockFrameMs(const uint8_t* engine);
