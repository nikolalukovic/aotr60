#include "test.h"

#include "camera_math.h"
#include "runtime.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace {

CameraPose Pose(float yawDeg, float x, float y, float z)
{
    float a = yawDeg * 3.14159265f / 180.0f;
    float c = std::cos(a);
    float s = std::sin(a);
    CameraPose p{};
    // rotation about z
    p.xf[0] = c;
    p.xf[1] = -s;
    p.xf[2] = 0.0f;
    p.xf[3] = x;
    p.xf[4] = s;
    p.xf[5] = c;
    p.xf[6] = 0.0f;
    p.xf[7] = y;
    p.xf[8] = 0.0f;
    p.xf[9] = 0.0f;
    p.xf[10] = 1.0f;
    p.xf[11] = z;
    p.vp[0] = -1.0f;
    p.vp[1] = -0.75f;
    p.vp[2] = 1.0f;
    p.vp[3] = 0.75f;
    p.vp[4] = 1.3333f;
    p.vp[5] = 1.0f;
    p.vp[6] = 5000.0f;
    return p;
}

bool Near(float a, float b, float eps = 1e-4f)
{
    return std::fabs(a - b) <= eps;
}

} // namespace

TEST(camera_halfway_translation_and_rotation)
{
    CameraPose a = Pose(10.0f, 0.0f, 0.0f, 300.0f);
    CameraPose b = Pose(30.0f, 100.0f, -40.0f, 280.0f);
    CameraPose m{};
    CHECK(InterpolateCameraHalfway(a, b, &m));
    CameraPose expect = Pose(20.0f, 50.0f, -20.0f, 290.0f);
    for (int i = 0; i < 12; ++i) {
        CHECK(Near(m.xf[i], expect.xf[i]));
    }
    for (int i = 0; i < 7; ++i) {
        CHECK(Near(m.vp[i], expect.vp[i]));
    }
}

TEST(camera_halfway_of_identical_poses_is_the_pose)
{
    CameraPose a = Pose(-75.0f, 12.0f, 34.0f, 56.0f);
    CameraPose m{};
    CHECK(InterpolateCameraHalfway(a, a, &m));
    for (int i = 0; i < 12; ++i) {
        CHECK(Near(m.xf[i], a.xf[i], 1e-5f));
    }
}

TEST(camera_halfway_takes_the_short_way_round)
{
    CameraPose a = Pose(170.0f, 0, 0, 0);
    CameraPose b = Pose(-170.0f, 0, 0, 0); // 20 degrees apart across +-180
    CameraPose m{};
    CHECK(InterpolateCameraHalfway(a, b, &m));
    CameraPose expect = Pose(180.0f, 0, 0, 0);
    for (int i = 0; i < 12; ++i) {
        CHECK(Near(m.xf[i], expect.xf[i]));
    }
}

TEST(camera_cuts_are_not_interpolated)
{
    CameraPose m{};
    CHECK(InterpolateCameraHalfway(Pose(0, 0, 0, 0), Pose(0, 400.0f, 0, 0), &m));   // a fast pan is not a cut
    CHECK(!InterpolateCameraHalfway(Pose(0, 0, 0, 0), Pose(0, 2000.0f, 0, 0), &m)); // distance
    CHECK(!InterpolateCameraHalfway(Pose(0, 0, 0, 0), Pose(60.0f, 0, 0, 0), &m));   // angle
    CameraPose b = Pose(0, 0, 0, 0);
    b.vp[4] = 1.7777f;
    CHECK(!InterpolateCameraHalfway(Pose(0, 0, 0, 0), b, &m)); // aspect changed
    CameraPose c = Pose(0, 0, 0, 0);
    c.vp[2] = 1.2f;
    CHECK(!InterpolateCameraHalfway(Pose(0, 0, 0, 0), c, &m)); // zoom extent changed by 20 %
}

TEST(camera_living_world_zoom_keeps_the_larger_far_plane)
{
    CameraPose a = Pose(0, 0, 0, 600.0f);
    CameraPose b = Pose(0, 0, 0, 700.0f);
    a.vp[6] = 2750.0f;
    b.vp[6] = 2600.0f;
    CameraCutLimits lw;
    lw.allowFarChange = true;
    CameraPose m{};
    CHECK(InterpolateCameraHalfway(a, b, &m, lw));
    CHECK(Near(m.xf[11], 650.0f));
    CHECK(m.vp[6] == 2750.0f);
    CHECK(InterpolateCameraHalfway(b, a, &m, lw));
    CHECK(m.vp[6] == 2750.0f);
    CHECK(!InterpolateCameraHalfway(a, b, &m)); // the tactical camera still cuts on a far-plane change
    CameraPose c = b;
    c.vp[5] = 20.0f;
    CHECK(!InterpolateCameraHalfway(a, c, &m, lw)); // near plane changed
    CameraPose d = b;
    d.vp[4] = 1.7777f;
    CHECK(!InterpolateCameraHalfway(a, d, &m, lw)); // aspect changed
}

