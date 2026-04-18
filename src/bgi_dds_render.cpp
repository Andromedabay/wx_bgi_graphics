/**
 * @file bgi_dds_render.cpp
 * @brief Phase D — DDS retained-mode renderer.
 *
 * wxbgi_render_dds(camName) traverses the DDS scene graph in insertion order
 * and reprojects every visible drawing primitive through the named camera into
 * the active BGI pixel buffer.
 *
 * Coordinate spaces handled:
 *  BgiPixel  — pixel integers recorded as (x, y, 0) world coords.  Through
 *               the default pixel-space ortho camera they reproduce the
 *               original immediate render exactly; through any other camera
 *               they reproject correctly as 2D world points.
 *  World3D   — world-space float coords from wxbgi_world_* functions.
 *  UcsLocal  — UCS-local float coords; the stored ucsName identifies the
 *               frame.  Transformed to world via ucsLocalToWorld before
 *               projection.
 *
 * Per-object style (DdsStyle) is applied to gState before each draw call and
 * the original draw state is restored after all objects have been rendered.
 */

#include "wx_bgi_dds.h"

// NOTE: glew.h must be included before any OpenGL/GLFW header.
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "bgi_camera.h"
#include "bgi_dds.h"
#include "bgi_draw.h"
#include "bgi_font.h"
#include "bgi_gl.h"
#include "bgi_image.h"
#include "bgi_overlay.h"
#include "bgi_solid_render.h"
#include "bgi_state.h"
#include "bgi_ucs.h"
#include <manifold/manifold.h>

// =============================================================================
// Internal helpers  (anonymous namespace — not part of the public API)
// =============================================================================

