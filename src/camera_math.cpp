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

struct SlopeEq {
    HeightSampleFn sample;
    const void* ctx;
    float px, py, fx, fy, stepFwd, h0, k;

    // 1 + k * (grid slope over the compensated forward step g*stepFwd)
    float CRaw(float g) const
    {
        float x = g * stepFwd;
        return 1.0f + k * (sample(ctx, px + fx * x, py + fy * x) - h0) / x;
    }
};

float ClampF(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

DecalVertex LerpVertex(const DecalVertex& a, const DecalVertex& b, float t)
{
    auto mix = [t](float p, float q) { return p + (q - p) * t; };
    DecalVertex r{mix(a.x, b.x), mix(a.y, b.y), mix(a.z, b.z), 0, mix(a.u, b.u), mix(a.v, b.v), mix(a.u2, b.u2),
                  mix(a.v2, b.v2)};
    for (int shift = 0; shift < 32; shift += 8) {
        float p = static_cast<float>((a.color >> shift) & 0xFF);
        float q = static_cast<float>((b.color >> shift) & 0xFF);
        r.color |= static_cast<uint32_t>(mix(p, q) + 0.5f) << shift;
    }
    return r;
}

// Moves `outer` towards `inner` until its texture coordinates that change along that edge are inside [0, 1].
void PullInside(DecalVertex& outer, const DecalVertex& inner)
{
    float t = 0.0f;
    float* const comps[2] = {&outer.u, &outer.v};
    const float innerComps[2] = {inner.u, inner.v};
    float bounds[2] = {-1.0f, -1.0f};
    for (int k = 0; k < 2; ++k) {
        float o = *comps[k];
        float i = innerComps[k];
        float bound = o < 0.0f ? 0.0f : (o > 1.0f ? 1.0f : -1.0f);
        if (bound < 0.0f || !(std::fabs(i - o) > 1e-6f)) {
            continue; // inside, or does not change along this edge
        }
        float tk = (bound - o) / (i - o);
        if (tk > 0.0f) {
            t = tk > t ? tk : t;
            bounds[k] = bound;
        }
    }
    if (!(t > 0.0f)) {
        return;
    }
    outer = LerpVertex(outer, inner, t < 1.0f ? t : 1.0f);
    for (int k = 0; k < 2; ++k) {
        if (bounds[k] >= 0.0f && (bounds[k] == 0.0f ? *comps[k] < 0.0f : *comps[k] > 1.0f)) {
            *comps[k] = bounds[k]; // rounding
        }
    }
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

float SlopeScrollFactor(HeightSampleFn sample, const void* ctx, float px, float py, float fx, float fy, float stepFwd,
                        float stepRight, const SlopeScrollParams& p)
{
    if (!sample || !(std::fabs(stepFwd) > 1e-3f) || !(p.lo > 0.0f) || !(p.hi >= 1.0f) || !(p.lo <= 1.0f)) {
        return 1.0f; // pure sideways step, bad parameters or NaN
    }
    SlopeEq eq{sample, ctx, px, py, fx, fy, stepFwd, sample(ctx, px, py), p.k};
    if (!(eq.h0 == eq.h0)) {
        return 1.0f;
    }
    const float wf = (p.sinPitch * stepFwd) * (p.sinPitch * stepFwd); // forward image term (screen units)
    const float wr = stepRight * stepRight;                            // sideways image term
    auto resid = [&](float g) {
        float c = ClampF(eq.CRaw(g), p.lo, p.hi);
        return g * g * (wr + wf * c * c) - (wr + wf);
    };
    float a = 1.0f / p.hi;
    float b = 1.0f / p.lo;
    float g;
    float ra = resid(a);
    float rb = resid(b);
    if (!(ra == ra) || !(rb == rb)) {
        return 1.0f;
    }
    if (ra >= 0.0f) {
        g = a;
    }
    else if (rb <= 0.0f) {
        g = b;
    }
    else {
        for (int i = 0; i < p.iterations; ++i) { // the residual is monotonic in g: a single root
            float m = 0.5f * (a + b);
            if (resid(m) > 0.0f) {
                b = m;
            }
            else {
                a = m;
            }
        }
        g = 0.5f * (a + b);
        float c = ClampF(eq.CRaw(g), p.lo, p.hi);
        float polished = std::sqrt((wr + wf) / (wr + wf * c * c)); // exact when c is constant (flat -> exactly 1)
        if (polished >= a - 1e-6f && polished <= b + 1e-6f) {
            g = polished;
        }
    }
    float cr = eq.CRaw(g);
    if (!(cr == cr)) {
        return 1.0f;
    }
    if (cr < p.fade) {
        float t = cr > 0.0f ? cr / p.fade : 0.0f;
        g = 1.0f + (g - 1.0f) * t;
    }
    return g;
}

void ClampDecalPatchToTexture(DecalVertex* v, int cols, int rows)
{
    if (!v || cols < 2 || rows < 2) {
        return;
    }
    for (int r = 0; r < rows; ++r) { // along the rows first, then the columns (corners get both)
        DecalVertex* row = v + r * cols;
        PullInside(row[0], row[1]);
        PullInside(row[cols - 1], row[cols - 2]);
    }
    for (int c = 0; c < cols; ++c) {
        PullInside(v[c], v[cols + c]);
        PullInside(v[(rows - 1) * cols + c], v[(rows - 2) * cols + c]);
    }
}

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
