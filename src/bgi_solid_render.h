#pragma once

#include "bgi_dds.h"
#include "bgi_types.h"

namespace bgi {

struct SolidTriangle
{
    glm::vec3 v[3];
    int       faceColor;
    int       edgeColor;
    glm::vec3 vn[3];
};

/**
 * Render a single Phase 4/5/6 solid/surface/extrusion DDS object through
 * the given camera into the active BGI pixel buffer.
 *
 * Called both from bgi_solid_api.cpp (immediate render) and from
 * bgi_dds_render.cpp (DDS re-render pass).
 *
 * Caller must already hold gMutex.
 */
void renderSolid3D(const Camera3D &cam, const DdsObject &obj);

/** Tessellate a solid/surface/extrusion DDS object into world-space triangles. */
bool tessellateSolid3D(const DdsObject &obj, std::vector<SolidTriangle> &tris);

/** Render a pre-tessellated triangle list using the current GL/software path. */
void renderSolidTriangles(const Camera3D &cam,
                          std::vector<SolidTriangle> &tris,
                          SolidDrawMode mode);

} // namespace bgi
