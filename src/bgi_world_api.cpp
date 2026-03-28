/**
 * @file bgi_world_api.cpp
 * @brief Phase 3 — world-coordinate and UCS-local drawing wrappers.
 *
 * All functions:
 *  1. Acquire gMutex.
 *  2. Resolve the active camera (and UCS for wxbgi_ucs_* variants).
 *  3. Project world/UCS coordinates to screen pixels via cameraWorldToScreen.
 *  4. Convert screen pixels to viewport-relative coordinates (subtract
 *     gState.viewport.left/top).  The internal draw functions re-add the
 *     viewport offset, so the net result is correct device-pixel output.
 *     As a side effect the current BGI viewport (setviewport) clips world
 *     drawing, exactly as it clips classic BGI primitives.
 *  5. Call the relevant bgi::draw*Internal / bgi::drawText function.
 *  6. Flush to screen.
 */

#include "wx_bgi_3d.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <string>
#include <vector>

#include "bgi_camera.h"
#include "bgi_draw.h"
#include "bgi_font.h"
#include "bgi_state.h"
#include "bgi_ucs.h"

// =============================================================================
// Internal helpers (anonymous namespace — not part of the public API)
// =============================================================================

namespace
{

    // -------------------------------------------------------------------------
    // Name resolution (mirrors the pattern from bgi_camera_api.cpp /
    // bgi_ucs_api.cpp — each TU has its own static helpers to avoid link
    // conflicts).
    // -------------------------------------------------------------------------

    const std::string &resolveActiveCam()
    {
        return bgi::gState.activeCamera;
    }

    const std::string resolveCamName(const char *name)
    {
        return (name == nullptr || name[0] == '\0') ? bgi::gState.activeCamera
                                                    : std::string(name);
    }

    const std::string resolveUcsName(const char *name)
    {
        return (name == nullptr || name[0] == '\0') ? bgi::gState.activeUcs
                                                    : std::string(name);
    }

    const bgi::Camera3D *findCam(const std::string &key)
    {
        auto it = bgi::gState.cameras.find(key);
        return (it != bgi::gState.cameras.end()) ? &it->second : nullptr;
    }

    const bgi::CoordSystem *findUcs(const std::string &key)
    {
        auto it = bgi::gState.ucsSystems.find(key);
        return (it != bgi::gState.ucsSystems.end()) ? &it->second : nullptr;
    }

    // -------------------------------------------------------------------------
    // Project a world point to viewport-relative pixel coordinates.
    //
    // Returns true if the point is within the camera frustum.
    // vpX / vpY are viewport-relative (suitable for drawLineInternal etc.).
    // -------------------------------------------------------------------------

    bool projectToVpRel(const bgi::Camera3D &cam,
                        float wx, float wy, float wz,
                        int &vpX, int &vpY)
    {
        float sx = 0.0f;
        float sy = 0.0f;
        if (!bgi::cameraWorldToScreen(cam, bgi::gState.width, bgi::gState.height,
                                      wx, wy, wz, sx, sy))
        {
            return false;
        }
        // Convert device pixel -> viewport-relative (the internal draw functions
        // will re-add the viewport offset, so the net device pixel is preserved).
        vpX = static_cast<int>(sx) - bgi::gState.viewport.left;
        vpY = static_cast<int>(sy) - bgi::gState.viewport.top;
        return true;
    }

    // Screen-space distance between two device-pixel projections.
    float screenDist(float sx1, float sy1, float sx2, float sy2)
    {
        const float dx = sx2 - sx1;
        const float dy = sy2 - sy1;
        return std::sqrt(dx * dx + dy * dy);
    }

} // anonymous namespace

// =============================================================================
// wxbgi_ucs_to_screen
// =============================================================================

