/**
 * wxbgi_camera_demo.cpp
 *
 * Demonstrates true RGB colour support (wxbgi_alloc_color), 3D solid rendering,
 * DDS retained-mode scene graph, per-panel viewport clipping, and the interactive
 * DDS object selection system.
 *
 * Scene:
 *  - Gradient-filled rectangle (Red→Yellow), world XY plane
 *  - Cylinder, Sphere, Box, Cone, Torus — coloured 3D solids
 *  - World-space circle, arc, and text label
 *  - World-space XYZ axes and reference grid overlays
 *
 * Four-panel 2×2 layout (1440 × 960 window):
 *
 *   Top-left   "cam_left"  fixed 2D top-down ortho
 *   Top-right  "cam2d"     interactive 2D pan / zoom / rotate  WASD  +/-  Q/E
 *   Bot-left   "cam3d"     interactive 3D perspective orbit    Arrow keys
 *   Bot-right  "cam_iso"   second 3D perspective (alt angle)   I/K  J/L
 *
 * Click any DDS object to select it (flashes orange).
 * Ctrl+click to multi-select.  Delete key removes selected objects.
 * Escape to quit.
 */

#include "wx_bgi.h"
#include "wx_bgi_3d.h"
#include "wx_bgi_ext.h"
#include "wx_bgi_overlay.h"
#include "wx_bgi_solid.h"
#include "bgi_types.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// World-space scene constants
// ---------------------------------------------------------------------------

static constexpr float kRx1 = -80.f, kRy1 = -60.f;   // gradient quad BL
static constexpr float kRx2 =  80.f, kRy2 =  60.f;   // gradient quad TR

// 3D solids
static constexpr float kCylX =  90.f, kCylY =   0.f, kCylZ = 0.f;
static constexpr float kCylR =  22.f, kCylH =  80.f;

static constexpr float kSphX = -90.f, kSphY =   0.f, kSphZ = 30.f;
static constexpr float kSphR =  28.f;

static constexpr float kBoxX =   0.f, kBoxY = -100.f, kBoxZ = 0.f;
static constexpr float kBoxW =  50.f, kBoxD =  50.f,  kBoxH = 60.f;

static constexpr float kConeX =  80.f, kConeY = -100.f, kConeZ = 0.f;
static constexpr float kConeR =  28.f, kConeH =  75.f;

static constexpr float kTorX  = -80.f, kTorY  = -100.f, kTorZ  = 18.f;
static constexpr float kTorMaj =  28.f, kTorMin =   9.f;

static constexpr int kGradSteps = 14;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void drawLabel(int x, int y, const char *text, int color)
{
    setcolor(color);
    outtextxy(x, y, const_cast<char *>(text));
}

// Set clipping viewport, render DDS, restore full-window viewport.
static void renderPanel(const char *cam, int x1, int y1, int x2, int y2,
                        int winW, int winH)
{
    setviewport(x1, y1, x2, y2, 1);
    wxbgi_render_dds(cam);
    setviewport(0, 0, winW - 1, winH - 1, 0);
}

