#include "camera_math.h"

#include <cmath>
#include <cstring>

namespace {

struct Quat {
    float w, x, y, z;
};

// Rotation part of a row-major 3x4 matrix to a unit quaternion (Shepperd's method).
Quat FromMatrix(const float* m)
{
    float m00 = m[0], m01 = m[1], m02 = m[2];
    float m10 = m[4], m11 = m[5], m12 = m[6];
    float m20 = m[8], m21 = m[9], m22 = m[10];
    Quat q;
    float trace = m00 + m11 + m22;
    if (trace > 0.0f) {
        float s = std::sqrt(trace + 1.0f) * 2.0f;
        q = {0.25f * s, (m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s};
    }
    else if (m00 > m11 && m00 > m22) {
        float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
        q = {(m21 - m12) / s, 0.25f * s, (m01 + m10) / s, (m02 + m20) / s};
    }
    else if (m11 > m22) {
        float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
        q = {(m02 - m20) / s, (m01 + m10) / s, 0.25f * s, (m12 + m21) / s};
    }
    else {
        float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
        q = {(m10 - m01) / s, (m02 + m20) / s, (m12 + m21) / s, 0.25f * s};
    }
    float n = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    return {q.w / n, q.x / n, q.y / n, q.z / n};
}

void ToMatrix(const Quat& q, float* m)
{
    float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
    m[0] = 1.0f - 2.0f * (yy + zz);
    m[1] = 2.0f * (xy - wz);
    m[2] = 2.0f * (xz + wy);
    m[4] = 2.0f * (xy + wz);
    m[5] = 1.0f - 2.0f * (xx + zz);
    m[6] = 2.0f * (yz - wx);
    m[8] = 2.0f * (xz - wy);
    m[9] = 2.0f * (yz + wx);
    m[10] = 1.0f - 2.0f * (xx + yy);
}

bool ExtentChanged(float a, float b, float limit)
{
    float scale = std::fabs(a) > std::fabs(b) ? std::fabs(a) : std::fabs(b);
    if (scale < 1e-6f) {
        return false;
    }
    return std::fabs(a - b) > limit * scale;
}

} // namespace

bool InterpolateCameraHalfway(const CameraPose& a, const CameraPose& b, CameraPose* out,
                              const CameraCutLimits& limits)
{
    // Aspect, near and far must be identical (far may differ with allowFarChange).
    int fixedCount = limits.allowFarChange ? 2 : 3;
    if (std::memcmp(&a.vp[4], &b.vp[4], fixedCount * sizeof(float)) != 0) {
        return false;
    }
    for (int i = 0; i < 4; ++i) {
        if (ExtentChanged(a.vp[i], b.vp[i], limits.maxExtentChange)) {
            return false;
        }
    }
    float dx = b.xf[3] - a.xf[3];
    float dy = b.xf[7] - a.xf[7];
    float dz = b.xf[11] - a.xf[11];
    if (dx * dx + dy * dy + dz * dz > limits.maxDistance * limits.maxDistance) {
        return false;
    }
    Quat qa = FromMatrix(a.xf);
    Quat qb = FromMatrix(b.xf);
    float dot = qa.w * qb.w + qa.x * qb.x + qa.y * qb.y + qa.z * qb.z;
    if (dot < 0.0f) {
        qb = {-qb.w, -qb.x, -qb.y, -qb.z};
        dot = -dot;
    }
    // Angle between the two rotations is 2*acos(dot).
    float cosHalfLimit = std::cos(limits.maxAngleDeg * 0.5f * 3.14159265f / 180.0f);
    if (dot < cosHalfLimit) {
        return false;
    }
    // slerp(½) == normalised sum.
    Quat mid = {qa.w + qb.w, qa.x + qb.x, qa.y + qb.y, qa.z + qb.z};
    float n = std::sqrt(mid.w * mid.w + mid.x * mid.x + mid.y * mid.y + mid.z * mid.z);
    mid = {mid.w / n, mid.x / n, mid.y / n, mid.z / n};

    CameraPose p;
    ToMatrix(mid, p.xf);
    p.xf[3] = 0.5f * (a.xf[3] + b.xf[3]);
    p.xf[7] = 0.5f * (a.xf[7] + b.xf[7]);
    p.xf[11] = 0.5f * (a.xf[11] + b.xf[11]);
    for (int i = 0; i < 4; ++i) {
        p.vp[i] = 0.5f * (a.vp[i] + b.vp[i]);
    }
    for (int i = 4; i < 7; ++i) {
        p.vp[i] = b.vp[i];
    }
    if (limits.allowFarChange && a.vp[6] > b.vp[6]) {
        p.vp[6] = a.vp[6];
    }
    *out = p;
    return true;
}