BGI_API int BGI_CALL wxbgi_ucs_to_screen(const char *ucsName, const char *camName,
                                          float ux, float uy, float uz,
                                          float *screenX, float *screenY)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);

    const auto *ucs = findUcs(resolveUcsName(ucsName));
    if (ucs == nullptr)
        return -1;

    const auto *cam = findCam(resolveCamName(camName));
    if (cam == nullptr)
        return -1;

    // UCS-local -> world
    float wx = 0.0f, wy = 0.0f, wz = 0.0f;
    bgi::ucsLocalToWorld(*ucs, ux, uy, uz, wx, wy, wz);

    // World -> screen
    float sx = 0.0f, sy = 0.0f;
    const bool visible = bgi::cameraWorldToScreen(*cam, bgi::gState.width,
                                                  bgi::gState.height,
                                                  wx, wy, wz, sx, sy);
    if (screenX != nullptr)
        *screenX = sx;
    if (screenY != nullptr)
        *screenY = sy;

    return visible ? 1 : 0;
}

// =============================================================================
// World-space drawing — helpers shared by the world_* functions
// =============================================================================

// A small RAII struct that holds the lock + resolved active camera.
// Used to avoid repeating the same lock / lookup / flush pattern.
// Note: we define these as free functions below rather than a struct so
// that the lock_guard stays scoped to the full function body.

// =============================================================================
// wxbgi_world_point
// =============================================================================

BGI_API void BGI_CALL wxbgi_world_point(float x, float y, float z, int color)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);

    const auto *cam = findCam(resolveActiveCam());
    if (cam == nullptr)
        return;

    int vpX = 0, vpY = 0;
    if (!projectToVpRel(*cam, x, y, z, vpX, vpY))
        return;

    bgi::setPixelWithMode(vpX, vpY, bgi::normalizeColor(color), bgi::gState.writeMode);
    bgi::flushToScreen();
}

// =============================================================================
// wxbgi_world_line
// =============================================================================

BGI_API void BGI_CALL wxbgi_world_line(float x1, float y1, float z1,
                                        float x2, float y2, float z2)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);

    const auto *cam = findCam(resolveActiveCam());
    if (cam == nullptr)
        return;

    int vx1 = 0, vy1 = 0, vx2 = 0, vy2 = 0;
    const bool v1 = projectToVpRel(*cam, x1, y1, z1, vx1, vy1);
    const bool v2 = projectToVpRel(*cam, x2, y2, z2, vx2, vy2);
    if (!v1 && !v2)
        return;

    bgi::drawLineInternal(vx1, vy1, vx2, vy2, bgi::gState.currentColor);
    bgi::flushToScreen();
}

// =============================================================================
// wxbgi_world_circle
// =============================================================================

BGI_API void BGI_CALL wxbgi_world_circle(float cx, float cy, float cz, float radius)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);

    const auto *cam = findCam(resolveActiveCam());
    if (cam == nullptr)
        return;

    // Project centre
    float sx0 = 0.0f, sy0 = 0.0f;
    if (!bgi::cameraWorldToScreen(*cam, bgi::gState.width, bgi::gState.height,
                                  cx, cy, cz, sx0, sy0))
        return;

    // Project a point offset by radius along world-X and world-Y; pick the
    // larger projected distance so circles in tilted planes look reasonable.
    float sx1 = 0.0f, sy1 = 0.0f, sx2 = 0.0f, sy2 = 0.0f;
    const bool hOk = bgi::cameraWorldToScreen(*cam, bgi::gState.width, bgi::gState.height,
                                              cx + radius, cy, cz, sx1, sy1);
    const bool vOk = bgi::cameraWorldToScreen(*cam, bgi::gState.width, bgi::gState.height,
                                              cx, cy + radius, cz, sx2, sy2);

    int screenRadius = 1;
    if (hOk)
        screenRadius = std::max(screenRadius,
                                static_cast<int>(std::round(screenDist(sx0, sy0, sx1, sy1))));
    if (vOk)
        screenRadius = std::max(screenRadius,
                                static_cast<int>(std::round(screenDist(sx0, sy0, sx2, sy2))));

    const int vpX = static_cast<int>(sx0) - bgi::gState.viewport.left;
    const int vpY = static_cast<int>(sy0) - bgi::gState.viewport.top;

    bgi::drawCircleInternal(vpX, vpY, screenRadius, bgi::gState.currentColor);
    bgi::flushToScreen();
}

