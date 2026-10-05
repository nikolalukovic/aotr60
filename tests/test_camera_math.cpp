#include "test.h"

#include "camera_math.h"
#include "runtime.h"

#include <cmath>

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
