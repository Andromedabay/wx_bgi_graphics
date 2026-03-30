/**
 * wxbgi_camera_demo.cpp
 *
 * Demonstrates true RGB colour support (wxbgi_alloc_color), 3D solid rendering,
 * DDS retained-mode scene graph, and per-panel viewport clipping.
 *
 * Scene:
 *  - Rectangle in the world XY plane, diagonally bisected BL→TR.
 *    The left/top triangle is filled with a smooth Red→Orange→Yellow gradient
 *    using wxbgi_alloc_color (no palette remapping needed).
 *  - A wxbgi_solid_cylinder positioned so its right side extends beyond the
 *    boundary of the left and centre panels — viewport clipping is clearly visible.
 *  - World-space XYZ axes and a reference grid.
 *
 * The same DDS scene is shown through THREE cameras, each clipped to its panel
 * via  setviewport( …, clip=1 ) :
 *
 *   Left   "cam_left"  fixed 2D top-down ortho
 *   Centre "cam2d"     interactive 2D pan / zoom / rotate   WASD  +/-  Q/E
 *   Right  "cam3d"     interactive 3D perspective orbit      Arrow keys
 *
 * Escape to quit.
 */

#include "wx_bgi.h"
#include "wx_bgi_3d.h"
#include "wx_bgi_ext.h"
#include "wx_bgi_overlay.h"
#include "wx_bgi_solid.h"

#include <GLFW/glfw3.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// World-space scene constants
// ---------------------------------------------------------------------------

// Rectangle in the XY plane (Z = 0)
static constexpr float kRx1 = -80.f, kRy1 = -60.f;   // BL world corner
static constexpr float kRx2 =  80.f, kRy2 =  60.f;   // TR world corner

// Cylinder: centre chosen so right edge (x=90+30=120) equals the cam_left
// world boundary of ±120  →  the rightmost slice is clipped in the left panel.
static constexpr float kCylX = 90.f, kCylY = 0.f, kCylZ = 0.f;
static constexpr float kCylR = 30.f, kCylH = 80.f;

// Number of horizontal gradient strips in the left/top triangle
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
    setviewport(x1, y1, x2, y2, 1);              // clip = ON
    wxbgi_render_dds(cam);
    setviewport(0, 0, winW - 1, winH - 1, 0);    // full window, no clip
}