// =============================================================================
// wxbgi_world_ellipse
// =============================================================================

BGI_API void BGI_CALL wxbgi_world_ellipse(float cx, float cy, float cz,
                                           float rx, float ry,
                                           int startAngle, int endAngle)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);

    const auto *cam = findCam(resolveActiveCam());
    if (cam == nullptr)
        return;

    float sx0 = 0.0f, sy0 = 0.0f;
    if (!bgi::cameraWorldToScreen(*cam, bgi::gState.width, bgi::gState.height,
                                  cx, cy, cz, sx0, sy0))
        return;

    float sx1 = 0.0f, sy1 = 0.0f, sx2 = 0.0f, sy2 = 0.0f;
    bgi::cameraWorldToScreen(*cam, bgi::gState.width, bgi::gState.height,
                             cx + rx, cy, cz, sx1, sy1);
    bgi::cameraWorldToScreen(*cam, bgi::gState.width, bgi::gState.height,
                             cx, cy + ry, cz, sx2, sy2);

    const int screenRx = std::max(1, static_cast<int>(std::round(screenDist(sx0, sy0, sx1, sy1))));
    const int screenRy = std::max(1, static_cast<int>(std::round(screenDist(sx0, sy0, sx2, sy2))));

    const int vpX = static_cast<int>(sx0) - bgi::gState.viewport.left;
    const int vpY = static_cast<int>(sy0) - bgi::gState.viewport.top;

    bgi::drawEllipseInternal(vpX, vpY, startAngle, endAngle,
                             screenRx, screenRy, bgi::gState.currentColor);
    bgi::flushToScreen();
}

// =============================================================================
// wxbgi_world_rectangle
// =============================================================================

BGI_API void BGI_CALL wxbgi_world_rectangle(float x1, float y1, float z1,
                                             float x2, float y2, float z2)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);

    const auto *cam = findCam(resolveActiveCam());
    if (cam == nullptr)
        return;

    float sx1 = 0.0f, sy1 = 0.0f, sx2 = 0.0f, sy2 = 0.0f;
    const bool v1 = bgi::cameraWorldToScreen(*cam, bgi::gState.width, bgi::gState.height,
                                             x1, y1, z1, sx1, sy1);
    const bool v2 = bgi::cameraWorldToScreen(*cam, bgi::gState.width, bgi::gState.height,
                                             x2, y2, z2, sx2, sy2);
    if (!v1 && !v2)
        return;

    // Screen-space bounding box of the two projected corners
    const int left   = std::min(static_cast<int>(sx1), static_cast<int>(sx2));
    const int top    = std::min(static_cast<int>(sy1), static_cast<int>(sy2));
    const int right  = std::max(static_cast<int>(sx1), static_cast<int>(sx2));
    const int bottom = std::max(static_cast<int>(sy1), static_cast<int>(sy2));

    const int vl = left   - bgi::gState.viewport.left;
    const int vt = top    - bgi::gState.viewport.top;
    const int vr = right  - bgi::gState.viewport.left;
    const int vb = bottom - bgi::gState.viewport.top;

    const int color = bgi::gState.currentColor;
    bgi::drawLineInternal(vl, vt, vr, vt, color); // top edge
    bgi::drawLineInternal(vr, vt, vr, vb, color); // right edge
    bgi::drawLineInternal(vr, vb, vl, vb, color); // bottom edge
    bgi::drawLineInternal(vl, vb, vl, vt, color); // left edge

    bgi::flushToScreen();
}

// =============================================================================
// wxbgi_world_polyline
// =============================================================================

