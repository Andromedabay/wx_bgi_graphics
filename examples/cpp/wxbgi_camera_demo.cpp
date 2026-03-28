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

int main()
{
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
    // Main loop
    // ------------------------------------------------------------------
    while (!wxbgi_should_close())
    {
        wxbgi_poll_events();

        // --- Input -------------------------------------------------------
        if (glfwGetKey(static_cast<GLFWwindow *>(nullptr), GLFW_KEY_ESCAPE) ==
            GLFW_PRESS)
            break;

        // 2-D camera pan / zoom / rotate
        if (wxbgi_is_key_down(GLFW_KEY_W)) wxbgi_cam2d_pan_by("cam2d",  0.f,  4.f);
        if (wxbgi_is_key_down(GLFW_KEY_S)) wxbgi_cam2d_pan_by("cam2d",  0.f, -4.f);
        if (wxbgi_is_key_down(GLFW_KEY_A)) wxbgi_cam2d_pan_by("cam2d", -4.f,  0.f);
        if (wxbgi_is_key_down(GLFW_KEY_D)) wxbgi_cam2d_pan_by("cam2d",  4.f,  0.f);
        if (wxbgi_is_key_down(GLFW_KEY_EQUAL))     wxbgi_cam2d_zoom_at("cam2d", 1.02f, 0.f, 0.f);
        if (wxbgi_is_key_down(GLFW_KEY_MINUS))     wxbgi_cam2d_zoom_at("cam2d", 0.98f, 0.f, 0.f);
        if (wxbgi_is_key_down(GLFW_KEY_Q))         wxbgi_cam2d_set_rotation("cam2d",
            [&]{ float r = 0.f; wxbgi_cam2d_get_rotation("cam2d", &r); return r + 1.f; }());
        if (wxbgi_is_key_down(GLFW_KEY_E))         wxbgi_cam2d_set_rotation("cam2d",
            [&]{ float r = 0.f; wxbgi_cam2d_get_rotation("cam2d", &r); return r - 1.f; }());

        // 3-D camera orbit
        bool orbitChanged = false;
        if (wxbgi_is_key_down(GLFW_KEY_LEFT))  { azimuth   -= 1.f; orbitChanged = true; }
        if (wxbgi_is_key_down(GLFW_KEY_RIGHT)) { azimuth   += 1.f; orbitChanged = true; }
        if (wxbgi_is_key_down(GLFW_KEY_UP))    { elevation  = std::min(89.f, elevation + 1.f); orbitChanged = true; }
        if (wxbgi_is_key_down(GLFW_KEY_DOWN))  { elevation  = std::max(-89.f, elevation - 1.f); orbitChanged = true; }
        if (orbitChanged) updateOrbitCamera();

        // --- Draw --------------------------------------------------------
        cleardevice();

        // ---- Left panel: classic BGI pixel-space (no camera transform) ----
        // Draw dividing lines
        setcolor(7); // LIGHTGRAY
        line(panelW, 0, panelW, kH - 1);
        line(panelW * 2, 0, panelW * 2, kH - 1);

        // Classic BGI drawing in the left panel (viewport-clipped)
        setviewport(0, 0, panelW - 1, kH - 1, 1);
        setcolor(15); circle(panelW / 2, kH / 2, 80);
        setcolor(14); rectangle(50, 50, panelW - 50, kH - 50);
        setcolor(10); line(0, 0, panelW - 1, kH - 1);
        setcolor(11); line(panelW - 1, 0, 0, kH - 1);
        drawLabel(10, 10, "Classic BGI (left panel)", 15);
        drawLabel(10, 25, "No camera transform", 7);
        setviewport(0, 0, kW - 1, kH - 1, 0); // reset to full window

        // ---- Centre panel: 2-D world camera ------------------------------
        {
            // Draw a grid in world space through the 2-D camera
            const char *camName = "cam2d";
            for (int gx = -200; gx <= 200; gx += 50)
            {
                float sx1, sy1, sx2, sy2;
                if (wxbgi_cam_world_to_screen(camName, static_cast<float>(gx), -200.f, 0.f, &sx1, &sy1) &&
                    wxbgi_cam_world_to_screen(camName, static_cast<float>(gx),  200.f, 0.f, &sx2, &sy2))
                {
                    setcolor(gx == 0 ? 10 : 8); // GREEN for Y axis, DARKGRAY otherwise
                    line(static_cast<int>(sx1), static_cast<int>(sy1),
                         static_cast<int>(sx2), static_cast<int>(sy2));
                }
            }
            for (int gy = -200; gy <= 200; gy += 50)
            {
                float sx1, sy1, sx2, sy2;
                if (wxbgi_cam_world_to_screen(camName, -200.f, static_cast<float>(gy), 0.f, &sx1, &sy1) &&
                    wxbgi_cam_world_to_screen(camName,  200.f, static_cast<float>(gy), 0.f, &sx2, &sy2))
                {
                    setcolor(gy == 0 ? 4 : 8); // RED for X axis, DARKGRAY otherwise
                    line(static_cast<int>(sx1), static_cast<int>(sy1),
                         static_cast<int>(sx2), static_cast<int>(sy2));
                }
            }

            // Draw world points
            for (const auto &wp : kWorldPoints)
            {
                float sx, sy;
                if (wxbgi_cam_world_to_screen(camName, wp.x, wp.y, 0.f, &sx, &sy) == 1)
                    drawCross(static_cast<int>(sx), static_cast<int>(sy), wp.color, 6);
            }

            // Label
            {
                float panX = 0.f, panY = 0.f, zoom = 1.f;
                wxbgi_cam2d_get_pan(camName, &panX, &panY);
                wxbgi_cam2d_get_zoom(camName, &zoom);
                char buf[80];
                std::snprintf(buf, sizeof(buf), "pan=(%.0f,%.0f) zoom=%.2f", panX, panY, zoom);
                drawLabel(panelW + 10, 10, "2-D Camera (WASD/+/-/Q/E)", 15);
                drawLabel(panelW + 10, 25, buf, 7);
            }
        }

        // ---- Right panel: 3-D perspective camera -------------------------
        {
            const char *camName = "cam3d";

            // Draw world-space axes projected to screen
            struct { float wx, wy, wz; float tx, ty, tz; int col; } axes[] = {
                {0,0,0,  100,0,0,    4},  // X – RED
                {0,0,0,  0,100,0,    2},  // Y – GREEN
                {0,0,0,  0,0,100,    1},  // Z – BLUE
            };
            for (const auto &ax : axes)
            {
                float sx1, sy1, sx2, sy2;
                if (wxbgi_cam_world_to_screen(camName, ax.wx, ax.wy, ax.wz, &sx1, &sy1) == 1 &&
                    wxbgi_cam_world_to_screen(camName, ax.tx, ax.ty, ax.tz, &sx2, &sy2) == 1)
                {
                    setcolor(ax.col);
                    line(static_cast<int>(sx1), static_cast<int>(sy1),
                         static_cast<int>(sx2), static_cast<int>(sy2));
                }
            }

            // Draw world points with depth-correct perspective
            for (const auto &wp : kWorldPoints)
            {
                float sx, sy;
                if (wxbgi_cam_world_to_screen(camName, wp.x, wp.y, wp.z, &sx, &sy) == 1)
                    drawCross(static_cast<int>(sx), static_cast<int>(sy), wp.color, 8);
            }

            // Label
            char buf[80];
            std::snprintf(buf, sizeof(buf),
                          "az=%.0f el=%.0f (Arrow keys)", azimuth, elevation);
            drawLabel(panelW * 2 + 10, 10, "3-D Perspective Camera", 15);
            drawLabel(panelW * 2 + 10, 25, buf, 7);
        }

        // Present
        wxbgi_swap_window_buffers();
    }

    closegraph();
    return 0;
}
