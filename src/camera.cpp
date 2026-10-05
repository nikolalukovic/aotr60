// Camera presentation (PLAN §1.6). The camera is stepped only by A-renders (stock code); B-renders present stock
// render k's camera M_k, and (phase 3) A-renders present the halfway picture between M_{k-1} and M_k during the
// scene render. Every swap is undone before anything can read the camera persistently.

#include "camera_math.h"
#include "log.h"
#include "runtime.h"

#include <cmath>
#include <cstring>

using namespace game;

namespace {

constexpr uint32_t kCamTransform = 0x18; // Matrix3D, 3x4 row-major (48 bytes)
constexpr uint32_t kCamViewPlane = 0xD8; // min.x, min.y, max.x, max.y, aspect, znear, zfar (28 bytes)
constexpr uint32_t kCamDirty = 0xFC;
constexpr uint32_t kViewCamera = 0x104;  // W3DView -> CameraClass*

struct CamRec {
    uint8_t* cam;
    float xf[12];
    uint32_t vp[7];
    uint32_t renderId;
    bool shake;
    bool valid;
    // Shake hold: the legacy view shake (W3DView::shake) is a pure XY offset of the look-at point and the
    // CameraShakerSystem a pure rotation, so the base camera can be interpolated and stock render k's shake
    // re-applied on top.
    float shakeOff[2];  // effective legacy shake offset in the recorded transform
    bool shakerRot;     // a shaker reaches the eye (rotation-only shake)
    bool shakeHoldOk;   // the recorded transform is a plain look-at camera rebuilt this update
};

struct SwapSave {
    uint8_t* cam;
    float xf[12];
    uint32_t vp[7];
    bool refit;
};

CamRec g_rec[2]; // [1] = M_k (this A-render), [0] = M_{k-1}
SwapSave g_save;

// Living World strategic map camera ([0xDE4958]+0xC0), rebuilt from the LW view state on every LW scene draw by
// 0x49B4A5. Recorded right after that build; [1] = this render's, [0] = the previous A-render's.
struct LwCamRec {
    uint8_t* view;
    uint8_t* cam;
    float xf[12];
    uint32_t vp[7];
    int32_t state;      // view+0x14: 2 fade-out, 3 fade-in
    uint8_t active;     // view+0x18
    uint8_t suspended;  // view+0x19
    uint32_t renderId;
    bool valid;
};
LwCamRec g_lwRec[2];
constexpr uint32_t kLwViewCamera = 0xC0;
constexpr float kLwCamCutDistance = 1500.0f; // eye translation between two A-renders (map extent 4200 x 3450)
bool g_lwMismatchLogged = false;

using SetTransformFn = void(__thiscall*)(void* cam, const float* xf);
using ShadowRefitFn = void(__thiscall*)(void* shadowManager, void* cam);
using ShakeActiveFn = bool(__thiscall*)(void* shaker);

void SetTransform(uint8_t* cam, const float* xf)
{
    uint8_t* vtable = *reinterpret_cast<uint8_t**>(cam);
    auto fn = *reinterpret_cast<SetTransformFn*>(vtable + 0x54);
    fn(cam, xf);
}

bool RefitWanted()
{
    uint8_t* gd = Ptr(kGlobalData);
    return Ptr(kTerrainVisual) && Ptr(kShadowManager) && gd && Field<uint8_t>(gd, 0x62);
}

void ShadowRefit(uint8_t* cam)
{
    if (uint8_t* sm = Ptr(kShadowManager)) {
        reinterpret_cast<ShadowRefitFn>(kFnShadowRefit)(sm, cam);
    }
}

// Shows (xf, vp) through `cam` until CamSwapEnd(). No-op when the camera already shows exactly that. `refit` fits
// the shadow manager to the swapped camera (tactical camera only: stock never fits it to the Living World camera).
void BeginSwap(uint8_t* cam, const float* xf, const uint32_t* vp, bool refit)
{
    bool sameXf = std::memcmp(cam + kCamTransform, xf, 48) == 0;
    bool sameVp = std::memcmp(cam + kCamViewPlane, vp, 28) == 0;
    if (sameXf && sameVp) {
        return;
    }
    g_save.cam = cam;
    std::memcpy(g_save.xf, cam + kCamTransform, 48);
    std::memcpy(g_save.vp, cam + kCamViewPlane, 28);
    g_save.refit = false;
    if (!sameXf) {
        SetTransform(cam, xf);
    }
    std::memcpy(cam + kCamViewPlane, vp, 28);
    cam[kCamDirty] = 0;
    if (refit && RefitWanted()) {
        ShadowRefit(cam);
        g_save.refit = true;
    }
    g_swapActive = 1;
}

void OpenWindow(volatile uint8_t& flag)
{
    if (!g_featPresent || g_pw1Open || g_pw2Open) {
        return;
    }
    uint8_t* ge = Ptr(kTheGameEngine);
    g_fracSaved = Field<float>(ge, 0x3C);
    SetField<float>(ge, 0x3C, g_presFrac);
    g_pwActive = 1;
    flag = 1;
}

// True when a CameraShakerSystem shaker reaches the eye of transform xf - the same test the engine applies in
// Compute_Rotations 0x4652D7 (|eye - pos|^2 <= radius^2). 0x4655DD only says "the shaker list is not empty", which
// is true during any battle with impacts anywhere on the map. List at [0xDC78D4]: head node sys+4, first node
// [sys+8], next [node+4], shaker [node+0xC]; shaker (ctor 0x465207): pos +0x08/+0x0C/+0x10, radius +0x14.
bool ShakerReachesEye(const float* xf)
{
    uint8_t* sys = Ptr(kCameraShaker);
    if (!sys) {
        return false;
    }
    uint8_t* head = sys + 4;
    uint8_t* node = *reinterpret_cast<uint8_t**>(sys + 8);
    for (int n = 0; node && node != head && n < 4096; ++n) {
        if (uint8_t* s = *reinterpret_cast<uint8_t**>(node + 0xC)) {
            float dx = xf[3] - Field<float>(s, 0x08);
            float dy = xf[7] - Field<float>(s, 0x0C);
            float dz = xf[11] - Field<float>(s, 0x10);
            float r = Field<float>(s, 0x14) + 1.0f;
            if (dx * dx + dy * dy + dz * dz <= r * r) {
                return true;
            }
        }
        node = *reinterpret_cast<uint8_t**>(node + 4);
    }
    return false;
}

uint8_t* TacticalCamera()
{
    uint8_t* tv = Ptr(kTacticalView);
    return tv ? *reinterpret_cast<uint8_t**>(tv + kViewCamera) : nullptr;
}

} // namespace

