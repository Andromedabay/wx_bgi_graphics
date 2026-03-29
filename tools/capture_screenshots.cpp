/**
 * capture_screenshots.cpp
 *
 * Standalone utility that builds the standard camera-demo DDS scene, renders
 * it through each of the three cameras, exports individual PNG screenshots,
 * then exports the combined full-window view.
 *
 * Run from the repository root:
 *
 *   Windows:  build\Debug\capture_screenshots.exe [images_dir]
 *   Linux:    build/capture_screenshots            [images_dir]
 *   macOS:    build/capture_screenshots            [images_dir]
 *
 * The optional argument sets the output directory (default: "images").
 */

#include "wx_bgi.h"
#include "wx_bgi_3d.h"
#include "wx_bgi_ext.h"
#include "wx_bgi_solid.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

// ---------------------------------------------------------------------------
// Scene constants  (mirror wxbgi_camera_demo.cpp)
// ---------------------------------------------------------------------------

static constexpr float kRx1      = -80.f, kRy1 = -60.f;
static constexpr float kRx2      =  80.f, kRy2 =  60.f;
static constexpr float kCylX     =  90.f, kCylY = 0.f, kCylZ = 0.f;
static constexpr float kCylR     =  30.f, kCylH = 80.f;
static constexpr int   kGradSteps = 14;

// ---------------------------------------------------------------------------
// Build scene
// ---------------------------------------------------------------------------

