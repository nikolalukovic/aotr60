#pragma once
#include <cstdint>
#include <intrin.h>

#include "sites.gen.h"

// Globals shared between the assembly stubs (src/stubs/stubs.asm) and the C++ runtime. Names are extern "C" so
// MASM can address them directly. Unless noted, they are written on the game's main thread only.

extern "C" {

// ---- frame state, published by C0 for every render (frame_ctl.cpp; model in frame_state.h) ----
extern volatile uint8_t g_m60;            // 60 mode active
extern volatile uint8_t g_uiTick;         // 0 only during a 60-mode B-render's clientUpdate
extern volatile uint8_t g_inB;            // a 60-mode B-render's clientUpdate is running
extern volatile uint8_t g_inClientUpdate; // inside the C0-bracketed clientUpdate
extern volatile uint8_t g_skipB;          // g_m60 && g_inB && g_inClientUpdate
extern volatile uint8_t g_forceHalt;      // HALT stub: take the stepper's halted branch
extern volatile uint32_t g_mainTid;       // main thread id (compared with fs:[0x24])
extern volatile uint32_t g_renderId;      // incremented once per main-loop iteration, before clientUpdate

// ---- C3 W3D sync clock ----
extern volatile int32_t g_syncOwed;
extern volatile uint32_t g_syncHalfPar;

// ---- presentation windows / C4 / camera ----
extern volatile uint8_t g_featPresent;    // phase 2b: unit interpolation windows
extern volatile uint8_t g_featCamInterp;  // phase 3: interpolated camera picture
extern volatile uint32_t g_pwActive;      // C4 key uses 2*m_frame-1 while set
extern volatile uint8_t g_pw1Open;        // drawable-pass window (S2..S2E)
extern volatile uint8_t g_pw2Open;        // scene-render window (SCENE_OPEN..SCENE_RESTORE)
extern volatile float g_presFrac;         // A-render presentation fraction min(1,(2k-1)/12)
extern volatile float g_fracSaved;        // stock fraction saved while a window is open
extern volatile uint8_t g_swapActive;     // W3D camera temporarily swapped
extern volatile uint8_t g_uniformScroll;  // camera pan speed by height above ground (CAVE_SCROLLNORM)

// ---- pacing ----
extern volatile uint32_t g_pacerRanRid;
extern volatile uint32_t g_mDrawRid;
extern volatile int32_t g_mDrawAM;
extern volatile uint8_t g_gapDevLost;
extern volatile uint8_t g_gapReset;

// ---- render-integrator operand redirects (stock value except during B-renders) ----
extern float g_ovlFrameStep;
extern float g_ovlStep003;
extern float g_ovlStep00225;
extern float g_ovlStep002;
extern double g_ovlStep0005d;
extern float g_ovlStep005;
extern float g_riverStepU;
extern float g_riverStepV;
extern float g_matPassDecay;
extern int32_t g_shoreStepA;
extern int32_t g_shoreStepB;
extern float g_lightPhaseNum;
extern float g_wakeStep;
extern float g_outlineStep;
extern float g_instFadeStep;
extern int32_t g_lightPulseLtr;
extern volatile uint8_t g_lp4b;       // phase 4b LightPulse half-steps (session constant)
extern volatile uint8_t g_uiPart4b;   // phase 4b UI particle half-steps (session constant)
extern volatile uint8_t g_wanim4b;    // phase 4b world-anim rise half-steps (session constant)
extern volatile uint32_t g_radarAFrame; // m_frame of the last radar overlay refresh test outside B-renders
extern volatile uint32_t g_uiSeqLast;   // flags the UI-sequence runner 0x80000F returned to the last 60-mode A-render

// ---- Living World strategic map (phase 6) ----
extern volatile uint32_t g_lwRestoreN;  // LW objects currently shown at their halfway transform (LwPresentOpen)

// ---- telemetry / anomalies ----
extern volatile uint32_t g_siteRun[sites::kSiteCount];
extern volatile uint32_t g_siteSkip[sites::kSiteCount];
extern volatile uint32_t g_tmFFin60;
extern volatile uint32_t g_anomaly;       // ANOM_* bits (any thread)
extern volatile uint32_t g_unknownPath;   // UP_* bits: 60 mode must stop at the next pair boundary
extern volatile uint32_t g_vt188Calls;
extern volatile uint32_t g_vt188LastRet;
extern volatile uint32_t g_palBadRet;
extern volatile uint32_t g_iguiBadRet;

// ---- C++ entry points called from the stubs (cdecl unless noted) ----
void __cdecl OnPreRender(uint8_t* engine);
void __cdecl OnPostRender();
void __cdecl OnEngineReset();
uint32_t __cdecl AotR60_Pacer(uint8_t* engine);
int __cdecl AotR60_PresentSkip();
void __cdecl AotR60_PresentDone();
void __cdecl CamSwapToMk_B(uint8_t* viewB4);
void __cdecl S2_RecordMkAndOpen(uint8_t* viewB4, uint32_t rebuilt);
void __cdecl SceneOpen_A();
void __cdecl SceneRestore();
void __cdecl CamSwapEndGuard();
float __cdecl ScrollZoomFactor(const uint8_t* view, const float* delta, const uint8_t* frame, float scalar);
bool __fastcall C5_CalcPhysicsXform(uint8_t* drawable, void* edx, float* out);
uint32_t __fastcall LogicUpdateWrapper(uint8_t* logic, void* edx, int sub);
void __fastcall LwLogicUpdateWrapper(uint8_t* lwLogic, void* edx);
void __cdecl LwIconSnapshot(uint8_t* view);
void __cdecl LwPresentOpen();
void __cdecl LwPresentClose();
void __cdecl LwCamAfterBuild(uint8_t* view);
void __cdecl LwCamSceneEnd();

// Assembly-side trampoline of the C5 detour (thiscall, same contract as 0x67BDC0).
void C5_Tramp();

} // extern "C"

