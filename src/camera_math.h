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
};

// Halfway pose between a and b: rotation slerp(½), translation and view-plane extents averaged, aspect/near/far
// from b. Returns false (out untouched) on a cut: too far, too much rotation, a changed aspect/near/far or a view
// plane extent that changed by more than the limit.
bool InterpolateCameraHalfway(const CameraPose& a, const CameraPose& b, CameraPose* out,
                              const CameraCutLimits& limits = {});

struct CameraStats {
    uint32_t bSwaps = 0;
    uint32_t bNoRecord = 0;
    uint32_t aSwaps = 0;
    uint32_t aNoHistory = 0;
    uint32_t aShake = 0;
    uint32_t aCuts = 0;
    uint32_t guardEnds = 0;
    uint32_t aShakerFar = 0; // shaker list non-empty but out of range (formerly blocked interpolation)
};
extern CameraStats g_cameraStats;
