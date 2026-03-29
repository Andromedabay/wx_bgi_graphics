#pragma once

/**
 * @file wx_bgi_solid.h
 * @brief Public C API for Phase 4 (3D Solid Primitives), Phase 5 (3D Surfaces),
 *        and Phase 6 (2D→3D Extrusion) of the wx_bgi_graphics library.
 *
 * All functions write to the active BGI pixel buffer AND append a retained-mode
 * DDS object to the scene graph.  Call wxbgi_render_dds() to re-render the
 * scene through a different camera.
 *
 * Coordinate convention: Z-up, right-handed world space (same as the camera
 * and UCS systems defined in wx_bgi_3d.h).
 */

#include "bgi_types.h"
#include "wx_bgi_dds.h"  // WXBGI_SOLID_* and WXBGI_PARAM_* constants

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Global solid rendering defaults
// =============================================================================

/** Set the draw mode for subsequently created solids.
 *  @param mode  WXBGI_SOLID_WIREFRAME (0) or WXBGI_SOLID_SOLID (1). */
BGI_API void BGI_CALL wxbgi_solid_set_draw_mode(int mode);

/** Returns the current solid draw mode (WXBGI_SOLID_WIREFRAME or WXBGI_SOLID_SOLID). */
BGI_API int  BGI_CALL wxbgi_solid_get_draw_mode(void);

/** Set the edge colour (BGI palette index) for subsequently created solids. */
BGI_API void BGI_CALL wxbgi_solid_set_edge_color(int color);

/** Set the face fill colour (BGI palette index) for subsequently created solids. */
BGI_API void BGI_CALL wxbgi_solid_set_face_color(int color);

// =============================================================================
// Phase 4 — 3D solid primitives (world-space, Z-up convention)
// =============================================================================

/**
 * Draw an axis-aligned box centred at (cx, cy, cz).
 * @param width   Extent along the world X axis.
 * @param depth   Extent along the world Y axis.
 * @param height  Extent along the world Z axis.
 */
BGI_API void BGI_CALL wxbgi_solid_box(
    float cx, float cy, float cz,
    float width, float depth, float height);

/**
 * Draw a UV sphere centred at (cx, cy, cz).
 * @param radius  Sphere radius in world units.
 * @param slices  Longitude subdivisions (≥3).
 * @param stacks  Latitude subdivisions (≥2).
 */
BGI_API void BGI_CALL wxbgi_solid_sphere(
    float cx, float cy, float cz,
    float radius, int slices, int stacks);

/**
 * Draw a Z-aligned cylinder centred at (cx, cy, cz).
 * @param radius  Cylinder radius.
 * @param height  Total height along Z.
 * @param slices  Circumference subdivisions (≥3).
 * @param caps    Non-zero → draw top and bottom disc caps.
 */
BGI_API void BGI_CALL wxbgi_solid_cylinder(
    float cx, float cy, float cz,
    float radius, float height,
    int slices, int caps);

/**
 * Draw a Z-aligned cone.  Base is at (cx, cy, cz), apex at (cx, cy, cz+height).
 * @param radius  Base radius.
 * @param height  Height along Z.
 * @param slices  Circumference subdivisions (≥3).
 * @param cap     Non-zero → draw the base disc cap.
 */
BGI_API void BGI_CALL wxbgi_solid_cone(
    float cx, float cy, float cz,
    float radius, float height,
    int slices, int cap);

/**
 * Draw a torus (donut) centred at (cx, cy, cz) in the XY plane.
 * @param major_r  Distance from torus centre to tube centre.
 * @param minor_r  Tube radius.
 * @param rings    Segments around the main axis (≥3).
 * @param sides    Segments of the tube cross-section (≥3).
 */
BGI_API void BGI_CALL wxbgi_solid_torus(
    float cx, float cy, float cz,
    float major_r, float minor_r,
    int rings, int sides);

// =============================================================================
// Phase 5 — 3D surfaces
// =============================================================================

/**
 * Draw a height-map surface.
 * @param ox,oy,oz   World-space origin at the bottom-left corner of the grid.
 * @param cell_w     World-unit spacing between grid columns (X direction).
 * @param cell_h     World-unit spacing between grid rows    (Y direction).
 * @param rows       Number of rows in the grid (≥2).
 * @param cols       Number of columns in the grid (≥2).
 * @param heights    Row-major flat array of (rows × cols) Z values.
 */
BGI_API void BGI_CALL wxbgi_surface_heightmap(
    float ox, float oy, float oz,
    float cell_w, float cell_h,
    int rows, int cols,
    const float *heights);

/**
 * Draw a parametric surface.
 * @param cx,cy,cz   Centre offset applied to all surface points.
 * @param formula    WXBGI_PARAM_* constant selecting the surface formula.
 * @param param1     Primary shape parameter (e.g. radius for sphere/cylinder).
 * @param param2     Secondary shape parameter (e.g. minor radius, height).
 * @param u_steps    Tessellation steps along the U parameter axis (≥2).
 * @param v_steps    Tessellation steps along the V parameter axis (≥2).
 */
BGI_API void BGI_CALL wxbgi_surface_parametric(
    float cx, float cy, float cz,
    int formula,
    float param1, float param2,
    int u_steps, int v_steps);

// =============================================================================
// Phase 6 — 2D→3D extrusion
// =============================================================================

/**
 * Extrude a 2-D polygon profile along a direction vector.
 *
 * The base profile is given as separate X and Y arrays of @p n_pts world-space
 * points (Z is taken as zero for each vertex).  The extrusion direction
 * @p (dir_x, dir_y, dir_z) also determines the extrusion length via its
 * magnitude.
 *
 * @param xs,ys     Arrays of @p n_pts world-space X and Y coordinates.
 * @param n_pts     Number of profile vertices (≥3).
 * @param dir_x,dir_y,dir_z  Extrusion direction vector (length = depth).
 * @param cap_start Non-zero → fill the base polygon.
 * @param cap_end   Non-zero → fill the top polygon.
 */
BGI_API void BGI_CALL wxbgi_extrude_polygon(
    const float *xs, const float *ys, int n_pts,
    float dir_x, float dir_y, float dir_z,
    int cap_start, int cap_end);

#ifdef __cplusplus
} // extern "C"
#endif
