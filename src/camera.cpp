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

// S2, A-render: record M_k, then (phase 2b) open the drawable-pass presentation window.
extern "C" void __cdecl S2_RecordMkAndOpen(uint8_t* viewB4)
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
    bool shake = Field<float>(viewB4, 0x64) != 0.0f || Field<float>(viewB4, 0x68) != 0.0f ||
                 Field<float>(viewB4, 0x74) > 0.01f;
    if (!shake) {
        shake = ShakerReachesEye(r.xf);
        if (!shake) {
            if (uint8_t* shaker = Ptr(kCameraShaker)) {
                if (reinterpret_cast<ShakeActiveFn>(kFnShakeActive)(shaker)) {
                    ++g_cameraStats.aShakerFar; // a shaker exists but does not reach the camera: keep interpolating
                }
            }
        }
    }
    r.shake = shake;
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
    if (r0.shake || r1.shake) {
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
    CameraPose mid;
    if (!InterpolateCameraHalfway(p0, p1, &mid)) {
        ++g_cameraStats.aCuts;
        return;
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