static void buildScene(std::array<int, kGradSteps> &gradColors)
{
    // Gradient colours: Red -> Orange -> Yellow
    for (int i = 0; i < kGradSteps; ++i)
    {
        float t = (float)i / (float)(kGradSteps - 1);
        gradColors[i] = wxbgi_alloc_color(255, (int)(255.f * t), 0);
    }

    // World XYZ axes
    setcolor(4); wxbgi_world_line(0,0,0, 120,0,0);
    setcolor(2); wxbgi_world_line(0,0,0,   0,120,0);
    setcolor(1); wxbgi_world_line(0,0,0,   0,  0,120);
    setcolor(4); wxbgi_world_outtextxy(125.f,  0.f,  0.f, "X");
    setcolor(2); wxbgi_world_outtextxy(  0.f,125.f,  0.f, "Y");
    setcolor(1); wxbgi_world_outtextxy(  0.f,  0.f,125.f, "Z");

    // Reference grid in the XY plane
    for (int g = -100; g <= 100; g += 25)
    {
        setcolor(g == 0 ? 2 : 7);
        wxbgi_world_line((float)g, -100.f, 0.f, (float)g, 100.f, 0.f);
    }
    for (int g = -100; g <= 100; g += 25)
    {
        setcolor(g == 0 ? 4 : 7);
        wxbgi_world_line(-100.f, (float)g, 0.f, 100.f, (float)g, 0.f);
    }

    // Gradient-filled left/top triangle (14 horizontal strips)
    for (int i = 0; i < kGradSteps; ++i)
    {
        float y0  = kRy1 + (kRy2 - kRy1) * (float)i       / (float)kGradSteps;
        float y1  = kRy1 + (kRy2 - kRy1) * (float)(i + 1) / (float)kGradSteps;
        float xd0 = kRx1 + (kRx2 - kRx1) * (y0 - kRy1)   / (kRy2 - kRy1);
        float xd1 = kRx1 + (kRx2 - kRx1) * (y1 - kRy1)   / (kRy2 - kRy1);

        int c = gradColors[i];
        setcolor(c);
        setfillstyle(bgi::SOLID_FILL, c);
        float pts[12] = { kRx1,y0,0.f, xd0,y0,0.f, xd1,y1,0.f, kRx1,y1,0.f };
        wxbgi_world_fillpoly(pts, 4);
    }

    // Rectangle outline + diagonal guide
    setcolor(15); wxbgi_world_rectangle(kRx1,kRy1,0.f, kRx2,kRy2,0.f);
    setcolor(7);  wxbgi_world_line(kRx1,kRy1,0.f, kRx2,kRy2,0.f);

    // Cylinder
    int cylFace = wxbgi_alloc_color(255, 100, 20);
    wxbgi_solid_set_draw_mode(WXBGI_SOLID_SOLID);
    wxbgi_solid_set_edge_color(15);
    wxbgi_solid_set_face_color(cylFace);
    wxbgi_solid_cylinder(kCylX, kCylY, kCylZ, kCylR, kCylH, 20, 1);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    // Output directory (default: "images")
    std::string outDir = "images";
    if (argc >= 2 && argv[1][0] != '-')
        outDir = argv[1];

    constexpr int kW = 1440, kH = 480;
    const int panelW = kW / 3;

    initwindow(kW, kH, "Screenshot Capture", 50, 50, 0, 0);
    if (graphresult() != 0)
    {
        std::fprintf(stderr, "capture_screenshots: initwindow failed\n");
        return 1;
    }

    // ------------------------------------------------------------------
    // Camera setup (mirrors camera demo)
    // ------------------------------------------------------------------

    wxbgi_cam2d_create("cam_left");
    wxbgi_cam2d_set_world_height("cam_left", 240.f);
    wxbgi_cam2d_set_pan("cam_left", 0.f, 0.f);
    wxbgi_cam_set_screen_viewport("cam_left", 0, 0, panelW, kH);

    wxbgi_cam2d_create("cam2d");
    wxbgi_cam2d_set_world_height("cam2d", 280.f);
    wxbgi_cam2d_set_pan("cam2d", 0.f, 0.f);
    wxbgi_cam_set_screen_viewport("cam2d", panelW, 0, panelW, kH);

    wxbgi_cam_create("cam3d", WXBGI_CAM_PERSPECTIVE);
    wxbgi_cam_set_perspective("cam3d", 60.f, 0.5f, 5000.f);
    wxbgi_cam_set_screen_viewport("cam3d", panelW * 2, 0, panelW, kH);

    const float az = 45.f * 3.14159265f / 180.f;
    const float el = 30.f * 3.14159265f / 180.f;
    const float R  = 300.f;
    wxbgi_cam_set_eye("cam3d", R*std::cos(el)*std::cos(az),
                               R*std::cos(el)*std::sin(az),
                               R*std::sin(el));
    wxbgi_cam_set_target("cam3d", 0.f, 0.f, 0.f);
    wxbgi_cam_set_up    ("cam3d", 0.f, 0.f, 1.f);

    // ------------------------------------------------------------------
    // Build the DDS scene
    // ------------------------------------------------------------------

    wxbgi_cam_set_active("cam_left");
    cleardevice();

    std::array<int, kGradSteps> gradColors{};
    buildScene(gradColors);

    // Render the three-panel composite to the window (for full-window export)
    cleardevice();

    setcolor(7);
    line(panelW - 1, 0, panelW - 1, kH - 1);
    line(2 * panelW - 1, 0, 2 * panelW - 1, kH - 1);

    setviewport(0,          0, panelW - 1,   kH - 1, 1); wxbgi_render_dds("cam_left");
    setviewport(panelW,     0, 2*panelW - 1, kH - 1, 1); wxbgi_render_dds("cam2d");
    setviewport(2*panelW,   0, kW - 1,       kH - 1, 1); wxbgi_render_dds("cam3d");
    setviewport(0, 0, kW - 1, kH - 1, 0);  // full window, no clip

    // ------------------------------------------------------------------
    // Export
    // ------------------------------------------------------------------

    struct Export { const char *cam; const char *file; const char *desc; };
    const Export exports[] = {
        { "cam_left", "screenshot_cam_left_2d.png",   "Fixed 2D top-down ortho" },
        { "cam2d",    "screenshot_cam2d_interactive.png", "Interactive 2D pan/zoom" },
        { "cam3d",    "screenshot_cam3d_persp.png",    "3D perspective orbit"    },
    };

    int ok = 0, fail = 0;
    for (const auto &e : exports)
    {
        std::string path = outDir + "/" + e.file;
        cleardevice();  // clear so each camera gets a clean render
        int rc = wxbgi_export_png_camera_view(e.cam, path.c_str());
        if (rc == 0) { std::printf("  OK   %s  (%s)\n", path.c_str(), e.desc); ++ok; }
        else         { std::fprintf(stderr, "  FAIL %s\n", path.c_str()); ++fail; }
    }

    // Full 3-panel composite
    {
        cleardevice();
        setcolor(7);
        line(panelW - 1, 0, panelW - 1, kH - 1);
        line(2 * panelW - 1, 0, 2 * panelW - 1, kH - 1);

        setviewport(0,        0, panelW-1,   kH-1, 1); wxbgi_render_dds("cam_left");
        setviewport(panelW,   0, 2*panelW-1, kH-1, 1); wxbgi_render_dds("cam2d");
        setviewport(2*panelW, 0, kW-1,       kH-1, 1); wxbgi_render_dds("cam3d");
        setviewport(0, 0, kW-1, kH-1, 0);

        std::string path = outDir + "/screenshot_camera_demo_3panel.png";
        int rc = wxbgi_export_png(path.c_str());
        if (rc == 0) { std::printf("  OK   %s  (3-panel composite)\n", path.c_str()); ++ok; }
        else         { std::fprintf(stderr, "  FAIL %s\n", path.c_str()); ++fail; }
    }

    std::printf("\n%d saved, %d failed.\n", ok, fail);

    closegraph();
    return fail > 0 ? 1 : 0;
}
