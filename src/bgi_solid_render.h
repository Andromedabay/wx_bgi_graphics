#pragma once

#include "bgi_dds.h"
#include "bgi_types.h"

namespace bgi {

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

} // namespace bgi
