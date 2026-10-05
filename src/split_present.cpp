// Split Present: show the inserted B frame in the middle of a long logic step (see split_present.h).
//
// State machine (g_ppState): IDLE -> PENDING at the B-render's Present (AotR60_PresentSkip returns 2, the stock
// code continues as if presented) -> PRESENTING while the main thread calls the device Present -> IDLE. Only the
// main thread presents; any thread may cancel a PENDING job (the B frame is then never shown, like a skipped Present).
// Present points, in order: a logic checkpoint once the timer thread has set g_cpDue; the drain right after the
// logic step (GameEngine::update returned); the nets at the window pump, the next frame's start, a second Present
// and C0. Cancels: Reset_Device, mode switches, engine reset, iconic / background window, shutdown.

#include "split_present.h"

#include "config.h"
#include "log.h"
#include "pacer.h"
#include "runtime.h"

#include <cstring>
#include <immintrin.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

using namespace game;

extern "C" {
volatile long g_ppState = 0;
volatile uint8_t g_cpDue = 0;
volatile uint8_t g_inLogic = 0;
alignas(16) uint8_t g_cpFx[512]; // fxsave area of the checkpoint slow path (stubs.asm, main thread only)
}

SplitStats g_spStats;

namespace {

constexpr long kIdle = 0;
constexpr long kPending = 1;
constexpr long kPresenting = 2;
constexpr uint32_t kDevice = 0xDD3474;     // IDirect3DDevice9*
constexpr uint32_t kDxMutexOwner = 0xDD34C8;
constexpr uint32_t kDxMutexCount = 0xDD34CC;
constexpr uint32_t kFnDxTryLock = 0x51EF50; // bool __cdecl (DWORD timeoutMs): the game's DX mutex
constexpr uint32_t kFnDxUnlock = 0x5208D0;  // bool __cdecl ()
constexpr long kDeviceLost = static_cast<long>(0x88760868);

int g_mode = 0;          // SplitPresent: 0 off, 1 on, 2 profile, 3 stress
bool g_installed = false;
bool g_early = false;
bool g_native = false;
bool g_user = true;      // Ctrl+Shift+F9
bool g_disabled = false; // sticky for the session after a failure
int g_wrapperOk = -1;    // d3d9.dll is the game folder's (DXVK) or SplitPresentNative=1; -1 = not checked yet
volatile long long g_cooldownUntil = 0; // written from several threads: interlocked

// Timer thread (never touches Direct3D, game memory or locks).
HANDLE g_thread = nullptr;
HANDLE g_arm = nullptr;
bool g_threadTried = false;
volatile long g_armGen = 0;
volatile long long g_armTarget = 0;
volatile long g_quit = 0;

struct Pending {
    uint32_t gen = 0;
    int64_t handoff = 0;
    int64_t target = 0;
    bool paced = false;
    int retries = 0;
};
Pending g_pb;
uint32_t g_gen = 0;
int64_t g_presentInCall = 0; // Present time inside the current logic call (not learned as logic)
int64_t g_logicEnd = 0;
int64_t g_drainWait = 0;
alignas(16) uint8_t g_spFx[512];
alignas(16) uint8_t g_spFxAfter[512];

// Logic cost per (logic frame % 10, sub): the AI players work every 5 and 10 logic frames.
int64_t g_hist[10][7][3] = {};
uint8_t g_histN[10][7] = {};
uint8_t g_histPos[10][7] = {};

// Profile mode: checkpoint coverage of every 10th logic frame.
bool g_profiling = false;
int g_profileSub = 0;
int64_t g_profileLast = 0;

int64_t Freq()
{
    return PacerFrequency();
}

int64_t Ms(double ms)
{
    return static_cast<int64_t>(ms * static_cast<double>(Freq()) / 1000.0);
}

void Cooldown()
{
    InterlockedExchange64(&g_cooldownUntil, PacerNow() + 2 * Freq());
}

int64_t CooldownUntil()
{
    return InterlockedCompareExchange64(&g_cooldownUntil, 0, 0);
}

// The due byte carries the job's generation, so a late store by the timer thread for an older job can never make
// a newer job due early (the asm fast path only tests it for nonzero).
uint8_t DueTag(uint32_t gen)
{
    return static_cast<uint8_t>(0x80 | (gen & 0x7F));
}

void Arm(int64_t target)
{
    InterlockedExchange64(&g_armTarget, target);
    InterlockedExchange(&g_armGen, static_cast<long>(g_pb.gen));
    SetEvent(g_arm);
}

bool ForegroundOk()
{
    HWND hwnd = Read<HWND>(0xDD3014);
    return hwnd && GetForegroundWindow() == hwnd && !IsIconic(hwnd);
}

bool CheckWrapper()
{
    if (g_wrapperOk >= 0) {
        return g_wrapperOk == 1;
    }
    wchar_t exe[MAX_PATH] = {};
    wchar_t d3d[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    HMODULE m = GetModuleHandleW(L"d3d9.dll");
    if (m) {
        GetModuleFileNameW(m, d3d, MAX_PATH);
    }
    wchar_t* slash = wcsrchr(exe, L'\\');
    size_t dirLen = slash ? static_cast<size_t>(slash - exe + 1) : 0;
    bool local = m && dirLen > 0 && _wcsnicmp(exe, d3d, dirLen) == 0 && !wcschr(d3d + dirLen, L'\\');
    g_wrapperOk = (local || g_native) ? 1 : 0;
    Log("split present: d3d9 = %ls (%s)%s", m ? d3d : L"(not loaded)", local ? "game folder, DXVK" : "system",
        g_wrapperOk ? "" : " - disabled (set SplitPresentNative=1 to allow it with the system d3d9)");
    return g_wrapperOk == 1;
}

long long ReadTarget()
{
    return InterlockedCompareExchange64(&g_armTarget, 0, 0);
}

void WaitUntilQpc(HANDLE timer, int64_t target)
{
    // Sleep in slices of at most 2 ms until 0.7 ms before the target, then spin (as the pacer's WaitUntil).
    int64_t spinFrom = target - Ms(0.7);
    for (;;) {
        int64_t now = PacerNow();
        if (now >= spinFrom) {
            return;
        }
        int64_t us = (spinFrom - now) * 1000000 / Freq();
        if (us > 2000) {
            us = 2000;
        }
        LARGE_INTEGER due;
        due.QuadPart = -us * 10;
        if (!timer || !SetWaitableTimerEx(timer, &due, 0, nullptr, nullptr, nullptr, 0)) {
            Sleep(1);
            return;
        }
        WaitForSingleObject(timer, 10);
        return; // re-check the job between slices
    }
}

DWORD WINAPI TimerThread(void*)
{
    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!timer) {
        timer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    }
    for (;;) {
        WaitForSingleObject(g_arm, INFINITE);
        if (g_quit) {
            break;
        }
        long gen = g_armGen;
        int64_t target = ReadTarget();
        for (;;) {
            if (g_quit || g_armGen != gen || g_ppState != kPending) {
                break;
            }
            int64_t now = PacerNow();
            if (now >= target) {
                g_cpDue = DueTag(static_cast<uint32_t>(gen));
                int64_t late = now - target;
                if (late > g_spStats.wakeLateMax) {
                    g_spStats.wakeLateMax = late; // benign race: statistics only
                }
                break;
            }
            if (target - now > Ms(0.7)) {
                WaitUntilQpc(timer, target);
            }
            else {
                YieldProcessor();
            }
        }
    }
    if (timer) {
        CloseHandle(timer);
    }
    return 0;
}

bool StartThread()
{
    if (g_threadTried) {
        return g_thread != nullptr;
    }
    g_threadTried = true;
    g_arm = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (g_arm) {
        g_thread = CreateThread(nullptr, 64 * 1024, TimerThread, nullptr, 0, nullptr);
    }
    if (!g_thread) {
        Log("split present: timer thread could not be started (error %lu) - disabled", GetLastError());
        g_disabled = true;
        return false;
    }
    SetThreadPriority(g_thread, THREAD_PRIORITY_HIGHEST);
    Log("split present: timer thread started");
    return true;
}

using TryLockFn = bool(__cdecl*)(DWORD);
using UnlockFn = bool(__cdecl*)();
using PresentFn = long(__stdcall*)(void* device, const RECT*, const RECT*, HWND, const void*);

bool LockDx(bool checkpoint)
{
    DWORD tid = GetCurrentThreadId();
    if (checkpoint && Read<uint32_t>(kDxMutexOwner) == tid && Read<uint32_t>(kDxMutexCount) > 0) {
        ++g_spStats.mutexOwnedSkips; // inside a bracketed D3D sequence: retry at the next checkpoint
        return false;
    }
    if (checkpoint) {
        if (reinterpret_cast<TryLockFn>(kFnDxTryLock)(0)) {
            return true;
        }
        ++g_spStats.tryLockFails;
        return false;
    }
    for (int i = 0; i < 50; ++i) {
        if (reinterpret_cast<TryLockFn>(kFnDxTryLock)(2)) {
            return true;
        }
    }
    ++g_spStats.tryLockFails;
    return false;
}

void Cancel(int reason)
{
    if (InterlockedCompareExchange(&g_ppState, kIdle, kPending) == kPending) {
        g_cpDue = 0;
        ++g_spStats.cancels[reason];
        if (g_spStats.cancels[reason] <= 5) {
            Log("split present: B Present cancelled (reason %d) at render %u", reason, g_renderId);
        }
        Cooldown();
    }
}

// Presents the pending B frame now (main thread). `checkpoint`: called from inside logic, never wait.
bool PresentNow(bool checkpoint)
{
    if (g_ppState != kPending) {
        return false;
    }
    // Own the DX mutex before claiming the job: the cancels that matter (Reset, other threads' frames) run under it.
    if (!LockDx(checkpoint)) {
        if (checkpoint) {
            g_cpDue = 0;
            if (++g_pb.retries <= 8) {
                ++g_spStats.retries;
                Arm(PacerNow() + Ms(0.25)); // try again shortly; the drain is the backstop
            }
            return false;
        }
        Cancel(kCancelMutexTimeout);
        g_disabled = true;
        Log("split present: DX mutex not available (owner thread %u) - disabled", Read<uint32_t>(kDxMutexOwner));
        return false;
    }
    if (InterlockedCompareExchange(&g_ppState, kPresenting, kPending) != kPending) {
        reinterpret_cast<UnlockFn>(kFnDxUnlock)(); // cancelled meanwhile
        return false;
    }
    g_cpDue = 0;
    uint8_t* device = Ptr(kDevice);
    if (!device) {
        g_ppState = kIdle;
        ++g_spStats.cancels[kCancelReset];
        Cooldown();
        reinterpret_cast<UnlockFn>(kFnDxUnlock)();
        return false;
    }
    _fxsave(g_spFx);
    DWORD lastError = GetLastError();
    uint32_t seed = Read<uint32_t>(kLogicRngSeed);
    auto present = *reinterpret_cast<PresentFn*>(*reinterpret_cast<uint8_t**>(device) + 0x44);
    int64_t t0 = PacerNow();
    long hr = present(device, nullptr, nullptr, nullptr, nullptr);
    int64_t t1 = PacerNow();
    ++g_siteRun[sites::PRESENT];
    _fxsave(g_spFxAfter);
    // Control bits only: FCW at offset 0, MXCSR at offset 24 (its sticky exception flags may legitimately change).
    uint16_t fcw0, fcw1;
    uint32_t mx0, mx1;
    std::memcpy(&fcw0, g_spFx, 2);
    std::memcpy(&fcw1, g_spFxAfter, 2);
    std::memcpy(&mx0, g_spFx + 24, 4);
    std::memcpy(&mx1, g_spFxAfter + 24, 4);
    if ((fcw0 & 0x1F3F) != (fcw1 & 0x1F3F) || (mx0 & 0xFFC0) != (mx1 & 0xFFC0)) {
        ++g_spStats.fpChanged;
    }
    _fxrstor(g_spFx);
    SetLastError(lastError);
    if (Read<uint32_t>(kLogicRngSeed) != seed) {
        ++g_spStats.seedChanged;
        g_disabled = true;
        Log("ERROR split present: the logic RNG seed changed across a Present - disabled");
    }
    reinterpret_cast<UnlockFn>(kFnDxUnlock)();

    PacerOnDeferredPresent(t0, t1, g_pb.paced);
    if (hr == kDeviceLost) {
        g_gapDevLost = 1;
        ++g_spStats.deviceLost;
        Cooldown();
    }
    else if (hr < 0) {
        if (++g_spStats.presentErrors <= 5) {
            Log("split present: deferred Present returned 0x%08lX", static_cast<unsigned long>(hr));
        }
    }
    if (g_inLogic) {
        g_presentInCall += t1 - t0;
        ++g_spStats.inLogicPresents;
        g_spStats.inLogicTicks += t1 - t0;
        if (t1 - t0 > g_spStats.inLogicMax) {
            g_spStats.inLogicMax = t1 - t0;
        }
    }
    int64_t late = t0 - g_pb.target;
    g_spStats.lateSum += late > 0 ? late : 0;
    if (late > g_spStats.lateMax) {
        g_spStats.lateMax = late;
    }
    ++g_spStats.lateCount;
    g_ppState = kIdle;
    return true;
}

bool NextLogicSlot(int* frameMod, int* sub)
{
    uint8_t* ge = Ptr(kTheGameEngine);
    uint8_t* gl = Ptr(kTheGameLogic);
    if (!ge || !gl || Field<uint8_t>(gl, 0x124)) {
        return false; // no logic or paused
    }
    // Stepper 0x632622: the next stock step runs s+1; past 6 it is sub 1 of the next logic frame.
    int s = Field<int32_t>(ge, 0x34);
    uint32_t frame = Field<uint32_t>(gl, 0x40);
    if (s < 1 || s > 6) {
        return false; // stalled (frozen time, failed ticks): the next call repeats a failed sub-1 attempt
    }
    if (s <= 5) {
        *sub = s + 1;
        *frameMod = static_cast<int>(frame % 10);
    }
    else {
        *sub = 1;
        *frameMod = static_cast<int>((frame + 1) % 10);
    }
    return true;
}

int64_t Median(int fm, int sub)
{
    uint8_t n = g_histN[fm][sub];
    const int64_t* v = g_hist[fm][sub];
    if (n == 0) {
        return -1;
    }
    if (n == 1) {
        return v[0];
    }
    if (n == 2) {
        return v[0] < v[1] ? v[0] : v[1];
    }
    int64_t a = v[0], b = v[1], c = v[2];
    if (a > b) {
        int64_t t = a;
        a = b;
        b = t;
    }
    if (b > c) {
        b = c;
    }
    return a > b ? a : b;
}

void Flush()
{
    std::memset(g_histN, 0, sizeof(g_histN));
    std::memset(g_histPos, 0, sizeof(g_histPos));
}

} // namespace