constexpr uint32_t ANOM_PAL_CALLER = 1;
constexpr uint32_t ANOM_IGUI_CALLER = 2;
constexpr uint32_t UP_VT188 = 1;

// Game addresses used by the C++ side.
namespace game {
constexpr uintptr_t kTheGameEngine = 0xDE4324;
constexpr uintptr_t kTheGameLogic = 0xDE412C;
constexpr uintptr_t kTheGameClient = 0xDE4388;
constexpr uintptr_t kGlobalData = 0xDE4364;
constexpr uintptr_t kTacticalView = 0xDE447C;
constexpr uintptr_t kInGameUI = 0xDE4830;
constexpr uintptr_t kTheNetwork = 0xDE4468;
constexpr uintptr_t kRecorder = 0xDE7CD8;
constexpr uintptr_t kScriptDebugDll = 0xDE3B98;
constexpr uintptr_t kGameLod = 0xDE3B84;
constexpr uintptr_t kSyncAccumulator = 0xDC7580;
constexpr uintptr_t kFrameLengthMs = 0xDC7A8C;
constexpr uintptr_t kLogicRngSeed = 0xDA1CA4;
constexpr uintptr_t kNetSpeedMultiplier = 0xD9F498;
constexpr uintptr_t kTerrainVisual = 0xDC78EC;
constexpr uintptr_t kShadowManager = 0xDC7A38;
constexpr uintptr_t kCameraShaker = 0xDC78D4;
constexpr uintptr_t kLwView = 0xDE4958;          // Living World client/view (vtbl 0xBDE918)
constexpr uintptr_t kLwLogic = 0xDE4950;         // TheLivingWorldLogic
constexpr uintptr_t kUiSequenceQueue = 0xDE8900; // head of the UI-sequence step queue run by 0x80000F

constexpr uint32_t kFnShadowRefit = 0x47D37D;   // thiscall(ShadowManager*, CameraClass*)
constexpr uint32_t kFnShakeActive = 0x4655DD;   // thiscall(shaker) -> AL
constexpr uint32_t kFnLogicUpdate = 0x62E4E8;   // GameLogic::update(sub), thiscall, ret 4

template <typename T>
inline T Read(uintptr_t address)
{
    return *reinterpret_cast<const volatile T*>(address);
}

template <typename T>
inline void Write(uintptr_t address, T value)
{
    *reinterpret_cast<volatile T*>(address) = value;
}

inline uint8_t* Ptr(uintptr_t globalAddress)
{
    return *reinterpret_cast<uint8_t* volatile*>(globalAddress);
}

template <typename T>
inline T Field(const uint8_t* object, uint32_t offset)
{
    return *reinterpret_cast<const volatile T*>(object + offset);
}

template <typename T>
inline void SetField(uint8_t* object, uint32_t offset, T value)
{
    *reinterpret_cast<volatile T*>(object + offset) = value;
}
} // namespace game