namespace
{

// ---------------------------------------------------------------------------
// Style save / apply / restore
// ---------------------------------------------------------------------------

struct StyleSnapshot
{
    int                   currentColor;
    int                   bkColor;
    int                   fillPattern;
    int                   fillColor;
    bgi::linesettingstype lineSettings;
    bgi::textsettingstype textSettings;
    int                   writeMode;
};

StyleSnapshot saveStyle()
{
    return {bgi::gState.currentColor,
            bgi::gState.bkColor,
            bgi::gState.fillPattern,
            bgi::gState.fillColor,
            bgi::gState.lineSettings,
            bgi::gState.textSettings,
            bgi::gState.writeMode};
}

void applyStyle(const bgi::DdsStyle &s)
{
    bgi::gState.currentColor = s.color;
    bgi::gState.bkColor      = s.bkColor;
    bgi::gState.fillPattern  = s.fillStyle.pattern;
    bgi::gState.fillColor    = s.fillStyle.color;
    bgi::gState.lineSettings = s.lineStyle;
    bgi::gState.textSettings = s.textStyle;
    bgi::gState.writeMode    = s.writeMode;
}

void restoreStyle(const StyleSnapshot &snap)
{
    bgi::gState.currentColor = snap.currentColor;
    bgi::gState.bkColor      = snap.bkColor;
    bgi::gState.fillPattern  = snap.fillPattern;
    bgi::gState.fillColor    = snap.fillColor;
    bgi::gState.lineSettings = snap.lineSettings;
    bgi::gState.textSettings = snap.textSettings;
    bgi::gState.writeMode    = snap.writeMode;
}

// ---------------------------------------------------------------------------
// Coordinate resolution and projection helpers
// ---------------------------------------------------------------------------

/**
 * Resolve a local point to world space according to CoordSpace.
 * BgiPixel and World3D are passed through; UcsLocal is transformed via
 * ucsLocalToWorld using the named UCS.
 */
bool resolveWorld(bgi::CoordSpace cs, const std::string &ucsName,
                  float lx, float ly, float lz,
                  float &wx, float &wy, float &wz)
{
    if (cs == bgi::CoordSpace::UcsLocal)
    {
        auto it = bgi::gState.ucsSystems.find(ucsName);
        if (it == bgi::gState.ucsSystems.end())
            return false;
        bgi::ucsLocalToWorld(it->second->ucs, lx, ly, lz, wx, wy, wz);
    }
    else
    {
        wx = lx;
        wy = ly;
        wz = lz;
    }
    return true;
}

/**
 * Project a world point through the camera and return viewport-relative pixel
 * coordinates (same convention as bgi_world_api.cpp — viewport.left/top are
 * subtracted so internal draw functions add them back correctly).
 *
 * Returns false if the point is outside the view frustum (NDC boundary test).
 * Use projectToVpRelForced when partial edge clipping is needed.
 */
bool projectToVpRel(const bgi::Camera3D &cam,
                    float wx, float wy, float wz,
                    int &vpX, int &vpY)
{
    float sx = 0.0f, sy = 0.0f;
    if (!bgi::cameraWorldToScreen(cam, bgi::gState.width, bgi::gState.height,
                                  wx, wy, wz, sx, sy))
        return false;
    vpX = static_cast<int>(sx) - bgi::gState.viewport.left;
    vpY = static_cast<int>(sy) - bgi::gState.viewport.top;
    return true;
}

// ---------------------------------------------------------------------------
// Sutherland-Hodgman pipeline helpers
// ---------------------------------------------------------------------------

/** Convert a world point to 4-D clip space via the camera's VP matrix. */
glm::vec4 worldToClip4(const bgi::Camera3D &cam,
                        float wx, float wy, float wz)
{
    return bgi::cameraWorldToClip(cam,
                                  bgi::gState.width, bgi::gState.height,
                                  wx, wy, wz);
}

/**
 * Project a clip-space point to viewport-relative pixel coords.
 * No NDC X/Y test — relies on per-pixel viewport clipping.
 * Returns false only when w <= 0.
 */
bool clipToVpRel(const bgi::Camera3D &cam, const glm::vec4 &clip,
                 int &vpX, int &vpY)
{
    float sx = 0.f, sy = 0.f;
    if (!bgi::cameraClipToScreen(cam,
                                 bgi::gState.width, bgi::gState.height,
                                 clip, sx, sy))
        return false;
    vpX = static_cast<int>(sx) - bgi::gState.viewport.left;
    vpY = static_cast<int>(sy) - bgi::gState.viewport.top;
    return true;
}

/**
 * Helper: clip a line edge A→B against near+far, then project both endpoints
 * to viewport-relative pixels.  Returns false if the entire segment is clipped.
 */
bool clipAndProjectLine(const bgi::Camera3D &cam,
                        glm::vec4 A, glm::vec4 B,
                        int &ax, int &ay, int &bx, int &by)
{
    if (!bgi::clipLineZPlanes(A, B)) return false;
    if (!clipToVpRel(cam, A, ax, ay)) return false;
    if (!clipToVpRel(cam, B, bx, by)) return false;
    return true;
}

float screenDist(float sx1, float sy1, float sx2, float sy2)
{
    const float dx = sx2 - sx1, dy = sy2 - sy1;
    return std::sqrt(dx * dx + dy * dy);
}

/**
 * Compute the screen-space pixel radius of a sphere centred at world point
 * (wcx, wcy, wcz) with world-space radius worldRadius.  Uses the same
 * offset-probe technique as bgi_world_api.cpp.
 */
int computeScreenRadius(const bgi::Camera3D &cam,
                        float wcx, float wcy, float wcz,
                        float worldRadius)
{
    float sx0 = 0.f, sy0 = 0.f;
    bgi::cameraWorldToScreen(cam, bgi::gState.width, bgi::gState.height,
                             wcx, wcy, wcz, sx0, sy0);

    float sx1 = 0.f, sy1 = 0.f, sx2 = 0.f, sy2 = 0.f;
    const bool hOk = bgi::cameraWorldToScreen(cam, bgi::gState.width, bgi::gState.height,
                                              wcx + worldRadius, wcy, wcz, sx1, sy1);
    const bool vOk = bgi::cameraWorldToScreen(cam, bgi::gState.width, bgi::gState.height,
                                              wcx, wcy + worldRadius, wcz, sx2, sy2);
    int r = 1;
    if (hOk) r = std::max(r, static_cast<int>(std::round(screenDist(sx0, sy0, sx1, sy1))));
    if (vOk) r = std::max(r, static_cast<int>(std::round(screenDist(sx0, sy0, sx2, sy2))));
    return r;
}

/**
 * Compute separate screen-space radii for ellipse X and Y semi-axes.
 */
void computeScreenEllipseRadii(const bgi::Camera3D &cam,
                                float wcx, float wcy, float wcz,
                                float worldRx, float worldRy,
                                int &screenRx, int &screenRy)
{
    float sx0 = 0.f, sy0 = 0.f;
    bgi::cameraWorldToScreen(cam, bgi::gState.width, bgi::gState.height,
                             wcx, wcy, wcz, sx0, sy0);

    float sx1 = 0.f, sy1 = 0.f, sx2 = 0.f, sy2 = 0.f;
    bgi::cameraWorldToScreen(cam, bgi::gState.width, bgi::gState.height,
                             wcx + worldRx, wcy, wcz, sx1, sy1);
    bgi::cameraWorldToScreen(cam, bgi::gState.width, bgi::gState.height,
                             wcx, wcy + worldRy, wcz, sx2, sy2);

    screenRx = std::max(1, static_cast<int>(std::round(screenDist(sx0, sy0, sx1, sy1))));
    screenRy = std::max(1, static_cast<int>(std::round(screenDist(sx0, sy0, sx2, sy2))));
}

/**
 * Resolve world-space X/Y ellipse radii, handling UCS scaling.
 * Returns false if the UCS cannot be found.
 */
bool resolveEllipseWorldRadii(bgi::CoordSpace cs, const std::string &ucsName,
                               float cx, float cy, float cz,
                               float localRx, float localRy,
                               float wcx, float wcy, float wcz,
                               float &worldRx, float &worldRy)
{
    if (cs == bgi::CoordSpace::UcsLocal)
    {
        auto it = bgi::gState.ucsSystems.find(ucsName);
        if (it == bgi::gState.ucsSystems.end())
            return false;
        const auto &ucs = it->second->ucs;
        float ex = 0.f, ey = 0.f, ez = 0.f;
        bgi::ucsLocalToWorld(ucs, cx + localRx, cy, cz, ex, ey, ez);
        worldRx = std::sqrt((ex - wcx) * (ex - wcx) +
                            (ey - wcy) * (ey - wcy) +
                            (ez - wcz) * (ez - wcz));
        bgi::ucsLocalToWorld(ucs, cx, cy + localRy, cz, ex, ey, ez);
        worldRy = std::sqrt((ex - wcx) * (ex - wcx) +
                            (ey - wcy) * (ey - wcy) +
                            (ez - wcz) * (ez - wcz));
    }
    else
    {
        worldRx = localRx;
        worldRy = localRy;
    }
    return true;
}

/**
 * Resolve world-space radius for a circular primitive (arc / circle / pieslice).
 * Handles UCS scaling via the UCS X-axis probe.
 */
float resolveCircleWorldRadius(bgi::CoordSpace cs, const std::string &ucsName,
                                float cx, float cy, float cz,
                                float localRadius,
                                float wcx, float wcy, float wcz)
{
    if (cs == bgi::CoordSpace::UcsLocal)
    {
        auto it = bgi::gState.ucsSystems.find(ucsName);
        if (it == bgi::gState.ucsSystems.end())
            return localRadius;
        float ex = 0.f, ey = 0.f, ez = 0.f;
        bgi::ucsLocalToWorld(it->second->ucs, cx + localRadius, cy, cz, ex, ey, ez);
        return std::sqrt((ex - wcx) * (ex - wcx) +
                         (ey - wcy) * (ey - wcy) +
                         (ez - wcz) * (ez - wcz));
    }
    return localRadius;
}

// ---------------------------------------------------------------------------
// Per-type render functions
// ---------------------------------------------------------------------------

void renderPoint(const bgi::Camera3D &cam, const bgi::DdsPoint &obj)
{
    float wx = 0.f, wy = 0.f, wz = 0.f;
    if (!resolveWorld(obj.coordSpace, obj.ucsName,
                      obj.pos.x, obj.pos.y, obj.pos.z, wx, wy, wz))
        return;
    int vpX = 0, vpY = 0;
    if (!projectToVpRel(cam, wx, wy, wz, vpX, vpY))
        return;
    // Use the stored per-pixel color (may differ from style.color for putpixel).
    bgi::setPixelWithMode(vpX, vpY, obj.color, bgi::gState.writeMode);
}

void renderLine(const bgi::Camera3D &cam, const bgi::DdsLine &obj)
{
    float wx1 = 0.f, wy1 = 0.f, wz1 = 0.f;
    float wx2 = 0.f, wy2 = 0.f, wz2 = 0.f;
    if (!resolveWorld(obj.coordSpace, obj.ucsName,
                      obj.p1.x, obj.p1.y, obj.p1.z, wx1, wy1, wz1))
        return;
    if (!resolveWorld(obj.coordSpace, obj.ucsName,
                      obj.p2.x, obj.p2.y, obj.p2.z, wx2, wy2, wz2))
        return;

    glm::vec4 A = worldToClip4(cam, wx1, wy1, wz1);
    glm::vec4 B = worldToClip4(cam, wx2, wy2, wz2);
    int vx1 = 0, vy1 = 0, vx2 = 0, vy2 = 0;
    if (clipAndProjectLine(cam, A, B, vx1, vy1, vx2, vy2))
        bgi::drawLineInternal(vx1, vy1, vx2, vy2, bgi::gState.currentColor);
}

void renderCircle(const bgi::Camera3D &cam, const bgi::DdsCircle &obj)
{
    float wx = 0.f, wy = 0.f, wz = 0.f;
    if (!resolveWorld(obj.coordSpace, obj.ucsName,
                      obj.centre.x, obj.centre.y, obj.centre.z, wx, wy, wz))
        return;
    float sx0 = 0.f, sy0 = 0.f;
    if (!bgi::cameraWorldToScreen(cam, bgi::gState.width, bgi::gState.height,
                                  wx, wy, wz, sx0, sy0))
        return;
    const float worldRadius = resolveCircleWorldRadius(
        obj.coordSpace, obj.ucsName, obj.centre.x, obj.centre.y, obj.centre.z,
        obj.radius, wx, wy, wz);
    const int screenRadius = computeScreenRadius(cam, wx, wy, wz, worldRadius);
    const int vpX = static_cast<int>(sx0) - bgi::gState.viewport.left;
    const int vpY = static_cast<int>(sy0) - bgi::gState.viewport.top;
    bgi::drawCircleInternal(vpX, vpY, screenRadius, bgi::gState.currentColor);
}

void renderArc(const bgi::Camera3D &cam, const bgi::DdsArc &obj)
{
    float wx = 0.f, wy = 0.f, wz = 0.f;
    if (!resolveWorld(obj.coordSpace, obj.ucsName,
                      obj.centre.x, obj.centre.y, obj.centre.z, wx, wy, wz))
        return;
    float sx0 = 0.f, sy0 = 0.f;
    if (!bgi::cameraWorldToScreen(cam, bgi::gState.width, bgi::gState.height,
                                  wx, wy, wz, sx0, sy0))
        return;
    const float worldRadius = resolveCircleWorldRadius(
        obj.coordSpace, obj.ucsName, obj.centre.x, obj.centre.y, obj.centre.z,
        obj.radius, wx, wy, wz);
    const int screenRadius = computeScreenRadius(cam, wx, wy, wz, worldRadius);
    const int vpX = static_cast<int>(sx0) - bgi::gState.viewport.left;
    const int vpY = static_cast<int>(sy0) - bgi::gState.viewport.top;
    bgi::drawEllipseInternal(vpX, vpY,
                             static_cast<int>(obj.startAngle),
                             static_cast<int>(obj.endAngle),
                             screenRadius, screenRadius,
                             bgi::gState.currentColor);
}

void renderEllipse(const bgi::Camera3D &cam, const bgi::DdsEllipse &obj)
{
    float wx = 0.f, wy = 0.f, wz = 0.f;
    if (!resolveWorld(obj.coordSpace, obj.ucsName,
                      obj.centre.x, obj.centre.y, obj.centre.z, wx, wy, wz))
        return;
    float sx0 = 0.f, sy0 = 0.f;
    if (!bgi::cameraWorldToScreen(cam, bgi::gState.width, bgi::gState.height,
                                  wx, wy, wz, sx0, sy0))
        return;
    float worldRx = 0.f, worldRy = 0.f;
    if (!resolveEllipseWorldRadii(obj.coordSpace, obj.ucsName,
                                  obj.centre.x, obj.centre.y, obj.centre.z,
                                  obj.rx, obj.ry, wx, wy, wz, worldRx, worldRy))
        return;
    int screenRx = 0, screenRy = 0;
    computeScreenEllipseRadii(cam, wx, wy, wz, worldRx, worldRy, screenRx, screenRy);
    const int vpX = static_cast<int>(sx0) - bgi::gState.viewport.left;
    const int vpY = static_cast<int>(sy0) - bgi::gState.viewport.top;
    bgi::drawEllipseInternal(vpX, vpY,
                             static_cast<int>(obj.startAngle),
                             static_cast<int>(obj.endAngle),
                             screenRx, screenRy,
                             bgi::gState.currentColor);
}

void renderFillEllipse(const bgi::Camera3D &cam, const bgi::DdsFillEllipse &obj)
{
    float wx = 0.f, wy = 0.f, wz = 0.f;
    if (!resolveWorld(obj.coordSpace, obj.ucsName,
                      obj.centre.x, obj.centre.y, obj.centre.z, wx, wy, wz))
        return;
    float sx0 = 0.f, sy0 = 0.f;
    if (!bgi::cameraWorldToScreen(cam, bgi::gState.width, bgi::gState.height,
                                  wx, wy, wz, sx0, sy0))
        return;
    float worldRx = 0.f, worldRy = 0.f;
    if (!resolveEllipseWorldRadii(obj.coordSpace, obj.ucsName,
                                  obj.centre.x, obj.centre.y, obj.centre.z,
                                  obj.rx, obj.ry, wx, wy, wz, worldRx, worldRy))
        return;
    int screenRx = 0, screenRy = 0;
    computeScreenEllipseRadii(cam, wx, wy, wz, worldRx, worldRy, screenRx, screenRy);
    const int vpX = static_cast<int>(sx0) - bgi::gState.viewport.left;
    const int vpY = static_cast<int>(sy0) - bgi::gState.viewport.top;
    bgi::fillEllipseInternal(vpX, vpY, screenRx, screenRy, bgi::gState.fillColor);
    bgi::drawEllipseInternal(vpX, vpY, 0, 360, screenRx, screenRy, bgi::gState.currentColor);
}

void renderPieSlice(const bgi::Camera3D &cam, const bgi::DdsPieSlice &obj)
{
    float wx = 0.f, wy = 0.f, wz = 0.f;
    if (!resolveWorld(obj.coordSpace, obj.ucsName,
                      obj.centre.x, obj.centre.y, obj.centre.z, wx, wy, wz))
        return;
    float sx0 = 0.f, sy0 = 0.f;
    if (!bgi::cameraWorldToScreen(cam, bgi::gState.width, bgi::gState.height,
                                  wx, wy, wz, sx0, sy0))
        return;
    const float worldRadius = resolveCircleWorldRadius(
        obj.coordSpace, obj.ucsName, obj.centre.x, obj.centre.y, obj.centre.z,
        obj.radius, wx, wy, wz);
    const int screenRadius = computeScreenRadius(cam, wx, wy, wz, worldRadius);
    const int vpX = static_cast<int>(sx0) - bgi::gState.viewport.left;
    const int vpY = static_cast<int>(sy0) - bgi::gState.viewport.top;
    const int sa  = static_cast<int>(obj.startAngle);
    const int ea  = static_cast<int>(obj.endAngle);

    auto arcPts = bgi::buildArcPoints(vpX, vpY, sa, ea, screenRadius, screenRadius);
    if (arcPts.empty())
        return;
    std::vector<std::pair<int, int>> polygon;
    polygon.emplace_back(vpX, vpY);
    polygon.insert(polygon.end(), arcPts.begin(), arcPts.end());
    polygon.emplace_back(vpX, vpY);
    bgi::fillPolygonInternal(polygon, bgi::gState.fillColor);
    bgi::drawLineInternal(vpX, vpY, arcPts.front().first, arcPts.front().second,
                          bgi::gState.currentColor);
    bgi::drawLineInternal(vpX, vpY, arcPts.back().first, arcPts.back().second,
                          bgi::gState.currentColor);
    bgi::drawEllipseInternal(vpX, vpY, sa, ea, screenRadius, screenRadius,
                             bgi::gState.currentColor);
}

void renderSector(const bgi::Camera3D &cam, const bgi::DdsSector &obj)
{
    float wx = 0.f, wy = 0.f, wz = 0.f;
    if (!resolveWorld(obj.coordSpace, obj.ucsName,
                      obj.centre.x, obj.centre.y, obj.centre.z, wx, wy, wz))
        return;
    float sx0 = 0.f, sy0 = 0.f;
    if (!bgi::cameraWorldToScreen(cam, bgi::gState.width, bgi::gState.height,
                                  wx, wy, wz, sx0, sy0))
        return;
    float worldRx = 0.f, worldRy = 0.f;
    if (!resolveEllipseWorldRadii(obj.coordSpace, obj.ucsName,
                                  obj.centre.x, obj.centre.y, obj.centre.z,
                                  obj.rx, obj.ry, wx, wy, wz, worldRx, worldRy))
        return;
    int screenRx = 0, screenRy = 0;
    computeScreenEllipseRadii(cam, wx, wy, wz, worldRx, worldRy, screenRx, screenRy);
    const int vpX = static_cast<int>(sx0) - bgi::gState.viewport.left;
    const int vpY = static_cast<int>(sy0) - bgi::gState.viewport.top;
    const int sa  = static_cast<int>(obj.startAngle);
    const int ea  = static_cast<int>(obj.endAngle);

    auto arcPts = bgi::buildArcPoints(vpX, vpY, sa, ea, screenRx, screenRy);
    if (arcPts.empty())
        return;
    std::vector<std::pair<int, int>> polygon;
    polygon.emplace_back(vpX, vpY);
    polygon.insert(polygon.end(), arcPts.begin(), arcPts.end());
    polygon.emplace_back(vpX, vpY);
    bgi::fillPolygonInternal(polygon, bgi::gState.fillColor);
    bgi::drawLineInternal(vpX, vpY, arcPts.front().first, arcPts.front().second,
                          bgi::gState.currentColor);
    bgi::drawLineInternal(vpX, vpY, arcPts.back().first, arcPts.back().second,
                          bgi::gState.currentColor);
    bgi::drawEllipseInternal(vpX, vpY, sa, ea, screenRx, screenRy,
                             bgi::gState.currentColor);
}

void renderRectangle(const bgi::Camera3D &cam, const bgi::DdsRectangle &obj)
{
    float wx1 = 0.f, wy1 = 0.f, wz1 = 0.f;
    float wx2 = 0.f, wy2 = 0.f, wz2 = 0.f;
    if (!resolveWorld(obj.coordSpace, obj.ucsName,
                      obj.p1.x, obj.p1.y, obj.p1.z, wx1, wy1, wz1))
        return;
    if (!resolveWorld(obj.coordSpace, obj.ucsName,
                      obj.p2.x, obj.p2.y, obj.p2.z, wx2, wy2, wz2))
        return;

    // Build the four corners in clip space.
    const glm::vec4 TL = worldToClip4(cam, wx1, wy1, wz1);
    const glm::vec4 TR = worldToClip4(cam, wx2, wy1, wz1);
    const glm::vec4 BR = worldToClip4(cam, wx2, wy2, wz2);
    const glm::vec4 BL = worldToClip4(cam, wx1, wy2, wz2);

    const int c = bgi::gState.currentColor;
    int ax, ay, bx, by;
    if (clipAndProjectLine(cam, TL, TR, ax, ay, bx, by)) bgi::drawLineInternal(ax, ay, bx, by, c);
    if (clipAndProjectLine(cam, TR, BR, ax, ay, bx, by)) bgi::drawLineInternal(ax, ay, bx, by, c);
    if (clipAndProjectLine(cam, BR, BL, ax, ay, bx, by)) bgi::drawLineInternal(ax, ay, bx, by, c);
    if (clipAndProjectLine(cam, BL, TL, ax, ay, bx, by)) bgi::drawLineInternal(ax, ay, bx, by, c);
}

void renderBar(const bgi::Camera3D &cam, const bgi::DdsBar &obj)
{
    float wx1 = 0.f, wy1 = 0.f, wz1 = 0.f;
    float wx2 = 0.f, wy2 = 0.f, wz2 = 0.f;
    if (!resolveWorld(obj.coordSpace, obj.ucsName,
                      obj.p1.x, obj.p1.y, obj.p1.z, wx1, wy1, wz1))
        return;
    if (!resolveWorld(obj.coordSpace, obj.ucsName,
                      obj.p2.x, obj.p2.y, obj.p2.z, wx2, wy2, wz2))
        return;
    // Use cameraWorldToScreenForced for fill-rect corners (filled bar doesn't
    // need SH triangle decomposition; per-pixel clipping handles boundaries).
    int vx1 = 0, vy1 = 0, vx2 = 0, vy2 = 0;
    float sx = 0.f, sy = 0.f;
    bgi::cameraWorldToScreenForced(cam, bgi::gState.width, bgi::gState.height,
                                   wx1, wy1, wz1, sx, sy);
    vx1 = static_cast<int>(sx) - bgi::gState.viewport.left;
    vy1 = static_cast<int>(sy) - bgi::gState.viewport.top;
    bgi::cameraWorldToScreenForced(cam, bgi::gState.width, bgi::gState.height,
                                   wx2, wy2, wz2, sx, sy);
    vx2 = static_cast<int>(sx) - bgi::gState.viewport.left;
    vy2 = static_cast<int>(sy) - bgi::gState.viewport.top;

    const int l = std::min(vx1, vx2), r = std::max(vx1, vx2);
    const int t = std::min(vy1, vy2), b = std::max(vy1, vy2);
    bgi::fillRectInternal(l, t, r, b, bgi::gState.fillColor);
}

void renderBar3D(const bgi::Camera3D &cam, const bgi::DdsBar3D &obj)
{
    const float left   = obj.p1.x, top    = obj.p1.y, lz = obj.p1.z;
    const float right  = obj.p2.x, bottom = obj.p2.y;
    const float d      = obj.depth;

    float wA[3], wB[3], wC[3], wD[3], wE[3], wF[3], wG[3];
    auto rw = [&](float lx, float ly, float *out) {
        resolveWorld(obj.coordSpace, obj.ucsName, lx, ly, lz, out[0], out[1], out[2]);
    };
    rw(left,    top,    wA);
    rw(right,   top,    wB);
    rw(right,   bottom, wC);
    rw(left,    bottom, wD);
    rw(right+d, top-d,  wE);
    rw(right+d, bottom-d, wF);
    rw(left+d,  top-d,  wG);

    // Front face fill (projected bounding rect of front corners using forced proj)
    {
        int vA[2], vD[2], vB[2], vC[2];
        float sx = 0.f, sy = 0.f;
        auto fp = [&](const float *w, int *v) {
            bgi::cameraWorldToScreenForced(cam, bgi::gState.width, bgi::gState.height,
                                           w[0], w[1], w[2], sx, sy);
            v[0] = (int)sx - bgi::gState.viewport.left;
            v[1] = (int)sy - bgi::gState.viewport.top;
        };
        fp(wA,vA); fp(wB,vB); fp(wC,vC); fp(wD,vD);
        const int fl = std::min(vA[0], vD[0]);
        const int ft = std::min(vA[1], vB[1]);
        const int fr = std::max(vB[0], vC[0]);
        const int fb = std::max(vC[1], vD[1]);
        bgi::fillRectInternal(fl, ft, fr, fb, bgi::gState.fillColor);
    }

    // All outline edges clipped with SH
    const int c = bgi::gState.currentColor;
    auto edge = [&](const float *p, const float *q) {
        glm::vec4 A = worldToClip4(cam, p[0], p[1], p[2]);
        glm::vec4 B = worldToClip4(cam, q[0], q[1], q[2]);
        int ax, ay, bx, by;
        if (clipAndProjectLine(cam, A, B, ax, ay, bx, by))
            bgi::drawLineInternal(ax, ay, bx, by, c);
    };
    edge(wA, wB); edge(wB, wC); edge(wC, wD); edge(wD, wA);
    edge(wB, wE); edge(wC, wF); edge(wE, wF);
    if (obj.topFlag) { edge(wA, wG); edge(wG, wE); }
}

void renderPolygon(const bgi::Camera3D &cam, const bgi::DdsPolygon &obj)
{
    if (obj.pts.size() < 2)
        return;

    // Collect clip-space vertices
    std::vector<glm::vec4> clip4;
    clip4.reserve(obj.pts.size());
    for (const auto &p : obj.pts)
    {
        float wx = 0.f, wy = 0.f, wz = 0.f;
        if (!resolveWorld(obj.coordSpace, obj.ucsName, p.x, p.y, p.z, wx, wy, wz))
            return;
        clip4.push_back(worldToClip4(cam, wx, wy, wz));
    }

    // Sutherland-Hodgman: clip against near + far Z planes
    bgi::clipPolyZPlanes(clip4);
    if (clip4.size() < 2)
        return;

    // Project to viewport-relative pixels
    std::vector<std::pair<int, int>> projected;
    projected.reserve(clip4.size());
    for (const auto &c4 : clip4)
    {
        int vpX = 0, vpY = 0;
        if (clipToVpRel(cam, c4, vpX, vpY))
            projected.emplace_back(vpX, vpY);
    }
    if (projected.size() < 2)
        return;
    bgi::drawPolygonInternal(projected, bgi::gState.currentColor);
}

void renderFillPoly(const bgi::Camera3D &cam, const bgi::DdsFillPoly &obj)
{
    if (obj.pts.size() < 3)
        return;

    // Collect clip-space vertices
    std::vector<glm::vec4> clip4;
    clip4.reserve(obj.pts.size());
    for (const auto &p : obj.pts)
    {
        float wx = 0.f, wy = 0.f, wz = 0.f;
        if (!resolveWorld(obj.coordSpace, obj.ucsName, p.x, p.y, p.z, wx, wy, wz))
            return;
        clip4.push_back(worldToClip4(cam, wx, wy, wz));
    }

    // Sutherland-Hodgman: clip against near + far Z planes
    bgi::clipPolyZPlanes(clip4);
    if (clip4.size() < 3)
        return;

    // Project to viewport-relative pixels (no NDC X/Y test; per-pixel clipping
    // inside fillPolygonInternal handles viewport boundaries)
    std::vector<std::pair<int, int>> projected;
    projected.reserve(clip4.size());
    for (const auto &c4 : clip4)
    {
        int vpX = 0, vpY = 0;
        if (clipToVpRel(cam, c4, vpX, vpY))
            projected.emplace_back(vpX, vpY);
    }
    if (projected.size() < 3)
        return;
    bgi::fillPolygonInternal(projected, bgi::gState.fillColor);
    bgi::drawPolygonInternal(projected, bgi::gState.currentColor);
}

void renderText(const bgi::Camera3D &cam, const bgi::DdsText &obj)
{
    if (obj.text.empty())
        return;
    float wx = 0.f, wy = 0.f, wz = 0.f;
    if (!resolveWorld(obj.coordSpace, obj.ucsName,
                      obj.pos.x, obj.pos.y, obj.pos.z, wx, wy, wz))
        return;
    int vpX = 0, vpY = 0;
    if (!projectToVpRel(cam, wx, wy, wz, vpX, vpY))
        return;

    // Apply text justification (mirrors drawTextAnchored in bgi_api.cpp)
    const auto [tw, th] = bgi::measureText(obj.text);
    switch (bgi::gState.textSettings.horiz)
    {
    case bgi::CENTER_TEXT: vpX -= tw / 2; break;
    case bgi::RIGHT_TEXT:  vpX -= tw;     break;
    default: break;
    }
    switch (bgi::gState.textSettings.vert)
    {
    case bgi::CENTER_TEXT: vpY -= th / 2; break;
    case bgi::BOTTOM_TEXT: vpY -= th;     break;
    default: break;
    }
    bgi::drawText(vpX, vpY, obj.text, bgi::gState.currentColor);
}

void renderImage(const bgi::Camera3D &cam, const bgi::DdsImage &obj)
{
    if (obj.pixels.empty() || obj.width <= 0 || obj.height <= 0)
        return;

    // Project the stored top-left position to viewport-relative coords.
    int vpX = 0, vpY = 0;
    if (!projectToVpRel(cam, obj.pos.x, obj.pos.y, obj.pos.z, vpX, vpY))
        return;

    // Reconstruct a BGI bitmap (ImageHeader + palette-indexed pixel bytes)
    // in a temporary buffer and let drawImage handle device-pixel placement.
    const std::size_t headerBytes = sizeof(bgi::ImageHeader);
    const std::size_t pixelCount  = static_cast<std::size_t>(obj.width) *
                                    static_cast<std::size_t>(obj.height);
    std::vector<uint8_t> bitmap(headerBytes + pixelCount);
    auto *hdr   = reinterpret_cast<bgi::ImageHeader *>(bitmap.data());
    hdr->width  = static_cast<uint32_t>(obj.width);
    hdr->height = static_cast<uint32_t>(obj.height);
    std::memcpy(bitmap.data() + headerBytes, obj.pixels.data(), pixelCount);

    // drawImage expects viewport-relative coordinates (same as putimage user coords).
    bgi::drawImage(vpX, vpY, bitmap.data(), bgi::gState.writeMode);
}

// ---------------------------------------------------------------------------
// Main per-object dispatch
// ---------------------------------------------------------------------------

glm::vec3 translationFromMatrix(const glm::mat4 &m)
{
    return {m[3][0], m[3][1], m[3][2]};
}

bool isLeafObject(bgi::DdsObjectType t)
{
    return t != bgi::DdsObjectType::Camera &&
           t != bgi::DdsObjectType::Ucs &&
           t != bgi::DdsObjectType::WorldExtentsObj &&
           t != bgi::DdsObjectType::Transform &&
           t != bgi::DdsObjectType::SetUnion &&
           t != bgi::DdsObjectType::SetIntersection &&
           t != bgi::DdsObjectType::SetDifference;
}

bool isSolid3DType(bgi::DdsObjectType t)
{
    switch (t)
    {
    case bgi::DdsObjectType::Box:
    case bgi::DdsObjectType::Sphere:
    case bgi::DdsObjectType::Cylinder:
    case bgi::DdsObjectType::Cone:
    case bgi::DdsObjectType::Torus:
    case bgi::DdsObjectType::HeightMap:
    case bgi::DdsObjectType::ParamSurface:
    case bgi::DdsObjectType::Extrusion:
        return true;
    default:
        return false;
    }
}

const std::vector<std::string> *childIds(const bgi::DdsObject &obj)
{
    switch (obj.type)
    {
    case bgi::DdsObjectType::Transform:
        return &static_cast<const bgi::DdsTransform &>(obj).children;
    case bgi::DdsObjectType::SetUnion:
        return &static_cast<const bgi::DdsSetUnion &>(obj).operands;
    case bgi::DdsObjectType::SetIntersection:
        return &static_cast<const bgi::DdsSetIntersection &>(obj).operands;
    case bgi::DdsObjectType::SetDifference:
        return &static_cast<const bgi::DdsSetDifference &>(obj).operands;
    default:
        return nullptr;
    }
}

void translatePoints(std::vector<glm::vec3> &pts, const glm::vec3 &delta)
{
    for (auto &p : pts)
        p += delta;
}

void renderObject(const bgi::Camera3D &cam,
                  const bgi::DdsObject &obj,
                  int colorOverride = -1,
                  const bgi::DdsStyle *styleOverride = nullptr)
{
    // Temporarily apply the baked draw state for this object.
    applyStyle(styleOverride ? *styleOverride : obj.style);
    if (colorOverride >= 0)
    {
        bgi::gState.currentColor = colorOverride;
        bgi::gState.fillColor    = colorOverride;
    }

    switch (obj.type)
    {
    case bgi::DdsObjectType::Point:
        renderPoint(cam, static_cast<const bgi::DdsPoint &>(obj));       break;
    case bgi::DdsObjectType::Line:
        renderLine(cam, static_cast<const bgi::DdsLine &>(obj));         break;
    case bgi::DdsObjectType::Circle:
        renderCircle(cam, static_cast<const bgi::DdsCircle &>(obj));     break;
    case bgi::DdsObjectType::Arc:
        renderArc(cam, static_cast<const bgi::DdsArc &>(obj));           break;
    case bgi::DdsObjectType::Ellipse:
        renderEllipse(cam, static_cast<const bgi::DdsEllipse &>(obj));   break;
    case bgi::DdsObjectType::FillEllipse:
        renderFillEllipse(cam, static_cast<const bgi::DdsFillEllipse &>(obj)); break;
    case bgi::DdsObjectType::PieSlice:
        renderPieSlice(cam, static_cast<const bgi::DdsPieSlice &>(obj)); break;
    case bgi::DdsObjectType::Sector:
        renderSector(cam, static_cast<const bgi::DdsSector &>(obj));     break;
    case bgi::DdsObjectType::Rectangle:
        renderRectangle(cam, static_cast<const bgi::DdsRectangle &>(obj)); break;
    case bgi::DdsObjectType::Bar:
        renderBar(cam, static_cast<const bgi::DdsBar &>(obj));           break;
    case bgi::DdsObjectType::Bar3D:
        renderBar3D(cam, static_cast<const bgi::DdsBar3D &>(obj));       break;
    case bgi::DdsObjectType::Polygon:
        renderPolygon(cam, static_cast<const bgi::DdsPolygon &>(obj));   break;
    case bgi::DdsObjectType::FillPoly:
        renderFillPoly(cam, static_cast<const bgi::DdsFillPoly &>(obj)); break;
    case bgi::DdsObjectType::Text:
        renderText(cam, static_cast<const bgi::DdsText &>(obj));         break;
    case bgi::DdsObjectType::Image:
        renderImage(cam, static_cast<const bgi::DdsImage &>(obj));       break;
    case bgi::DdsObjectType::Box:
    case bgi::DdsObjectType::Sphere:
    case bgi::DdsObjectType::Cylinder:
    case bgi::DdsObjectType::Cone:
    case bgi::DdsObjectType::Torus:
    case bgi::DdsObjectType::HeightMap:
    case bgi::DdsObjectType::ParamSurface:
    case bgi::DdsObjectType::Extrusion:
        bgi::gState.solidColorOverride = colorOverride;
        bgi::renderSolid3D(cam, obj);
        bgi::gState.solidColorOverride = -1;
        break;
    default: break;
    }
}

void renderTranslatedLeaf(const bgi::Camera3D &cam,
                          const bgi::DdsObject &obj,
                          const glm::vec3 &delta,
                          int colorOverride,
                          const bgi::DdsStyle *styleOverride)
{
    if (delta == glm::vec3(0.f))
    {
        renderObject(cam, obj, colorOverride, styleOverride);
        return;
    }

    switch (obj.type)
    {
    case bgi::DdsObjectType::Point: {
        auto copy = static_cast<const bgi::DdsPoint &>(obj);
        copy.pos += delta;
        renderObject(cam, copy, colorOverride, styleOverride);
        break;
    }
    case bgi::DdsObjectType::Line: {
        auto copy = static_cast<const bgi::DdsLine &>(obj);
        copy.p1 += delta;
        copy.p2 += delta;
        renderObject(cam, copy, colorOverride, styleOverride);
        break;
    }
    case bgi::DdsObjectType::Circle: {
        auto copy = static_cast<const bgi::DdsCircle &>(obj);
        copy.centre += delta;
        renderObject(cam, copy, colorOverride, styleOverride);
        break;
    }
    case bgi::DdsObjectType::Arc: {
        auto copy = static_cast<const bgi::DdsArc &>(obj);
        copy.centre += delta;
        renderObject(cam, copy, colorOverride, styleOverride);
        break;
    }
    case bgi::DdsObjectType::Ellipse: {
        auto copy = static_cast<const bgi::DdsEllipse &>(obj);
        copy.centre += delta;
        renderObject(cam, copy, colorOverride, styleOverride);
        break;
    }
    case bgi::DdsObjectType::FillEllipse: {
        auto copy = static_cast<const bgi::DdsFillEllipse &>(obj);
        copy.centre += delta;
        renderObject(cam, copy, colorOverride, styleOverride);
        break;
    }
    case bgi::DdsObjectType::PieSlice: {
        auto copy = static_cast<const bgi::DdsPieSlice &>(obj);
        copy.centre += delta;
        renderObject(cam, copy, colorOverride, styleOverride);
        break;
    }
    case bgi::DdsObjectType::Sector: {
        auto copy = static_cast<const bgi::DdsSector &>(obj);
        copy.centre += delta;
        renderObject(cam, copy, colorOverride, styleOverride);
        break;
    }
    case bgi::DdsObjectType::Rectangle: {
        auto copy = static_cast<const bgi::DdsRectangle &>(obj);
        copy.p1 += delta;
        copy.p2 += delta;
        renderObject(cam, copy, colorOverride, styleOverride);
        break;
    }
    case bgi::DdsObjectType::Bar: {
        auto copy = static_cast<const bgi::DdsBar &>(obj);
        copy.p1 += delta;
        copy.p2 += delta;
        renderObject(cam, copy, colorOverride, styleOverride);
        break;
    }
    case bgi::DdsObjectType::Bar3D: {
        auto copy = static_cast<const bgi::DdsBar3D &>(obj);
        copy.p1 += delta;
        copy.p2 += delta;
        renderObject(cam, copy, colorOverride, styleOverride);
        break;
    }
    case bgi::DdsObjectType::Polygon: {
        auto copy = static_cast<const bgi::DdsPolygon &>(obj);
        translatePoints(copy.pts, delta);
        renderObject(cam, copy, colorOverride, styleOverride);
        break;
    }
    case bgi::DdsObjectType::FillPoly: {
        auto copy = static_cast<const bgi::DdsFillPoly &>(obj);
        translatePoints(copy.pts, delta);
        renderObject(cam, copy, colorOverride, styleOverride);
        break;
    }
    case bgi::DdsObjectType::Text: {
        auto copy = static_cast<const bgi::DdsText &>(obj);
        copy.pos += delta;
        renderObject(cam, copy, colorOverride, styleOverride);
        break;
    }
    case bgi::DdsObjectType::Image: {
        auto copy = static_cast<const bgi::DdsImage &>(obj);
        copy.pos += delta;
        renderObject(cam, copy, colorOverride, styleOverride);
        break;
    }
    case bgi::DdsObjectType::Box: {
        auto copy = static_cast<const bgi::DdsBox &>(obj);
        copy.origin += delta;
        renderObject(cam, copy, colorOverride, styleOverride);
        break;
    }
    case bgi::DdsObjectType::Sphere: {
        auto copy = static_cast<const bgi::DdsSphere &>(obj);
        copy.origin += delta;
        renderObject(cam, copy, colorOverride, styleOverride);
        break;
    }
    case bgi::DdsObjectType::Cylinder: {
        auto copy = static_cast<const bgi::DdsCylinder &>(obj);
        copy.origin += delta;
        renderObject(cam, copy, colorOverride, styleOverride);
        break;
    }
    case bgi::DdsObjectType::Cone: {
        auto copy = static_cast<const bgi::DdsCone &>(obj);
        copy.origin += delta;
        renderObject(cam, copy, colorOverride, styleOverride);
        break;
    }
    case bgi::DdsObjectType::Torus: {
        auto copy = static_cast<const bgi::DdsTorus &>(obj);
        copy.origin += delta;
        renderObject(cam, copy, colorOverride, styleOverride);
        break;
    }
    case bgi::DdsObjectType::HeightMap: {
        auto copy = static_cast<const bgi::DdsHeightMap &>(obj);
        copy.origin += delta;
        renderObject(cam, copy, colorOverride, styleOverride);
        break;
    }
    case bgi::DdsObjectType::ParamSurface: {
        auto copy = static_cast<const bgi::DdsParamSurface &>(obj);
        copy.origin += delta;
        renderObject(cam, copy, colorOverride, styleOverride);
        break;
    }
    case bgi::DdsObjectType::Extrusion: {
        auto copy = static_cast<const bgi::DdsExtrusion &>(obj);
        copy.origin += delta;
        translatePoints(copy.baseProfile, delta);
        renderObject(cam, copy, colorOverride, styleOverride);
        break;
    }
    default:
        renderObject(cam, obj, colorOverride, styleOverride);
        break;
    }
}

void renderMaskOntoActive(const std::vector<std::uint8_t> &mask, std::uint8_t color)
{
    auto &dst = bgi::activePageBuffer();
    const std::size_t n = std::min(dst.size(), mask.size());
    for (std::size_t i = 0; i < n; ++i)
    {
        if (mask[i] != 0)
            dst[i] = color;
    }
}

manifold::MeshGL meshFromTriangles(const std::vector<bgi::SolidTriangle> &tris)
{
    manifold::MeshGL mesh;
    mesh.numProp = 3;
    mesh.vertProperties.reserve(tris.size() * 9);
    mesh.triVerts.reserve(tris.size() * 3);

    std::uint32_t idx = 0;
    for (const auto &tri : tris)
    {
        for (int i = 0; i < 3; ++i)
        {
            mesh.vertProperties.push_back(tri.v[i].x);
            mesh.vertProperties.push_back(tri.v[i].y);
            mesh.vertProperties.push_back(tri.v[i].z);
            mesh.triVerts.push_back(idx++);
        }
    }
    mesh.Merge();
    return mesh;
}

struct ExactSetOpCacheKey
{
    std::string   id;
    std::uint32_t txBits{0};
    std::uint32_t tyBits{0};
    std::uint32_t tzBits{0};