// ---------------------------------------------------------------------------
// buildDdsScene – populate the retained-mode scene (called once before loop)
// ---------------------------------------------------------------------------
static void buildDdsScene(std::array<int, kGradSteps> &gradColors, bool testMode)
{
    // --- Gradient colours: Red → Orange → Yellow ---
    for (int i = 0; i < kGradSteps; ++i)
    {
        float t = (float)i / (float)(kGradSteps - 1);
        gradColors[i] = wxbgi_alloc_color(255, (int)(255.f * t), 0);
    }

    // --- Gradient-filled left/top triangle of the rectangle diagonal ---
    for (int i = 0; i < kGradSteps; ++i)
    {
        float y0 = kRy1 + (kRy2 - kRy1) * (float)i         / (float)kGradSteps;
        float y1 = kRy1 + (kRy2 - kRy1) * (float)(i + 1)   / (float)kGradSteps;
        float xd0 = kRx1 + (kRx2 - kRx1) * (y0 - kRy1) / (kRy2 - kRy1);
        float xd1 = kRx1 + (kRx2 - kRx1) * (y1 - kRy1) / (kRy2 - kRy1);

        int c = gradColors[i];
        setcolor(c);
        setfillstyle(bgi::SOLID_FILL, c);

        float pts[12] = {
            kRx1, y0, 0.f,
            xd0,  y0, 0.f,
            xd1,  y1, 0.f,
            kRx1, y1, 0.f
        };
        wxbgi_world_fillpoly(pts, 4);
    }

    // Rectangle outline (WHITE) + diagonal guide line (DARKGRAY)
    setcolor(15);
    wxbgi_world_rectangle(kRx1, kRy1, 0.f, kRx2, kRy2, 0.f);
    setcolor(8);
    wxbgi_world_line(kRx1, kRy1, 0.f, kRx2, kRy2, 0.f);

    // World circle at origin (CYAN)
    setcolor(3);
    wxbgi_world_circle(0.f, 0.f, 0.f, 55.f);

    // World text label at origin
    setcolor(15);
    wxbgi_world_outtextxy(-10.f, -15.f, 0.f, "origin");

    // ----- 3D Solids -------------------------------------------------
    // In test mode use minimal tessellation so debug builds finish quickly.
    const int cylSl  = testMode ? 6  : 12;
    const int sphSl  = testMode ? 6  : 12;
    const int sphSt  = testMode ? 4  : 8;
    const int coneSl = testMode ? 6  : 12;
    const int torTu  = testMode ? 8  : 16;
    const int torSi  = testMode ? 4  : 8;
    // Cylinder — orange-red
    int cylFace = wxbgi_alloc_color(255, 100, 20);
    wxbgi_solid_set_draw_mode(WXBGI_SOLID_SOLID);
    wxbgi_solid_set_edge_color(15);
    wxbgi_solid_set_face_color(cylFace);
    wxbgi_solid_cylinder(kCylX, kCylY, kCylZ, kCylR, kCylH, cylSl, 1);

    // Sphere — magenta
    int sphFace = wxbgi_alloc_color(200, 40, 200);
    wxbgi_solid_set_face_color(sphFace);
    wxbgi_solid_sphere(kSphX, kSphY, kSphZ, kSphR, sphSl, sphSt);

    // Box — cobalt blue
    int boxFace = wxbgi_alloc_color(40, 80, 220);
    wxbgi_solid_set_face_color(boxFace);
    wxbgi_solid_box(kBoxX, kBoxY, kBoxZ, kBoxW, kBoxD, kBoxH);

    // Cone — teal/cyan
    int coneFace = wxbgi_alloc_color(20, 180, 180);
    wxbgi_solid_set_face_color(coneFace);
    wxbgi_solid_cone(kConeX, kConeY, kConeZ, kConeR, kConeH, coneSl, 1);

    // Torus — lime green
    int torFace = wxbgi_alloc_color(60, 200, 60);
    wxbgi_solid_set_face_color(torFace);
    wxbgi_solid_torus(kTorX, kTorY, kTorZ, kTorMaj, kTorMin, torTu, torSi);

}

// ---------------------------------------------------------------------------
// File-scope interactive state (shared between main and doRenderFrame)
// ---------------------------------------------------------------------------
static int   panelW          = 0;
static int   panelH          = 0;
static int   s_kW            = 0;
static int   s_kH            = 0;
static float azimuth3d       = 45.f;
static float elevation3d     = 30.f;
static float azimuthIso      = 225.f;
static float elevationIso    = 22.f;
static bool  redrawRequested = true;
static double lastFlashRedraw = 0.0;

static constexpr float orbitR = 350.f;

static void updateOrbit3d()
{
    const float az = azimuth3d   * 3.14159265f / 180.f;
    const float el = elevation3d * 3.14159265f / 180.f;
    wxbgi_cam_set_eye("cam3d",
        orbitR * std::cos(el) * std::cos(az),
        orbitR * std::cos(el) * std::sin(az),
        orbitR * std::sin(el));
    wxbgi_cam_set_target("cam3d", 0.f, 0.f, 0.f);
    wxbgi_cam_set_up("cam3d", 0.f, 0.f, 1.f);
}

