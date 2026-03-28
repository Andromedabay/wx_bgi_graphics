/**
 * wxbgi_camera_demo.cpp
 *
 * Demonstrates the Phase 1 camera viewport API on top of classic BGI drawing.
 *
 * The demo shows three panels in a split-screen layout:
 *  - Left:   default pixel-space camera (classic BGI coordinates, unchanged)
 *  - Centre: 2-D world camera with pan and zoom driven by GLFW keyboard input
 *  - Right:  3-D perspective camera orbiting a set of world-space points
 *
 * Controls (keyboard):
 *   W / S       – 2-D pan up / down
 *   A / D       – 2-D pan left / right
 *   + / -       – 2-D zoom in / out
 *   Q / E       – rotate 2-D view
 *   Arrow keys  – orbit the perspective camera (azimuth / elevation)
 *   Escape      – close
 */

#include "wx_bgi.h"
#include "wx_bgi_3d.h"
#include "wx_bgi_ext.h"

#include <GLFW/glfw3.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

// ---------------------------------------------------------------------------
// Geometry: a simple grid of points in world space (Z-up, XY ground plane)
// ---------------------------------------------------------------------------

struct WorldPoint { float x, y, z; int color; };

static const std::array<WorldPoint, 9> kWorldPoints = {{
    {   0.f,   0.f,  0.f, 15 }, // origin – WHITE
    { 100.f,   0.f,  0.f,  4 }, // +X axis end – RED
    {   0.f, 100.f,  0.f,  2 }, // +Y axis end – GREEN
    {   0.f,   0.f,100.f,  1 }, // +Z axis end – BLUE
    {  50.f,  50.f, 20.f, 14 }, // YELLOW
    { -50.f,  50.f,  0.f, 11 }, // LIGHTCYAN
    {  50.f, -50.f,  0.f, 12 }, // LIGHTRED
    { -50.f, -50.f, 30.f, 13 }, // LIGHTMAGENTA
    {   0.f,   0.f, 60.f,  3 }, // CYAN – high point
}};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void drawCross(int screenX, int screenY, int color, int sz = 5)
{
    setcolor(color);
    line(screenX - sz, screenY, screenX + sz, screenY);
    line(screenX, screenY - sz, screenX, screenY + sz);
}