TEST(uniform_scroll_factor_uses_height_above_ground)
{
    CHECK(Near(UniformScrollFactor(50.0f, 50.0f, 540.0f, 0.0f), 50.0f / 540.0f));   // zoomed in close: not floored
    CHECK(Near(UniformScrollFactor(10.0f, 120.0f, 540.0f, 0.0f), 60.0f / 540.0f));  // transient below half the desired
    CHECK(Near(UniformScrollFactor(540.0f, 540.0f, 540.0f, 20.0f), 560.0f / 540.0f));
    CHECK(Near(UniformScrollFactor(0.0f, 0.0f, 450.0f, 0.0f), 1.0f / 450.0f));
    CHECK(Near(UniformScrollFactor(5000.0f, 300.0f, 300.0f, 0.0f), 4.0f));          // capped at 4x the max height
}

namespace {

struct RampY {
    float y0, y1, h0, h1; // height along y (camera forward = +y): h0 before y0, linear to h1 at y1
};

float RampHeight(const void* ctx, float, float y)
{
    const RampY* r = static_cast<const RampY*>(ctx);
    if (y <= r->y0) {
        return r->h0;
    }
    if (y >= r->y1) {
        return r->h1;
    }
    return r->h0 + (r->h1 - r->h0) * (y - r->y0) / (r->y1 - r->y0);
}

float Slope(const RampY& r, float py, float fwd, float right)
{
    SlopeScrollParams p; // k = cot(37.5), sin(37.5), lo 0.5, hi 1.4, fade 0.15
    return SlopeScrollFactor(RampHeight, &r, 0.0f, py, 0.0f, 1.0f, fwd, right, p);
}

} // namespace

TEST(slope_scroll_factor_flat_and_sideways_unchanged)
{
    CHECK(Slope({0, 1000, 0, 0}, 500, 25, 0) == 1.0f);        // flat: exactly 1
    CHECK(Slope({0, 1000, 300, 0}, 500, 0, 25) == 1.0f);      // sideways along the contour of a 0.3 slope
    CHECK(Slope({0, 1000, 300, 0}, 500, 0.0f, 0.0f) == 1.0f); // no motion
}

TEST(slope_scroll_factor_cancels_one_plus_k_s)
{
    CHECK(Near(Slope({0, 1000, 300, 0}, 500, 25, 0), 1.641948f, 1e-4f));  // falling away
    CHECK(Near(Slope({0, 1000, 300, 0}, 500, -25, 0), 1.641948f, 1e-4f)); // back = uphill on the same slope
    CHECK(Near(Slope({0, 1000, 0, 300}, 500, 25, 0), 0.718924f, 1e-4f));  // facing
}

TEST(slope_scroll_factor_clamps_and_fades)
{
    CHECK(Near(Slope({0, 1000, 450, 0}, 500, 25, 0), 2.0f));               // capped at 2x
    CHECK(Near(Slope({0, 1000, 600, 0}, 500, 25, 0), 2.0f));
    CHECK(Near(Slope({0, 1000, 650, 0}, 500, 25, 0), 2.0f));
    CHECK(Near(Slope({0, 1000, 700, 0}, 500, 25, 0), 1.584948f, 1e-4f));   // nearly as steep as the view ray: fading
    CHECK(Slope({0, 1000, 1200, 0}, 500, 25, 0) == 1.0f);                  // steeper than the view ray: no boost
    CHECK(Near(Slope({0, 1000, 0, 600}, 500, 25, 0), 1.0f / 1.4f));        // facing: at most 0.71x
    CHECK(Near(Slope({0, 1000, 0, 1200}, 500, 25, 0), 1.0f / 1.4f));
}

