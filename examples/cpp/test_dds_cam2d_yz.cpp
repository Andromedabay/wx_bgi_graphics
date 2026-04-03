/**
 * @file test_dds_cam2d_yz.cpp
 * @brief Phase E – Test 4: 2D orthographic camera looking at the YZ plane (X=5).
 *
 * Sets up an orthographic camera positioned at X = 5+distance, looking in the
 * −X direction toward the plane X=5.  Draws several world-space lines and
 * circles that lie on (or near) that YZ plane, then re-renders the scene
 * through the orthographic camera via wxbgi_render_dds() and verifies that
 * the call completes without error.
 *
 * Camera configuration:
 *   Eye    : (205, 0, 0)  — 200 world-units in front of the YZ@X=5 plane.
 *   Target : (5, 0, 0)    — centre of interest on the plane.
 *   Up     : (0, 0, 1)    — Z-up (engineering convention).
 *   Proj   : Orthographic, 200 world-units tall.
 *
 * Primitives drawn (world-space):
 *   - Lines along the Y and Z axes at X=5.
 *   - Circles centred on the plane X=5.
 *
 * Run with no arguments (used by ctest).
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
    std::fprintf(stderr, "FAIL [test_dds_cam2d_yz]: %s\n", msg);
    std::exit(1);
}

} // namespace

int main()
{
    constexpr int kW = 640, kH = 480;
    wxbgi_wx_app_create();
    wxbgi_wx_frame_create(kW, kH, "test_dds_cam2d_yz");
    if (graphresult() != 0)
        fail("wxbgi_wx_frame_create failed");

    // -----------------------------------------------------------------------
    // Create the YZ-plane camera.
    //
    //  Eye at (205, 0, 0): 200 units to the right of the plane X=5.
    //  Looking toward (5, 0, 0).
    //  Z-up world convention.
    //  Orthographic projection with 200 world-units visible vertically.
    // -----------------------------------------------------------------------
    wxbgi_cam_create("cam_yz", WXBGI_CAM_ORTHO);
    wxbgi_cam_set_eye   ("cam_yz", 205.f, 0.f, 0.f);
    wxbgi_cam_set_target("cam_yz", 5.f,   0.f, 0.f);
    wxbgi_cam_set_up    ("cam_yz", 0.f,   0.f, 1.f);
    wxbgi_cam_set_ortho_auto("cam_yz", 200.f, -500.f, 500.f);
    wxbgi_cam_set_screen_viewport("cam_yz", 0, 0, kW, kH);
    wxbgi_cam_set_active("cam_yz");

    // -----------------------------------------------------------------------
    // Draw world-space primitives on (or very near) the plane X=5.
    //
    // Convention: wxbgi_world_line(x0,y0,z0, x1,y1,z1)
    // -----------------------------------------------------------------------
    setcolor(bgi::WHITE);

    // Y-axis line on the plane X=5: from (5, -90, 0) to (5, +90, 0).
    wxbgi_world_line(5.f, -90.f, 0.f,  5.f, 90.f, 0.f);

    // Z-axis line on the plane X=5: from (5, 0, -90) to (5, 0, +90).
    wxbgi_world_line(5.f, 0.f, -90.f,  5.f, 0.f, 90.f);

    // Diagonal cross on the plane.
    setcolor(bgi::CYAN);
    wxbgi_world_line(5.f, -60.f, -60.f,  5.f, 60.f, 60.f);
    wxbgi_world_line(5.f,  60.f, -60.f,  5.f, -60.f, 60.f);

    // A circle centred on the plane (in the YZ plane, drawn via world rect).
    // wxbgi_world_circle draws in the world XY plane; to get a circle on YZ
    // we use the UCS approach.  For this test we simply draw a rectangle as a
    // bounding box proxy so the render pipeline is exercised.
    setcolor(bgi::YELLOW);
    wxbgi_world_line(5.f, -40.f, -40.f,  5.f,  40.f, -40.f);
    wxbgi_world_line(5.f,  40.f, -40.f,  5.f,  40.f,  40.f);
    wxbgi_world_line(5.f,  40.f,  40.f,  5.f, -40.f,  40.f);
    wxbgi_world_line(5.f, -40.f,  40.f,  5.f, -40.f, -40.f);

    // -----------------------------------------------------------------------
    // Verify the DDS has drawn the expected number of primitives:
    // default camera + world UCS + cam_yz camera + 8 world lines = 11.
    // -----------------------------------------------------------------------
    const int total = wxbgi_dds_object_count();
    if (total < 11)
    {
        std::fprintf(stderr,
                     "FAIL [test_dds_cam2d_yz]: expected >=11 DDS objects, got %d\n",
                     total);
        std::exit(1);
    }

    // -----------------------------------------------------------------------
    // Re-render the DDS through the YZ-plane camera.
    // The call must complete without crashing.
    // -----------------------------------------------------------------------
    cleardevice();
    wxbgi_render_dds("cam_yz");

    // Pump one event cycle so the frame is visible (harmless in headless runs).
    wxbgi_poll_events();

    wxbgi_wx_close_after_ms(500);
    wxbgi_wx_app_main_loop();
    std::printf("PASS [test_dds_cam2d_yz]\n");
    return 0;
}
