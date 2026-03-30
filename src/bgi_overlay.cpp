// NOTE: glew.h must be included before any OpenGL/GLFW header.
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "bgi_overlay.h"

#include "bgi_camera.h"
#include "bgi_dds.h"
#include "bgi_draw.h"
#include "bgi_font.h"
#include "bgi_state.h"
#include "bgi_ucs.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace bgi
{

namespace
{

// ---------------------------------------------------------------------------
// Projection helpers
// ---------------------------------------------------------------------------

/// Convert absolute screen pixel X to viewport-relative X (what draw*Internal expects).
inline int vpRelX(float sx) { return static_cast<int>(sx) - gState.viewport.left; }
/// Convert absolute screen pixel Y to viewport-relative Y.
inline int vpRelY(float sy) { return static_cast<int>(sy) - gState.viewport.top;  }

/**
 * Project a world point through @p cam and return viewport-relative pixel
 * coordinates (@p vx, @p vy).  Returns false only if the point is completely
 * behind the camera (clip.w <= 0); out-of-viewport points still return true
 * so that drawLineInternal / drawCircleInternal can handle partial clipping.
 */
bool projectToVpRel(const Camera3D &cam, float wx, float wy, float wz,
                    int &vx, int &vy)
{
    float sx, sy;
    if (!cameraWorldToScreenForced(cam, gState.width, gState.height,
                                   wx, wy, wz, sx, sy))
        return false;
    vx = vpRelX(sx);
    vy = vpRelY(sy);
    return true;
}

/**
 * Draw one axis arm from @p origin through @p axisDir for @p length world
 * units.  Both endpoints are projected through @p cam.  On success the
 * viewport-relative tip pixel is written to @p tipVx / @p tipVy.
 */
bool drawAxisArm(const Camera3D &cam,
                 float ox, float oy, float oz,
                 float ax, float ay, float az,
                 float length, int color,
                 int &tipVx, int &tipVy)
{
    float tx = ox + ax * length;
    float ty = oy + ay * length;
    float tz = oz + az * length;

    int vx0, vy0;
    if (!projectToVpRel(cam, ox, oy, oz, vx0, vy0)) return false;
    if (!projectToVpRel(cam, tx, ty, tz, tipVx, tipVy)) return false;

    drawLineInternal(vx0, vy0, tipVx, tipVy, color);
    return true;
}

// ---------------------------------------------------------------------------
// Grid overlay
// ---------------------------------------------------------------------------

void drawGridOverlay(const Camera3D &cam)
{
    // Resolve UCS.
    const std::string &ucsKey = gState.overlayGrid.ucsName.empty()
                                    ? gState.activeUcs
                                    : gState.overlayGrid.ucsName;
    auto ucsIt = gState.ucsSystems.find(ucsKey);
    if (ucsIt == gState.ucsSystems.end())
        return;
    const CoordSystem &cs = ucsIt->second->ucs;

    const float sp  = std::max(0.01f, gState.overlayGrid.spacing);
    const int   he  = std::max(1, gState.overlayGrid.halfExtent);
    const float ext = sp * static_cast<float>(he);

    const int xAxisColor = gState.overlayGrid.xAxisColor;
    const int yAxisColor = gState.overlayGrid.yAxisColor;
    const int gridColor  = gState.overlayGrid.gridColor;

    // Save line thickness; overlays always draw at NORM_WIDTH (1 px).
    const int savedThickness = gState.lineSettings.thickness;
    gState.lineSettings.thickness = NORM_WIDTH;

    // Lines parallel to the UCS X-axis: one per row at UCS Y = i*sp.
    // The row at i==0 is the UCS X-axis itself → xAxisColor.
    for (int i = -he; i <= he; ++i)
    {
        const float gy = sp * static_cast<float>(i);
        float wx1, wy1, wz1, wx2, wy2, wz2;
        ucsLocalToWorld(cs, -ext, gy, 0.f, wx1, wy1, wz1);
        ucsLocalToWorld(cs,  ext, gy, 0.f, wx2, wy2, wz2);
        int vx1, vy1, vx2, vy2;
        const bool ok1 = projectToVpRel(cam, wx1, wy1, wz1, vx1, vy1);
        const bool ok2 = projectToVpRel(cam, wx2, wy2, wz2, vx2, vy2);
        if (!ok1 && !ok2) continue;
        if (!ok1) { vx1 = vx2; vy1 = vy2; }
        if (!ok2) { vx2 = vx1; vy2 = vy1; }
        drawLineInternal(vx1, vy1, vx2, vy2, (i == 0) ? xAxisColor : gridColor);
    }

    // Lines parallel to the UCS Y-axis: one per column at UCS X = i*sp.
    // The column at i==0 is the UCS Y-axis itself → yAxisColor.
    for (int i = -he; i <= he; ++i)
    {
        const float gx = sp * static_cast<float>(i);
        float wx1, wy1, wz1, wx2, wy2, wz2;
        ucsLocalToWorld(cs, gx, -ext, 0.f, wx1, wy1, wz1);
        ucsLocalToWorld(cs, gx,  ext, 0.f, wx2, wy2, wz2);
        int vx1, vy1, vx2, vy2;
        const bool ok1 = projectToVpRel(cam, wx1, wy1, wz1, vx1, vy1);
        const bool ok2 = projectToVpRel(cam, wx2, wy2, wz2, vx2, vy2);
        if (!ok1 && !ok2) continue;
        if (!ok1) { vx1 = vx2; vy1 = vy2; }
        if (!ok2) { vx2 = vx1; vy2 = vy1; }
        drawLineInternal(vx1, vy1, vx2, vy2, (i == 0) ? yAxisColor : gridColor);
    }

    gState.lineSettings.thickness = savedThickness;
}

// ---------------------------------------------------------------------------
// UCS axes overlay
// ---------------------------------------------------------------------------

void drawUcsAxesOverlay(const Camera3D &cam)
{
    const float len = std::max(1.f, gState.overlayUcsAxes.axisLength);

    const int savedThickness = gState.lineSettings.thickness;
    gState.lineSettings.thickness = NORM_WIDTH;

    auto drawAxesForCs = [&](const CoordSystem &cs, bool isActive)
    {
        const float ox = cs.originX, oy = cs.originY, oz = cs.originZ;
        const char *xLabel = isActive ? "x" : "X";
        const char *yLabel = isActive ? "y" : "Y";
        const char *zLabel = isActive ? "z" : "Z";

        int tipVx, tipVy;

        // X axis — RED
        if (drawAxisArm(cam, ox, oy, oz,
                        cs.xAxisX, cs.xAxisY, cs.xAxisZ, len, RED, tipVx, tipVy))
            drawText(tipVx + 2, tipVy - 4, xLabel, RED);

        // Y axis — GREEN
        if (drawAxisArm(cam, ox, oy, oz,
                        cs.yAxisX, cs.yAxisY, cs.yAxisZ, len, GREEN, tipVx, tipVy))
            drawText(tipVx + 2, tipVy - 4, yLabel, GREEN);

        // Z axis — BLUE (colour index 1)
        if (drawAxisArm(cam, ox, oy, oz,
                        cs.zAxisX, cs.zAxisY, cs.zAxisZ, len, BLUE, tipVx, tipVy))
            drawText(tipVx + 2, tipVy - 4, zLabel, BLUE);
    };

    if (gState.overlayUcsAxes.showWorld)
    {
        CoordSystem world{}; // identity frame: origin (0,0,0), axes X/Y/Z
        drawAxesForCs(world, false);
    }

    if (gState.overlayUcsAxes.showActive)
    {
        auto it = gState.ucsSystems.find(gState.activeUcs);
        if (it != gState.ucsSystems.end())
            drawAxesForCs(it->second->ucs, true);
    }

    gState.lineSettings.thickness = savedThickness;
}

// ---------------------------------------------------------------------------
// Concentric circles + crosshair overlay
// ---------------------------------------------------------------------------

void drawConcentricOverlay(const std::string &camName, const Camera3D &cam)
{
    auto camIt = gState.cameras.find(camName);
    if (camIt == gState.cameras.end()) return;

    const auto &oc = camIt->second->camera.concentricOverlay;
    if (!oc.enabled) return;

    const int winW = gState.width;
    const int winH = gState.height;

    // World-space centre of the overlay.
    float cx, cy, cz;
    if (cam.is2D)
    {
        cx = cam.pan2dX;
        cy = cam.pan2dY;
        cz = 0.f;
    }
    else
    {
        cx = cam.targetX;
        cy = cam.targetY;
        cz = cam.targetZ;
    }

    // Project centre to screen pixels.
    float scx, scy;
    if (!cameraWorldToScreenForced(cam, winW, winH, cx, cy, cz, scx, scy))
        return;
    const int vcx = vpRelX(scx);
    const int vcy = vpRelY(scy);

    const int savedThickness = gState.lineSettings.thickness;
    gState.lineSettings.thickness = NORM_WIDTH;

    // --- Concentric circles ---
    const int   n    = std::clamp(oc.count, 1, 8);
    const float rMin = std::max(0.f, oc.innerRadius);
    const float rMax = std::max(rMin, oc.outerRadius);
    const float step = (n > 1) ? (rMax - rMin) / static_cast<float>(n - 1) : 0.f;

    int outerPxRadius = 0;
    for (int i = 0; i < n; ++i)
    {
        const float r = rMin + step * static_cast<float>(i);
        if (r <= 0.f) continue;

        // Project world point at (cx + r, cy, cz) to get pixel radius.
        // For 2D cameras the projection is isotropic in XY, so the X-axis
        // distance equals the true radius after projection.
        float prx, pry;
        if (!cameraWorldToScreenForced(cam, winW, winH, cx + r, cy, cz, prx, pry))
            continue;
        const int pxRadius = static_cast<int>(
            std::round(std::hypot(prx - scx, pry - scy)));
        if (pxRadius <= 0) continue;

        drawCircleInternal(vcx, vcy, pxRadius, LIGHTGRAY);
        outerPxRadius = std::max(outerPxRadius, pxRadius);
    }

    // --- Crosshair ---
    const int extend = std::max(winW, winH) + 50;

    // "Up" direction in screen space — used for both crosshair and rotation arrow.
    float upDx = 0.f, upDy = -1.f; // default: straight up on screen

    if (cam.is2D)
    {
        const float probeLen = 1000.f;

        float prxX, pryX;
        cameraWorldToScreenForced(cam, winW, winH,
                                  cx + probeLen, cy, cz, prxX, pryX);
        float dxX = prxX - scx, dyX = pryX - scy;
        const float lenX = std::hypot(dxX, dyX);
        if (lenX > 0.f) { dxX /= lenX; dyX /= lenX; }

        float prxY, pryY;
        cameraWorldToScreenForced(cam, winW, winH,
                                  cx, cy + probeLen, cz, prxY, pryY);
        float dxY = prxY - scx, dyY = pryY - scy;
        const float lenY = std::hypot(dxY, dyY);
        if (lenY > 0.f) { dxY /= lenY; dyY /= lenY; }

        // Camera "up" = direction toward screen top = negative of world +Y projection
        // (because BGI Y increases downward, world +Y maps toward screen bottom).
        upDx = -dxY;
        upDy = -dyY;

        // Arm 1 — along world X projected direction
        drawLineInternal(
            vcx - static_cast<int>(dxX * extend),
            vcy - static_cast<int>(dyX * extend),
            vcx + static_cast<int>(dxX * extend),
            vcy + static_cast<int>(dyX * extend),
            WHITE);

        // Arm 2 — along world Y projected direction
        drawLineInternal(
            vcx - static_cast<int>(dxY * extend),
            vcy - static_cast<int>(dyY * extend),
            vcx + static_cast<int>(dxY * extend),
            vcy + static_cast<int>(dyY * extend),
            WHITE);
    }
    else
    {
        // 3D camera: fixed screen-space crosshair; up = camera up vector projected.
        drawLineInternal(vcx - extend, vcy, vcx + extend, vcy, WHITE);
        drawLineInternal(vcx, vcy - extend, vcx, vcy + extend, WHITE);

        // Project camera up vector to screen space.
        float prxUp, pryUp;
        if (cameraWorldToScreenForced(cam, winW, winH,
                                      cx + cam.upX * 1000.f,
                                      cy + cam.upY * 1000.f,
                                      cz + cam.upZ * 1000.f,
                                      prxUp, pryUp))
        {
            float dxUp = prxUp - scx, dyUp = pryUp - scy;
            const float lenUp = std::hypot(dxUp, dyUp);
            if (lenUp > 0.f) { upDx = dxUp / lenUp; upDy = dyUp / lenUp; }
        }
    }

    // --- Rotation indicator arrow ---
    // Drawn along the camera "up" direction; starts at center, tip beyond outermost circle.
    if (outerPxRadius > 0)
    {
        static constexpr float kPi = 3.14159265358979f;
        const float arrowLen  = static_cast<float>(outerPxRadius) + 20.f;
        const float headLen   = 10.f;
        const float headAngle = 25.f * kPi / 180.f;
        const float cosA = std::cos(headAngle), sinA = std::sin(headAngle);

        const int tipX = vcx + static_cast<int>(upDx * arrowLen);
        const int tipY = vcy + static_cast<int>(upDy * arrowLen);

        drawLineInternal(vcx, vcy, tipX, tipY, YELLOW);

        // Back-direction from tip
        const float bx = -upDx, by = -upDy;

        // Left arrowhead arm
        drawLineInternal(tipX, tipY,
            tipX + static_cast<int>((bx * cosA - by * sinA) * headLen),
            tipY + static_cast<int>((bx * sinA + by * cosA) * headLen),
            YELLOW);
        // Right arrowhead arm
        drawLineInternal(tipX, tipY,
            tipX + static_cast<int>((bx * cosA + by * sinA) * headLen),
            tipY + static_cast<int>((-bx * sinA + by * cosA) * headLen),
            YELLOW);
    }

    gState.lineSettings.thickness = savedThickness;
}

// ---------------------------------------------------------------------------
// Selection picking helpers
// ---------------------------------------------------------------------------

/// Resolve a DDS coordinate-space point to world 3-D.
static glm::vec3 toWorldSpace(CoordSpace cs, const std::string &ucsName,
                               const glm::vec3 &localPt)
{
    if (cs != CoordSpace::UcsLocal)
        return localPt; // BgiPixel and World3D are already "world" for projection.

    const std::string &name = ucsName.empty() ? gState.activeUcs : ucsName;
    auto it = gState.ucsSystems.find(name);
    if (it == gState.ucsSystems.end())
        return localPt;
    const glm::mat4 m = ucsLocalToWorldMatrix(it->second->ucs);
    return glm::vec3(m * glm::vec4(localPt, 1.f));
}

/// Minimum screen-space distance (absolute pixels) from click (mx, my) to @p obj.
/// @p viewMat  Pre-computed camera view matrix (from cameraViewMatrix()).
/// @p outDepth Camera-space depth (= -camZ) of the representative point that
///             produced the minimum screen distance.  Positive = in front of camera.
static float screenDistToObject(const Camera3D &cam, const glm::mat4 &viewMat,
                                 const DdsObject &obj, int mx, int my, float &outDepth)
{
    const int W = gState.width, H = gState.height;
    float minDist = 1e9f;
    outDepth = 0.f;

    auto probe = [&](const glm::vec3 &wpt) {
        float sx, sy;
        if (!cameraWorldToScreenForced(cam, W, H, wpt.x, wpt.y, wpt.z, sx, sy))
            return;
        const float dx = sx - static_cast<float>(mx);
        const float dy = sy - static_cast<float>(my);
        const float d = std::hypot(dx, dy);
        if (d < minDist)
        {
            minDist  = d;
            // Camera-space depth: OpenGL camera looks down -Z, so objects in
            // front have negative camZ.  outDepth = -camZ → positive = closer.
            const glm::vec4 camPt = viewMat * glm::vec4(wpt, 1.f);
            outDepth = -camPt.z;
        }
    };

    auto tw = [&](CoordSpace cs, const std::string &ucsN, const glm::vec3 &p) -> glm::vec3 {
        return toWorldSpace(cs, ucsN, p);
    };

    switch (obj.type)
    {
    case DdsObjectType::Point:
    {
        const auto &o = static_cast<const DdsPoint &>(obj);
        probe(tw(o.coordSpace, o.ucsName, o.pos));
        break;
    }
    case DdsObjectType::Line:
    {
        const auto &o = static_cast<const DdsLine &>(obj);
        const glm::vec3 w1 = tw(o.coordSpace, o.ucsName, o.p1);
        const glm::vec3 w2 = tw(o.coordSpace, o.ucsName, o.p2);
        probe(w1); probe(w2); probe((w1 + w2) * 0.5f);
        break;
    }
    case DdsObjectType::Circle:
    {
        const auto &o = static_cast<const DdsCircle &>(obj);
        const glm::vec3 c = tw(o.coordSpace, o.ucsName, o.centre);
        probe(c);
        for (auto dp : {glm::vec3{o.radius,0,0}, glm::vec3{-o.radius,0,0},
                        glm::vec3{0,o.radius,0}, glm::vec3{0,-o.radius,0}})
            probe(tw(o.coordSpace, o.ucsName, o.centre + dp));
        break;
    }
    case DdsObjectType::Arc:
    case DdsObjectType::PieSlice:
    {
        const auto &o = static_cast<const DdsArc &>(obj);
        probe(tw(o.coordSpace, o.ucsName, o.centre));
        break;
    }
    case DdsObjectType::Ellipse:
    {
        const auto &o = static_cast<const DdsEllipse &>(obj);
        const glm::vec3 c = tw(o.coordSpace, o.ucsName, o.centre);
        probe(c);
        for (auto dp : {glm::vec3{o.rx,0,0}, glm::vec3{-o.rx,0,0},
                        glm::vec3{0,o.ry,0}, glm::vec3{0,-o.ry,0}})
            probe(tw(o.coordSpace, o.ucsName, o.centre + dp));
        break;
    }
    case DdsObjectType::FillEllipse:
    {
        const auto &o = static_cast<const DdsFillEllipse &>(obj);
        const glm::vec3 c = tw(o.coordSpace, o.ucsName, o.centre);
        probe(c);
        for (auto dp : {glm::vec3{o.rx,0,0}, glm::vec3{-o.rx,0,0},
                        glm::vec3{0,o.ry,0}, glm::vec3{0,-o.ry,0}})
            probe(tw(o.coordSpace, o.ucsName, o.centre + dp));
        break;
    }
    case DdsObjectType::Sector:
    {
        const auto &o = static_cast<const DdsSector &>(obj);
        const glm::vec3 c = tw(o.coordSpace, o.ucsName, o.centre);
        probe(c);
        for (auto dp : {glm::vec3{o.rx,0,0}, glm::vec3{-o.rx,0,0},
                        glm::vec3{0,o.ry,0}, glm::vec3{0,-o.ry,0}})
            probe(tw(o.coordSpace, o.ucsName, o.centre + dp));
        break;
    }
    case DdsObjectType::Rectangle:
    {
        const auto &o = static_cast<const DdsRectangle &>(obj);
        const glm::vec3 p1 = tw(o.coordSpace, o.ucsName, o.p1);
        const glm::vec3 p2 = tw(o.coordSpace, o.ucsName, o.p2);
        probe(p1); probe(p2);
        probe(tw(o.coordSpace, o.ucsName, {o.p1.x, o.p2.y, 0}));
        probe(tw(o.coordSpace, o.ucsName, {o.p2.x, o.p1.y, 0}));
        probe((p1 + p2) * 0.5f);
        break;
    }
    case DdsObjectType::Bar:
    {
        const auto &o = static_cast<const DdsBar &>(obj);
        const glm::vec3 p1 = tw(o.coordSpace, o.ucsName, o.p1);
        const glm::vec3 p2 = tw(o.coordSpace, o.ucsName, o.p2);
        probe(p1); probe(p2);
        probe(tw(o.coordSpace, o.ucsName, {o.p1.x, o.p2.y, 0}));
        probe(tw(o.coordSpace, o.ucsName, {o.p2.x, o.p1.y, 0}));
        probe((p1 + p2) * 0.5f);
        break;
    }
    case DdsObjectType::Bar3D:
    {
        const auto &o = static_cast<const DdsBar3D &>(obj);
        const glm::vec3 p1 = tw(o.coordSpace, o.ucsName, o.p1);
        const glm::vec3 p2 = tw(o.coordSpace, o.ucsName, o.p2);
        probe(p1); probe(p2);
        probe(tw(o.coordSpace, o.ucsName, {o.p1.x, o.p2.y, 0}));
        probe(tw(o.coordSpace, o.ucsName, {o.p2.x, o.p1.y, 0}));
        probe((p1 + p2) * 0.5f);
        break;
    }
    case DdsObjectType::Polygon:
    {
        const auto &o = static_cast<const DdsPolygon &>(obj);
        for (const auto &p : o.pts)
            probe(tw(o.coordSpace, o.ucsName, p));
        break;
    }
    case DdsObjectType::FillPoly:
    {
        const auto &o = static_cast<const DdsFillPoly &>(obj);
        for (const auto &p : o.pts)
            probe(tw(o.coordSpace, o.ucsName, p));
        break;
    }
    case DdsObjectType::Text:
    {
        const auto &o = static_cast<const DdsText &>(obj);
        probe(tw(o.coordSpace, o.ucsName, o.pos));
        break;
    }
    case DdsObjectType::Image:
    {
        const auto &o = static_cast<const DdsImage &>(obj);
        probe(tw(o.coordSpace, "", o.pos));
        break;
    }
    // ---- 3D Solid primitives ----------------------------------------
    case DdsObjectType::Cylinder:
    {
        const auto &o = static_cast<const DdsCylinder &>(obj);
        const glm::vec3 top = o.origin + glm::vec3(0.f, 0.f, o.height);
        probe(tw(o.coordSpace, o.ucsName, o.origin));
        probe(tw(o.coordSpace, o.ucsName, top));
        // mid-height centre
        probe(tw(o.coordSpace, o.ucsName, o.origin + glm::vec3(0.f, 0.f, o.height * 0.5f)));
        for (auto dp : {glm::vec3{o.radius, 0, 0}, glm::vec3{-o.radius, 0, 0},
                        glm::vec3{0, o.radius, 0}, glm::vec3{0, -o.radius, 0}})
        {
            probe(tw(o.coordSpace, o.ucsName, o.origin + dp));
            probe(tw(o.coordSpace, o.ucsName, top + dp));
        }
        break;
    }
    case DdsObjectType::Sphere:
    {
        const auto &o = static_cast<const DdsSphere &>(obj);
        probe(tw(o.coordSpace, o.ucsName, o.origin));
        for (auto dp : {glm::vec3{o.radius, 0, 0}, glm::vec3{-o.radius, 0, 0},
                        glm::vec3{0, o.radius, 0}, glm::vec3{0, -o.radius, 0},
                        glm::vec3{0, 0, o.radius}, glm::vec3{0, 0, -o.radius}})
            probe(tw(o.coordSpace, o.ucsName, o.origin + dp));
        break;
    }
    case DdsObjectType::Box:
    {
        const auto &o = static_cast<const DdsBox &>(obj);
        const float hw = o.width  * 0.5f;
        const float hd = o.depth  * 0.5f;
        // All 8 corners + centre
        for (float dz : {0.f, o.height})
            for (auto dxy : {glm::vec2{-hw,-hd}, glm::vec2{hw,-hd},
                             glm::vec2{ hw, hd}, glm::vec2{-hw, hd}})
                probe(tw(o.coordSpace, o.ucsName,
                         o.origin + glm::vec3(dxy.x, dxy.y, dz)));
        probe(tw(o.coordSpace, o.ucsName, o.origin + glm::vec3(0.f, 0.f, o.height * 0.5f)));
        break;
    }
    case DdsObjectType::Cone:
    {
        const auto &o = static_cast<const DdsCone &>(obj);
        probe(tw(o.coordSpace, o.ucsName, o.origin));
        // Apex
        probe(tw(o.coordSpace, o.ucsName, o.origin + glm::vec3(0.f, 0.f, o.height)));
        // Base perimeter
        for (auto dp : {glm::vec3{o.radius, 0, 0}, glm::vec3{-o.radius, 0, 0},
                        glm::vec3{0, o.radius, 0}, glm::vec3{0, -o.radius, 0}})
            probe(tw(o.coordSpace, o.ucsName, o.origin + dp));
        break;
    }
    case DdsObjectType::Torus:
    {
        const auto &o = static_cast<const DdsTorus &>(obj);
        probe(tw(o.coordSpace, o.ucsName, o.origin));
        const float outer = o.majorRadius + o.minorRadius;
        const float inner = o.majorRadius - o.minorRadius;
        for (auto dp : {glm::vec3{outer, 0, 0}, glm::vec3{-outer, 0, 0},
                        glm::vec3{0, outer, 0}, glm::vec3{0, -outer, 0},
                        glm::vec3{inner, 0, 0}, glm::vec3{-inner, 0, 0}})
            probe(tw(o.coordSpace, o.ucsName, o.origin + dp));
        break;
    }
    case DdsObjectType::HeightMap:
    {
        const auto &o = static_cast<const DdsHeightMap &>(obj);
        probe(tw(o.coordSpace, o.ucsName, o.origin));
        const float w = o.cellWidth  * static_cast<float>(o.cols);
        const float d = o.cellHeight * static_cast<float>(o.rows);
        for (auto dp : {glm::vec3{w, 0, 0}, glm::vec3{0, d, 0}, glm::vec3{w, d, 0}})
            probe(tw(o.coordSpace, o.ucsName, o.origin + dp));
        break;
    }
    case DdsObjectType::Extrusion:
    {
        const auto &o = static_cast<const DdsExtrusion &>(obj);
        probe(tw(o.coordSpace, o.ucsName, o.origin));
        for (const auto &p : o.baseProfile)
        {
            probe(tw(o.coordSpace, o.ucsName, p));
            probe(tw(o.coordSpace, o.ucsName, p + o.extrudeDir));
        }
        break;
    }
    default: // ParamSurface and any future solid types
    {
        const auto &o = static_cast<const DdsSolid3D &>(obj);
        probe(tw(o.coordSpace, o.ucsName, o.origin));
        break;
    }
    }
    return minDist;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public (within the bgi namespace) entry points
// ---------------------------------------------------------------------------

void overlayPerformPick(int mx, int my, bool multiSelect)
{
    const int winW = gState.width;
    const int winH = gState.height;

    for (const auto &[camName, dcam] : gState.cameras)
    {
        const Camera3D &cam = dcam->camera;
        if (!cam.selCursorOverlay.enabled)
            continue;

        // Check if click is inside this camera's screen viewport.
        int vpX, vpY, vpW, vpH;
        cameraEffectiveViewport(cam, winW, winH, vpX, vpY, vpW, vpH);
        if (mx < vpX || mx >= vpX + vpW || my < vpY || my >= vpY + vpH)
            continue;

        // Pre-compute the view matrix once for depth calculations.
        const glm::mat4 viewMat = cameraViewMatrix(cam);

        // Collect ALL DDS objects within the pick radius.
        // Primary sort key: camera depth (closest to eye first).
        // Secondary: screen-space distance.
        struct Candidate
        {
            std::string id;
            float       screenDist;
            float       depth;      // -camZ; positive = in front of camera
        };
        std::vector<Candidate> within;
        within.reserve(16);

        const float threshold = static_cast<float>(gState.selectionPickRadiusPx);

        gState.dds->forEachDrawing([&](const DdsObject &obj) {
            float depth = 0.f;
            const float d = screenDistToObject(cam, viewMat, obj, mx, my, depth);
            if (d <= threshold)
                within.push_back({obj.id, d, depth});
        });

        std::string nearestId;
        if (!within.empty())
        {
            // Sort: smallest depth (closest to eye) first;
            // screen distance breaks ties within a 0.5 world-unit depth band.
            std::sort(within.begin(), within.end(),
                [](const Candidate &a, const Candidate &b) {
                    const float dd = a.depth - b.depth;
                    if (dd < -0.5f || dd > 0.5f)
                        return a.depth < b.depth;
                    return a.screenDist < b.screenDist;
                });
            nearestId = within[0].id;
        }

        if (!nearestId.empty())
        {
            if (!multiSelect)
                gState.selectedObjectIds.clear();

            auto it = std::find(gState.selectedObjectIds.begin(),
                                gState.selectedObjectIds.end(), nearestId);
            if (it != gState.selectedObjectIds.end())
                gState.selectedObjectIds.erase(it);     // toggle off
            else
                gState.selectedObjectIds.push_back(nearestId); // select
        }
        else if (!multiSelect)
        {
            gState.selectedObjectIds.clear();
        }
        break; // first matching camera viewport wins
    }
}

void drawOverlaysForCamera(const std::string &camName, const Camera3D &cam)
{
    if (gState.overlayGrid.enabled)
        drawGridOverlay(cam);

    if (gState.overlayUcsAxes.enabled)
        drawUcsAxesOverlay(cam);

    drawConcentricOverlay(camName, cam);
}

// ---------------------------------------------------------------------------
// Selection cursor — drawn in the GL pass of flushToScreen()
// ---------------------------------------------------------------------------

void drawSelectionCursorsGL()
{
    // Colour palette: [colorScheme 0..2][phase 0=bright / 1=dark][R, G, B] 0-255.
    static constexpr float kColors[3][2][3] = {
        {{100.f, 150.f, 255.f}, {20.f,  60.f, 180.f}},  // 0 = blue
        {{100.f, 255.f, 150.f}, {20.f, 150.f,  60.f}},  // 1 = green
        {{255.f, 130.f, 100.f}, {180.f, 40.f,  20.f}},  // 2 = red
    };

    const double t = glfwGetTime();

    for (const auto &[name, dcam] : gState.cameras)
    {
        const Camera3D &cam = dcam->camera;
        const auto &sc = cam.selCursorOverlay;
        if (!sc.enabled) continue;

        // Camera's screen viewport rectangle.
        int vpX, vpY, vpW, vpH;
        cameraEffectiveViewport(cam, gState.width, gState.height,
                                vpX, vpY, vpW, vpH);

        // Only draw if mouse is inside this camera's viewport.
        const int mx = gState.mouseX;
        const int my = gState.mouseY;
        if (mx < vpX || mx >= vpX + vpW ||
            my < vpY || my >= vpY + vpH)
            continue;

        // Alternate between bright and dark every 2 seconds.
        const int phase = static_cast<int>(t * 0.5) & 1; // 0 = bright, 1 = dark
        const int cs    = std::clamp(sc.colorScheme, 0, 2);
        const float *c  = kColors[cs][phase];

        // Half-size (≥ 1 px).
        const int half = std::max(1, sc.sizePx / 2);
        const int x0 = mx - half, y0 = my - half;
        const int x1 = mx + half, y1 = my + half;

        // Draw as a filled-outline square directly in GL (pixel-space ortho is
        // already set by flushToScreen, so coordinates are window pixels).
        glColor3f(c[0] / 255.f, c[1] / 255.f, c[2] / 255.f);
        glLineWidth(1.0f);
        glBegin(GL_LINE_LOOP);
        glVertex2i(x0, y0);
        glVertex2i(x1, y0);
        glVertex2i(x1, y1);
        glVertex2i(x0, y1);
        glEnd();
    }
}

} // namespace bgi