void SplitPresentInit(const Config& cfg, bool sitesInstalled)
{
    g_mode = cfg.splitPresent;
    g_installed = sitesInstalled && g_mode != 0;
    g_early = cfg.splitPresentEarly;
    g_native = cfg.splitPresentNative;
    if (g_mode != 0) {
        Log("split present: mode %d (%s), early release %s, %s", g_mode,
            g_mode == 1 ? "on" : g_mode == 2 ? "profile" : g_mode == 3 ? "stress" : "?", g_early ? "on" : "off",
            g_installed ? "checkpoints installed" : "checkpoints NOT installed");
    }
}

bool SplitPresentInstalled()
{
    return g_installed;
}

bool SplitPresentActive()
{
    return g_installed && (g_mode == 1 || g_mode == 3) && g_user && !g_disabled && g_wrapperOk == 1 && g_thread;
}

bool SplitPresentEarlyRelease()
{
    return g_early && SplitPresentActive();
}

bool SplitPresentStress()
{
    return g_mode == 3;
}

void SplitPresentToggleUser()
{
    g_user = !g_user;
    Log("hotkey: split present %s", g_user ? "on" : "off");
}

bool SplitPresentUserOn()
{
    return g_user;
}

void SplitPresentOnC0()
{
    if (!g_installed) {
        return;
    }
    if (g_ppState != kIdle) {
        AotR60_SpNet(kNetC0); // a job outlived its iteration (exception unwind past the drain?)
    }
    if (g_wrapperOk < 0 && Ptr(kDevice)) {
        CheckWrapper();
    }
    if (g_mode != 0 && g_mode != 2 && !ForegroundOk()) {
        Cooldown(); // the 2 s count from when focus / restore comes back
    }
    if ((g_mode == 1 || g_mode == 3) && g_wrapperOk == 1 && !g_disabled) {
        StartThread();
    }
}

