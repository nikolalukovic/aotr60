#pragma once
#include <cstdint>

// Pure camera math for the phase-3 halfway picture (no game memory access; unit-tested).

struct CameraPose {
    float xf[12]; // Matrix3D 3x4 row-major: rotation rows with translation in xf[3], xf[7], xf[11]
    float vp[7];  // view plane min.x, min.y, max.x, max.y, aspect, znear, zfar
};

struct CameraCutLimits {
    float maxDistance = 1500.0f; // world units between the two poses (fast pans stay smooth; minimap jumps cut)
    float maxAngleDeg = 45.0f;   // rotation between the two poses
    float maxExtentChange = 0.05f;
    // Living World map: the far plane follows the zoom (zfar = 1.5 * distance + 2000, 0x49B597), so it may differ;
    // the halfway pose keeps the larger one. Aspect and near must still be identical.
    bool allowFarChange = false;
};

// Halfway pose between a and b: rotation slerp(½), translation and view-plane extents averaged, aspect/near/far
// from b (far: the larger of the two with allowFarChange). Returns false (out untouched) on a cut: too far, too much
// rotation, a changed aspect/near/far or a view plane extent that changed by more than the limit.
bool InterpolateCameraHalfway(const CameraPose& a, const CameraPose& b, CameraPose* out,
                              const CameraCutLimits& limits = {});

// Uniform scroll slope term. On a camera-height-grid ramp along the camera's forward direction the screen-centre
// flow of a scroll step is (1 + k*s) times the flat-ground flow (k = cot(pitch), s = grid slope over the step), so
// the forward part of the step is scaled by g with g * clamp(1 + k*chord(g*step), lo, hi) = 1, weighted in screen
// space with the sideways part (a pure sideways step keeps g = 1). Falling slopes: at most 1/lo faster; facing slopes:
// at most 1/hi slower; nearly as steep as the view ray: the boost fades out.
struct SlopeScrollParams {
    float k = 1.3032254f;        // |cameraOffset.xy| / cameraOffset.z = cot(pitch)
    float sinPitch = 0.6087614f;
    float lo = 0.5f;             // max speed-up 2x (falling slopes; exact for s >= -0.384)
    float hi = 1.4f;             // max slow-down 0.71x (facing slopes; exact for s <= +0.307)
    float fade = 0.15f;          // 1 + k*chord below this: the boost fades to 1 at 0
    int iterations = 12;
};
using HeightSampleFn = float (*)(const void* ctx, float x, float y);
// Step multiplier for a step of stepFwd along the unit forward (fx, fy) and stepRight sideways, from (px, py).
float SlopeScrollFactor(HeightSampleFn sample, const void* ctx, float px, float py, float fx, float fy, float stepFwd,
                        float stepRight, const SlopeScrollParams& p);

struct CameraStats {
    uint32_t bSwaps = 0;
    uint32_t bNoRecord = 0;
    uint32_t aSwaps = 0;
    uint32_t aNoHistory = 0;
    uint32_t aShake = 0;
    uint32_t aCuts = 0;
    uint32_t guardEnds = 0;
    uint32_t aShakerFar = 0; // shaker list non-empty but out of range (formerly blocked interpolation)
    uint32_t aShakeHeld = 0; // interpolated through a shake (base camera interpolated, stock shake re-applied)
};
extern CameraStats g_cameraStats;