static void updateOrbitIso()
{
    const float az = azimuthIso   * 3.14159265f / 180.f;
    const float el = elevationIso * 3.14159265f / 180.f;
    wxbgi_cam_set_eye("cam_iso",
        orbitR * std::cos(el) * std::cos(az),
        orbitR * std::cos(el) * std::sin(az),
        orbitR * std::sin(el));
    wxbgi_cam_set_target("cam_iso", 0.f, 0.f, 0.f);
    wxbgi_cam_set_up("cam_iso", 0.f, 0.f, 1.f);
}

// ---------------------------------------------------------------------------
// doRenderFrame — idle callback for interactive (wx) mode
// ---------------------------------------------------------------------------
static void BGI_CALL doRenderFrame()
{
    wxbgi_poll_events();

    if (wxbgi_is_key_down(WXBGI_KEY_ESCAPE))
    {
        wxbgi_wx_close_frame();
        return;
    }

    bool changed = false;

    // Delete key: remove all selected DDS objects.
    if (wxbgi_is_key_down(WXBGI_KEY_DELETE))
    {
        wxbgi_selection_delete_selected();
        changed = true;
    }

    // Top-right cam2d: pan / zoom / rotate.
    if (wxbgi_is_key_down(87)) { wxbgi_cam2d_pan_by("cam2d",  0.f,  5.f); changed = true; }
    if (wxbgi_is_key_down(83)) { wxbgi_cam2d_pan_by("cam2d",  0.f, -5.f); changed = true; }
    if (wxbgi_is_key_down(65)) { wxbgi_cam2d_pan_by("cam2d", -5.f,  0.f); changed = true; }
    if (wxbgi_is_key_down(68)) { wxbgi_cam2d_pan_by("cam2d",  5.f,  0.f); changed = true; }
    if (wxbgi_is_key_down(WXBGI_KEY_EQUAL) || wxbgi_is_key_down(334))
    { wxbgi_cam2d_zoom_at("cam2d", 1.02f, 0.f, 0.f); changed = true; }
    if (wxbgi_is_key_down(WXBGI_KEY_MINUS) || wxbgi_is_key_down(335))
    { wxbgi_cam2d_zoom_at("cam2d", 0.98f, 0.f, 0.f); changed = true; }
    if (wxbgi_is_key_down(81))
    {
        float r = 0.f;
        wxbgi_cam2d_get_rotation("cam2d", &r);
        wxbgi_cam2d_set_rotation("cam2d", r + 1.f);
        changed = true;
    }
    if (wxbgi_is_key_down(69))
    {
        float r = 0.f;
        wxbgi_cam2d_get_rotation("cam2d", &r);
        wxbgi_cam2d_set_rotation("cam2d", r - 1.f);
        changed = true;
    }

    // Bottom-left cam3d: orbit with arrow keys.
    if (wxbgi_is_key_down(WXBGI_KEY_LEFT))  { azimuth3d   -= 1.f; updateOrbit3d(); changed = true; }
    if (wxbgi_is_key_down(WXBGI_KEY_RIGHT)) { azimuth3d   += 1.f; updateOrbit3d(); changed = true; }
    if (wxbgi_is_key_down(WXBGI_KEY_UP))
    { elevation3d = std::min(89.f, elevation3d + 1.f); updateOrbit3d(); changed = true; }
    if (wxbgi_is_key_down(WXBGI_KEY_DOWN))
    { elevation3d = std::max(-89.f, elevation3d - 1.f); updateOrbit3d(); changed = true; }

    // Bottom-right cam_iso: orbit with I/K/J/L.
    if (wxbgi_is_key_down(74)) { azimuthIso   -= 1.f; updateOrbitIso(); changed = true; }
    if (wxbgi_is_key_down(76)) { azimuthIso   += 1.f; updateOrbitIso(); changed = true; }
    if (wxbgi_is_key_down(73))
    { elevationIso = std::min(89.f, elevationIso + 1.f); updateOrbitIso(); changed = true; }
    if (wxbgi_is_key_down(75))
    { elevationIso = std::max(-89.f, elevationIso - 1.f); updateOrbitIso(); changed = true; }

    redrawRequested = redrawRequested || changed;

    if (wxbgi_mouse_moved())
        redrawRequested = true;

    // Periodic redraw for flash animations (~20 fps idle).
    const double now = wxbgi_get_time_seconds();
    if (now - lastFlashRedraw >= 0.05)
    {
        lastFlashRedraw = now;
        redrawRequested = true;
    }

    if (!redrawRequested)
        return;

    // ---- Render -----------------------------------------------------
    cleardevice();

    setcolor(8);
    line(panelW - 1, 0,          panelW - 1, s_kH - 1);
    line(0,          panelH - 1, s_kW - 1,   panelH - 1);

    renderPanel("cam_left", 0,      0,      panelW - 1, panelH - 1, s_kW, s_kH);
    renderPanel("cam2d",    panelW, 0,      s_kW - 1,   panelH - 1, s_kW, s_kH);
    renderPanel("cam3d",    0,      panelH, panelW - 1, s_kH - 1,   s_kW, s_kH);
    renderPanel("cam_iso",  panelW, panelH, s_kW - 1,   s_kH - 1,   s_kW, s_kH);

    drawLabel(6, 6, "cam_left: fixed 2D ortho (read-only)", 15);

    {
        float panX = 0.f, panY = 0.f, zoom = 1.f;
        wxbgi_cam2d_get_pan("cam2d", &panX, &panY);
        wxbgi_cam2d_get_zoom("cam2d", &zoom);
        char buf[80];
        std::snprintf(buf, sizeof(buf), "pan=(%.0f,%.0f)  zoom=%.2f  WASD/+−/Q/E", panX, panY, zoom);
        drawLabel(panelW + 6, 6,  "cam2d: interactive 2D", 15);
        drawLabel(panelW + 6, 20, buf, 8);
    }
    {
        char buf[80];
        std::snprintf(buf, sizeof(buf),
            "az=%.0f  el=%.0f  (Arrow keys)", azimuth3d, elevation3d);
        drawLabel(6, panelH + 6,  "cam3d: 3D perspective orbit", 15);
        drawLabel(6, panelH + 20, buf, 8);
    }
    {
        char buf[80];
        std::snprintf(buf, sizeof(buf),
            "az=%.0f  el=%.0f  (J/L/I/K)", azimuthIso, elevationIso);
        drawLabel(panelW + 6, panelH + 6,  "cam_iso: alt 3D perspective orbit", 15);
        drawLabel(panelW + 6, panelH + 20, buf, 8);
    }
    {
        const int selCount = wxbgi_selection_count();
        char selBuf[80];
        if (selCount > 0)
            std::snprintf(selBuf, sizeof(selBuf),
                "Selected: %d  (Del=delete  Ctrl+click=multi-select)", selCount);
        else
            std::snprintf(selBuf, sizeof(selBuf),
                "Click any DDS object to select  |  Ctrl+click to multi-select");
        drawLabel(6, panelH + 34, selBuf, selCount > 0 ? 14 : 8);
    }

    wxbgi_swap_window_buffers();
    redrawRequested = false;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    const bool testMode = (argc >= 2 && std::strcmp(argv[1], "--test") == 0);

    // In test mode use a small window so the software painter's-algorithm
    // renderer finishes in well under the ctest timeout (even in Debug builds).
    const int kW = testMode ? 480 : 1440;
    const int kH = testMode ? 320 : 960;
    panelW = kW / 2;
    panelH = kH / 2;
    s_kW = kW;
    s_kH = kH;

    // ------------------------------------------------------------------
    // Camera setup — 2×2 grid
    // ------------------------------------------------------------------

    // Top-left: fixed 2D top-down ortho.
    wxbgi_cam2d_create("cam_left");
    wxbgi_cam2d_set_world_height("cam_left", 300.f);
    wxbgi_cam2d_set_pan("cam_left", 0.f, 0.f);
    wxbgi_cam_set_screen_viewport("cam_left", 0, 0, panelW, panelH);

    // Top-right: interactive 2D pan/zoom/rotate.
    wxbgi_cam2d_create("cam2d");
    wxbgi_cam2d_set_world_height("cam2d", 320.f);
    wxbgi_cam2d_set_pan("cam2d", 0.f, 0.f);
    wxbgi_cam_set_screen_viewport("cam2d", panelW, 0, panelW, panelH);

    // Bottom-left: 3D perspective orbit (arrow keys).
    wxbgi_cam_create("cam3d", WXBGI_CAM_PERSPECTIVE);
    wxbgi_cam_set_perspective("cam3d", 55.f, 0.5f, 5000.f);
    wxbgi_cam_set_screen_viewport("cam3d", 0, panelH, panelW, panelH);
    updateOrbit3d();

    // Bottom-right: second 3D perspective (iso-style, I/K/J/L keys).
    wxbgi_cam_create("cam_iso", WXBGI_CAM_PERSPECTIVE);
    wxbgi_cam_set_perspective("cam_iso", 45.f, 0.5f, 5000.f);
    wxbgi_cam_set_screen_viewport("cam_iso", panelW, panelH, panelW, panelH);
    updateOrbitIso();

    // ------------------------------------------------------------------
    // Visual Overlays
    // ------------------------------------------------------------------

    wxbgi_overlay_grid_enable();
    wxbgi_overlay_grid_set_spacing(25.f);
    wxbgi_overlay_grid_set_extent(5);            // ±5 × 25 = ±125 wu

    wxbgi_overlay_ucs_axes_enable();
    wxbgi_overlay_ucs_axes_set_length(120.f);

    // Concentric circles on all 4 cameras.
    const char *allCams[] = {"cam_left", "cam2d", "cam3d", "cam_iso"};
    for (const char *cam : allCams)
    {
        wxbgi_overlay_concentric_enable(cam);
        wxbgi_overlay_concentric_set_count(cam, 3);
    }
    wxbgi_overlay_concentric_set_radii("cam_left", 40.f, 120.f);
    wxbgi_overlay_concentric_set_radii("cam2d",    40.f, 120.f);
    wxbgi_overlay_concentric_set_radii("cam3d",    60.f, 180.f);
    wxbgi_overlay_concentric_set_radii("cam_iso",  60.f, 180.f);

    // Selection cursor — all panels except the fixed left one.
    wxbgi_overlay_cursor_enable("cam2d");
    wxbgi_overlay_cursor_set_size("cam2d", 14);
    wxbgi_overlay_cursor_set_color("cam2d", 0);    // blue

    wxbgi_overlay_cursor_enable("cam3d");
    wxbgi_overlay_cursor_set_size("cam3d", 14);
    wxbgi_overlay_cursor_set_color("cam3d", 1);    // green

    wxbgi_overlay_cursor_enable("cam_iso");
    wxbgi_overlay_cursor_set_size("cam_iso", 14);
    wxbgi_overlay_cursor_set_color("cam_iso", 2);  // red

    if (testMode)
    {
        wxbgi_wx_app_create();
        wxbgi_wx_frame_create(kW, kH, "wx_bgi test");
        if (graphresult() != 0)
            return 1;

        wxbgi_cam_set_active("cam_left");
        cleardevice();

        std::array<int, kGradSteps> gradColors{};
        buildDdsScene(gradColors, testMode);

        cleardevice();

        // In test mode render only the fast 2D panel to keep debug builds quick.
        setcolor(8);
        line(panelW - 1, 0,          panelW - 1, kH - 1);
        line(0,          panelH - 1, kW - 1,     panelH - 1);

        renderPanel("cam_left", 0, 0, panelW - 1, panelH - 1, kW, kH);

        drawLabel(6, 6, "cam_left: fixed 2D ortho (read-only)", 15);

        wxbgi_wx_close_after_ms(500);
        wxbgi_wx_app_main_loop();
        return 0;
    }

    // ------------------------------------------------------------------
    // Interactive (wx standalone) mode
    // ------------------------------------------------------------------
    wxbgi_wx_app_create();
    wxbgi_wx_frame_create(kW, kH,
        "wx_bgi 4-View Demo \xe2\x80\x94 Click objects to select, Del to delete");
    if (graphresult() != 0)
        return 1;

    wxbgi_cam_set_active("cam_left");
    cleardevice();

    std::array<int, kGradSteps> gradColors{};
    buildDdsScene(gradColors, testMode);

    cleardevice();

    wxbgi_wx_set_idle_callback(doRenderFrame);
    wxbgi_wx_set_frame_rate(60);
    wxbgi_wx_app_main_loop();
    return 0;
}