CameraStats g_cameraStats;
LwStats g_lwStats;

void CameraReset()
{
    g_rec[0].valid = false;
    g_rec[1].valid = false;
    g_lwRec[0].valid = false;
    g_lwRec[1].valid = false;
    LwPresentReset();
}

void CamSwapEnd()
{
    if (!g_swapActive || !OnMainThread()) {
        return;
    }
    uint8_t* cam = g_save.cam;
    SetTransform(cam, g_save.xf);
    std::memcpy(cam + kCamViewPlane, g_save.vp, 28);
    cam[kCamDirty] = 0;
    if (g_save.refit) {
        ShadowRefit(cam);
    }
    g_swapActive = 0;
}

extern "C" void __cdecl CamSwapEndGuard()
{
    if (g_swapActive && OnMainThread()) {
        ++g_cameraStats.guardEnds;
        CamSwapEnd();
    }
}

// S1, B-render: present M_k recorded by this pair's A-render.
extern "C" void __cdecl CamSwapToMk_B(uint8_t* viewB4)
{
    uint8_t* cam = *reinterpret_cast<uint8_t**>(viewB4 + 0x50);
    const CamRec& r = g_rec[1];
    if (g_swapActive || !r.valid || r.cam != cam || r.renderId != g_renderId - 1) {
        ++g_cameraStats.bNoRecord;
        return;
    }
    BeginSwap(cam, r.xf, r.vp, true);
    ++g_cameraStats.bSwaps;
}