BGI_API void BGI_CALL wxbgi_world_polyline(const float *xyzTriplets, int pointCount)
{
    if (xyzTriplets == nullptr || pointCount < 2)
        return;

    std::lock_guard<std::mutex> lock(bgi::gMutex);

    const auto *cam = findCam(resolveActiveCam());
    if (cam == nullptr)
        return;

    const int color = bgi::gState.currentColor;
    bool prevVisible = false;
    int pvX = 0, pvY = 0;

    for (int i = 0; i < pointCount; ++i)
    {
        const float wx = xyzTriplets[i * 3 + 0];
        const float wy = xyzTriplets[i * 3 + 1];
        const float wz = xyzTriplets[i * 3 + 2];

        int cvX = 0, cvY = 0;
        const bool curVisible = projectToVpRel(*cam, wx, wy, wz, cvX, cvY);

        if (i > 0 && (prevVisible || curVisible))
        {
            bgi::drawLineInternal(pvX, pvY, cvX, cvY, color);
        }

        pvX = cvX;
        pvY = cvY;
        prevVisible = curVisible;
    }

    bgi::flushToScreen();
}

// =============================================================================
// wxbgi_world_fillpoly
// =============================================================================

BGI_API void BGI_CALL wxbgi_world_fillpoly(const float *xyzTriplets, int pointCount)
{
    if (xyzTriplets == nullptr || pointCount < 3)
        return;

    std::lock_guard<std::mutex> lock(bgi::gMutex);

    const auto *cam = findCam(resolveActiveCam());
    if (cam == nullptr)
        return;

    std::vector<std::pair<int, int>> pts;
    pts.reserve(static_cast<std::size_t>(pointCount));

    for (int i = 0; i < pointCount; ++i)
    {
        const float wx = xyzTriplets[i * 3 + 0];
        const float wy = xyzTriplets[i * 3 + 1];
        const float wz = xyzTriplets[i * 3 + 2];

        int vpX = 0, vpY = 0;
        if (projectToVpRel(*cam, wx, wy, wz, vpX, vpY))
        {
            pts.emplace_back(vpX, vpY);
        }
    }

    if (static_cast<int>(pts.size()) < 3)
        return;

    bgi::fillPolygonInternal(pts, bgi::gState.currentColor);
    bgi::flushToScreen();
}

// =============================================================================
// wxbgi_world_outtextxy
// =============================================================================

BGI_API void BGI_CALL wxbgi_world_outtextxy(float x, float y, float z, const char *text)
{
    if (text == nullptr)
        return;

    std::lock_guard<std::mutex> lock(bgi::gMutex);

    const auto *cam = findCam(resolveActiveCam());
    if (cam == nullptr)
        return;

    int vpX = 0, vpY = 0;
    if (!projectToVpRel(*cam, x, y, z, vpX, vpY))
        return;

    // Apply text justification (mirrors drawTextAnchored in bgi_api.cpp)
    const auto [tw, th] = bgi::measureText(std::string(text));
    switch (bgi::gState.textSettings.horiz)
    {
    case bgi::CENTER_TEXT: vpX -= tw / 2; break;
    case bgi::RIGHT_TEXT:  vpX -= tw;     break;
    default:                              break;
    }
    switch (bgi::gState.textSettings.vert)
    {
    case bgi::CENTER_TEXT: vpY -= th / 2; break;
    case bgi::BOTTOM_TEXT: vpY -= th;     break;
    default:                              break;
    }

    bgi::drawText(vpX, vpY, std::string(text), bgi::gState.currentColor);
    bgi::flushToScreen();
}

// =============================================================================
// UCS-local variants — transform UCS-local -> world, then delegate to world
// draw helpers (re-locking avoided by calling the internal path directly).
// =============================================================================

// Internal helper: world draw wrapped without taking the lock (caller holds it).
namespace
{
    void worldLineNoLock(const bgi::Camera3D &cam,
                         float x1, float y1, float z1,
                         float x2, float y2, float z2)
    {
        int vx1 = 0, vy1 = 0, vx2 = 0, vy2 = 0;
        const bool v1 = projectToVpRel(cam, x1, y1, z1, vx1, vy1);
        const bool v2 = projectToVpRel(cam, x2, y2, z2, vx2, vy2);
        if (v1 || v2)
            bgi::drawLineInternal(vx1, vy1, vx2, vy2, bgi::gState.currentColor);
    }

