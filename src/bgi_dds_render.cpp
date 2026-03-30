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
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "bgi_camera.h"
#include "bgi_dds.h"
#include "bgi_draw.h"
#include "bgi_font.h"
#include "bgi_image.h"
#include "bgi_overlay.h"
#include "bgi_solid_render.h"
#include "bgi_state.h"
#include "bgi_ucs.h"

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

void renderObject(const bgi::Camera3D &cam, const bgi::DdsObject &obj, int colorOverride = -1)
{
    // Temporarily apply the baked draw state for this object.
    applyStyle(obj.style);
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
        bgi::renderSolid3D(cam, obj);
        break;
    default: break;
    }
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

    bgi::gState.dds->forEachDrawing([&](const bgi::DdsObject &obj) {
        if (!obj.visible)
            return;
        const bool selected = !selIds.empty() &&
            std::find(selIds.begin(), selIds.end(), obj.id) != selIds.end();
        renderObject(cam, obj, (selected && flashPhase == 1) ? flashColor : -1);
    });

    restoreStyle(savedStyle);

    // Draw visual aids overlays (grid, UCS axes, concentric circles).
    // These write into the same page buffer as the DDS objects but are not
    // DDS objects themselves — they are not serialised or selectable.
    bgi::drawOverlaysForCamera(key, cam);

    bgi::flushToScreen();
}