void SplitPresentReset(int cancelReason)
{
    if (!g_installed) {
        return;
    }
    Cancel(cancelReason);
    Flush();
    Cooldown();
}

void SplitPresentCooldown()
{
    if (g_installed) {
        Cooldown();
    }
}

int64_t SpPredictNextLogic()
{
    int fm = 0;
    int sub = 0;
    if (!NextLogicSlot(&fm, &sub)) {
        return -1;
    }
    ++g_spStats.predictions;
    int64_t v = Median(fm, sub);
    if (v < 0) {
        ++g_spStats.emptySlots;
    }
    return v;
}

void SpOnLogicBegin(uint8_t* logic, int sub)
{
    g_presentInCall = 0;
    g_inLogic = OnMainThread() ? 1 : 0;
    uint32_t frame = Field<uint32_t>(logic, 0x40);
    uint32_t profiled = sub == 1 ? frame + 1 : frame; // sub 1 produces the next logic frame
    if (g_mode == 2 && g_installed && g_inLogic && profiled % 10 == 0) {
        g_profiling = true;
        g_profileSub = sub >= 1 && sub <= 6 ? sub : 0;
        g_profileLast = PacerNow();
        g_cpDue = 1;
    }
}

void SpOnLogicEnd(uint8_t* logic, int sub, uint32_t frameBefore, int64_t ticks)
{
    g_inLogic = 0;
    g_logicEnd = PacerNow();
    if (g_profiling) {
        int64_t gap = g_logicEnd - g_profileLast;
        if (gap > g_spStats.profileGapMax[g_profileSub]) {
            g_spStats.profileGapMax[g_profileSub] = gap;
        }
        g_profiling = false;
        g_cpDue = 0;
    }
    if (!g_installed || sub < 1 || sub > 6) {
        return;
    }
    int64_t d = ticks - g_presentInCall;
    uint32_t frame = Field<uint32_t>(logic, 0x40);
    bool advanced = sub >= 2 || frame == frameBefore + 1; // a failed (frozen) sub-1 attempt is not learned
    if (!advanced || frame < 25 || frame == 2 || Field<uint8_t>(logic, 0x125) || g_gapDevLost || g_gapReset) {
        return;
    }
    int fm = static_cast<int>(frame % 10);
    int64_t median = Median(fm, sub);
    if (g_histN[fm][sub] >= 2 && d > 3 * median + Ms(5.0)) {
        d = 3 * median + Ms(5.0); // one-off spike: do not replay it as a prediction
        ++g_spStats.outlierClips;
    }
    uint8_t& pos = g_histPos[fm][sub];
    g_hist[fm][sub][pos] = d;
    pos = static_cast<uint8_t>((pos + 1) % 3);
    if (g_histN[fm][sub] < 3) {
        ++g_histN[fm][sub];
    }
}

