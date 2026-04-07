/**
 * @file wxbgi_multi_scene_demo.cpp
 * @brief Interactive demo for the Multi-CHDOP / Multi-Scene feature.
 *
 * Demonstrates viewing two independent DDS scene graphs through three cameras
 * in a split-screen layout:
 *
 *   ┌───────────────┬────────────────┐
 *   │  cam_a        │  cam_b         │
 *   │  (left half)  │  (right-top)   │
 *   │  "main" scene │  "main" scene  │
 *   ├───────────────┴────────────────┤
 *   │  cam_c  (bottom half)          │
 *   │  "secondary" scene             │
 *   └────────────────────────────────┘
 *
 * "main" scene    : world-axes + a shaded 3-D box + shaded sphere.
 * "secondary"     : a 2-D bar chart drawn in pixel coordinates.
 *
 * Controls:
 *   Escape         — quit
 *   Left/Right     — orbit cam_a (yaw)
 *   Up/Down        — orbit cam_b (pitch)
 *   cam_a shading  — S=Smooth  F=Flat  W=Wireframe
 *   cam_b shading  — 1=Smooth  2=Flat  3=Wireframe
 *
 * Pass  --test  on the command line to run one frame and exit (ctest mode).
 */

#include "wx_bgi.h"
#include "wx_bgi_3d.h"
#include "wx_bgi_dds.h"
#include "wx_bgi_ext.h"
#include "wx_bgi_solid.h"

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ---------------------------------------------------------------------------
// Window geometry
// ---------------------------------------------------------------------------
static constexpr int kW = 900;
static constexpr int kH = 600;

// Camera angles (degrees) for interactive rotation
static float g_camAYaw   =  30.f;
static float g_camBPitch =  20.f;

// Independent shading mode per perspective camera
static int g_camAShading = WXBGI_SOLID_SMOOTH;
static int g_camBShading = WXBGI_SOLID_SMOOTH;

// ---------------------------------------------------------------------------
// Camera helpers
// ---------------------------------------------------------------------------

static void positionPerspCam(const char *name, float yaw, float pitch, float dist = 20.f)
{
    const float yawR   = yaw   * (float)M_PI / 180.f;
    const float pitchR = pitch * (float)M_PI / 180.f;
    const float ex = dist * std::cos(pitchR) * std::sin(yawR);
    const float ey = dist * std::cos(pitchR) * std::cos(yawR);
    const float ez = dist * std::sin(pitchR);
    wxbgi_cam_set_eye   (name, ex, ey, ez);
    wxbgi_cam_set_target(name, 0.f, 0.f, 0.f);
    wxbgi_cam_set_up    (name, 0.f, 0.f, 1.f);
}

// ---------------------------------------------------------------------------
// Build scenes (called once; DDS persists between frames)
// ---------------------------------------------------------------------------

static void buildMainScene()
{
    wxbgi_dds_scene_set_active("main");

    // World-space reference axes (captured as DDS world_line objects).
    setcolor(bgi::RED);
    wxbgi_world_line(0,0,0,  6,0,0);   // +X (red)
    setcolor(bgi::GREEN);
    wxbgi_world_line(0,0,0,  0,6,0);   // +Y (green)
    setcolor(bgi::LIGHTBLUE);
    wxbgi_world_line(0,0,0,  0,0,6);   // +Z (light blue)

    // Shaded box centred at (0,0,2), 6×6×4 world units.
    // Captured as a DDS solid; shading mode toggles via wxbgi_dds_set_solid_draw_mode().
    wxbgi_solid_set_draw_mode(WXBGI_SOLID_SMOOTH);
    wxbgi_solid_set_face_color(bgi::YELLOW);
    wxbgi_solid_set_edge_color(bgi::DARKGRAY);
    wxbgi_solid_box(0.f, 0.f, 2.f,  6.f, 6.f, 4.f);

    // Shaded sphere floating above the box.
    wxbgi_solid_set_face_color(bgi::CYAN);
    wxbgi_solid_set_edge_color(bgi::DARKGRAY);
    wxbgi_solid_sphere(0.f, 0.f, 5.5f, 1.2f, 24, 16);

    wxbgi_dds_scene_set_active("default");
}