float Clamp(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

// S2, A-render: record M_k, then (phase 2b) open the drawable-pass presentation window.
// `rebuilt` is W3DView::update's local [ebp-0xD]: the camera transform was rebuilt in this update (0x48C6E2).
extern "C" void __cdecl S2_RecordMkAndOpen(uint8_t* viewB4, uint32_t rebuilt)
{
    uint8_t* cam = *reinterpret_cast<uint8_t**>(viewB4 + 0x50);
    if (!cam || cam != TacticalCamera()) {
        return;
    }
    if (g_rec[1].renderId != g_renderId) {
        g_rec[0] = g_rec[1];
    }
    CamRec& r = g_rec[1];
    r.cam = cam;
    std::memcpy(r.xf, cam + kCamTransform, 48);
    std::memcpy(r.vp, cam + kCamViewPlane, 28);
    r.renderId = g_renderId;
    // Legacy view shake: offsets view+0x118/+0x11C (viewB4+0x64/+0x68, intensity +0x74, 0x48C0E3..0x48C16E) are
    // added to the look-at position (0x502936..0x502950); with the camera-bounds clamp on (byte view+0x241C) the
    // effective offset is the clamped difference.
    float off[2] = {Field<float>(viewB4, 0x64), Field<float>(viewB4, 0x68)};
    bool viewShake = off[0] != 0.0f || off[1] != 0.0f || Field<float>(viewB4, 0x74) > 0.01f;
    if (Field<uint8_t>(viewB4, 0x2368)) {
        float pos[2] = {Field<float>(viewB4 - 0xA8, 0), Field<float>(viewB4 - 0xA4, 0)}; // view+0xC/+0x10
        float lo[2] = {Field<float>(viewB4, 0x2358), Field<float>(viewB4, 0x235C)};
        float hi[2] = {Field<float>(viewB4, 0x2360), Field<float>(viewB4, 0x2364)};
        for (int i = 0; i < 2; ++i) {
            r.shakeOff[i] = Clamp(pos[i] + off[i], lo[i], hi[i]) - Clamp(pos[i], lo[i], hi[i]);
        }
    }
    else {
        r.shakeOff[0] = off[0];
        r.shakeOff[1] = off[1];
    }
    r.shakerRot = ShakerReachesEye(r.xf);
    if (!viewShake && !r.shakerRot) {
        if (uint8_t* shaker = Ptr(kCameraShaker)) {
            if (reinterpret_cast<ShakeActiveFn>(kFnShakeActive)(shaker)) {
                ++g_cameraStats.aShakerFar; // a shaker exists but does not reach the camera: keep interpolating
            }
        }
    }
    r.shake = viewShake || r.shakerRot;
    // The hold is exact only for a plain look-at camera whose transform was rebuilt from the current offsets in
    // this update: no scripted camera mode 2/3/4 (view+0x2354), no look-at override (+0x23C8), no object lock
    // (+0x1DC), no one-shot eye placement (+0x2438).
    uint32_t mode = Field<uint32_t>(viewB4, 0x22A0);
    r.shakeHoldOk = mode != 2 && mode != 3 && mode != 4 && !Field<uint8_t>(viewB4, 0x2314) &&
                    !Field<uint8_t>(viewB4, 0x128) && !Field<uint8_t>(viewB4, 0x2384) &&
                    (!r.shake || rebuilt || Field<uint8_t>(viewB4, 0x2385));
    r.valid = true;
    OpenWindow(g_pw1Open);
}

// Scene render, A-render: (2b) presentation window; (3) halfway camera picture.
extern "C" void __cdecl SceneOpen_A()
{
    uint8_t* gd = Ptr(kGlobalData);
    if (!gd || Field<uint8_t>(gd, 0xAF6) == 1) {
        return; // load-screen render
    }
    if (LwSceneShown()) {
        return; // Living World map: its own camera presentation (LwCamAfterBuild); nothing reads the fraction
    }
    OpenWindow(g_pw2Open);
    if (!g_featCamInterp || g_swapActive) {
        return;
    }
    uint8_t* cam = TacticalCamera();
    const CamRec& r1 = g_rec[1];
    const CamRec& r0 = g_rec[0];
    if (!cam || !r1.valid || !r0.valid || r1.cam != cam || r0.cam != cam || r1.renderId != g_renderId ||
        r0.renderId != g_renderId - 2) {
        ++g_cameraStats.aNoHistory;
        return;
    }
    bool shaking = r0.shake || r1.shake;
    if (shaking && !(r0.shakeHoldOk && r1.shakeHoldOk)) {
        ++g_cameraStats.aShake;
        return;
    }
    if (std::memcmp(r0.xf, r1.xf, 48) == 0 && std::memcmp(r0.vp, r1.vp, 28) == 0) {
        return; // camera did not move
    }
    CameraPose p0;
    CameraPose p1;
    std::memcpy(p0.xf, r0.xf, 48);
    std::memcpy(p0.vp, r0.vp, 28);
    std::memcpy(p1.xf, r1.xf, 48);
    std::memcpy(p1.vp, r1.vp, 28);
    if (shaking) {
        // Interpolate the unshaken base cameras, then apply stock render k's shake.
        p0.xf[3] -= r0.shakeOff[0];
        p0.xf[7] -= r0.shakeOff[1];
        p1.xf[3] -= r1.shakeOff[0];
        p1.xf[7] -= r1.shakeOff[1];
    }
    CameraPose mid;
    if (!InterpolateCameraHalfway(p0, p1, &mid)) {
        ++g_cameraStats.aCuts;
        return;
    }
    if (shaking) {
        mid.xf[3] += r1.shakeOff[0];
        mid.xf[7] += r1.shakeOff[1];
        if (r0.shakerRot || r1.shakerRot) {
            for (int row = 0; row < 3; ++row) {
                std::memcpy(&mid.xf[4 * row], &r1.xf[4 * row], 3 * sizeof(float)); // M_k rotation, shaker included
            }
        }
        ++g_cameraStats.aShakeHeld;
    }
    // The current camera is C_k' (after input); only the scene shows the halfway picture.
    BeginSwap(cam, mid.xf, reinterpret_cast<const uint32_t*>(mid.vp), true);
    ++g_cameraStats.aSwaps;
}

// LW6_CAM_REC: right after the LW camera build 0x49B4A5 in the LW scene draw (60 mode, main thread, clientUpdate).
// A-render: record M_k and show the halfway camera between M_{k-1} and M_k for the LW scene render only
// (LW6_CAM_SCENE_END ends it). Only the W3D camera is swapped, never the LW view fields: LW logic reads the target
// +0x110 the build derives from them (vt70 0x49A64F). B-render: the build reproduced M_k from unchanged state;
// verify it.
extern "C" void __cdecl LwCamAfterBuild(uint8_t* view)
{
    uint8_t* cam = *reinterpret_cast<uint8_t**>(view + kLwViewCamera);
    if (!cam) {
        return;
    }
    if (g_inB) {
        const LwCamRec& r = g_lwRec[1];
        if (!r.valid || r.renderId != g_renderId - 1 || r.cam != cam) {
            ++g_lwStats.bNoRecord;
            return;
        }
        if (std::memcmp(cam + kCamTransform, r.xf, 48) == 0 && std::memcmp(cam + kCamViewPlane, r.vp, 28) == 0) {
            ++g_lwStats.bExact;
            return;
        }
        ++g_lwStats.bMismatch;
        if (!g_lwMismatchLogged) {
            g_lwMismatchLogged = true;
            Log("WARN Living World camera on a B-render differs from the A-render's: pos %.3f %.3f %.3f angle %.4f "
                "zoom %.4f vel %.5f fade %.3f state %d active %u suspended %u",
                Field<float>(view, 0xF8), Field<float>(view, 0xFC), Field<float>(view, 0x100),
                Field<float>(view, 0x11C), Field<float>(view, 0x134), Field<float>(view, 0x138),
                Field<float>(view, 0x198), Field<int32_t>(view, 0x14), Field<uint8_t>(view, 0x18),
                Field<uint8_t>(view, 0x19));
        }
        return;
    }
    if (g_lwRec[1].renderId != g_renderId) {
        g_lwRec[0] = g_lwRec[1];
    }
    LwCamRec& r1 = g_lwRec[1];
    r1.view = view;
    r1.cam = cam;
    std::memcpy(r1.xf, cam + kCamTransform, 48);
    std::memcpy(r1.vp, cam + kCamViewPlane, 28);
    r1.state = Field<int32_t>(view, 0x14);
    r1.active = Field<uint8_t>(view, 0x18);
    r1.suspended = Field<uint8_t>(view, 0x19);
    r1.renderId = g_renderId;
    r1.valid = true;
    if (!g_featCamInterp || g_swapActive) {
        return;
    }
    const LwCamRec& r0 = g_lwRec[0];
    if (!r0.valid || r0.renderId != g_renderId - 2 || r0.cam != cam || r0.view != view) {
        ++g_lwStats.aNoHistory;
        return;
    }
    if (r0.state != r1.state || r0.active != r1.active || r0.suspended != r1.suspended) {
        ++g_lwStats.aCuts; // activation, fades, suspend: present M_k
        return;
    }
    if (std::memcmp(r0.xf, r1.xf, 48) == 0 && std::memcmp(r0.vp, r1.vp, 28) == 0) {
        return; // camera did not move
    }
    CameraPose p0;
    CameraPose p1;
    std::memcpy(p0.xf, r0.xf, 48);
    std::memcpy(p0.vp, r0.vp, 28);
    std::memcpy(p1.xf, r1.xf, 48);
    std::memcpy(p1.vp, r1.vp, 28);
    CameraCutLimits limits;
    limits.maxDistance = kLwCamCutDistance;
    limits.allowFarChange = true;
    CameraPose mid;
    if (!InterpolateCameraHalfway(p0, p1, &mid, limits)) {
        ++g_lwStats.aCuts; // instant jumps (0x6BF878: battle return, DelayedSplineCamera)
        return;
    }
    BeginSwap(cam, mid.xf, reinterpret_cast<const uint32_t*>(mid.vp), false);
    ++g_lwStats.aSwaps;
}

// LW6_CAM_SCENE_END: right after WW3D::Render of the LW scene. The UI drawn next and everything after the draw
// see the real camera.
extern "C" void __cdecl LwCamSceneEnd()
{
    if (g_swapActive && g_save.cam == g_lwRec[1].cam) {
        CamSwapEnd();
        ++g_lwStats.swapEnds;
    }
}

// ---- Uniform scroll (UniformScroll, Ctrl+Shift+F8) ----
// Stock W3DView::scrollBy (0x48C774) moves the camera by |scroll| * zoom * 0.25 * scalar per update, and zoom is the
// eye's ABSOLUTE height / the map's max camera height (W3DView+0x23F0, 540 on most AotR maps): over low ground the
// same step looks slower, over high ground faster. And while the player scrolls at or above ScrollAmountCutoff (AotR:
// 50, copied to W3DView+0x2404 at 0x48BCD8; only reader 0x48C314) the eye height is frozen while the look-at point
// follows the terrain. With the switch on (single player only): the step uses the height above the ground plus a
// reference terrain height (taken where the player first scrolls on the map, so the speed there is unchanged), and
// the cutoff is raised so the camera keeps following the terrain while scrolling. Only player scrolling is affected
// (InGameUI isScrolling); scripted and follow cameras are stock.
namespace {

bool g_scrollRefPending = true;
float g_scrollRefHeight = 0.0f;
bool g_scrollModeOk = false;       // single-player battle (modes 0/2/6, no network)
float g_savedCutoff = 0.0f;        // the view's cutoff before the DLL raised it
constexpr float kRaisedCutoff = 3.0e38f;

} // namespace

float UniformScrollFactor(float hag, float hagDesired, float maxHeight, float refHeight)
{
    float lo = (hagDesired == hagDesired && hagDesired > 2.0f) ? 0.5f * hagDesired : 1.0f;
    hag = Clamp(hag, lo, 4.0f * maxHeight); // only transients (minimap jumps, fast climbs) are limited
    return (hag + refHeight) / maxHeight;
}

extern "C" float __cdecl ScrollZoomFactor(const uint8_t* view)
{
    float zoom = Field<float>(view, 0x3C);
    uint8_t* ui = Ptr(kInGameUI);
    if (!g_uniformScroll || !g_scrollModeOk || !ui || !Field<uint8_t>(ui, 0x7F8)) {
        return zoom;
    }
    float maxHeight = Field<float>(view, 0x23F0);
    float hag = Field<float>(view, 0x50); // eye height above the terrain (last update)
    if (!(maxHeight > 1.0f) || !(hag == hag)) {
        return zoom;
    }
    if (g_scrollRefPending) {
        float t = Field<float>(view, 0x54); // terrain height under the camera where the player first scrolls
        g_scrollRefHeight = (t == t) ? Clamp(t, 0.0f, 3000.0f) : 0.0f;
        g_scrollRefPending = false;
        Log("camera: uniform scroll reference height %.1f", g_scrollRefHeight);
    }
    return UniformScrollFactor(hag, Field<float>(view, 0x40), maxHeight, g_scrollRefHeight);
}

void UniformScrollInit(bool on)
{
    g_uniformScroll = on ? 1 : 0;
}

void UniformScrollToggle()
{
    g_uniformScroll = g_uniformScroll ? 0 : 1;
    Log("hotkey: uniform camera scroll %s", g_uniformScroll ? "on" : "off");
    UniformScrollOnC0();
}

void UniformScrollOnReset()
{
    g_scrollRefHeight = 0.0f;
    g_scrollRefPending = true;
}

void UniformScrollOnC0()
{
    uint8_t* tv = Ptr(kTacticalView);
    uint8_t* gl = Ptr(kTheGameLogic);
    if (!tv) {
        return;
    }
    uint32_t mode = gl ? Field<uint32_t>(gl, 0x110) : 0xFFFFFFFFu;
    g_scrollModeOk = gl && (mode == 0 || mode == 2 || mode == 6) && !Ptr(kTheNetwork);
    bool active = g_uniformScroll && g_scrollModeOk;
    float cutoff = Field<float>(tv, 0x2404);
    if (active && cutoff != kRaisedCutoff) {
        g_savedCutoff = cutoff;
        SetField<float>(tv, 0x2404, kRaisedCutoff);
    }
    else if (!active && cutoff == kRaisedCutoff) {
        SetField<float>(tv, 0x2404, g_savedCutoff); // restore exactly what was there
    }
}

// drawFrame exit (0x44A271): real camera and stock fraction/key back.
extern "C" void __cdecl SceneRestore()
{
    if (g_lwRestoreN && OnMainThread()) {
        ++g_lwStats.iconLateCloses;
        LwPresentClose();
    }
    if (g_swapActive && OnMainThread()) {
        CamSwapEnd();
    }
    if (g_pw2Open) {
        SetField<float>(Ptr(kTheGameEngine), 0x3C, g_fracSaved);
        g_pwActive = 0;
        g_pw2Open = 0;
    }
}