    void worldCircleNoLock(const bgi::Camera3D &cam,
                           float cx, float cy, float cz, float radius)
    {
        float sx0 = 0.0f, sy0 = 0.0f;
        if (!bgi::cameraWorldToScreen(cam, bgi::gState.width, bgi::gState.height,
                                      cx, cy, cz, sx0, sy0))
            return;

        float sx1 = 0.0f, sy1 = 0.0f, sx2 = 0.0f, sy2 = 0.0f;
        const bool hOk = bgi::cameraWorldToScreen(cam, bgi::gState.width, bgi::gState.height,
                                                  cx + radius, cy, cz, sx1, sy1);
        const bool vOk = bgi::cameraWorldToScreen(cam, bgi::gState.width, bgi::gState.height,
                                                  cx, cy + radius, cz, sx2, sy2);
        int screenRadius = 1;
        if (hOk) screenRadius = std::max(screenRadius, static_cast<int>(std::round(screenDist(sx0, sy0, sx1, sy1))));
        if (vOk) screenRadius = std::max(screenRadius, static_cast<int>(std::round(screenDist(sx0, sy0, sx2, sy2))));

        const int vpX = static_cast<int>(sx0) - bgi::gState.viewport.left;
        const int vpY = static_cast<int>(sy0) - bgi::gState.viewport.top;
        bgi::drawCircleInternal(vpX, vpY, screenRadius, bgi::gState.currentColor);
    }
} // anonymous namespace (second block)

// =============================================================================
// wxbgi_ucs_point
// =============================================================================

BGI_API void BGI_CALL wxbgi_ucs_point(const char *ucsName,
                                       float ux, float uy, float uz, int color)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);

    const auto *ucs = findUcs(resolveUcsName(ucsName));
    if (ucs == nullptr) return;
    const auto *cam = findCam(resolveActiveCam());
    if (cam == nullptr) return;

    float wx = 0.0f, wy = 0.0f, wz = 0.0f;
    bgi::ucsLocalToWorld(*ucs, ux, uy, uz, wx, wy, wz);

    int vpX = 0, vpY = 0;
    if (!projectToVpRel(*cam, wx, wy, wz, vpX, vpY)) return;

    bgi::setPixelWithMode(vpX, vpY, bgi::normalizeColor(color), bgi::gState.writeMode);
    bgi::flushToScreen();
}

// =============================================================================
// wxbgi_ucs_line
// =============================================================================

BGI_API void BGI_CALL wxbgi_ucs_line(const char *ucsName,
                                      float ux1, float uy1, float uz1,
                                      float ux2, float uy2, float uz2)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);

    const auto *ucs = findUcs(resolveUcsName(ucsName));
    if (ucs == nullptr) return;
    const auto *cam = findCam(resolveActiveCam());
    if (cam == nullptr) return;

    float wx1 = 0.0f, wy1 = 0.0f, wz1 = 0.0f;
    float wx2 = 0.0f, wy2 = 0.0f, wz2 = 0.0f;
    bgi::ucsLocalToWorld(*ucs, ux1, uy1, uz1, wx1, wy1, wz1);
    bgi::ucsLocalToWorld(*ucs, ux2, uy2, uz2, wx2, wy2, wz2);

    worldLineNoLock(*cam, wx1, wy1, wz1, wx2, wy2, wz2);
    bgi::flushToScreen();
}

// =============================================================================
// wxbgi_ucs_circle
// =============================================================================

