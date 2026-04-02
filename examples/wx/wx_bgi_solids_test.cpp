// wx_bgi_solids_test.cpp
// Automated headless test for the wx-embedded BGI mode.
//
// Calls wxbgi_wx_init_for_canvas() to initialise BGI state without GLFW/GL,
// then exercises the camera and 3-D solid primitive API.  Exits 0 on success.
// Does NOT create a wx window or enter the wx event loop — safe on headless
// CI machines and in CTest.
//
// The interactive wx demo is in examples/wx/wx_bgi_app.cpp (wx_bgi_app target).

#include "wx_bgi.h"
#include "wx_bgi_ext.h"
#include "wx_bgi_3d.h"
#include "wx_bgi_solid.h"

#include <cstdio>
#include <cstdlib>

using namespace bgi;

int main()
{
    // Init BGI in wx-embedded mode (no GLFW, no GL context required)
    wxbgi_wx_init_for_canvas(640, 480);

    setbkcolor(BLACK);
    cleardevice();

    // ---- 3-D perspective camera ----------------------------------------
    int r;
    r = wxbgi_cam_create("testcam", WXBGI_CAM_PERSPECTIVE);
    if (r != 1) { fprintf(stderr, "FAIL: wxbgi_cam_create returned %d\n", r); return 1; }

    wxbgi_cam_set_perspective("testcam", 55.0f, 0.1f, 200.0f);
    wxbgi_cam_set_eye   ("testcam",  6.0f,  5.0f, 5.0f);
    wxbgi_cam_set_target("testcam",  0.0f,  0.0f, 0.0f);
    wxbgi_cam_set_up    ("testcam",  0.0f,  0.0f, 1.0f);
    wxbgi_cam_set_screen_viewport("testcam", 0, 0, 640, 480);
    wxbgi_cam_set_active("testcam");

    // ---- Solid primitives -----------------------------------------------
    wxbgi_solid_set_draw_mode(WXBGI_SOLID_SOLID);

    wxbgi_solid_set_face_color(RED);
    wxbgi_solid_set_edge_color(WHITE);
    wxbgi_solid_box(0.0f, 0.0f, 0.0f, 2.0f, 2.0f, 2.0f);

    wxbgi_solid_set_face_color(CYAN);
    wxbgi_solid_sphere(3.5f, 0.0f, 0.0f, 0.9f, 20, 20);

    wxbgi_solid_set_face_color(YELLOW);
    wxbgi_solid_cylinder(0.0f, 3.5f, 0.0f, 0.6f, 2.0f, 24, 1);

    printf("wx_bgi_solids_test: camera + solid primitives API OK\n");
    return 0;
}