TEST(slope_scroll_factor_uses_the_chord_of_the_step)
{
    // A step crossing a ramp end counts only the part on the ramp (no overshoot).
    CHECK(Near(Slope({0, 1000, 300, 0}, 990, 25, 0), 1.156384f, 1e-4f));
    CHECK(Near(Slope({0, 1000, 600, 0}, 980, 40, 0), 1.390999f, 1e-4f));
    CHECK(Near(Slope({0, 1000, 0, 600}, -10, 25, 0), 0.736707f, 1e-4f)); // entering a facing ramp
}

namespace {

// A decal square [x0, x0 + size] x [y0, y0 + size] on a terrain patch of 10-unit cells covering it (floor/ceil, as the
// projector builds it); height z = 2x + 3y, alpha = 100 per column (at most 255).
struct Patch {
    std::vector<DecalVertex> v;
    int cols = 0, rows = 0;
};

Patch MakePatch(float x0, float y0, float size)
{
    Patch p;
    int i0 = static_cast<int>(std::floor(x0 / 10.0f));
    int i1 = static_cast<int>(std::ceil((x0 + size) / 10.0f));
    int j0 = static_cast<int>(std::floor(y0 / 10.0f));
    int j1 = static_cast<int>(std::ceil((y0 + size) / 10.0f));
    p.cols = i1 - i0 + 1;
    p.rows = j1 - j0 + 1;
    for (int j = j0; j <= j1; ++j) {
        for (int i = i0; i <= i1; ++i) {
            float x = i * 10.0f;
            float y = j * 10.0f;
            DecalVertex d{x, y, 2.0f * x + 3.0f * y, static_cast<uint32_t>(std::min((i - i0) * 100, 255)) << 24 | 0x00FFFFFFu,
                          (x - x0) / size, (y - y0) / size, 0.0f, 0.0f};
            p.v.push_back(d);
        }
    }
    return p;
}

} // namespace

TEST(decal_patch_ends_on_the_texture_square)
{
    Patch p = MakePatch(3.0f, 14.0f, 300.0f); // 3 units past a grid line on the left, 6 on the bottom
    Patch before = p;
    ClampDecalPatchToTexture(p.v.data(), p.cols, p.rows);
    for (int r = 0; r < p.rows; ++r) {
        for (int c = 0; c < p.cols; ++c) {
            const DecalVertex& d = p.v[r * p.cols + c];
            CHECK(d.u >= 0.0f && d.u <= 1.0f);
            CHECK(d.v >= 0.0f && d.v <= 1.0f);
            CHECK(Near(d.z, 2.0f * d.x + 3.0f * d.y, 1e-2f));       // still on the plane: moved along the grid edges
            CHECK(Near(d.u, (d.x - 3.0f) / 300.0f, 1e-5f));         // the texture stays where it was
            CHECK(Near(d.v, (d.y - 14.0f) / 300.0f, 1e-5f));
            bool outer = r == 0 || c == 0 || r == p.rows - 1 || c == p.cols - 1;
            if (!outer) {
                CHECK(std::memcmp(&d, &before.v[r * p.cols + c], sizeof d) == 0);
            }
        }
    }
    const DecalVertex& corner = p.v[0];
    CHECK(Near(corner.x, 3.0f) && Near(corner.y, 14.0f));
    const DecalVertex& top = p.v[(p.rows - 1) * p.cols + p.cols - 1];
    CHECK(Near(top.x, 303.0f) && Near(top.y, 314.0f));
    // colour interpolated along the edge: the left column moved 0.3 of the way to column 1 (alpha 0 -> 100)
    CHECK_EQ(p.v[p.cols].color >> 24, 30u);
    CHECK_EQ(p.v[p.cols].color & 0x00FFFFFFu, 0x00FFFFFFu);
}

TEST(decal_patch_on_the_grid_is_unchanged)
{
    Patch p = MakePatch(20.0f, 40.0f, 300.0f); // the stock snapped case: the patch is the texture square
    Patch before = p;
    ClampDecalPatchToTexture(p.v.data(), p.cols, p.rows);
    CHECK(std::memcmp(p.v.data(), before.v.data(), p.v.size() * sizeof(DecalVertex)) == 0);
    ClampDecalPatchToTexture(nullptr, 5, 5);
    ClampDecalPatchToTexture(p.v.data(), 1, p.rows);
}

TEST(slope_scroll_factor_diagonal_is_screen_weighted)
{
    CHECK(Near(Slope({0, 1000, 300, 0}, 500, 17.68f, 17.68f), 1.097705f, 1e-4f));
    CHECK(Near(Slope({0, 1000, 0, 600}, 500, 17.68f, 17.68f), 0.891022f, 1e-4f));
}