    bool operator==(const ExactSetOpCacheKey &other) const
    {
        return id == other.id &&
               txBits == other.txBits &&
               tyBits == other.tyBits &&
               tzBits == other.tzBits;
    }
};

struct ExactSetOpCacheKeyHash
{
    std::size_t operator()(const ExactSetOpCacheKey &key) const
    {
        std::size_t h = std::hash<std::string>{}(key.id);
        h ^= static_cast<std::size_t>(key.txBits) + 0x9e3779b9u + (h << 6) + (h >> 2);
        h ^= static_cast<std::size_t>(key.tyBits) + 0x9e3779b9u + (h << 6) + (h >> 2);
        h ^= static_cast<std::size_t>(key.tzBits) + 0x9e3779b9u + (h << 6) + (h >> 2);
        return h;
    }
};

struct ExactSetOpCacheEntry
{
    std::vector<glm::vec3> vertices;
    bool                   empty{false};
};

struct ExactSetOpCacheState
{
    const bgi::DdsScene *scene{nullptr};
    std::uint64_t        revision{0};
    std::unordered_map<ExactSetOpCacheKey, ExactSetOpCacheEntry, ExactSetOpCacheKeyHash> entries;
};

std::uint32_t floatBits(float value)
{
    if (value == 0.f)
        value = 0.f;
    return std::bit_cast<std::uint32_t>(value);
}

ExactSetOpCacheKey makeExactSetOpCacheKey(const bgi::DdsObject &obj, const glm::vec3 &translation)
{
    return {obj.id, floatBits(translation.x), floatBits(translation.y), floatBits(translation.z)};
}

ExactSetOpCacheState &exactSetOpCache(const bgi::DdsScene &scene)
{
    static ExactSetOpCacheState cache;
    if (cache.scene != &scene || cache.revision != scene.revision)
    {
        cache.scene = &scene;
        cache.revision = scene.revision;
        cache.entries.clear();
    }
    return cache;
}

std::vector<glm::vec3> cachedVerticesFromMesh(const manifold::MeshGL &mesh)
{
    std::vector<glm::vec3> verts;
    verts.reserve(mesh.triVerts.size());
    for (std::size_t i = 0; i < mesh.triVerts.size(); ++i)
    {
        const auto vi = static_cast<std::size_t>(mesh.triVerts[i]) * mesh.numProp;
        verts.emplace_back(mesh.vertProperties[vi + 0],
                           mesh.vertProperties[vi + 1],
                           mesh.vertProperties[vi + 2]);
    }
    return verts;
}

std::vector<bgi::SolidTriangle> trianglesFromCachedVertices(const std::vector<glm::vec3> &verts,
                                                            int faceColor,
                                                            int edgeColor)
{
    std::vector<bgi::SolidTriangle> tris;
    tris.reserve(verts.size() / 3);
    for (std::size_t i = 0; i + 2 < verts.size(); i += 3)
    {
        bgi::SolidTriangle tri{};
        tri.faceColor = faceColor;
        tri.edgeColor = edgeColor;
        tri.v[0] = verts[i + 0];
        tri.v[1] = verts[i + 1];
        tri.v[2] = verts[i + 2];
        tri.vn[0] = glm::vec3(0.f, 0.f, 1.f);
        tri.vn[1] = glm::vec3(0.f, 0.f, 1.f);
        tri.vn[2] = glm::vec3(0.f, 0.f, 1.f);
        tris.push_back(tri);
    }
    return tris;
}

std::vector<bgi::SolidTriangle> trianglesFromMesh(const manifold::MeshGL &mesh,
                                                  int faceColor,
                                                  int edgeColor)
{
    std::vector<bgi::SolidTriangle> tris;
    tris.reserve(mesh.triVerts.size() / 3);
    for (std::size_t i = 0; i + 2 < mesh.triVerts.size(); i += 3)
    {
        bgi::SolidTriangle tri{};
        tri.faceColor = faceColor;
        tri.edgeColor = edgeColor;
        for (int c = 0; c < 3; ++c)
        {
            const auto vi = static_cast<std::size_t>(mesh.triVerts[i + c]) * mesh.numProp;
            tri.v[c] = glm::vec3(mesh.vertProperties[vi + 0],
                                 mesh.vertProperties[vi + 1],
                                 mesh.vertProperties[vi + 2]);
            tri.vn[c] = glm::vec3(0.f, 0.f, 1.f);
        }
        tris.push_back(tri);
    }
    return tris;
}

bool manifoldFromAnalyticSolid(const bgi::DdsObject &obj,
                               const glm::vec3 &translation,
                               manifold::Manifold &out)
{
    switch (obj.type)
    {
    case bgi::DdsObjectType::Box: {
        const auto &box = static_cast<const bgi::DdsBox &>(obj);
        out = manifold::Manifold::Cube({box.width, box.depth, box.height}, true)
                  .Translate({box.origin.x + translation.x,
                              box.origin.y + translation.y,
                              box.origin.z + translation.z});
        return out.Status() == manifold::Manifold::Error::NoError;
    }
    case bgi::DdsObjectType::Sphere: {
        const auto &sphere = static_cast<const bgi::DdsSphere &>(obj);
        const int segs = std::max(8, std::max(sphere.slices, sphere.stacks * 2));
        out = manifold::Manifold::Sphere(sphere.radius, segs)
                  .Translate({sphere.origin.x + translation.x,
                              sphere.origin.y + translation.y,
                              sphere.origin.z + translation.z});
        return out.Status() == manifold::Manifold::Error::NoError;
    }
    case bgi::DdsObjectType::Cylinder: {
        const auto &cyl = static_cast<const bgi::DdsCylinder &>(obj);
        if (!cyl.caps)
            return false;
        out = manifold::Manifold::Cylinder(cyl.height, cyl.radius, cyl.radius,
                                           std::max(3, cyl.slices), true)
                  .Translate({cyl.origin.x + translation.x,
                              cyl.origin.y + translation.y,
                              cyl.origin.z + translation.z});
        return out.Status() == manifold::Manifold::Error::NoError;
    }
    case bgi::DdsObjectType::Cone: {
        const auto &cone = static_cast<const bgi::DdsCone &>(obj);
        if (!cone.cap)
            return false;
        out = manifold::Manifold::Cylinder(cone.height, cone.radius, 0.0,
                                           std::max(3, cone.slices), false)
                  .Translate({cone.origin.x + translation.x,
                              cone.origin.y + translation.y,
                              cone.origin.z + translation.z});
        return out.Status() == manifold::Manifold::Error::NoError;
    }
    default:
        return false;
    }
}

enum class ExactEvalState
{
    Invalid,
    Empty,
    Solid
};

struct ExactEvalResult
{
    ExactEvalState      state{ExactEvalState::Invalid};
    manifold::Manifold  manifold;
};

ExactEvalResult makeExactEvalResult(manifold::Manifold mf)
{
    if (mf.Status() != manifold::Manifold::Error::NoError)
        return {};
    if (mf.IsEmpty())
        return {ExactEvalState::Empty, manifold::Manifold()};
    return {ExactEvalState::Solid, std::move(mf)};
}

ExactEvalResult applyUnion3D(const std::vector<ExactEvalResult> &children)
{
    struct QueueItem
    {
        manifold::Manifold manifold;
        std::size_t        triCount;
        std::size_t        order;
    };
    struct QueueItemGreater
    {
        bool operator()(const QueueItem &lhs, const QueueItem &rhs) const
        {
            return (lhs.triCount > rhs.triCount) ||
                   (lhs.triCount == rhs.triCount && lhs.order > rhs.order);
        }
    };

    std::priority_queue<QueueItem, std::vector<QueueItem>, QueueItemGreater> q;
    std::size_t order = 0;
    for (const auto &child : children)
    {
        if (child.state == ExactEvalState::Invalid)
            return {};
        if (child.state != ExactEvalState::Solid)
            continue;
        q.push({child.manifold, child.manifold.NumTri(), order++});
    }

    while (q.size() > 1)
    {
        auto lhs = q.top();
        q.pop();
        auto rhs = q.top();
        q.pop();
        auto combined = lhs.manifold + rhs.manifold;
        auto result = makeExactEvalResult(std::move(combined));
        if (result.state == ExactEvalState::Invalid)
            return {};
        if (result.state == ExactEvalState::Solid)
        {
            const auto triCount = result.manifold.NumTri();
            q.push({std::move(result.manifold), triCount, order++});
        }
    }

    if (q.empty())
        return {ExactEvalState::Empty, manifold::Manifold()};
    return makeExactEvalResult(q.top().manifold);
}

ExactEvalResult exactEvalFromNode(const bgi::DdsScene &scene,
                                  const bgi::DdsObject &obj,
                                  const glm::vec3 &translation,
                                  std::unordered_set<std::string> &visiting);

ExactEvalResult applyOperator3D(const bgi::DdsScene &scene,
                                const std::vector<std::string> &children,
                                bgi::DdsObjectType opType,
                                const glm::vec3 &translation,
                                std::unordered_set<std::string> &visiting)
{
    ExactEvalResult current;
    bool foundFirst = false;

    for (const auto &childId : children)
    {
        auto child = scene.findById(childId);
        if (!child)
            return {};

        auto childEval = exactEvalFromNode(scene, *child, translation, visiting);
        if (childEval.state == ExactEvalState::Invalid)
            return {};

        if (!foundFirst)
        {
            current = std::move(childEval);
            foundFirst = true;
            continue;
        }

        if (childEval.state == ExactEvalState::Empty)
        {
            if (opType == bgi::DdsObjectType::SetIntersection)
                current = {ExactEvalState::Empty, manifold::Manifold()};
            continue;
        }

        if (current.state != ExactEvalState::Solid)
            continue;

        manifold::Manifold next;
        switch (opType)
        {
        case bgi::DdsObjectType::SetIntersection:
            next = current.manifold ^ childEval.manifold;
            break;
        case bgi::DdsObjectType::SetDifference:
            next = current.manifold - childEval.manifold;
            break;
        default:
            return {};
        }
        current = makeExactEvalResult(std::move(next));
        if (current.state == ExactEvalState::Invalid)
            return {};
    }

    if (!foundFirst)
        return {ExactEvalState::Empty, manifold::Manifold()};
    return current;
}

ExactEvalResult exactEvalFromNode(const bgi::DdsScene &scene,
                                  const bgi::DdsObject &obj,
                                  const glm::vec3 &translation,
                                  std::unordered_set<std::string> &visiting)
{
    if (!obj.id.empty() && !visiting.insert(obj.id).second)
        return {};

    ExactEvalResult result;
    switch (obj.type)
    {
    case bgi::DdsObjectType::Transform: {
        const auto &tx = static_cast<const bgi::DdsTransform &>(obj);
        const glm::vec3 nextTranslation = translation + translationFromMatrix(tx.matrix);
        std::vector<ExactEvalResult> childResults;
        childResults.reserve(tx.children.size());
        for (const auto &childId : tx.children)
        {
            auto child = scene.findById(childId);
            if (!child)
            {
                result = {};
                goto finish_exact_eval;
            }
            childResults.push_back(exactEvalFromNode(scene, *child, nextTranslation, visiting));
            if (childResults.back().state == ExactEvalState::Invalid)
            {
                result = {};
                goto finish_exact_eval;
            }
        }
        result = applyUnion3D(childResults);
        break;
    }
    case bgi::DdsObjectType::SetUnion: {
        const auto *children = childIds(obj);
        if (!children)
        {
            result = {ExactEvalState::Empty, manifold::Manifold()};
            break;
        }
        std::vector<ExactEvalResult> childResults;
        childResults.reserve(children->size());
        for (const auto &childId : *children)
        {
            auto child = scene.findById(childId);
            if (!child)
            {
                result = {};
                goto finish_exact_eval;
            }
            childResults.push_back(exactEvalFromNode(scene, *child, translation, visiting));
            if (childResults.back().state == ExactEvalState::Invalid)
            {
                result = {};
                goto finish_exact_eval;
            }
        }
        result = applyUnion3D(childResults);
        break;
    }
    case bgi::DdsObjectType::SetIntersection:
    case bgi::DdsObjectType::SetDifference: {
        const auto *children = childIds(obj);
        if (!children)
        {
            result = {ExactEvalState::Empty, manifold::Manifold()};
            break;
        }
        result = applyOperator3D(scene, *children, obj.type, translation, visiting);
        break;
    }
    default:
        if (isSolid3DType(obj.type))
        {
            manifold::Manifold mf;
            if (manifoldFromAnalyticSolid(obj, translation, mf))
            {
                result = makeExactEvalResult(std::move(mf));
                break;
            }
            std::vector<bgi::SolidTriangle> tris;
            if (bgi::tessellateSolid3D(obj, tris))
            {
                for (auto &tri : tris)
                    for (auto &v : tri.v)
                        v += translation;
                result = makeExactEvalResult(manifold::Manifold(meshFromTriangles(tris)));
                break;
            }
        }
        result = {};
        break;
    }

finish_exact_eval:
    if (!obj.id.empty())
        visiting.erase(obj.id);
    return result;
}

bool renderExactSetOperation3D(const bgi::Camera3D &cam,
                               const bgi::DdsScene &scene,
                               const bgi::DdsObject &obj,
                               const glm::vec3 &translation,
                               int colorOverride)
{
    int faceColor = obj.style.fillStyle.color;
    int edgeColor = obj.style.color;
    bgi::SolidDrawMode mode = bgi::SolidDrawMode::Flat;
    if (obj.type == bgi::DdsObjectType::SetUnion)
    {
        const auto &setObj = static_cast<const bgi::DdsSetUnion &>(obj);
        faceColor = setObj.faceColor;
        edgeColor = setObj.edgeColor;
        mode      = static_cast<bgi::SolidDrawMode>(setObj.drawMode);
    }
    else if (obj.type == bgi::DdsObjectType::SetIntersection)
    {
        const auto &setObj = static_cast<const bgi::DdsSetIntersection &>(obj);
        faceColor = setObj.faceColor;
        edgeColor = setObj.edgeColor;
        mode      = static_cast<bgi::SolidDrawMode>(setObj.drawMode);
    }
    else if (obj.type == bgi::DdsObjectType::SetDifference)
    {
        const auto &setObj = static_cast<const bgi::DdsSetDifference &>(obj);
        faceColor = setObj.faceColor;
        edgeColor = setObj.edgeColor;
        mode      = static_cast<bgi::SolidDrawMode>(setObj.drawMode);
    }

    const bool canCache = !obj.id.empty();
    ExactSetOpCacheKey cacheKey;
    ExactSetOpCacheEntry *cachedEntry = nullptr;
    if (canCache)
    {
        auto &cache = exactSetOpCache(scene);
        cacheKey = makeExactSetOpCacheKey(obj, translation);
        auto it = cache.entries.find(cacheKey);
        if (it != cache.entries.end())
        {
            if (it->second.empty)
                return true;
            auto tris = trianglesFromCachedVertices(it->second.vertices, faceColor, edgeColor);
            const int savedOverride = bgi::gState.solidColorOverride;
            bgi::gState.solidColorOverride = colorOverride;
            bgi::renderSolidTriangles(cam, tris, mode);
            bgi::gState.solidColorOverride = savedOverride;
            return true;
        }
        cachedEntry = &cache.entries[cacheKey];
    }

    std::unordered_set<std::string> evalVisiting;
    auto eval = exactEvalFromNode(scene, obj, translation, evalVisiting);
    if (eval.state == ExactEvalState::Invalid)
        return false;
    if (eval.state == ExactEvalState::Empty)
    {
        if (cachedEntry)
            cachedEntry->empty = true;
        return true;
    }

    auto &mf = eval.manifold;
    const auto mesh = mf.GetMeshGL();
    if (cachedEntry)
        cachedEntry->vertices = cachedVerticesFromMesh(mesh);
    auto tris = trianglesFromMesh(mesh, faceColor, edgeColor);
    const int savedOverride = bgi::gState.solidColorOverride;
    bgi::gState.solidColorOverride = colorOverride;
    bgi::renderSolidTriangles(cam, tris, mode);
    bgi::gState.solidColorOverride = savedOverride;
    return true;
}

void renderNodeRecursive(const bgi::Camera3D &cam,
                         const bgi::DdsScene &scene,
                         const bgi::DdsObject &obj,
                         const glm::vec3 &translation,
                         int colorOverride,
                         const bgi::DdsStyle *styleOverride,
                         std::unordered_set<std::string> &visiting);

std::vector<std::uint8_t> renderNodeToMask(const bgi::Camera3D &cam,
                                           const bgi::DdsScene &scene,
                                           const bgi::DdsObject &obj,
                                           const glm::vec3 &translation,
                                           const bgi::DdsStyle *styleOverride,
                                           std::unordered_set<std::string> &visiting)
{
    constexpr std::uint8_t kMaskColor = 255;
    auto &buffer = bgi::activePageBuffer();
    const auto savedBuffer = buffer;
    const auto savedPending = bgi::gState.pendingGl;
    const auto savedLegacy = bgi::gState.legacyGlRender;
    std::fill(buffer.begin(), buffer.end(), 0);
    bgi::gState.pendingGl.clear();
    bgi::gState.legacyGlRender = true;

    renderNodeRecursive(cam, scene, obj, translation, kMaskColor, styleOverride, visiting);

    std::vector<std::uint8_t> mask(buffer.size(), 0);
    for (std::size_t i = 0; i < buffer.size(); ++i)
        mask[i] = (buffer[i] == kMaskColor) ? 1u : 0u;

    buffer = savedBuffer;
    bgi::gState.pendingGl = savedPending;
    bgi::gState.legacyGlRender = savedLegacy;
    return mask;
}

void renderSetOperation(const bgi::Camera3D &cam,
                         const bgi::DdsScene &scene,
                         const bgi::DdsObject &obj,
                         const std::vector<std::string> &operands,
                         bgi::DdsObjectType opType,
                         const glm::vec3 &translation,
                         int colorOverride,
                         std::unordered_set<std::string> &visiting)
{
    if (operands.empty())
        return;

    if (renderExactSetOperation3D(cam, scene, obj, translation, colorOverride))
        return;

    std::vector<std::uint8_t> combined;
    bool haveCombined = false;
    for (const auto &childId : operands)
    {
        auto child = scene.findById(childId);
        if (!child)
            continue;
        auto mask = renderNodeToMask(cam, scene, *child, translation, &obj.style, visiting);
        if (!haveCombined)
        {
            combined = std::move(mask);
            haveCombined = true;
            continue;
        }

        const std::size_t n = std::min(combined.size(), mask.size());
        for (std::size_t i = 0; i < n; ++i)
        {
            switch (opType)
            {
            case bgi::DdsObjectType::SetIntersection:
                combined[i] = static_cast<std::uint8_t>(combined[i] && mask[i]);
                break;
            case bgi::DdsObjectType::SetDifference:
                combined[i] = static_cast<std::uint8_t>(combined[i] && !mask[i]);
                break;
            default:
                combined[i] = static_cast<std::uint8_t>(combined[i] || mask[i]);
                break;
            }
        }
    }

    if (!haveCombined)
        return;

    const int drawColor = (colorOverride >= 0) ? colorOverride : obj.style.fillStyle.color;
    renderMaskOntoActive(combined, static_cast<std::uint8_t>(std::clamp(drawColor, 0, 255)));
}

void renderNodeRecursive(const bgi::Camera3D &cam,
                         const bgi::DdsScene &scene,
                         const bgi::DdsObject &obj,
                         const glm::vec3 &translation,
                         int colorOverride,
                         const bgi::DdsStyle *styleOverride,
                         std::unordered_set<std::string> &visiting)
{
    if (!obj.id.empty())
    {
        if (!visiting.insert(obj.id).second)
            return;
    }

    switch (obj.type)
    {
    case bgi::DdsObjectType::Transform: {
        const auto &tx = static_cast<const bgi::DdsTransform &>(obj);
        const glm::vec3 nextTranslation = translation + translationFromMatrix(tx.matrix);
        for (const auto &childId : tx.children)
        {
            auto child = scene.findById(childId);
            if (child)
                renderNodeRecursive(cam, scene, *child, nextTranslation, colorOverride, styleOverride, visiting);
        }
        break;
    }
    case bgi::DdsObjectType::SetUnion:
        renderSetOperation(cam, scene, obj,
                           static_cast<const bgi::DdsSetUnion &>(obj).operands,
                           bgi::DdsObjectType::SetUnion, translation, colorOverride, visiting);
        break;
    case bgi::DdsObjectType::SetIntersection:
        renderSetOperation(cam, scene, obj,
                           static_cast<const bgi::DdsSetIntersection &>(obj).operands,
                           bgi::DdsObjectType::SetIntersection, translation, colorOverride, visiting);
        break;
    case bgi::DdsObjectType::SetDifference:
        renderSetOperation(cam, scene, obj,
                           static_cast<const bgi::DdsSetDifference &>(obj).operands,
                           bgi::DdsObjectType::SetDifference, translation, colorOverride, visiting);
        break;
    default:
        if (isLeafObject(obj.type))
            renderTranslatedLeaf(cam, obj, translation, colorOverride, styleOverride);
        break;
    }

    if (!obj.id.empty())
        visiting.erase(obj.id);
}

} // anonymous namespace

