/**
 * @file bgi_solid_api.cpp
 * @brief C wrapper implementation for wx_bgi_solid.h.
 *
 * Each function:
 *  1. Acquires gMutex.
 *  2. Checks that the graphics subsystem is ready.
 *  3. Creates a concrete DdsSolid3D-derived object populated from gState solid
 *     defaults (solidDrawMode / solidEdgeColor / solidFaceColor).
 *  4. Appends it to the DDS scene graph.
 *  5. Renders it immediately through the active camera.
 *  6. Flushes the pixel buffer to the screen.
 */

#include "wx_bgi_solid.h"

#include <mutex>

#include <glm/glm.hpp>

#include "bgi_dds.h"
#include "bgi_draw.h"
#include "bgi_solid_render.h"
#include "bgi_state.h"

// =============================================================================
// Global solid rendering defaults
// =============================================================================

BGI_API void BGI_CALL wxbgi_solid_set_draw_mode(int mode)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.solidDrawMode = mode;
}

BGI_API int BGI_CALL wxbgi_solid_get_draw_mode(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    return bgi::gState.solidDrawMode;
}

BGI_API void BGI_CALL wxbgi_dds_set_solid_draw_mode(int mode)
{
    const auto dm = static_cast<bgi::SolidDrawMode>(mode);
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    for (const auto &id : bgi::gState.dds->order)
    {
        auto it = bgi::gState.dds->index.find(id);
        if (it == bgi::gState.dds->index.end() || it->second->deleted)
            continue;
        switch (it->second->type)
        {
        case bgi::DdsObjectType::Box:
        case bgi::DdsObjectType::Sphere:
        case bgi::DdsObjectType::Cylinder:
        case bgi::DdsObjectType::Cone:
        case bgi::DdsObjectType::Torus:
        case bgi::DdsObjectType::HeightMap:
        case bgi::DdsObjectType::ParamSurface:
        case bgi::DdsObjectType::Extrusion:
            static_cast<bgi::DdsSolid3D *>(it->second.get())->drawMode = dm;
            break;
        default:
            break;
        }
    }
}

BGI_API void BGI_CALL wxbgi_solid_set_edge_color(int color)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.solidEdgeColor = color;
}

BGI_API void BGI_CALL wxbgi_solid_set_face_color(int color)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.solidFaceColor = color;
}

// =============================================================================
// Internal helpers
// =============================================================================

namespace
{

/** Populate solid base fields from current gState defaults. */
void applyDefaults(bgi::DdsSolid3D &s)
{
    s.coordSpace = bgi::CoordSpace::World3D;
    s.drawMode   = static_cast<bgi::SolidDrawMode>(bgi::gState.solidDrawMode);
    s.edgeColor  = bgi::gState.solidEdgeColor;
    s.faceColor  = bgi::gState.solidFaceColor;
}

/** Render immediately through the active camera (if available). */
void renderAndFlush(const bgi::DdsObject &obj)
{
    auto camIt = bgi::gState.cameras.find(bgi::gState.activeCamera);
    if (camIt != bgi::gState.cameras.end())
        bgi::renderSolid3D(camIt->second->camera, obj);
    bgi::flushToScreen();
}

} // anonymous namespace

// =============================================================================
// Phase 4 — 3D solid primitives
// =============================================================================

BGI_API void BGI_CALL wxbgi_solid_box(float cx, float cy, float cz,
                                       float width, float depth, float height)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!bgi::isReady()) return;

    auto obj    = std::make_shared<bgi::DdsBox>();
    applyDefaults(*obj);
    obj->origin = {cx, cy, cz};
    obj->width  = width;
    obj->depth  = depth;
    obj->height = height;
    bgi::gState.dds->append(obj);
    renderAndFlush(*obj);
}

BGI_API void BGI_CALL wxbgi_solid_sphere(float cx, float cy, float cz,
                                          float radius, int slices, int stacks)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!bgi::isReady()) return;

    auto obj    = std::make_shared<bgi::DdsSphere>();
    applyDefaults(*obj);
    obj->origin = {cx, cy, cz};
    obj->radius = radius;
    obj->slices = slices;
    obj->stacks = stacks;
    bgi::gState.dds->append(obj);
    renderAndFlush(*obj);
}

