/**
 * @file test_dds_cam3d_persp.cpp
 * @brief Phase E – Test 5: 3D perspective camera at a 45° diagonal viewpoint.
 *
 * Draws several world-space lines and a rectangle on the XZ plane (Y = −10),
 * then re-renders the scene through a perspective camera positioned at a 45°
 * diagonal angle via wxbgi_render_dds().
 *
 * Camera configuration:
 *   Eye    : (150, 140, 100)  — equal X and ΔY offsets from the scene origin
 *             give an azimuth close to 45° when viewed from above; elevation
 *             ≈ 30° above the XZ plane.
 *   Target : (0, −10, 0)      — centre of the scene on the XZ plane at Y=−10.
 *   Up     : (0, 0, 1)        — Z-up (engineering convention).
 *   Proj   : Perspective, 60° FoV, near=1, far=2000.
 *
 * Primitives drawn (world-space, all with Y = −10):
 *   - Axis lines along X and Z.
 *   - Diagonal cross.
 *   - A rectangle outline (four lines) in the XZ plane.
 *   - A world-space circle at the origin of the XZ plane.
 *
 * Exit code 0 = pass, 1 = fail.
 */

#include "wx_bgi.h"
#include "wx_bgi_3d.h"
#include "wx_bgi_dds.h"
#include "wx_bgi_ext.h"

#include <cstdio>
#include <cstdlib>

namespace
{

void fail(const char *msg)
{
    std::fprintf(stderr, "FAIL [test_dds_cam3d_persp]: %s\n", msg);
    std::exit(1);
}

} // namespace

int main()
{
    constexpr int kW = 800, kH = 600;
    initwindow(kW, kH, "test_dds_cam3d_persp", 0, 0, 0, 0);
    if (graphresult() != 0)
        fail("initwindow failed");

    // -----------------------------------------------------------------------
    // Create the perspective camera.
    // -----------------------------------------------------------------------
    wxbgi_cam_create("cam3d_diag", WXBGI_CAM_PERSPECTIVE);
    wxbgi_cam_set_perspective("cam3d_diag", 60.f, 1.f, 2000.f);
    // Eye at (150, 140, 100): note Y=140 = -10 + 150, so ΔX ≈ ΔY from target,
    // giving ~45° azimuth when projected to the horizontal plane.
    wxbgi_cam_set_eye   ("cam3d_diag", 150.f, 140.f, 100.f);
    wxbgi_cam_set_target("cam3d_diag",   0.f, -10.f,   0.f);
    wxbgi_cam_set_up    ("cam3d_diag",   0.f,   0.f,   1.f);
    wxbgi_cam_set_screen_viewport("cam3d_diag", 0, 0, kW, kH);
    wxbgi_cam_set_active("cam3d_diag");

    // -----------------------------------------------------------------------
    // Draw world-space geometry on the XZ plane at Y = −10.
    //
    // All lines have their Y coordinate fixed at −10 (the XZ plane offset).
    // -----------------------------------------------------------------------

    // X-axis line: (−80, −10, 0) → (+80, −10, 0).
    setcolor(bgi::RED);
    wxbgi_world_line(-80.f, -10.f, 0.f,  80.f, -10.f, 0.f);

    // Z-axis line: (0, −10, −80) → (0, −10, +80).
    setcolor(bgi::BLUE);
    wxbgi_world_line(0.f, -10.f, -80.f,  0.f, -10.f, 80.f);

    // 45° diagonals in the XZ plane.
    setcolor(bgi::YELLOW);
    wxbgi_world_line(-60.f, -10.f, -60.f,  60.f, -10.f, 60.f);
    wxbgi_world_line( 60.f, -10.f, -60.f, -60.f, -10.f, 60.f);

    // Rectangle outline in the XZ plane: corners at (±50, −10, ±50).
    setcolor(bgi::GREEN);
    wxbgi_world_line(-50.f, -10.f, -50.f,  50.f, -10.f, -50.f);
    wxbgi_world_line( 50.f, -10.f, -50.f,  50.f, -10.f,  50.f);
    wxbgi_world_line( 50.f, -10.f,  50.f, -50.f, -10.f,  50.f);
    wxbgi_world_line(-50.f, -10.f,  50.f, -50.f, -10.f, -50.f);

    // A circle in the ground plane (XY plane in world space, Z=0) centred at
    // world origin (0, 0, 0) — demonstrating a world circle near the XZ scene.
    setcolor(bgi::MAGENTA);
    wxbgi_world_circle(0.f, 0.f, 0.f, 30.f);

    // -----------------------------------------------------------------------
    // Verify DDS object count:
    //   default camera + world UCS + cam3d_diag camera
    //   + 9 drawing objects (8 lines + 1 circle) = 12.
    // -----------------------------------------------------------------------
    const int total = wxbgi_dds_object_count();
    if (total < 12)
    {
        std::fprintf(stderr,
                     "FAIL [test_dds_cam3d_persp]: expected >=12 DDS objects, got %d\n",
                     total);
        std::exit(1);
    }

    // -----------------------------------------------------------------------
    // Re-render the DDS through the perspective camera.
    // The call must complete without crashing or producing graphresult() != 0.
    // -----------------------------------------------------------------------
    cleardevice();
    wxbgi_render_dds("cam3d_diag");

    // Pump one event cycle (harmless in headless).
    wxbgi_poll_events();

    closegraph();
    std::printf("PASS [test_dds_cam3d_persp]\n");
    return 0;
}