int64_t SpLogicEndTime()
{
    return g_logicEnd;
}

int64_t SpPresentInCall()
{
    return g_presentInCall;
}

int64_t SpTakeDrainWait()
{
    int64_t w = g_drainWait;
    g_drainWait = 0;
    return w;
}

namespace {

// Gates that do not depend on the target time; -1 when deferral is allowed.
int StateGate(bool queueRoom)
{
    uint8_t* gl = Ptr(kTheGameLogic);
    if (!SplitPresentActive()) {
        return kGateOff;
    }
    if (!queueRoom) {
        return kGateFifoFull;
    }
    if (g_ppState != kIdle) {
        return kGateBusy;
    }
    if (!gl) {
        return kGateGame;
    }
    if (Field<uint8_t>(gl, 0x124)) {
        return kGatePaused;
    }
    uint32_t mode = Field<uint32_t>(gl, 0x110);
    if (Field<uint8_t>(gl, 0x125) || (mode != 0 && mode != 2 && mode != 6)) {
        return kGateLwMap;
    }
    if (Read<int32_t>(0xDC7568) > 0 || (Ptr(kGlobalData) && Field<uint8_t>(Ptr(kGlobalData), 0xEA0))) {
        return kGateCapture; // a frame save or tile capture reads the back buffer after Present
    }
    if (g_gapReset || g_gapDevLost || PacerNow() < CooldownUntil()) {
        return kGateCooldown;
    }
    if (!ForegroundOk()) {
        return kGateNotForeground;
    }
    return -1;
}

} // namespace