BGI_API void BGI_CALL wxbgi_solid_cylinder(float cx, float cy, float cz,
                                            float radius, float height,
                                            int slices, int caps)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!bgi::isReady()) return;

    auto obj    = std::make_shared<bgi::DdsCylinder>();
    applyDefaults(*obj);
    obj->origin = {cx, cy, cz};
    obj->radius = radius;
    obj->height = height;
    obj->slices = slices;
    obj->caps   = caps;
    bgi::gState.dds->append(obj);
    renderAndFlush(*obj);
}

BGI_API void BGI_CALL wxbgi_solid_cone(float cx, float cy, float cz,
                                        float radius, float height,
                                        int slices, int cap)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!bgi::isReady()) return;

    auto obj    = std::make_shared<bgi::DdsCone>();
    applyDefaults(*obj);
    obj->origin = {cx, cy, cz};
    obj->radius = radius;
    obj->height = height;
    obj->slices = slices;
    obj->cap    = cap;
    bgi::gState.dds->append(obj);
    renderAndFlush(*obj);
}

BGI_API void BGI_CALL wxbgi_solid_torus(float cx, float cy, float cz,
                                         float major_r, float minor_r,
                                         int rings, int sides)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!bgi::isReady()) return;

    auto obj         = std::make_shared<bgi::DdsTorus>();
    applyDefaults(*obj);
    obj->origin      = {cx, cy, cz};
    obj->majorRadius = major_r;
    obj->minorRadius = minor_r;
    obj->slices      = rings;  // rings = slices
    obj->stacks      = sides;  // sides = stacks
    bgi::gState.dds->append(obj);
    renderAndFlush(*obj);
}

// =============================================================================
// Phase 5 — 3D surfaces
// =============================================================================

BGI_API void BGI_CALL wxbgi_surface_heightmap(float ox, float oy, float oz,
                                               float cell_w, float cell_h,
                                               int rows, int cols,
                                               const float *heights)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!bgi::isReady()) return;
    if (heights == nullptr || rows < 2 || cols < 2) return;

    auto obj        = std::make_shared<bgi::DdsHeightMap>();
    applyDefaults(*obj);
    obj->origin     = {ox, oy, oz};
    obj->rows       = rows;
    obj->cols       = cols;
    obj->cellWidth  = cell_w;
    obj->cellHeight = cell_h;
    obj->heights.assign(heights, heights + rows * cols);
    bgi::gState.dds->append(obj);
    renderAndFlush(*obj);
}

BGI_API void BGI_CALL wxbgi_surface_parametric(float cx, float cy, float cz,
                                                int formula,
                                                float param1, float param2,
                                                int u_steps, int v_steps)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!bgi::isReady()) return;

    auto obj     = std::make_shared<bgi::DdsParamSurface>();
    applyDefaults(*obj);
    obj->origin  = {cx, cy, cz};
    obj->formula = static_cast<bgi::ParamSurfaceFormula>(formula);
    obj->param1  = param1;
    obj->param2  = param2;
    obj->uSteps  = std::max(2, u_steps);
    obj->vSteps  = std::max(2, v_steps);
    bgi::gState.dds->append(obj);
    renderAndFlush(*obj);
}

// =============================================================================
// Phase 6 — 2D→3D extrusion
// =============================================================================

BGI_API void BGI_CALL wxbgi_extrude_polygon(const float *xs, const float *ys,
                                             int n_pts,
                                             float dir_x, float dir_y, float dir_z,
                                             int cap_start, int cap_end)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (!bgi::isReady()) return;
    if (xs == nullptr || ys == nullptr || n_pts < 3) return;

    auto obj          = std::make_shared<bgi::DdsExtrusion>();
    applyDefaults(*obj);
    obj->extrudeDir   = {dir_x, dir_y, dir_z};
    obj->capStart     = cap_start;
    obj->capEnd       = cap_end;
    obj->baseProfile.reserve(n_pts);
    for (int i = 0; i < n_pts; ++i)
        obj->baseProfile.emplace_back(xs[i], ys[i], 0.f);
    bgi::gState.dds->append(obj);
    renderAndFlush(*obj);
}
