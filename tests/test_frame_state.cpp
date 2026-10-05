#include "test.h"

#include "frame_state.h"

#include <vector>

namespace {

// Model of the stock main loop around GameEngine::update 0x6325A0 (see docs/analysis/agents/core.md):
// clientUpdate (m_frame++ iff GC+0xC8, then the draw with sync += 33*d), then the stepper.
struct SimEngine {
    int s = 6;            // GE+0x34
    bool c8 = true;       // GC+0xC8
    uint32_t mFrame = 0;  // GC+0x10
    uint32_t lastDrawFrame = 0;
    uint32_t logicFrame = 0;
    int64_t sync = 0;     // 0xDC7580
    bool paused = false;  // GL+0x124 (or frozen time): sub 1 does not advance

    struct LogicEvent {
        int sub;
        uint32_t logicFrame;
        uint32_t mFrame;
        int64_t sync;
        bool operator==(const LogicEvent& o) const
        {
            return sub == o.sub && logicFrame == o.logicFrame && mFrame == o.mFrame && sync == o.sync;
        }
    };
    std::vector<LogicEvent> logic;
    int renders = 0;
    int advancingRenders = 0;
    int uiTicks = 0;
    int tickAttempts = 0;

    void Step(int sub)
    {
        if (sub == 1) {
            ++tickAttempts;
            if (paused) {
                c8 = false;
                return;
            }
            ++logicFrame;
        }
        logic.push_back({sub, logicFrame, mFrame, sync});
    }

    void Iteration(FrameState& fs, bool allowed, bool resetDuringRender = false)
    {
        fs.BeginIteration(allowed, s, c8);
        // clientUpdate
        ++renders;
        if (fs.uiTick) {
            ++uiTicks;
        }
        if (c8) {
            ++mFrame;
            ++advancingRenders;
        }
        int32_t d = c8 ? static_cast<int32_t>(mFrame - lastDrawFrame) : 0;
        if (c8) {
            lastDrawFrame = mFrame;
        }
        sync += fs.SyncDelta(d, 33);
        if (resetDuringRender) {
            fs.Reset();
        }
        // halt hook at 0x6325D5
        if (fs.ForceHalt()) {
            c8 = false;
            return;
        }
        // unmodified stock stepper
        c8 = true;
        s = s + 1;
        if (s <= 6) {
            Step(s);
        }
        else {
            int old = s;
            s = 1;
            Step(1);
            if (!c8) {
                s = old;
            }
        }
    }
};

// Runs until `steps` logic events have been recorded.
SimEngine RunUntilLogicSteps(bool sixty, size_t steps)
{
    SimEngine e;
    FrameState fs;
    while (e.logic.size() < steps) {
        e.Iteration(fs, sixty);
    }
    return e;
}

} // namespace

TEST(sim_stock_mode_is_unchanged)
{
    SimEngine e;
    FrameState fs;
    for (int i = 0; i < 60; ++i) {
        e.Iteration(fs, false);
    }
    CHECK_EQ(e.renders, 60);
    CHECK_EQ(e.advancingRenders, 60);
    CHECK_EQ(e.uiTicks, 60);
    CHECK_EQ(e.sync, int64_t{60 * 33});
    CHECK_EQ(e.logic.size(), size_t{60});
}

TEST(sim_60_mode_logic_trace_identical_to_stock)
{
    const size_t steps = 6 * 50; // 50 ticks
    SimEngine stock = RunUntilLogicSteps(false, steps);
    SimEngine sixty = RunUntilLogicSteps(true, steps);
    CHECK(stock.logic.size() == sixty.logic.size());
    bool same = true;
    for (size_t i = 0; i < stock.logic.size() && i < sixty.logic.size(); ++i) {
        if (!(stock.logic[i] == sixty.logic[i])) {
            std::printf("  first difference at logic step %zu: sub %d/%d frame %u/%u m_frame %u/%u sync %lld/%lld\n", i,
                        stock.logic[i].sub, sixty.logic[i].sub, stock.logic[i].logicFrame, sixty.logic[i].logicFrame,
                        stock.logic[i].mFrame, sixty.logic[i].mFrame, stock.logic[i].sync, sixty.logic[i].sync);
            same = false;
            break;
        }
    }
    CHECK(same);
    // 60 mode renders twice per logic sub-step once enabled; client frames and sync stay stock.
    CHECK(sixty.renders > stock.renders);
    CHECK_EQ(sixty.mFrame, stock.mFrame);
}

TEST(sim_60_mode_per_tick_invariants)
{
    SimEngine e;
    FrameState fs;
    // Let 60 mode engage (needs s==1 after a successful tick).
    while (!fs.m60) {
        e.Iteration(fs, true);
    }
    for (int tick = 0; tick < 20; ++tick) {
        int renders0 = e.renders, adv0 = e.advancingRenders, ui0 = e.uiTicks;
        uint32_t m0 = e.mFrame;
        int64_t sync0 = e.sync;
        size_t logic0 = e.logic.size();
        for (int i = 0; i < 12; ++i) {
            e.Iteration(fs, true);
        }
        CHECK_EQ(e.renders - renders0, 12);
        CHECK_EQ(e.advancingRenders - adv0, 6);
        CHECK_EQ(e.uiTicks - ui0, 6);
        CHECK_EQ(e.mFrame - m0, 6u);
        CHECK_EQ(e.sync - sync0, int64_t{6 * 33});
        CHECK_EQ(e.logic.size() - logic0, size_t{6});
    }
    // Sub-steps stay strictly cyclic 1..6: none skipped, none repeated.
    for (size_t k = 1; k < e.logic.size(); ++k) {
        CHECK_EQ(e.logic[k].sub, e.logic[k - 1].sub % 6 + 1);
    }
}

