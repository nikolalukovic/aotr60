#pragma once
#include <cstdint>

// Decides when 60 FPS must give way to stock 30 FPS (pure logic; unit-tested in tests/test_pacer_policy.cpp).
//
// Inputs are 5-second windows of 60-mode time. Isolated hitches (shader compiles, the start of a match, a heavy
// AI tick) also cost the stock game time, so they are no reason to leave 60. Only sustained overload is:
//  - most iterations released noticeably late (the PC cannot render + step within half a stock frame), or
//  - a large share of time lost window after window, or
//  - (vsync) too many B Presents dropped to keep up with the display.
struct PacerWindow {
    double lostRatio = 0.0;     // game time lost (not caught up) / window length
    double lateRatio = 0.0;     // iterations released more than a quarter interval late / iterations
    double skipRatio = 0.0;     // late-skipped B Presents / B-renders (vsync only)
};

class FallbackPolicy {
public:
    // Feeds one window; returns true when 60 mode should fall back now.
    bool AddWindow(const PacerWindow& w);
    void Reset();
    // Backoff for the next fallback, in seconds (30, 60, 120, 240, 480; resets after 5 clean minutes).
    int NextBackoffSeconds();

private:
    int overloadedWindows_ = 0;
    int slowWindows_ = 0;
    int skipWindows_ = 0;
    int cleanWindows_ = 0;
    int fallbacks_ = 0;
};
