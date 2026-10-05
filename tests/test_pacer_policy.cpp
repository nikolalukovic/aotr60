#include "test.h"

#include "pacer_policy.h"

namespace {

PacerWindow W(double lost, double late = 0.0, double skip = 0.0)
{
    PacerWindow w;
    w.lostRatio = lost;
    w.lateRatio = late;
    w.skipRatio = skip;
    return w;
}

} // namespace

TEST(fallback_ignores_the_hitches_of_the_play_test_session)
{
    // 5 s windows derived from the 2026-10-05 12:22 session (lost time 1-4 % from 100-200 ms hitches and
    // overrunning logic ticks, 2-10 % of iterations late), including the 13 % loading hitch at match start.
    FallbackPolicy p;
    const double lost[] = {0.13, 0.04, 0.031, 0.035, 0.02, 0.03, 0.006, 0.012, 0.018, 0.012, 0.022,
                           0.011, 0.024, 0.023, 0.018, 0.009, 0.0, 0.042, 0.022};
    const double late[] = {0.20, 0.08, 0.06, 0.07, 0.03, 0.05, 0.01, 0.02, 0.03, 0.02, 0.05,
                           0.04, 0.06, 0.08, 0.08, 0.08, 0.09, 0.11, 0.09};
    for (size_t i = 0; i < sizeof(lost) / sizeof(lost[0]); ++i) {
        CHECK(!p.AddWindow(W(lost[i], late[i])));
    }
}

TEST(fallback_on_a_pc_that_cannot_render_at_60)
{
    FallbackPolicy p;
    CHECK(!p.AddWindow(W(0.30, 0.95)));
    CHECK(p.AddWindow(W(0.31, 0.97))); // two overloaded windows in a row
}

TEST(fallback_on_sustained_slowness)
{
    // Iterations just over budget: each is only a little late, but time is lost every window.
    FallbackPolicy p;
    CHECK(!p.AddWindow(W(0.08, 0.1)));
    CHECK(!p.AddWindow(W(0.09, 0.1)));
    CHECK(p.AddWindow(W(0.08, 0.1)));
}

TEST(fallback_needs_consecutive_bad_windows)
{
    FallbackPolicy p;
    CHECK(!p.AddWindow(W(0.01, 0.9))); // overloaded (most iterations late) but little time lost
    CHECK(!p.AddWindow(W(0.01, 0.0)));
    CHECK(!p.AddWindow(W(0.01, 0.9)));
    CHECK(!p.AddWindow(W(0.06)));      // slow
    CHECK(!p.AddWindow(W(0.06)));      // slow
    CHECK(!p.AddWindow(W(0.01)));
    CHECK(!p.AddWindow(W(0.06)));
    CHECK(!p.AddWindow(W(0.06)));
    CHECK(p.AddWindow(W(0.30)));       // third slow window in a row
}

TEST(fallback_on_vsync_present_drops)
{
    FallbackPolicy p;
    CHECK(!p.AddWindow(W(0.0, 0.0, 0.012))); // 60 Hz FIFO deficit: about 1 % of B Presents, normal
    CHECK(!p.AddWindow(W(0.0, 0.0, 0.20)));
    CHECK(p.AddWindow(W(0.0, 0.0, 0.25)));
}

TEST(fallback_backoff_doubles_and_resets_after_clean_play)
{
    FallbackPolicy p;
    CHECK_EQ(p.NextBackoffSeconds(), 30);
    CHECK_EQ(p.NextBackoffSeconds(), 60);
    CHECK_EQ(p.NextBackoffSeconds(), 120);
    for (int i = 0; i < 60; ++i) {
        CHECK(!p.AddWindow(W(0.005)));
    }
    CHECK_EQ(p.NextBackoffSeconds(), 30);
}
