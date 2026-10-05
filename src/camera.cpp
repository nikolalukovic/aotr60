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

// Shows (xf, vp) through `cam` until CamSwapEnd(). No-op when the camera already shows exactly that.
void BeginSwap(uint8_t* cam, const float* xf, const uint32_t* vp)
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
    if (RefitWanted()) {
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

void CameraReset()
{
    g_rec[0].valid = false;
    g_rec[1].valid = false;
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
    BeginSwap(cam, r.xf, r.vp);
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
    BeginSwap(cam, mid.xf, reinterpret_cast<const uint32_t*>(mid.vp));
    ++g_cameraStats.aSwaps;
}

// drawFrame exit (0x44A271): real camera and stock fraction/key back.
extern "C" void __cdecl SceneRestore()
{
    if (g_swapActive && OnMainThread()) {
        CamSwapEnd();
    }
    if (g_pw2Open) {
        SetField<float>(Ptr(kTheGameEngine), 0x3C, g_fracSaved);
        g_pwActive = 0;
        g_pw2Open = 0;
    }
}