bool SpDeferLikely(bool queueRoom)
{
    return StateGate(queueRoom) < 0;
}

bool SpTryDefer(int64_t now, int64_t target, bool paced, bool queueRoom)
{
    ++g_spStats.handoffs;
    int gate = paced ? StateGate(queueRoom) : kGateUnpaced;
    if (gate < 0 && g_mode != 3 && target <= now + Ms(0.3)) {
        gate = kGateNoTarget; // due now anyway: present normally
    }
    if (gate >= 0) {
        ++g_spStats.notDeferred[gate];
        return false;
    }
    if (InterlockedCompareExchange(&g_ppState, kPending, kIdle) != kIdle) {
        ++g_spStats.notDeferred[kGateBusy];
        return false;
    }
    g_pb.gen = ++g_gen;
    g_pb.handoff = now;
    g_pb.target = g_mode == 3 ? now : target;
    g_pb.paced = paced;
    g_pb.retries = 0;
    ++g_spStats.deferred;
    if (g_mode == 3) {
        g_cpDue = DueTag(g_pb.gen); // stress: present at the first checkpoint
        return true;
    }
    g_cpDue = 0;
    Arm(g_pb.target);
    return true;
}

bool SpPending()
{
    return g_ppState == kPending;
}

void SpCancelBeforePresent()
{
    // The back buffer has been drawn into again (a frame that passed no net): the pending B is gone. Drop it.
    ++g_spStats.nets[kNetPresent];
    Cancel(kCancelSecondPresent);
}