// ---------------------------------------------------------------------------
// buildDdsScene – populate the retained-mode scene (called once before loop)
// ---------------------------------------------------------------------------
static void buildDdsScene(std::array<int, kGradSteps> &gradColors)
{
    // --- Allocate kGradSteps true-RGB gradient colours (Red → Orange → Yellow) ---
    for (int i = 0; i < kGradSteps; ++i)
    {
        float t = (float)i / (float)(kGradSteps - 1);
        gradColors[i] = wxbgi_alloc_color(255, (int)(255.f * t), 0);
    }

    // --- Gradient-filled left/top triangle of the diagonal bisection ---
    //
    // Rectangle corners (world XY): BL(-80,-60)  BR(80,-60)  TR(80,60)  TL(-80,60)
    // Diagonal: BL(-80,-60) → TR(80,60)
    // Left/top triangle:  BL, TL, TR
    //
    // At world Y = y the strip spans x ∈ [kRx1 , x_diag(y)]
    //   where  x_diag(y) = kRx1 + (kRx2-kRx1)*(y-kRy1)/(kRy2-kRy1)
    //
    // Strips run bottom (kRy1=-60, RED) → top (kRy2=+60, YELLOW).
    for (int i = 0; i < kGradSteps; ++i)
    {
        float y0 = kRy1 + (kRy2 - kRy1) * (float)i         / (float)kGradSteps;
        float y1 = kRy1 + (kRy2 - kRy1) * (float)(i + 1)   / (float)kGradSteps;
        float xd0 = kRx1 + (kRx2 - kRx1) * (y0 - kRy1) / (kRy2 - kRy1);
        float xd1 = kRx1 + (kRx2 - kRx1) * (y1 - kRy1) / (kRy2 - kRy1);

        int c = gradColors[i];
        setcolor(c);
        setfillstyle(bgi::SOLID_FILL, c);

        // Trapezoid vertices (world XY, Z=0): BL → BR → TR → TL
        float pts[12] = {
            kRx1, y0, 0.f,
            xd0,  y0, 0.f,
            xd1,  y1, 0.f,
            kRx1, y1, 0.f
        };
        wxbgi_world_fillpoly(pts, 4);
    }

    // Rectangle outline (WHITE) + diagonal guide line (LIGHTGRAY)
    setcolor(15);
    wxbgi_world_rectangle(kRx1, kRy1, 0.f, kRx2, kRy2, 0.f);
    setcolor(7);
    wxbgi_world_line(kRx1, kRy1, 0.f, kRx2, kRy2, 0.f);

    // --- Cylinder ---
    // True-RGB face colour: vivid orange-red from extended palette
    int cylFace = wxbgi_alloc_color(255, 100, 20);
    wxbgi_solid_set_draw_mode(WXBGI_SOLID_SOLID);
    wxbgi_solid_set_edge_color(15);           // WHITE edges
    wxbgi_solid_set_face_color(cylFace);
    wxbgi_solid_cylinder(kCylX, kCylY, kCylZ, kCylR, kCylH, 20, 1);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    const bool testMode = (argc >= 2 && std::strcmp(argv[1], "--test") == 0);

    constexpr int kW = 1440, kH = 480;
    initwindow(kW, kH, "RGB Gradient + Cylinder — 3-View Clipping Demo", 50, 50, 0, 0);
    if (graphresult() != 0)
        return 1;

    const int panelW = kW / 3;   // 480 px each

    // ------------------------------------------------------------------
    // Camera setup
    // ------------------------------------------------------------------

    // Left panel — fixed 2D top-down ortho.
    // World height 240 → visible X range ±120.
    // Cylinder right edge is at x = 90+30 = 120 → clips exactly at panel edge.
    wxbgi_cam2d_create("cam_left");
    wxbgi_cam2d_set_world_height("cam_left", 240.f);
    wxbgi_cam2d_set_pan("cam_left", 0.f, 0.f);
    wxbgi_cam_set_screen_viewport("cam_left", 0, 0, panelW, kH);

    // Centre panel — interactive 2D pan/zoom.
    wxbgi_cam2d_create("cam2d");
    wxbgi_cam2d_set_world_height("cam2d", 280.f);
    wxbgi_cam2d_set_pan("cam2d", 0.f, 0.f);
    wxbgi_cam_set_screen_viewport("cam2d", panelW, 0, panelW, kH);

    // Right panel — 3D perspective orbit.
    wxbgi_cam_create("cam3d", WXBGI_CAM_PERSPECTIVE);
    wxbgi_cam_set_perspective("cam3d", 60.f, 0.5f, 5000.f);
    wxbgi_cam_set_screen_viewport("cam3d", panelW * 2, 0, panelW, kH);

    float azimuth   = 45.f;
    float elevation = 30.f;
    const float orbitR = 300.f;

    auto updateOrbit = [&]()
    {
        const float az = azimuth   * 3.14159265f / 180.f;
        const float el = elevation * 3.14159265f / 180.f;
        wxbgi_cam_set_eye("cam3d",
            orbitR * std::cos(el) * std::cos(az),
            orbitR * std::cos(el) * std::sin(az),
            orbitR * std::sin(el));
        wxbgi_cam_set_target("cam3d", 0.f, 0.f, 0.f);
        wxbgi_cam_set_up("cam3d", 0.f, 0.f, 1.f);
    };
    updateOrbit();

    // ------------------------------------------------------------------
    // Visual Overlays — enabled before building the scene so they appear
    // in all three panels on the very first frame.
    // ------------------------------------------------------------------

    // Reference grid: UCS XY plane, ±100 world units, 25-unit spacing.
    wxbgi_overlay_grid_enable();
    wxbgi_overlay_grid_set_spacing(25.f);
    wxbgi_overlay_grid_set_extent(4);            // ±4 × 25 = ±100 wu

    // UCS axes at the world origin (upper-case X/Y/Z labels) and at any
    // active UCS origin (lower-case labels).
    wxbgi_overlay_ucs_axes_enable();
    wxbgi_overlay_ucs_axes_set_length(110.f);

    // Concentric circles + crosshair in each camera panel.
    // For the fixed 2D panel, use tighter radii.
    wxbgi_overlay_concentric_enable("cam_left");
    wxbgi_overlay_concentric_set_count("cam_left", 3);
    wxbgi_overlay_concentric_set_radii("cam_left", 30.f, 90.f);

    // For the interactive 2D panel, use the same default radii.
    wxbgi_overlay_concentric_enable("cam2d");
    wxbgi_overlay_concentric_set_count("cam2d", 3);
    wxbgi_overlay_concentric_set_radii("cam2d", 30.f, 90.f);

    // For the 3D perspective panel, use slightly larger radii.
    wxbgi_overlay_concentric_enable("cam3d");
    wxbgi_overlay_concentric_set_count("cam3d", 3);
    wxbgi_overlay_concentric_set_radii("cam3d", 50.f, 150.f);

    // Selection cursor square — enabled in the interactive panels only.
    // Blue cursor in centre panel, green cursor in 3D panel.
    wxbgi_overlay_cursor_enable("cam2d");
    wxbgi_overlay_cursor_set_size("cam2d", 12);
    wxbgi_overlay_cursor_set_color("cam2d", 0);    // blue

    wxbgi_overlay_cursor_enable("cam3d");
    wxbgi_overlay_cursor_set_size("cam3d", 12);
    wxbgi_overlay_cursor_set_color("cam3d", 1);    // green

    // ------------------------------------------------------------------
    // Build the DDS scene (once — all objects are stored for re-render)
    // ------------------------------------------------------------------
    wxbgi_cam_set_active("cam_left");   // immediate renders go to left panel
    cleardevice();

    std::array<int, kGradSteps> gradColors{};
    buildDdsScene(gradColors);

    cleardevice();   // wipe the immediate-render residue from DDS building

    // ------------------------------------------------------------------
    // Main render loop
    // ------------------------------------------------------------------
    bool redrawRequested = true;

    do
    {
        if (!testMode)
        {
            wxbgi_poll_events();

            if (wxbgi_is_key_down(GLFW_KEY_ESCAPE))
                break;

            bool changed = false;

            // Delete key: remove all selected DDS objects.
            if (wxbgi_is_key_down(GLFW_KEY_DELETE))
            {
                wxbgi_selection_delete_selected();
                changed = true;
            }

            // Centre 2D camera: pan / zoom / rotate
            if (wxbgi_is_key_down(GLFW_KEY_W)) { wxbgi_cam2d_pan_by("cam2d",  0.f,  4.f); changed = true; }
            if (wxbgi_is_key_down(GLFW_KEY_S)) { wxbgi_cam2d_pan_by("cam2d",  0.f, -4.f); changed = true; }
            if (wxbgi_is_key_down(GLFW_KEY_A)) { wxbgi_cam2d_pan_by("cam2d", -4.f,  0.f); changed = true; }
            if (wxbgi_is_key_down(GLFW_KEY_D)) { wxbgi_cam2d_pan_by("cam2d",  4.f,  0.f); changed = true; }
            if (wxbgi_is_key_down(GLFW_KEY_EQUAL) || wxbgi_is_key_down(GLFW_KEY_KP_ADD))
            { wxbgi_cam2d_zoom_at("cam2d", 1.02f, 0.f, 0.f); changed = true; }
            if (wxbgi_is_key_down(GLFW_KEY_MINUS) || wxbgi_is_key_down(GLFW_KEY_KP_SUBTRACT))
            { wxbgi_cam2d_zoom_at("cam2d", 0.98f, 0.f, 0.f); changed = true; }
            if (wxbgi_is_key_down(GLFW_KEY_Q))
            {
                float r = 0.f;
                wxbgi_cam2d_get_rotation("cam2d", &r);
                wxbgi_cam2d_set_rotation("cam2d", r + 1.f);
                changed = true;
            }
            if (wxbgi_is_key_down(GLFW_KEY_E))
            {
                float r = 0.f;
                wxbgi_cam2d_get_rotation("cam2d", &r);
                wxbgi_cam2d_set_rotation("cam2d", r - 1.f);
                changed = true;
            }

            // Right 3D camera: orbit
            if (wxbgi_is_key_down(GLFW_KEY_LEFT))
            { azimuth -= 1.f; updateOrbit(); changed = true; }
            if (wxbgi_is_key_down(GLFW_KEY_RIGHT))
            { azimuth += 1.f; updateOrbit(); changed = true; }
            if (wxbgi_is_key_down(GLFW_KEY_UP))
            { elevation = std::min(89.f, elevation + 1.f); updateOrbit(); changed = true; }
            if (wxbgi_is_key_down(GLFW_KEY_DOWN))
            { elevation = std::max(-89.f, elevation - 1.f); updateOrbit(); changed = true; }

            redrawRequested = redrawRequested || changed;

            // Redraw when mouse moves (selection cursor square tracks the pointer).
            if (wxbgi_mouse_moved())
                redrawRequested = true;

            // Periodic redraw for flash animations (selection flash every ~1 s,
            // cursor colour toggle every ~2 s).  Force a redraw at ~20 fps so
            // these effects are visible even when no key/mouse event occurred.
            static double lastFlashRedraw = 0.0;
            const double now = glfwGetTime();
            if (now - lastFlashRedraw >= 0.05)   // 50 ms ≈ 20 fps cap for idle
            {
                lastFlashRedraw = now;
                redrawRequested = true;
            }

            if (!redrawRequested) { delay(8); continue; }
        }

        // ---- Render -------------------------------------------------
        cleardevice();

        // Panel dividers (full window, no clip)
        setcolor(7);
        line(panelW - 1, 0, panelW - 1, kH - 1);
        line(2 * panelW - 1, 0, 2 * panelW - 1, kH - 1);

        // Three panels — each with clipping viewport
        renderPanel("cam_left", 0,          0, panelW - 1,     kH - 1, kW, kH);
        renderPanel("cam2d",    panelW,     0, 2*panelW - 1,   kH - 1, kW, kH);
        renderPanel("cam3d",    2*panelW,   0, kW - 1,         kH - 1, kW, kH);

        // Panel labels (pixel-space, drawn after render so they appear on top)
        drawLabel(6, 6,  "cam_left: 2D fixed ortho (clip ON)", 15);
        drawLabel(6, 20, "Cylinder right edge clips here ->",  7);

        {
            float panX = 0.f, panY = 0.f, zoom = 1.f;
            wxbgi_cam2d_get_pan("cam2d", &panX, &panY);
            wxbgi_cam2d_get_zoom("cam2d", &zoom);
            char buf[80];
            std::snprintf(buf, sizeof(buf), "pan=(%.0f,%.0f)  zoom=%.2f", panX, panY, zoom);
            drawLabel(panelW + 6, 6,  "cam2d: 2D pan/zoom  WASD/+/-/Q/E", 15);
            drawLabel(panelW + 6, 20, buf, 7);
        }
        {
            char buf[64];
            std::snprintf(buf, sizeof(buf),
                          "az=%.0f  el=%.0f  (arrow keys)", azimuth, elevation);
            drawLabel(2*panelW + 6, 6,  "cam3d: 3D perspective orbit", 15);
            drawLabel(2*panelW + 6, 20, buf, 7);
        }

        {
            const int selCount = wxbgi_selection_count();
            char selBuf[64];
            if (selCount > 0)
                std::snprintf(selBuf, sizeof(selBuf), "Selected: %d  (Del=delete, Ctrl+click=multi)", selCount);
            else
                std::snprintf(selBuf, sizeof(selBuf), "Click DDS object to select");
            drawLabel(panelW + 6, 34, selBuf, selCount > 0 ? 14 : 8);
        }

        wxbgi_swap_window_buffers();

        if (testMode)
            break;

        redrawRequested = false;
    }
    while (!testMode && !wxbgi_should_close());

    closegraph();
    return 0;
}