static void buildSecondaryScene()
{
    wxbgi_dds_scene_set_active("secondary");

    const int vpH   = kH / 2;       // height of the bottom viewport (300 px)
    const int bx    = 20, by = 20;

    // Border.
    setcolor(bgi::WHITE);
    rectangle(bx, by, kW - bx, vpH - by);

    // Title.
    setcolor(bgi::YELLOW);
    settextstyle(bgi::DEFAULT_FONT, bgi::HORIZ_DIR, 2);
    outtextxy(bx + 20, by + 15,
              const_cast<char*>("cam_c: 'secondary' scene (pixel-space 2D chart)"));

    // Bar chart data.
    const int vals[]  = {60, 100, 45, 130, 80};
    const int bars    = 5;
    const int bar_w   = 80;
    const int bar_gap = 20;
    const int base_y  = vpH - by - 40;   // baseline y (260 px from top of panel)
    const int start_x = bx + 60;

    // Value labels and bars.
    settextstyle(bgi::DEFAULT_FONT, bgi::HORIZ_DIR, 1);
    for (int i = 0; i < bars; ++i)
    {
        int x1 = start_x + i * (bar_w + bar_gap);
        int y2 = base_y;
        int y1 = y2 - vals[i];

        // Filled bar.
        setfillstyle(bgi::SOLID_FILL, bgi::CYAN);
        bar(x1, y1, x1 + bar_w, y2);

        // Bar outline.
        setcolor(bgi::WHITE);
        rectangle(x1, y1, x1 + bar_w, y2);

        // Value text above each bar.
        char label[8];
        std::snprintf(label, sizeof(label), "%d", vals[i]);
        setcolor(bgi::YELLOW);
        outtextxy(x1 + bar_w/2 - 8, y1 - 14, label);
    }

    // Thick baseline below all bars.
    setlinestyle(bgi::SOLID_LINE, 0, bgi::THICK_WIDTH);
    setcolor(bgi::WHITE);
    line(start_x - 10, base_y + 1,
         start_x + bars * (bar_w + bar_gap), base_y + 1);
    setlinestyle(bgi::SOLID_LINE, 0, bgi::NORM_WIDTH);   // restore

    // X-axis tick labels.
    settextstyle(bgi::DEFAULT_FONT, bgi::HORIZ_DIR, 1);
    const char* xlabels[] = { "A", "B", "C", "D", "E" };
    for (int i = 0; i < bars; ++i)
    {
        int x1 = start_x + i * (bar_w + bar_gap);
        setcolor(bgi::LIGHTGRAY);
        outtextxy(x1 + bar_w/2 - 4, base_y + 6, const_cast<char*>(xlabels[i]));
    }

    wxbgi_dds_scene_set_active("default");
}

// ---------------------------------------------------------------------------
// Shading mode label helper
// ---------------------------------------------------------------------------
static const char* shadingLabel(int mode)
{
    switch (mode)
    {
    case WXBGI_SOLID_SMOOTH:    return "Smooth";
    case WXBGI_SOLID_FLAT:      return "Flat";
    case WXBGI_SOLID_WIREFRAME: return "Wire";
    default:                    return "?";
    }
}

// Set solid draw mode on the "main" scene without changing the active scene
// for DDS capture (restores "default" afterwards).
static void applyMainSceneShading(int mode)
{
    wxbgi_dds_scene_set_active("main");
    wxbgi_dds_set_solid_draw_mode(mode);
    wxbgi_dds_scene_set_active("default");
}

// ---------------------------------------------------------------------------
// Render one frame
// ---------------------------------------------------------------------------
static void renderFrame(bool testMode)
{
    cleardevice();

    // cam_a: left-top panel — apply its shading before rendering.
    applyMainSceneShading(g_camAShading);
    setviewport(0, 0, kW/2 - 1, kH/2 - 1, 1);
    wxbgi_render_dds("cam_a");
    setviewport(0, 0, kW - 1, kH - 1, 0);

    // cam_b: right-top panel — apply its (potentially different) shading.
    applyMainSceneShading(g_camBShading);
    setviewport(kW/2, 0, kW - 1, kH/2 - 1, 1);
    wxbgi_render_dds("cam_b");
    setviewport(0, 0, kW - 1, kH - 1, 0);

    // cam_c: bottom panel (pixel-space 2D bar chart — no shading involved).
    setviewport(0, kH/2, kW - 1, kH - 1, 1);
    wxbgi_render_dds("cam_c");
    setviewport(0, 0, kW - 1, kH - 1, 0);

    // Panel dividers.
    setcolor(bgi::DARKGRAY);
    line(kW/2, 0,      kW/2,   kH/2 - 1);
    line(0,    kH/2,   kW - 1, kH/2);

    // Panel header labels.
    setcolor(bgi::WHITE);
    settextstyle(bgi::DEFAULT_FONT, bgi::HORIZ_DIR, 1);
    char buf[160];
    std::snprintf(buf, sizeof(buf),
        "cam_a | ←/→ yaw=%.0f | S/F/W: %s",
        g_camAYaw, shadingLabel(g_camAShading));
    outtextxy(4, 4, buf);
    std::snprintf(buf, sizeof(buf),
        "cam_b | ↑/↓ pitch=%.0f | 1/2/3: %s",
        g_camBPitch, shadingLabel(g_camBShading));
    outtextxy(kW/2 + 4, 4, buf);

    if (!testMode)
        wxbgi_swap_window_buffers();
}

