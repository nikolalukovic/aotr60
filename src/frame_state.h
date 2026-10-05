#pragma once
#include <cstdint>

// Pure frame-state logic of the 60 FPS mode (no game memory access), shared by the stubs and the tests.
//
// 60 mode splits every stock main-loop iteration into two:
//   X: A-render (the stock render) followed by a halted step (no logic),
//   Y: B-render (presentation only)  followed by the unmodified stock step (logic sub-step).
// See aotr60/docs/PLAN.md §1.

struct FrameState {
    // ---- read by assembly stubs (keep layout simple) ----
    uint8_t m60 = 0;        // 60 mode active
    uint8_t uiTick = 1;     // 1 on X renders (and always in 30 mode): A-only work runs
    uint8_t inB = 0;        // current render is a B-render
    uint8_t presWindow = 0; // inside an A-render presentation window
    uint32_t renderId = 0;

    // ---- internal ----
    bool nextIsX = true;          // parity of the next 60-mode iteration
    bool prevAdvancingA = false;  // previous render was an A-render that advanced m_frame
    bool syncToggle = false;
    int32_t owedSyncMs = 0;

    // Called at C0, before clientUpdate. `allowed` is the mode predicate (PLAN §1.8); `stepperS` and
    // `logicAdvanced` (GC+0xC8) are the values the previous stepper left behind.
    void BeginIteration(bool allowed, int stepperS, bool logicAdvanced);

    // Halt hook decision, right after clientUpdate: force the stepper's halted branch?
    bool ForceHalt() const { return m60 && uiTick; }

    // Reset hook (GameEngine::reset): back to stock immediately.
    void Reset();

    // C3: milliseconds to add to the W3D sync accumulator for this render. `frames` is the stock m_frame
    // delta d (0 when the render is frozen), `frameLengthMs` the stock [0xDC7A8C] (33).
    int32_t SyncDelta(int32_t frames, int32_t frameLengthMs);

    // Interpolation fraction presented by the A-render's presentation window for stepper value s.
    static float PresentationFraction(int s);

    // C4 drawable-cache key replacing getFrame() at the three cache sites. Never collides across modes:
    // 30 mode = m_frame (stock); 60 mode = 0x80000000 | (2*m_frame - presWindow).
    uint32_t CacheKey(uint32_t mFrame) const;
};