// =============================================================================
// wxbgi_render_dds — public implementation  (replaces stub in bgi_dds_api.cpp)
// =============================================================================

BGI_API void BGI_CALL wxbgi_render_dds(const char *camName)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);

    // Resolve camera name: NULL or "" → active camera.
    const std::string key = (camName != nullptr && camName[0] != '\0')
                                ? std::string(camName)
                                : bgi::gState.activeCamera;

    auto camIt = bgi::gState.cameras.find(key);
    if (camIt == bgi::gState.cameras.end())
        return;

    const bgi::Camera3D &cam = camIt->second->camera;

    // Clear accumulated GL geometry from the previous frame so solids don't
    // pile up across calls.  renderSolid3D() will repopulate this.
    bgi::gState.pendingGl.clear();

    // Save draw state (restored after render so immediate-mode state is unchanged).
    const auto savedStyle = saveStyle();

    // Traverse DDS in insertion order; skip Camera / Ucs / WorldExtents objects.
    // Selected objects flash between their original colour and the selection colour.
    const double flashTime  = glfwGetTime();
    const int    flashPhase = static_cast<int>(flashTime) & 1; // 0 or 1 per second
    const int    flashColor = (bgi::gState.selectionFlashScheme == 0)
                                  ? bgi::kSelectionOrangeColor
                                  : bgi::kSelectionPurpleColor;
    const auto  &selIds     = bgi::gState.selectedObjectIds;

    // Determine which DDS scene this camera should render.
    // Falls back to the "default" scene if the assigned scene no longer exists.
    const std::string &sceneName = cam.assignedSceneName;
    auto sceneIt = bgi::gState.ddsRegistry.find(sceneName);
    if (sceneIt == bgi::gState.ddsRegistry.end())
        sceneIt = bgi::gState.ddsRegistry.find("default");
    if (sceneIt == bgi::gState.ddsRegistry.end())
        return; // should never happen

    sceneIt->second->forEachRenderRoot([&](const bgi::DdsObject &obj) {
        if (!obj.visible)
            return;
        const bool selected = !selIds.empty() &&
            std::find(selIds.begin(), selIds.end(), obj.id) != selIds.end();
        std::unordered_set<std::string> visiting;
        renderNodeRecursive(cam, *sceneIt->second, obj, glm::vec3(0.f),
                            (selected && flashPhase == 1) ? flashColor : -1,
                            nullptr, visiting);
    });

    restoreStyle(savedStyle);

    // Visual-aids overlays (grid, UCS axes, concentric circles) are drawn AFTER
    // the 3-D solid GL pass so they always appear in front of all primitives:
    //   - GLFW mode: flushToScreen() reads pendingOverlayCam and does an alpha blit.
    //   - wx mode  : CameraPanel::PostBlit() calls wxbgi_wx_render_overlays_for_camera().
    // Store the camera name here so flushToScreen() knows which overlays to draw.
    bgi::gState.pendingOverlayCam = key;

    // In wxEmbedded mode flushToScreen() is a no-op (the GL context is managed
    // by WxBgiCanvas::Render).  Queue the current camera's GL geometry + matrices
    // so the canvas can replay all panels with the correct per-panel viewport.
    if (bgi::gState.wxEmbedded && bgi::gState.pendingGl.hasPending())
    {
        const float ar = (cam.vpW > 0 && cam.vpH > 0)
            ? static_cast<float>(cam.vpW) / static_cast<float>(cam.vpH)
            : ((bgi::gState.height > 0)
               ? static_cast<float>(bgi::gState.width) / static_cast<float>(bgi::gState.height)
               : 1.f);
        const glm::mat4 view  = bgi::cameraViewMatrix(cam);
        const glm::mat4 proj  = bgi::cameraProjMatrix(cam, ar);
        const glm::mat4 vpMat = proj * view;

        bgi::PendingGlFrame frame;
        frame.geometry = std::move(bgi::gState.pendingGl);
        std::memcpy(frame.viewProj, glm::value_ptr(vpMat), 16 * sizeof(float));
        frame.eyeX = cam.eyeX;
        frame.eyeY = cam.eyeY;
        frame.eyeZ = cam.eyeZ;
        frame.light    = bgi::gState.lightState;
        frame.camVpX   = cam.vpX;
        frame.camVpY   = cam.vpY;
        frame.camVpW   = cam.vpW;
        frame.camVpH   = cam.vpH;
        bgi::gState.pendingGlQueue.push_back(std::move(frame));
        bgi::gState.pendingGl.clear();
    }

    bgi::flushToScreen();
}