// ---------------------------------------------------------------------------
// Interactive idle callback
// ---------------------------------------------------------------------------
static void BGI_CALL onIdle()
{
    wxbgi_poll_events();

    if (wxbgi_is_key_down(WXBGI_KEY_ESCAPE))
    {
        wxbgi_wx_close_frame();
        return;
    }

    // Camera orbit (continuous while held).
    bool camChanged = false;
    if (wxbgi_is_key_down(WXBGI_KEY_LEFT))  { g_camAYaw -= 1.f; camChanged = true; }
    if (wxbgi_is_key_down(WXBGI_KEY_RIGHT)) { g_camAYaw += 1.f; camChanged = true; }
    if (wxbgi_is_key_down(WXBGI_KEY_UP))
        { g_camBPitch = std::min(89.f, g_camBPitch + 1.f); camChanged = true; }
    if (wxbgi_is_key_down(WXBGI_KEY_DOWN))
        { g_camBPitch = std::max(-89.f, g_camBPitch - 1.f); camChanged = true; }
    if (camChanged)
    {
        positionPerspCam("cam_a", g_camAYaw, 20.f);
        positionPerspCam("cam_b", -60.f, g_camBPitch);
    }

    // Shading mode toggle — edge-detect so one press = one change.
    // cam_a: S / F / W
    static bool prevS = false, prevF = false, prevW = false;
    bool curS = wxbgi_is_key_down(83) != 0;   // 'S'
    bool curF = wxbgi_is_key_down(70) != 0;   // 'F'
    bool curW = wxbgi_is_key_down(87) != 0;   // 'W'
    if (curS && !prevS) g_camAShading = WXBGI_SOLID_SMOOTH;
    if (curF && !prevF) g_camAShading = WXBGI_SOLID_FLAT;
    if (curW && !prevW) g_camAShading = WXBGI_SOLID_WIREFRAME;
    prevS = curS; prevF = curF; prevW = curW;

    // cam_b: 1 / 2 / 3
    static bool prev1 = false, prev2 = false, prev3 = false;
    bool cur1 = wxbgi_is_key_down(49) != 0;   // '1'
    bool cur2 = wxbgi_is_key_down(50) != 0;   // '2'
    bool cur3 = wxbgi_is_key_down(51) != 0;   // '3'
    if (cur1 && !prev1) g_camBShading = WXBGI_SOLID_SMOOTH;
    if (cur2 && !prev2) g_camBShading = WXBGI_SOLID_FLAT;
    if (cur3 && !prev3) g_camBShading = WXBGI_SOLID_WIREFRAME;
    prev1 = cur1; prev2 = cur2; prev3 = cur3;

    renderFrame(false);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char **argv)
{
    bool testMode = false;
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--test") == 0)
            testMode = true;

    wxbgi_wx_app_create();
    wxbgi_wx_frame_create(kW, kH, "Multi-Scene Demo");
    if (graphresult() != 0)
    {
        std::fprintf(stderr, "initgraph failed\n");
        return 1;
    }

    // Create named scenes.
    wxbgi_dds_scene_create("main");
    wxbgi_dds_scene_create("secondary");

    // cam_a: left-top, perspective, "main" scene.
    wxbgi_cam_create("cam_a", WXBGI_CAM_PERSPECTIVE);
    wxbgi_cam_set_scene("cam_a", "main");
    wxbgi_cam_set_screen_viewport("cam_a", 0, 0, kW/2, kH/2);
    wxbgi_cam_set_perspective("cam_a", 45.f, 0.1f, 200.f);
    positionPerspCam("cam_a", g_camAYaw, 20.f);

    // cam_b: right-top, perspective, "main" scene (different view angle).
    wxbgi_cam_create("cam_b", WXBGI_CAM_PERSPECTIVE);
    wxbgi_cam_set_scene("cam_b", "main");
    wxbgi_cam_set_screen_viewport("cam_b", kW/2, 0, kW/2, kH/2);
    wxbgi_cam_set_perspective("cam_b", 45.f, 0.1f, 200.f);
    positionPerspCam("cam_b", -60.f, g_camBPitch);

    // cam_c: bottom half, 2-D pixel-space ortho, "secondary" scene.
    wxbgi_cam_create("cam_c", WXBGI_CAM_ORTHO);
    wxbgi_cam_set_scene("cam_c", "secondary");
    wxbgi_cam_set_screen_viewport("cam_c", 0, kH/2, kW, kH/2);
    wxbgi_cam_set_ortho("cam_c", 0.f, (float)kW, (float)(kH/2), 0.f, -1.f, 1.f);

    // Populate scenes.
    buildMainScene();
    buildSecondaryScene();

    std::printf("Multi-Scene Demo\n");
    std::printf("  cam_a -> scene '%s'\n", wxbgi_cam_get_scene("cam_a"));
    std::printf("  cam_b -> scene '%s'\n", wxbgi_cam_get_scene("cam_b"));
    std::printf("  cam_c -> scene '%s'\n", wxbgi_cam_get_scene("cam_c"));
    std::printf("  Keys: S=Smooth  F=Flat  W=Wireframe  Arrows=orbit  Esc=quit\n");

    if (testMode)
    {
        renderFrame(true);
        std::printf("wxbgi_multi_scene_demo: test frame OK\n");
        closegraph();
        return 0;
    }

    wxbgi_wx_set_idle_callback(onIdle);
    wxbgi_wx_app_main_loop();
    return 0;
}