extern "C" void __cdecl AotR60_Checkpoint(int index)
{
    if (g_profiling) {
        int64_t gap = PacerNow() - g_profileLast;
        if (gap > g_spStats.profileGapMax[g_profileSub]) {
            g_spStats.profileGapMax[g_profileSub] = gap;
        }
        if (index >= 0 && index < 8) {
            ++g_spStats.profileHits[index];
        }
        g_profileLast = PacerNow(); // the slow path's own cost is not a gap
        return; // g_cpDue stays set for the whole profiled call
    }
    uint8_t due = g_cpDue;
    if (g_ppState != kPending || due != DueTag(g_pb.gen)) {
        // Stale due flag (an older job, or the job was presented or cancelled). Keep a newer store.
        _InterlockedCompareExchange8(reinterpret_cast<volatile char*>(&g_cpDue), 0, static_cast<char>(due));
        return;
    }
    if (!OnMainThread() || !g_inLogic) {
        return;
    }
    if (PresentNow(true) && index >= 0 && index < 8) {
        ++g_spStats.checkpointPresents[index];
    }
}

extern "C" void __cdecl AotR60_SpDrain()
{
    if (g_ppState != kPending || !OnMainThread()) {
        return;
    }
    if (!ForegroundOk()) {
        Cancel(kCancelIconic);
        return;
    }
    int64_t now = PacerNow();
    int64_t w = g_pb.target;
    int64_t next = PacerNextXDeadline();
    if (next != 0 && next < w) {
        w = next;
    }
    if (g_pb.handoff + Ms(60.0) < w) {
        w = g_pb.handoff + Ms(60.0);
    }
    if (w > now) {
        PacerWaitUntil(w);
        g_drainWait += PacerNow() - now;
        ++g_spStats.drainWaited;
    }
    if (PresentNow(false)) {
        ++g_spStats.drainPresents;
    }
}

extern "C" void __cdecl AotR60_SpNet(int reason)
{
    if (g_ppState != kPending) {
        return;
    }
    if (reason >= 0 && reason < kNetCount) {
        ++g_spStats.nets[reason];
        if (g_spStats.nets[reason] <= 5 && reason != kNetPump) {
            Log("split present: net %d presented a pending B at render %u", reason, g_renderId);
        }
    }
    if (!OnMainThread()) {
        Cancel(kCancelOffMain);
        return;
    }
    if (!ForegroundOk()) {
        Cancel(kCancelIconic);
        return;
    }
    PresentNow(false);
}

extern "C" void __cdecl AotR60_SpCancel(int reason)
{
    if (reason >= 0 && reason < kCancelCount) {
        Cancel(reason);
    }
}

extern "C" void __cdecl AotR60_SpShutdown()
{
    Cancel(kCancelShutdown);
    g_disabled = true;
    if (g_thread) {
        g_quit = 1;
        if (g_arm) {
            SetEvent(g_arm);
        }
    }
}
