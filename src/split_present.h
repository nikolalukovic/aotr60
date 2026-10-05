#pragma once
#include <cstdint>

// Split Present (heavy logic steps). The B-render of a 60-mode pair is fully drawn before the logic sub-step that
// follows it. When that step is long (large battles), the screen would hold the B frame for the whole step plus the
// next A-render. Instead the B frame's Present is deferred: a timer thread marks it due at a target time (the middle
// of the predicted gap) and the main thread presents it from the next cheap checkpoint inside the logic step, or at
// the latest right after the step. All Direct3D calls stay on the main thread, under the game's own DX mutex.

struct Config;

struct SplitStats {
    uint32_t handoffs = 0;        // B Presents offered for deferral
    uint32_t deferred = 0;
    uint32_t notDeferred[12] = {}; // by reason (SpGate)
    uint32_t checkpointPresents[8] = {};
    uint32_t drainPresents = 0;
    uint32_t drainWaited = 0;
    uint32_t nets[6] = {};        // by SpNetReason
    uint32_t cancels[8] = {};     // by SpCancelReason
    uint32_t mutexOwnedSkips = 0;
    uint32_t tryLockFails = 0;
    uint32_t fpChanged = 0;       // x87 control word or MXCSR changed by a Present (restored; expected 0)
    uint32_t seedChanged = 0;     // logic RNG seed changed across a Present (must be 0)
    uint32_t presentErrors = 0;
    uint32_t deviceLost = 0;
    uint32_t inLogicPresents = 0;
    int64_t inLogicTicks = 0;
    int64_t inLogicMax = 0;
    int64_t lateSum = 0;          // Present call time minus target (ticks), presented jobs
    int64_t lateMax = 0;
    uint32_t lateCount = 0;
    int64_t wakeLateMax = 0;      // timer thread wake-up lateness
    uint32_t earlyReleases = 0;
    int64_t earlyTicks = 0;
    uint32_t borrowedNotDeferred = 0; // released early but the B was then not deferred (expected rare)
    uint32_t retries = 0;             // checkpoint Presents postponed because the DX mutex was busy
    uint32_t predictions = 0;
    uint32_t emptySlots = 0;
    uint32_t outlierClips = 0;
    // profile mode
    uint32_t profileHits[8] = {};
    int64_t profileGapMax[7] = {}; // per sub: longest stretch without a checkpoint
};
extern SplitStats g_spStats;

enum SpGate : int {
    kGateOff, kGateUnpaced, kGateFifoFull, kGateBusy, kGateGame, kGatePaused, kGateLwMap, kGateCapture,
    kGateNotForeground, kGateCooldown, kGateNoTarget, kGateCount
};
enum SpNetReason : int { kNetPump, kNetTcl, kNetBeginRender, kNetPresent, kNetC0, kNetCount };
enum SpCancelReason : int {
    kCancelReset, kCancelEngineReset, kCancelMode, kCancelIconic, kCancelShutdown, kCancelOffMain,
    kCancelMutexTimeout, kCancelSecondPresent, kCancelCount
};

void SplitPresentInit(const Config& cfg, bool sitesInstalled);
bool SplitPresentInstalled();          // the checkpoint sites are in the game image
bool SplitPresentActive();             // deferral allowed now (installed, mode on, hotkey on, healthy)
bool SplitPresentEarlyRelease();       // stage S2 enabled
bool SplitPresentStress();             // SplitPresent=3: every gated B is due at once
void SplitPresentToggleUser();         // Ctrl+Shift+F9
bool SplitPresentUserOn();
void SplitPresentOnC0();               // safety net + lazy start of the timer thread (main thread)
void SplitPresentReset(int cancelReason); // mode switch / engine reset: cancel and forget the predictor
void SplitPresentCooldown();           // no deferral for 2 s (pacing gap, alt-tab, Reset, device loss)
bool SpDeferLikely(bool queueRoom);    // the time-independent deferral gates hold (for the early release)

// Predicted duration (QPC ticks) of the logic call that follows the coming B-render, or -1 when unknown.
int64_t SpPredictNextLogic();
// The logic wrapper (main thread) around every GameLogic::update call.
void SpOnLogicBegin(uint8_t* logic, int sub);
void SpOnLogicEnd(uint8_t* logic, int sub, uint32_t frameBefore, int64_t ticks);
int64_t SpLogicEndTime();              // QPC at the end of the last logic call
int64_t SpPresentInCall();             // Present time inside the last logic call (excluded from logic timings)
int64_t SpTakeDrainWait();             // drain wait since the last call (excluded from the Y tail estimate)

// B-render handoff from AotR60_PresentSkip (main thread). `queueRoom`: no vsync or the FIFO queue has room.
// Returns true when the Present is deferred (the caller returns 2: do not present now).
bool SpTryDefer(int64_t now, int64_t target, bool paced, bool queueRoom);
bool SpPending();
void SpCancelBeforePresent();          // a new Present while one is pending (should not happen): drop the old one

extern "C" {
extern volatile long g_ppState;        // 0 idle, 1 pending, 2 presenting
extern volatile uint8_t g_cpDue;       // the only byte the checkpoint fast path reads
extern volatile uint8_t g_inLogic;
void __cdecl AotR60_Checkpoint(int index);
void __cdecl AotR60_SpDrain();
void __cdecl AotR60_SpNet(int reason);
void __cdecl AotR60_SpCancel(int reason);
void __cdecl AotR60_SpShutdown();
}