BGI_API void BGI_CALL wxbgi_ucs_circle(const char *ucsName,
                                        float cx, float cy, float cz, float radius)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);

    const auto *ucs = findUcs(resolveUcsName(ucsName));
    if (ucs == nullptr) return;
    const auto *cam = findCam(resolveActiveCam());
    if (cam == nullptr) return;

    float wx = 0.0f, wy = 0.0f, wz = 0.0f;
    bgi::ucsLocalToWorld(*ucs, cx, cy, cz, wx, wy, wz);

    // Scale the radius: use the UCS X-axis to project a radial offset
    // into world space (correct for uniform-scale UCS).
    float ex = 0.0f, ey = 0.0f, ez = 0.0f;
    bgi::ucsLocalToWorld(*ucs, cx + radius, cy, cz, ex, ey, ez);
    const float worldRadius = std::sqrt((ex - wx) * (ex - wx) +
                                        (ey - wy) * (ey - wy) +
                                        (ez - wz) * (ez - wz));

    worldCircleNoLock(*cam, wx, wy, wz, worldRadius);
    bgi::flushToScreen();
}

// =============================================================================
// wxbgi_ucs_polyline
// =============================================================================

BGI_API void BGI_CALL wxbgi_ucs_polyline(const char *ucsName,
                                          const float *xyzTriplets, int pointCount)
{
    if (xyzTriplets == nullptr || pointCount < 2)
        return;

    std::lock_guard<std::mutex> lock(bgi::gMutex);

    const auto *ucs = findUcs(resolveUcsName(ucsName));
    if (ucs == nullptr) return;
    const auto *cam = findCam(resolveActiveCam());
    if (cam == nullptr) return;

    const int color = bgi::gState.currentColor;
    bool prevVisible = false;
    int pvX = 0, pvY = 0;

    for (int i = 0; i < pointCount; ++i)
    {
        const float lx = xyzTriplets[i * 3 + 0];
        const float ly = xyzTriplets[i * 3 + 1];
        const float lz = xyzTriplets[i * 3 + 2];

        float wx = 0.0f, wy = 0.0f, wz = 0.0f;
        bgi::ucsLocalToWorld(*ucs, lx, ly, lz, wx, wy, wz);

        int cvX = 0, cvY = 0;
        const bool curVisible = projectToVpRel(*cam, wx, wy, wz, cvX, cvY);

        if (i > 0 && (prevVisible || curVisible))
        {
            bgi::drawLineInternal(pvX, pvY, cvX, cvY, color);
        }

        pvX = cvX;
        pvY = cvY;
        prevVisible = curVisible;
    }

    bgi::flushToScreen();
}

// =============================================================================
// wxbgi_ucs_outtextxy
// =============================================================================

BGI_API void BGI_CALL wxbgi_ucs_outtextxy(const char *ucsName,
                                           float ux, float uy, float uz,
                                           const char *text)
{
    if (text == nullptr) return;

    std::lock_guard<std::mutex> lock(bgi::gMutex);

    const auto *ucs = findUcs(resolveUcsName(ucsName));
    if (ucs == nullptr) return;
    const auto *cam = findCam(resolveActiveCam());
    if (cam == nullptr) return;

    float wx = 0.0f, wy = 0.0f, wz = 0.0f;
    bgi::ucsLocalToWorld(*ucs, ux, uy, uz, wx, wy, wz);

    int vpX = 0, vpY = 0;
    if (!projectToVpRel(*cam, wx, wy, wz, vpX, vpY)) return;

    const auto [tw, th] = bgi::measureText(std::string(text));
    switch (bgi::gState.textSettings.horiz)
    {
    case bgi::CENTER_TEXT: vpX -= tw / 2; break;
    case bgi::RIGHT_TEXT:  vpX -= tw;     break;
    default:                              break;
    }
    switch (bgi::gState.textSettings.vert)
    {
    case bgi::CENTER_TEXT: vpY -= th / 2; break;
    case bgi::BOTTOM_TEXT: vpY -= th;     break;
    default:                              break;
    }

    bgi::drawText(vpX, vpY, std::string(text), bgi::gState.currentColor);
    bgi::flushToScreen();
}