TEST(sim_60_mode_sync_halves_and_is_stock_at_logic_time)
{
    SimEngine e;
    FrameState fs;
    while (!fs.m60) {
        e.Iteration(fs, true);
    }
    int64_t prev = e.sync;
    for (int i = 0; i < 24; ++i) {
        bool b = !fs.nextIsX; // the render about to happen is B if the next iteration is not X
        e.Iteration(fs, true);
        int64_t delta = e.sync - prev;
        prev = e.sync;
        CHECK(delta == 16 || delta == 17);
        (void)b;
    }
    // Every logic event sees a multiple of 33 ms of sync relative to the stock run.
    for (const auto& ev : e.logic) {
        CHECK_EQ(ev.sync, int64_t{33} * ev.mFrame);
    }
}

namespace {

// Pauses and runs until the first failed tick attempt (pause only takes effect at sub 1, as in stock).
void PauseUntilStalled(SimEngine& e, FrameState& fs, bool sixty)
{
    e.paused = true;
    while (e.s < 7) {
        e.Iteration(fs, sixty);
    }
}

} // namespace

TEST(sim_pause_keeps_stock_attempt_rate_and_freezes_sync)
{
    // Stock: one tick attempt per render while paused (30/s), sync frozen.
    SimEngine stock;
    FrameState fsStock;
    for (int i = 0; i < 30; ++i) {
        stock.Iteration(fsStock, false);
    }
    PauseUntilStalled(stock, fsStock, false);
    int attempts0 = stock.tickAttempts;
    int64_t sync0 = stock.sync;
    for (int i = 0; i < 30; ++i) {
        stock.Iteration(fsStock, false);
    }
    CHECK_EQ(stock.tickAttempts - attempts0, 30);
    CHECK_EQ(stock.sync, sync0);

    // 60 mode: 60 renders in the same wall time, still 30 attempts; A-only work alternates; sync frozen.
    SimEngine sixty;
    FrameState fs;
    while (!fs.m60) {
        sixty.Iteration(fs, true);
    }
    for (int i = 0; i < 12; ++i) {
        sixty.Iteration(fs, true);
    }
    PauseUntilStalled(sixty, fs, true);
    // Finish the pair in progress so the measurement starts on an X iteration.
    if (!fs.nextIsX) {
        sixty.Iteration(fs, true);
    }
    attempts0 = sixty.tickAttempts;
    sync0 = sixty.sync;
    int ui0 = sixty.uiTicks;
    for (int i = 0; i < 60; ++i) {
        sixty.Iteration(fs, true);
    }
    CHECK_EQ(sixty.tickAttempts - attempts0, 30);
    CHECK_EQ(sixty.uiTicks - ui0, 30);
    CHECK_EQ(sixty.sync, sync0);
    CHECK(fs.m60);
}

TEST(sim_reset_during_render_returns_to_stock_immediately)
{
    SimEngine e;
    FrameState fs;
    while (!fs.m60) {
        e.Iteration(fs, true);
    }
    e.Iteration(fs, true); // X
    e.Iteration(fs, true); // Y
    size_t logic0 = e.logic.size();
    e.Iteration(fs, true, /*resetDuringRender=*/true); // X, but GameEngine::reset runs inside the A-render
    CHECK(!fs.m60);
    CHECK_EQ(e.logic.size(), logic0 + 1); // the stepper was not halted: stock step ran
    // With the predicate false, 60 mode stays off and every iteration is stock.
    int renders0 = e.renders;
    size_t logic1 = e.logic.size();
    for (int i = 0; i < 10; ++i) {
        e.Iteration(fs, false);
    }
    CHECK_EQ(e.renders - renders0, 10);
    CHECK_EQ(e.logic.size() - logic1, size_t{10});
}

TEST(sim_mode_switches_only_at_pair_boundaries)
{
    SimEngine e;
    FrameState fs;
    while (!fs.m60) {
        e.Iteration(fs, true); // the iteration that enables 60 mode is already an X iteration
    }
    e.Iteration(fs, true); // Y
    e.Iteration(fs, true); // X
    // Predicate turns false before a Y iteration: 60 mode must finish the pair first.
    e.Iteration(fs, false); // Y
    CHECK(fs.m60);
    e.Iteration(fs, false); // boundary: switches off, stock iteration
    CHECK(!fs.m60);
    CHECK_EQ(fs.uiTick, uint8_t{1});
}

TEST(presentation_fraction_and_cache_keys)
{
    CHECK_EQ(FrameState::PresentationFraction(1), 1.0f / 12.0f);
    CHECK_EQ(FrameState::PresentationFraction(6), 11.0f / 12.0f);
    CHECK_EQ(FrameState::PresentationFraction(7), 1.0f);
    CHECK_EQ(FrameState::PresentationFraction(0), 1.0f);

    FrameState fs;
    CHECK_EQ(fs.CacheKey(1234), 1234u); // 30 mode: stock getFrame()
    fs.m60 = 1;
    CHECK_EQ(fs.CacheKey(1234), 0x80000000u | 2468u);
    fs.presWindow = 1;
    CHECK_EQ(fs.CacheKey(1234), 0x80000000u | 2467u);
    CHECK(fs.CacheKey(0x7FFFFFFF) != 0xFFFFFFFFu); // never the invalidation value
}