// Draw a label at a screen position (clipped to window bounds).
static void drawLabel(int x, int y, const char *text, int color)
{
    setcolor(color);
    outtextxy(x, y, const_cast<char *>(text));
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    // When invoked with --test, draw one frame then exit (for ctest).
    const bool testMode = (argc >= 2 && std::strcmp(argv[1], "--test") == 0);

    constexpr int kW = 1440, kH = 480;
    initwindow(kW, kH, "Camera Demo", 50, 50, 0, 0);

    if (graphresult() != 0)
        return 1;

    const int panelW = kW / 3; // ~480 px each

    // ------------------------------------------------------------------
    // Set up 2-D overhead camera (centre panel)
    // ------------------------------------------------------------------
    wxbgi_cam2d_create("cam2d");
    wxbgi_cam2d_set_world_height("cam2d", 300.f); // 300 world-units visible
    wxbgi_cam2d_set_pan("cam2d", 0.f, 0.f);
    wxbgi_cam_set_screen_viewport("cam2d", panelW, 0, panelW, kH);

    // ------------------------------------------------------------------
    // Set up 3-D perspective camera (right panel)
    // ------------------------------------------------------------------
    wxbgi_cam_create("cam3d", WXBGI_CAM_PERSPECTIVE);
    wxbgi_cam_set_perspective("cam3d", 60.f, 0.5f, 5000.f);
    wxbgi_cam_set_screen_viewport("cam3d", panelW * 2, 0, panelW, kH);

    float azimuth   = 45.f;  // degrees around Z axis
    float elevation = 30.f;  // degrees above XY plane
    float orbitR    = 250.f; // orbit radius

    auto updateOrbitCamera = [&]()
    {
        const float azR = azimuth   * 3.14159265f / 180.f;
        const float elR = elevation * 3.14159265f / 180.f;
        const float ex  = orbitR * std::cos(elR) * std::cos(azR);
        const float ey  = orbitR * std::cos(elR) * std::sin(azR);
        const float ez  = orbitR * std::sin(elR);
        wxbgi_cam_set_eye("cam3d", ex, ey, ez);
        wxbgi_cam_set_target("cam3d", 0.f, 0.f, 30.f); // look at scene centre
        wxbgi_cam_set_up("cam3d", 0.f, 0.f, 1.f);      // Z-up
    };
    updateOrbitCamera();

    // ------------------------------------------------------------------
    // Main loop  (runs once in --test mode, interactively otherwise)
    // ------------------------------------------------------------------
    bool redrawRequested = true; // draw initial frame once
    do
    {
        if (!testMode)
        {
            wxbgi_poll_events();

            const auto keyDown = [](int key) -> bool
            {
                return wxbgi_is_key_down(key) == 1;
            };

            if (keyDown(GLFW_KEY_ESCAPE))
                break;

            bool cameraChanged = false;

            // 2-D camera pan / zoom / rotate
            if (keyDown(GLFW_KEY_W)) { wxbgi_cam2d_pan_by("cam2d",  0.f,  4.f); cameraChanged = true; }
            if (keyDown(GLFW_KEY_S)) { wxbgi_cam2d_pan_by("cam2d",  0.f, -4.f); cameraChanged = true; }
            if (keyDown(GLFW_KEY_A)) { wxbgi_cam2d_pan_by("cam2d", -4.f,  0.f); cameraChanged = true; }
            if (keyDown(GLFW_KEY_D)) { wxbgi_cam2d_pan_by("cam2d",  4.f,  0.f); cameraChanged = true; }

            // Support both main keyboard and numpad +/- for zoom controls.
            if (keyDown(GLFW_KEY_EQUAL) || keyDown(GLFW_KEY_KP_ADD))
            {
                wxbgi_cam2d_zoom_at("cam2d", 1.02f, 0.f, 0.f);
                cameraChanged = true;
            }
            if (keyDown(GLFW_KEY_MINUS) || keyDown(GLFW_KEY_KP_SUBTRACT))
            {
                wxbgi_cam2d_zoom_at("cam2d", 0.98f, 0.f, 0.f);
                cameraChanged = true;
            }

            if (keyDown(GLFW_KEY_Q))
            {
                wxbgi_cam2d_set_rotation("cam2d",
                    [&]{ float r = 0.f; wxbgi_cam2d_get_rotation("cam2d", &r); return r + 1.f; }());
                cameraChanged = true;
            }
            if (keyDown(GLFW_KEY_E))
            {
                wxbgi_cam2d_set_rotation("cam2d",
                    [&]{ float r = 0.f; wxbgi_cam2d_get_rotation("cam2d", &r); return r - 1.f; }());
                cameraChanged = true;
            }

            // 3-D camera orbit
            bool orbitChanged = false;
            if (keyDown(GLFW_KEY_LEFT))  { azimuth   -= 1.f; orbitChanged = true; }
            if (keyDown(GLFW_KEY_RIGHT)) { azimuth   += 1.f; orbitChanged = true; }
            if (keyDown(GLFW_KEY_UP))    { elevation  = std::min(89.f, elevation + 1.f); orbitChanged = true; }
            if (keyDown(GLFW_KEY_DOWN))  { elevation  = std::max(-89.f, elevation - 1.f); orbitChanged = true; }
            if (orbitChanged)
            {
                updateOrbitCamera();
                cameraChanged = true;
            }

            // Render only when the camera state changes due to keyboard input.
            redrawRequested = redrawRequested || cameraChanged;
            if (!redrawRequested)
            {
                delay(8);
                continue;
            }
        }

        // --- Draw --------------------------------------------------------
        cleardevice();

        // ---- Left panel: classic BGI pixel-space (no camera transform) ----
        // Draw dividing lines
        setcolor(7); // LIGHTGRAY
        line(panelW - 1, 0, panelW - 1, kH - 1);
        line(panelW * 2 - 1, 0, panelW * 2 - 1, kH - 1);

        // Classic BGI drawing in the left panel (viewport-clipped)
        setviewport(0, 0, panelW - 1, kH - 1, 1);
        setcolor(15); circle(panelW / 2, kH / 2, 80);
        setcolor(14); rectangle(50, 50, panelW - 50, kH - 50);
        setcolor(10); line(0, 0, panelW - 1, kH - 1);
        setcolor(11); line(panelW - 1, 0, 0, kH - 1);
        drawLabel(10, 10, "Classic BGI (left panel)", 15);
        drawLabel(10, 25, "No camera transform", 7);
        setviewport(0, 0, kW - 1, kH - 1, 0); // reset to full window

        // ---- Centre panel: 2-D world camera (Phase 3 wxbgi_world_* API) ----
        {
            // Activate the 2-D camera so wxbgi_world_* functions project
            // through it automatically.
            wxbgi_cam_set_active("cam2d");

            // World-space grid drawn with wxbgi_world_line (no manual project)
            for (int gx = -200; gx <= 200; gx += 50)
            {
                setcolor(gx == 0 ? 10 : 8); // GREEN for Y-axis, DARKGRAY otherwise
                wxbgi_world_line(static_cast<float>(gx), -200.f, 0.f,
                                 static_cast<float>(gx),  200.f, 0.f);
            }
            for (int gy = -200; gy <= 200; gy += 50)
            {
                setcolor(gy == 0 ? 4 : 8); // RED for X-axis, DARKGRAY otherwise
                wxbgi_world_line(-200.f, static_cast<float>(gy), 0.f,
                                  200.f, static_cast<float>(gy), 0.f);
            }

            // A circle and rectangle in world units
            setcolor(14); // YELLOW
            wxbgi_world_circle(0.f, 0.f, 0.f, 80.f);

            setcolor(11); // LIGHTCYAN
            wxbgi_world_rectangle(-60.f, -60.f, 0.f, 60.f, 60.f, 0.f);

            // World points drawn with wxbgi_world_point
            for (const auto &wp : kWorldPoints)
                wxbgi_world_point(wp.x, wp.y, 0.f, wp.color);

            // Axis labels using wxbgi_world_outtextxy
            setcolor(4); wxbgi_world_outtextxy( 110.f,    0.f, 0.f, "+X");
            setcolor(2); wxbgi_world_outtextxy(   0.f,  110.f, 0.f, "+Y");

            // Status label (screen-space, classic BGI)
            float panX = 0.f, panY = 0.f, zoom = 1.f;
            wxbgi_cam2d_get_pan("cam2d", &panX, &panY);
            wxbgi_cam2d_get_zoom("cam2d", &zoom);
            char buf[80];
            std::snprintf(buf, sizeof(buf), "pan=(%.0f,%.0f) zoom=%.2f", panX, panY, zoom);
            drawLabel(panelW + 10, 10, "2-D World Camera (WASD/+/-/Q/E)", 15);
            drawLabel(panelW + 10, 25, buf, 7);
            drawLabel(panelW + 10, 40, "Grid/circle/rect via wxbgi_world_*", 7);

            // Restore default camera for the next panel's classic BGI ops
            wxbgi_cam_set_active("default");
        }

        // ---- Right panel: 3-D perspective camera (Phase 3 world draw) -----
        {
            wxbgi_cam_set_active("cam3d");

            // World-space axes via wxbgi_world_line
            setcolor(4);  wxbgi_world_line(0,0,0, 100,0,0);    // X – RED
            setcolor(2);  wxbgi_world_line(0,0,0, 0,100,0);    // Y – GREEN
            setcolor(1);  wxbgi_world_line(0,0,0, 0,0,100);    // Z – BLUE

            // Axis tip labels via wxbgi_world_outtextxy
            setcolor(4);  wxbgi_world_outtextxy(105.f,   0.f,   0.f, "X");
            setcolor(2);  wxbgi_world_outtextxy(  0.f, 105.f,   0.f, "Y");
            setcolor(1);  wxbgi_world_outtextxy(  0.f,   0.f, 105.f, "Z");

            // A circle in the XY plane and one in the XZ plane
            setcolor(14); wxbgi_world_circle(0.f, 0.f,  0.f, 60.f); // XY ground
            setcolor(11); wxbgi_world_circle(0.f, 0.f, 30.f, 40.f); // elevated

            // World points with coloured crosses (manual projection still works)
            for (const auto &wp : kWorldPoints)
            {
                float sx, sy;
                if (wxbgi_cam_world_to_screen("cam3d", wp.x, wp.y, wp.z, &sx, &sy) == 1)
                    drawCross(static_cast<int>(sx), static_cast<int>(sy), wp.color, 8);
            }

            // Label
            char buf[80];
            std::snprintf(buf, sizeof(buf),
                          "az=%.0f el=%.0f (Arrow keys)", azimuth, elevation);
            drawLabel(panelW * 2 + 10, 10, "3-D Perspective Camera", 15);
            drawLabel(panelW * 2 + 10, 25, buf, 7);
            drawLabel(panelW * 2 + 10, 40, "Axes/circles via wxbgi_world_*", 7);

            wxbgi_cam_set_active("default");
        }

        // Present
        wxbgi_swap_window_buffers();

        if (testMode)
            break;

        redrawRequested = false;
    }
    while (!testMode && !wxbgi_should_close());

    closegraph();
    return 0;
}