// runtime.cpp
void InitIntegratorVariables();              // DllMain: copy the stock operand constants into the redirect variables
void SetIntegratorVariablesForB(bool bRender); // B-render values vs stock values

// camera.cpp
void CamSwapEnd();      // restore the real camera if a swap is active (main thread)
void CameraReset();     // forget the camera history (mode switches, resets)
void UniformScrollInit(bool on, bool slope);
void UniformScrollToggle();
void UniformScrollOnC0();    // keeps the tactical view's scroll cutoff in line with the switch (main thread)
void UniformScrollOnReset(); // a new map: capture the reference height again
float UniformScrollFactor(float hag, float hagDesired, float maxHeight, float refHeight); // pure, unit-tested

// Living World strategic map presentation counters (camera.cpp, lw_present.cpp).
struct LwStats {
    uint32_t aSwaps = 0;        // A-render LW scene drawn with the halfway camera
    uint32_t aNoHistory = 0;
    uint32_t aCuts = 0;
    uint32_t swapEnds = 0;
    uint32_t bExact = 0;        // B-render rebuilt exactly the A-render's camera (expected)
    uint32_t bMismatch = 0;     // expected 0
    uint32_t bNoRecord = 0;
    uint32_t iconSnapshots = 0;
    uint32_t iconPresented = 0; // objects drawn at a halfway transform
    uint32_t iconFar = 0;       // moved more than the cutoff: drawn as is
    uint32_t iconRestoreBad = 0; // restore did not reproduce the exact transform (expected 0)
    uint32_t iconLateCloses = 0; // restored by the drawFrame exit backstop (expected 0)
    uint32_t iconOverflow = 0;
};
extern LwStats g_lwStats;

// lw_present.cpp
bool LwSceneShown();          // drawFrame draws the LW scene (LW view active and not suspended)
void LwPresentReset();        // forget the snapshot (mode switches, resets)
uint32_t LwIconHash();        // order-independent hash of every LW object transform (telemetry)

// c5_physics.cpp
void C5Reset();
struct C5Stats {
    uint32_t replays = 0;
    uint32_t suppressed = 0;
    uint32_t drops = 0;
};
extern C5Stats g_c5Stats;

// frame_ctl.cpp
struct Config;
void FrameControlInit(const Config& cfg, bool installed);

// pacer.cpp
void PacerInit(const Config& cfg);
bool PacerFallbackActive();           // the mode controller must not (re)enable 60 mode
void PacerOnModeChange(bool on);
int PacerFallbackSecondsLeft();       // 0 when no fallback is active

// telemetry.cpp
void TelemetryInit(const Config& cfg);
void TelemetryIterationBegin();     // first thing at C0: closes the previous main-loop iteration's timing
void TelemetryOnPreRender(int stepperS);
void TelemetryOnPostRender();
void TelemetryOnModeChange(bool on, const char* reason);
void TelemetryOnReset();

// Same test as the stubs: TEB.ClientId.UniqueThread == main thread id.
inline bool OnMainThread()
{
    return __readfsdword(0x24) == g_mainTid;
}
