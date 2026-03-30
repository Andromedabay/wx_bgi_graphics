/**
 * @file bgi_solid_render.cpp
 * @brief Phase 4/5/6 — software tessellation and painter's-algorithm renderer
 *        for 3D solid primitives, parametric surfaces, and extruded polygons.
 *
 * All primitives are tessellated into a list of triangles in world space.
 * The triangles are then sorted back-to-front relative to the camera eye and
 * rendered into the active BGI pixel buffer using the existing
 * drawLineInternal / fillPolygonInternal primitives.
 *
 * This module is called from:
 *  - bgi_solid_api.cpp  — immediate render at draw-call time.
 *  - bgi_dds_render.cpp — retained-mode re-render via wxbgi_render_dds().
 *
 * Thread safety: caller must already hold bgi::gMutex before calling
 * renderSolid3D.  No locking is performed inside this file.
 */

#include "bgi_solid_render.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "bgi_camera.h"
#include "bgi_draw.h"
#include "bgi_state.h"
#include "bgi_ucs.h"

// =============================================================================
// Anonymous namespace — tessellation helpers and renderer
// =============================================================================

namespace
{

constexpr float kPi = 3.14159265358979323846f;

// ---------------------------------------------------------------------------
// Triangle — the fundamental rendering unit
// ---------------------------------------------------------------------------

struct Triangle
{
    glm::vec3 v[3];   ///< world-space vertices
    int       faceColor;
    int       edgeColor;
};

// ---------------------------------------------------------------------------
// Projection helpers
// ---------------------------------------------------------------------------

/** Project a 4-D clip-space point to viewport-relative pixels.
 *  No NDC X/Y test — relies on per-pixel clipping at the viewport boundary.
 *  Returns false only when w <= 0 (guaranteed not to occur after near-plane clip). */
bool projectCP(const bgi::Camera3D &cam,
               const glm::vec4 &cp,
               int &px, int &py)
{
    float sx = 0.f, sy = 0.f;
    if (!bgi::cameraClipToScreen(cam, bgi::gState.width, bgi::gState.height,
                                 cp, sx, sy))
        return false;
    px = static_cast<int>(sx) - bgi::gState.viewport.left;
    py = static_cast<int>(sy) - bgi::gState.viewport.top;
    return true;
}

// ---------------------------------------------------------------------------
// Painter's-algorithm triangle renderer
// ---------------------------------------------------------------------------

void renderTriangles(const bgi::Camera3D &cam,
                     std::vector<Triangle> &tris,
                     bgi::SolidDrawMode mode)
{
    if (tris.empty())
        return;

    // Camera eye position for front-face determination.
    // For 2-D cameras the stored eyeX/Y/Z are the 3-D camera defaults and are
    // meaningless; derive the effective eye from the 2-D camera parameters
    // (pan centre plus a large positive Z so it "looks down" from above in
    // the Z-up world convention).
    glm::vec3 eye;
    if (cam.is2D)
        eye = glm::vec3(cam.pan2dX, cam.pan2dY, 10000.f);
    else
        eye = glm::vec3(cam.eyeX, cam.eyeY, cam.eyeZ);

    // Build view matrix for depth sorting.
    glm::mat4 view = bgi::cameraViewMatrix(cam);

    // Compute camera-space Z for each triangle centroid (more negative = farther away).
    struct TriDepth { int idx; float camZ; };
    std::vector<TriDepth> depth;
    depth.reserve(tris.size());
    for (int i = 0; i < static_cast<int>(tris.size()); ++i)
    {
        glm::vec3 centroid = (tris[i].v[0] + tris[i].v[1] + tris[i].v[2]) / 3.f;
        glm::vec4 cv = view * glm::vec4(centroid, 1.f);
        depth.push_back({i, cv.z});
    }
    // Sort back-to-front (most negative camZ first = farthest first).
    std::sort(depth.begin(), depth.end(),
              [](const TriDepth &a, const TriDepth &b){ return a.camZ < b.camZ; });

    for (const auto &td : depth)
    {
        const Triangle &tri = tris[td.idx];

        // Compute face normal.
        glm::vec3 e1 = tri.v[1] - tri.v[0];
        glm::vec3 e2 = tri.v[2] - tri.v[0];
        glm::vec3 normal = glm::cross(e1, e2);

        // Skip degenerate triangles.
        if (glm::dot(normal, normal) < 1e-10f)
            continue;

        // Determine if this is a front face (for solid-fill).
        glm::vec3 centroid = (tri.v[0] + tri.v[1] + tri.v[2]) / 3.f;
        glm::vec3 viewDir  = eye - centroid;
        bool frontFace = glm::dot(normal, viewDir) > 0.f;

        // Sutherland-Hodgman: clip the triangle against near + far Z planes
        // in 4-D clip space, then project and rasterize each resulting sub-triangle.
        std::vector<glm::vec4> clip4 = {
            bgi::cameraWorldToClip(cam, bgi::gState.width, bgi::gState.height,
                                   tri.v[0].x, tri.v[0].y, tri.v[0].z),
            bgi::cameraWorldToClip(cam, bgi::gState.width, bgi::gState.height,
                                   tri.v[1].x, tri.v[1].y, tri.v[1].z),
            bgi::cameraWorldToClip(cam, bgi::gState.width, bgi::gState.height,
                                   tri.v[2].x, tri.v[2].y, tri.v[2].z)
        };
        bgi::clipPolyZPlanes(clip4);
        if (clip4.size() < 3)
            continue;

        // Triangle fan from vertex 0 of the clipped polygon
        for (std::size_t fi = 1; fi + 1 < clip4.size(); ++fi)
        {
            int px0 = 0, py0 = 0, px1 = 0, py1 = 0, px2 = 0, py2 = 0;
            if (!projectCP(cam, clip4[0], px0, py0)) continue;
            if (!projectCP(cam, clip4[fi], px1, py1)) continue;
            if (!projectCP(cam, clip4[fi + 1], px2, py2)) continue;

            std::vector<std::pair<int,int>> pts = {
                {px0, py0}, {px1, py1}, {px2, py2}
            };

            if (mode == bgi::SolidDrawMode::Solid && frontFace)
            {
                const int fc = (bgi::gState.solidColorOverride >= 0)
                                   ? bgi::gState.solidColorOverride
                                   : tri.faceColor;
                int savedFillPat  = bgi::gState.fillPattern;
                int savedFillCol  = bgi::gState.fillColor;
                bgi::gState.fillPattern = bgi::SOLID_FILL;
                bgi::gState.fillColor   = fc;
                bgi::fillPolygonInternal(pts, fc);
                bgi::gState.fillPattern = savedFillPat;
                bgi::gState.fillColor   = savedFillCol;
            }

            {
                const int ec = (bgi::gState.solidColorOverride >= 0)
                                   ? bgi::gState.solidColorOverride
                                   : tri.edgeColor;
                bgi::drawPolygonInternal(pts, ec);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Coordinate-space resolution
// ---------------------------------------------------------------------------

/** Resolve a solid's origin through its coordinate space into world space. */
glm::vec3 resolveOrigin(const bgi::DdsSolid3D &s)
{
    if (s.coordSpace == bgi::CoordSpace::UcsLocal)
    {
        auto it = bgi::gState.ucsSystems.find(s.ucsName);
        if (it != bgi::gState.ucsSystems.end())
        {
            float wx = 0.f, wy = 0.f, wz = 0.f;
            bgi::ucsLocalToWorld(it->second->ucs,
                                 s.origin.x, s.origin.y, s.origin.z,
                                 wx, wy, wz);
            return {wx, wy, wz};
        }
    }
    return s.origin;
}

// ---------------------------------------------------------------------------
// Phase 4 tessellators
// ---------------------------------------------------------------------------

void tessBox(const bgi::DdsBox &o, const glm::vec3 &cen, std::vector<Triangle> &tris)
{
    const float hw = o.width  * 0.5f;
    const float hd = o.depth  * 0.5f;
    const float hh = o.height * 0.5f;

    // 8 corner vertices (X±hw, Y±hd, Z±hh)
    glm::vec3 c[8] = {
        cen + glm::vec3(-hw, -hd, -hh),  // 0 left  front bottom
        cen + glm::vec3( hw, -hd, -hh),  // 1 right front bottom
        cen + glm::vec3( hw,  hd, -hh),  // 2 right back  bottom
        cen + glm::vec3(-hw,  hd, -hh),  // 3 left  back  bottom
        cen + glm::vec3(-hw, -hd,  hh),  // 4 left  front top
        cen + glm::vec3( hw, -hd,  hh),  // 5 right front top
        cen + glm::vec3( hw,  hd,  hh),  // 6 right back  top
        cen + glm::vec3(-hw,  hd,  hh),  // 7 left  back  top
    };

    // Each face as two triangles with outward-facing winding.
    auto addFace = [&](int a, int b, int cc, int d) {
        tris.push_back({{c[a], c[b],  c[cc]}, o.faceColor, o.edgeColor});
        tris.push_back({{c[a], c[cc], c[d]},  o.faceColor, o.edgeColor});
    };

    addFace(0, 3, 2, 1);  // Bottom (-Z)
    addFace(4, 5, 6, 7);  // Top    (+Z)
    addFace(0, 1, 5, 4);  // Front  (-Y)
    addFace(2, 3, 7, 6);  // Back   (+Y)
    addFace(0, 4, 7, 3);  // Left   (-X)
    addFace(1, 2, 6, 5);  // Right  (+X)
}

void tessSphere(const bgi::DdsSphere &o, const glm::vec3 &cen, std::vector<Triangle> &tris)
{
    const int sl = std::max(3, o.slices);
    const int st = std::max(2, o.stacks);
    const float r = o.radius;

    // Generate UV grid: phi 0..pi (south→north), theta 0..2pi (longitude).
    std::vector<std::vector<glm::vec3>> grid(st + 1, std::vector<glm::vec3>(sl + 1));
    for (int i = 0; i <= st; ++i)
    {
        const float phi = kPi * i / st;
        for (int j = 0; j <= sl; ++j)
        {
            const float theta = 2.f * kPi * j / sl;
            // Z-up: sin(phi) drives XY, cos(phi) drives Z.
            grid[i][j] = cen + glm::vec3(
                r * std::sin(phi) * std::cos(theta),
                r * std::sin(phi) * std::sin(theta),
                r * std::cos(phi));
        }
    }
    for (int i = 0; i < st; ++i)
        for (int j = 0; j < sl; ++j)
        {
            const glm::vec3 &v00 = grid[i][j],     &v01 = grid[i][j+1];
            const glm::vec3 &v10 = grid[i+1][j],   &v11 = grid[i+1][j+1];
            tris.push_back({{v00, v10, v11}, o.faceColor, o.edgeColor});
            tris.push_back({{v00, v11, v01}, o.faceColor, o.edgeColor});
        }
}

void tessCylinder(const bgi::DdsCylinder &o, const glm::vec3 &cen, std::vector<Triangle> &tris)
{
    const int sl = std::max(3, o.slices);
    const float r = o.radius;
    const float hh = o.height * 0.5f;

    std::vector<glm::vec3> bot(sl), top(sl);
    for (int i = 0; i < sl; ++i)
    {
        const float theta = 2.f * kPi * i / sl;
        const float x = r * std::cos(theta), y = r * std::sin(theta);
        bot[i] = cen + glm::vec3(x, y, -hh);
        top[i] = cen + glm::vec3(x, y,  hh);
    }

    // Side quads as triangle pairs.
    for (int i = 0; i < sl; ++i)
    {
        const int j = (i + 1) % sl;
        tris.push_back({{bot[i], bot[j], top[j]}, o.faceColor, o.edgeColor});
        tris.push_back({{bot[i], top[j], top[i]}, o.faceColor, o.edgeColor});
    }

    // Optional disc caps.
    if (o.caps)
    {
        const glm::vec3 bc = cen + glm::vec3(0.f, 0.f, -hh);
        const glm::vec3 tc = cen + glm::vec3(0.f, 0.f,  hh);
        for (int i = 0; i < sl; ++i)
        {
            const int j = (i + 1) % sl;
            tris.push_back({{bc, bot[j], bot[i]}, o.faceColor, o.edgeColor});
            tris.push_back({{tc, top[i], top[j]}, o.faceColor, o.edgeColor});
        }
    }
}

void tessCone(const bgi::DdsCone &o, const glm::vec3 &cen, std::vector<Triangle> &tris)
{
    const int sl = std::max(3, o.slices);
    const float r = o.radius;
    const glm::vec3 apex = cen + glm::vec3(0.f, 0.f, o.height);

    std::vector<glm::vec3> base(sl);
    for (int i = 0; i < sl; ++i)
    {
        const float theta = 2.f * kPi * i / sl;
        base[i] = cen + glm::vec3(r * std::cos(theta), r * std::sin(theta), 0.f);
    }

    // Side triangles.
    for (int i = 0; i < sl; ++i)
    {
        const int j = (i + 1) % sl;
        tris.push_back({{base[i], base[j], apex}, o.faceColor, o.edgeColor});
    }

    // Optional base cap.
    if (o.cap)
    {
        for (int i = 0; i < sl; ++i)
        {
            const int j = (i + 1) % sl;
            tris.push_back({{cen, base[j], base[i]}, o.faceColor, o.edgeColor});
        }
    }
}

void tessTorus(const bgi::DdsTorus &o, const glm::vec3 &cen, std::vector<Triangle> &tris)
{
    const int rings = std::max(3, o.slices);
    const int sides = std::max(3, o.stacks);
    const float R = o.majorRadius, r = o.minorRadius;

    // Grid[ring][side] — torus major circle in XY plane (Z-up).
    std::vector<std::vector<glm::vec3>> grid(rings + 1, std::vector<glm::vec3>(sides + 1));
    for (int i = 0; i <= rings; ++i)
    {
        const float phi = 2.f * kPi * i / rings;
        for (int j = 0; j <= sides; ++j)
        {
            const float theta = 2.f * kPi * j / sides;
            const float x = (R + r * std::cos(theta)) * std::cos(phi);
            const float y = (R + r * std::cos(theta)) * std::sin(phi);
            const float z = r * std::sin(theta);
            grid[i][j] = cen + glm::vec3(x, y, z);
        }
    }
    for (int i = 0; i < rings; ++i)
        for (int j = 0; j < sides; ++j)
        {
            tris.push_back({{grid[i][j],   grid[i+1][j],   grid[i+1][j+1]}, o.faceColor, o.edgeColor});
            tris.push_back({{grid[i][j],   grid[i+1][j+1], grid[i][j+1]},   o.faceColor, o.edgeColor});
        }
}

// ---------------------------------------------------------------------------
// Phase 5 tessellators
// ---------------------------------------------------------------------------

void tessHeightMap(const bgi::DdsHeightMap &o, std::vector<Triangle> &tris)
{
    if (o.rows < 2 || o.cols < 2 ||
        static_cast<int>(o.heights.size()) < o.rows * o.cols)
        return;

    auto vtx = [&](int row, int col) -> glm::vec3 {
        const float z = o.heights[row * o.cols + col];
        return o.origin + glm::vec3(col * o.cellWidth, row * o.cellHeight, z);
    };

    for (int row = 0; row < o.rows - 1; ++row)
        for (int col = 0; col < o.cols - 1; ++col)
        {
            const glm::vec3 v00 = vtx(row,   col);
            const glm::vec3 v01 = vtx(row,   col + 1);
            const glm::vec3 v10 = vtx(row+1, col);
            const glm::vec3 v11 = vtx(row+1, col + 1);
            tris.push_back({{v00, v10, v11}, o.faceColor, o.edgeColor});
            tris.push_back({{v00, v11, v01}, o.faceColor, o.edgeColor});
        }
}

/** Evaluate the selected parametric surface formula at (u, v) in [0,1]^2. */
glm::vec3 evalParam(bgi::ParamSurfaceFormula formula,
                    float param1, float param2,
                    float u, float v)
{
    switch (formula)
    {
    case bgi::ParamSurfaceFormula::Sphere:
    {
        const float phi   = v * kPi;
        const float theta = u * 2.f * kPi;
        return { param1 * std::sin(phi) * std::cos(theta),
                 param1 * std::sin(phi) * std::sin(theta),
                 param1 * std::cos(phi) };
    }
    case bgi::ParamSurfaceFormula::Cylinder:
    {
        const float theta = u * 2.f * kPi;
        return { param1 * std::cos(theta),
                 param1 * std::sin(theta),
                 v * param2 };
    }
    case bgi::ParamSurfaceFormula::Torus:
    {
        const float phi   = u * 2.f * kPi;
        const float theta = v * 2.f * kPi;
        return { (param1 + param2 * std::cos(theta)) * std::cos(phi),
                 (param1 + param2 * std::cos(theta)) * std::sin(phi),
                 param2 * std::sin(theta) };
    }
    case bgi::ParamSurfaceFormula::Saddle:
    {
        const float x = (u - 0.5f) * 2.f * param1;
        const float y = (v - 0.5f) * 2.f * param1;
        return { x, y, param2 * (x * x - y * y) };
    }
    case bgi::ParamSurfaceFormula::Mobius:
    {
        // v mapped from -param2 to +param2
        const float vMapped = (v - 0.5f) * 2.f * param2;
        const float uAngle  = u * kPi;      // half-turn for Möbius
        const float uFull   = u * 2.f * kPi;
        return { (param1 + vMapped * std::cos(uAngle)) * std::cos(uFull),
                 (param1 + vMapped * std::cos(uAngle)) * std::sin(uFull),
                 vMapped * std::sin(uAngle) };
    }
    default:
        return glm::vec3{0.f};
    }
}

void tessParamSurface(const bgi::DdsParamSurface &o,
                      const glm::vec3 &cen,
                      std::vector<Triangle> &tris)
{
    const int us = std::max(2, o.uSteps);
    const int vs = std::max(2, o.vSteps);

    // Build a (us+1) × (vs+1) grid.
    std::vector<std::vector<glm::vec3>> grid(vs + 1, std::vector<glm::vec3>(us + 1));
    for (int j = 0; j <= vs; ++j)
        for (int i = 0; i <= us; ++i)
            grid[j][i] = cen + evalParam(o.formula, o.param1, o.param2,
                                         static_cast<float>(i) / us,
                                         static_cast<float>(j) / vs);

    for (int j = 0; j < vs; ++j)
        for (int i = 0; i < us; ++i)
        {
            const glm::vec3 &v00 = grid[j][i],     &v01 = grid[j][i+1];
            const glm::vec3 &v10 = grid[j+1][i],   &v11 = grid[j+1][i+1];
            tris.push_back({{v00, v10, v11}, o.faceColor, o.edgeColor});
            tris.push_back({{v00, v11, v01}, o.faceColor, o.edgeColor});
        }
}

// ---------------------------------------------------------------------------
// Phase 6 tessellator — extrusion
// ---------------------------------------------------------------------------

void tessExtrusion(const bgi::DdsExtrusion &o, std::vector<Triangle> &tris)
{
    const int n = static_cast<int>(o.baseProfile.size());
    if (n < 3)
        return;

    // Build the top profile by offsetting each base vertex by extrudeDir.
    std::vector<glm::vec3> top(n);
    for (int i = 0; i < n; ++i)
        top[i] = o.baseProfile[i] + o.extrudeDir;

    // Side quads (each side quad becomes two triangles).
    for (int i = 0; i < n; ++i)
    {
        const int j = (i + 1) % n;
        tris.push_back({{o.baseProfile[i], o.baseProfile[j], top[j]},
                        o.faceColor, o.edgeColor});
        tris.push_back({{o.baseProfile[i], top[j], top[i]},
                        o.faceColor, o.edgeColor});
    }

    // Base cap (fan triangulation).
    if (o.capStart)
    {
        for (int i = 1; i < n - 1; ++i)
            tris.push_back({{o.baseProfile[0], o.baseProfile[i], o.baseProfile[i+1]},
                            o.faceColor, o.edgeColor});
    }

    // Top cap (reverse winding so normal faces outward).
    if (o.capEnd)
    {
        for (int i = 1; i < n - 1; ++i)
            tris.push_back({{top[0], top[i+1], top[i]},
                            o.faceColor, o.edgeColor});
    }
}

} // anonymous namespace

// =============================================================================
// bgi::renderSolid3D — the exported implementation
// =============================================================================

namespace bgi {

void renderSolid3D(const Camera3D &cam, const DdsObject &baseObj)
{
    std::vector<Triangle> tris;
    const DdsSolid3D &s = static_cast<const DdsSolid3D &>(baseObj);
    const glm::vec3 cen = resolveOrigin(s);
    const SolidDrawMode mode = s.drawMode;

    switch (baseObj.type)
    {
    case DdsObjectType::Box:
        tessBox(static_cast<const DdsBox &>(baseObj), cen, tris);
        break;
    case DdsObjectType::Sphere:
        tessSphere(static_cast<const DdsSphere &>(baseObj), cen, tris);
        break;
    case DdsObjectType::Cylinder:
        tessCylinder(static_cast<const DdsCylinder &>(baseObj), cen, tris);
        break;
    case DdsObjectType::Cone:
        tessCone(static_cast<const DdsCone &>(baseObj), cen, tris);
        break;
    case DdsObjectType::Torus:
        tessTorus(static_cast<const DdsTorus &>(baseObj), cen, tris);
        break;
    case DdsObjectType::HeightMap:
        tessHeightMap(static_cast<const DdsHeightMap &>(baseObj), tris);
        break;
    case DdsObjectType::ParamSurface:
        tessParamSurface(static_cast<const DdsParamSurface &>(baseObj), cen, tris);
        break;
    case DdsObjectType::Extrusion:
        tessExtrusion(static_cast<const DdsExtrusion &>(baseObj), tris);
        break;
    default:
        return;
    }

    renderTriangles(cam, tris, mode);
}

} // namespace bgi
