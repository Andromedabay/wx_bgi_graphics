#pragma once

#include "bgi_types.h"

/**
 * @file wx_bgi_3d.h
 * @brief Camera viewport and 3-D world coordinate extension API.
 *
 * All functions are prefixed with @c wxbgi_cam_ (3-D camera) or
 * @c wxbgi_cam2d_ (2-D overhead camera convenience wrapper) to avoid
 * collisions with the classic BGI symbols in wx_bgi.h.
 *
 * **Coordinate system**: Z-up, right-handed (AutoCAD / engineering style).
 * The XY plane is the ground plane; +Z points upward.  For 2-D cameras the
 * ground plane is the canvas and the camera looks straight down from +Z.
 *
 * **Default camera**: A camera named @c "default" is created automatically by
 * @ref initwindow() / @ref initgraph().  It is a pixel-space orthographic
 * camera where world coordinate (0,0) maps to the top-left screen pixel and
 * world coordinate (windowWidth, windowHeight) maps to the bottom-right pixel,
 * exactly replicating the classic BGI coordinate system.  Classic BGI drawing
 * functions are unaffected by the camera system.
 *
 * **Thread safety**: All functions acquire the internal library mutex.
 */

/** @defgroup wxbgi_cam_api Camera Viewport API
 *  @{
 */

// =============================================================================
// Camera type constants (pass to wxbgi_cam_create)
// =============================================================================

/** Create an orthographic camera (parallel projection). */
#define WXBGI_CAM_ORTHO        0
/** Create a perspective camera (converging projection). */
#define WXBGI_CAM_PERSPECTIVE  1

// =============================================================================
// Camera lifecycle
// =============================================================================

/**
 * @brief Creates a named camera and adds it to the registry.
 *
 * Returns an error if a camera with the same name already exists. The
 * reserved name @c "default" (created automatically by initwindow()) is
 * always rejected.  Newly created cameras have the following defaults:
 * - Orthographic projection; eye at (0,-10,5) looking at (0,0,0); Z-up.
 * - Auto-fit ortho extents (2 world-units tall).
 * - Screen viewport: full window.
 *
 * @param name Camera identifier string (UTF-8, must not be NULL).
 * @param type @ref WXBGI_CAM_ORTHO or @ref WXBGI_CAM_PERSPECTIVE.
 * @return 1 on success, 0 on invalid input, -1 if no window is open,
 *         -2 if a camera with that name already exists
 *         (sets @c graphresult() to @c grDuplicateName).
 */
BGI_API int BGI_CALL wxbgi_cam_create(const char *name, int type);

/**
 * @brief Removes a named camera from the registry.
 *
 * The @c "default" camera cannot be destroyed.
 */
BGI_API void BGI_CALL wxbgi_cam_destroy(const char *name);

/**
 * @brief Sets the active camera used by @ref wxbgi_cam_world_to_screen and
 *        related helpers when @c NULL is passed as the camera name.
 */
BGI_API void BGI_CALL wxbgi_cam_set_active(const char *name);

/**
 * @brief Returns the name of the currently active camera.
 *
 * The returned pointer is valid until the next call to this function from the
 * same thread.
 */
BGI_API const char *BGI_CALL wxbgi_cam_get_active(void);

// =============================================================================
// 3-D camera: eye / target / up
// =============================================================================

/**
 * @brief Sets the eye (camera position) in world space.
 * @param name Camera name, or NULL for the active camera.
 */
BGI_API void BGI_CALL wxbgi_cam_set_eye(const char *name,
                                         float x, float y, float z);

/**
 * @brief Sets the look-at target in world space.
 * @param name Camera name, or NULL for the active camera.
 */
BGI_API void BGI_CALL wxbgi_cam_set_target(const char *name,
                                            float x, float y, float z);

/**
 * @brief Sets the up vector.  For Z-up worlds use (0, 0, 1).
 * @param name Camera name, or NULL for the active camera.
 */
BGI_API void BGI_CALL wxbgi_cam_set_up(const char *name,
                                        float x, float y, float z);

/** @brief Retrieves the current eye position. */
BGI_API void BGI_CALL wxbgi_cam_get_eye(const char *name,
                                         float *x, float *y, float *z);

/** @brief Retrieves the current look-at target. */
BGI_API void BGI_CALL wxbgi_cam_get_target(const char *name,
                                            float *x, float *y, float *z);

/** @brief Retrieves the current up vector. */
BGI_API void BGI_CALL wxbgi_cam_get_up(const char *name,
                                        float *x, float *y, float *z);

// =============================================================================
// Projection
// =============================================================================

/**
 * @brief Switches a camera to perspective projection.
 *
 * @param name      Camera name, or NULL for the active camera.
 * @param fovYDeg   Vertical field-of-view in degrees.
 * @param nearPlane Near clip distance (positive, e.g. 0.1).
 * @param farPlane  Far clip distance (e.g. 10000).
 */
BGI_API void BGI_CALL wxbgi_cam_set_perspective(const char *name,
                                                 float fovYDeg,
                                                 float nearPlane,
                                                 float farPlane);

/**
 * @brief Sets explicit orthographic clip extents.
 *
 * @param name   Camera name, or NULL for the active camera.
 * @param left   Left world-unit boundary.
 * @param right  Right world-unit boundary.
 * @param bottom Bottom world-unit boundary (use > @p top for Y-flip).
 * @param top    Top world-unit boundary.
 * @param nearPlane Near clip distance (may be negative for 2-D use).
 * @param farPlane  Far clip distance.
 */
BGI_API void BGI_CALL wxbgi_cam_set_ortho(const char *name,
                                           float left, float right,
                                           float bottom, float top,
                                           float nearPlane, float farPlane);

/**
 * @brief Sets orthographic projection with automatic aspect-correct extents.
 *
 * The visible width is computed automatically from the camera's screen
 * viewport aspect ratio so that @p worldUnitsHigh world-units are always
 * visible vertically.
 *
 * @param name          Camera name, or NULL for the active camera.
 * @param worldUnitsHigh Visible height in world units.
 * @param nearPlane      Near clip distance.
 * @param farPlane       Far clip distance.
 */
BGI_API void BGI_CALL wxbgi_cam_set_ortho_auto(const char *name,
                                                float worldUnitsHigh,
                                                float nearPlane,
                                                float farPlane);

// =============================================================================
// Screen viewport mapping
// =============================================================================

/**
 * @brief Restricts the camera's output to a sub-rectangle of the window.
 *
 * Pass all zeros to use the full window (default).
 *
 * @param name  Camera name, or NULL for the active camera.
 * @param x,y   Top-left corner of the viewport in pixels.
 * @param w,h   Viewport width and height in pixels.
 */
BGI_API void BGI_CALL wxbgi_cam_set_screen_viewport(const char *name,
                                                     int x, int y,
                                                     int w, int h);

// =============================================================================
// Matrix access (column-major float[16], same layout as OpenGL)
// =============================================================================

/**
 * @brief Writes the view matrix into @p out16 (column-major float[16]).
 * @param name Camera name, or NULL for the active camera.
 */
BGI_API void BGI_CALL wxbgi_cam_get_view_matrix(const char *name, float *out16);

/**
 * @brief Writes the projection matrix into @p out16 (column-major float[16]).
 * @param name Camera name, or NULL for the active camera.
 */
BGI_API void BGI_CALL wxbgi_cam_get_proj_matrix(const char *name, float *out16);

/**
 * @brief Writes the combined view-projection matrix into @p out16.
 * @param name Camera name, or NULL for the active camera.
 */
BGI_API void BGI_CALL wxbgi_cam_get_vp_matrix(const char *name, float *out16);

// =============================================================================
// Coordinate utilities
// =============================================================================

/**
 * @brief Projects a world-space point to a screen pixel coordinate.
 *
 * Screen coordinates follow the classic BGI convention: (0,0) is the top-left
 * corner of the window and Y increases downward.
 *
 * @param name           Camera name, or NULL for the active camera.
 * @param wx, wy, wz     World-space point.
 * @param screenX        Output screen X pixel (may be NULL).
 * @param screenY        Output screen Y pixel (may be NULL).
 * @return  1 if the point is visible (inside the view frustum),
 *          0 if clipped,
 *         -1 if the camera is not found or no window is open.
 */
BGI_API int BGI_CALL wxbgi_cam_world_to_screen(const char *name,
                                                float wx, float wy, float wz,
                                                float *screenX, float *screenY);

/**
 * @brief Unprojects a screen pixel to a world-space ray.
 *
 * The ray originates on the near clip plane.  The direction vector is
 * normalised.  Screen coordinates follow the BGI top-left / Y-down convention.
 *
 * @param name               Camera name, or NULL for the active camera.
 * @param screenX, screenY   Input screen pixel.
 * @param rayOx,rayOy,rayOz  Output ray origin (world space, may be NULL).
 * @param rayDx,rayDy,rayDz  Output normalised ray direction (may be NULL).
 */
BGI_API void BGI_CALL wxbgi_cam_screen_to_ray(const char *name,
                                               float screenX, float screenY,
                                               float *rayOx, float *rayOy, float *rayOz,
                                               float *rayDx, float *rayDy, float *rayDz);

/** @} */

/** @defgroup wxbgi_cam2d_api 2-D Camera Convenience API
 *
 *  A 2-D camera is an orthographic camera that looks straight down at the XY
 *  ground plane (Z = 0) in the Z-up world.  Pan, zoom, and rotation are
 *  managed through dedicated functions that keep the view always centred and
 *  aspect-correct.
 *
 *  Internally a 2-D camera is stored as a @c Camera3D with @c is2D = @c true;
 *  it participates in the same registry and all @c wxbgi_cam_* functions work
 *  on it as well.
 *  @{
 */

/**
 * @brief Creates a 2-D overhead camera.
 *
 * Returns an error if a camera with the same name already exists. The
 * reserved name @c "default" is always rejected.
 * The initial view is centred on world origin (0,0), zoom = 1, no rotation,
 * and the window height in world-units visible vertically.
 *
 * @param name Camera identifier string (must not be NULL).
 * @return 1 on success, 0 on invalid input, -1 if no window is open,
 *         -2 if a camera with that name already exists
 *         (sets @c graphresult() to @c grDuplicateName).
 */
BGI_API int BGI_CALL wxbgi_cam2d_create(const char *name);

/**
 * @brief Sets the 2-D camera pan (world-space centre of the view).
 * @param name Camera name, or NULL for the active camera.
 */
BGI_API void BGI_CALL wxbgi_cam2d_set_pan(const char *name, float x, float y);

/**
 * @brief Pans the 2-D camera by a world-space delta.
 * @param name Camera name, or NULL for the active camera.
 */
BGI_API void BGI_CALL wxbgi_cam2d_pan_by(const char *name, float dx, float dy);

/**
 * @brief Sets the zoom level (1 = nominal, 2 = 2× magnification).
 *
 * The zoom is clamped to a minimum of 1e-6 to prevent division by zero.
 * @param name Camera name, or NULL for the active camera.
 */
BGI_API void BGI_CALL wxbgi_cam2d_set_zoom(const char *name, float zoom);

/**
 * @brief Zooms around a pivot point in world space.
 *
 * The world point under @p pivotX / @p pivotY remains stationary on screen
 * while the zoom level is multiplied by @p factor.
 *
 * @param name   Camera name, or NULL for the active camera.
 * @param factor Zoom multiplier (e.g. 1.1 to zoom in 10 %).
 * @param pivotX World X of the zoom pivot.
 * @param pivotY World Y of the zoom pivot.
 */
BGI_API void BGI_CALL wxbgi_cam2d_zoom_at(const char *name,
                                           float factor,
                                           float pivotX, float pivotY);

/**
 * @brief Sets the 2-D view rotation around the Z axis.
 * @param name      Camera name, or NULL for the active camera.
 * @param angleDeg  Rotation in degrees (positive = counter-clockwise).
 */
BGI_API void BGI_CALL wxbgi_cam2d_set_rotation(const char *name, float angleDeg);

/** @brief Retrieves the current pan position. */
BGI_API void BGI_CALL wxbgi_cam2d_get_pan(const char *name,
                                           float *x, float *y);

/** @brief Retrieves the current zoom level. */
BGI_API void BGI_CALL wxbgi_cam2d_get_zoom(const char *name, float *zoom);

/** @brief Retrieves the current rotation in degrees. */
BGI_API void BGI_CALL wxbgi_cam2d_get_rotation(const char *name, float *angleDeg);

/**
 * @brief Sets the number of world-units visible vertically at zoom = 1.
 *
 * Changing this value rescales the entire coordinate space without altering
 * the zoom or pan.  Default is 720 world-units (matching a 720-pixel-tall
 * window at 1:1 scale).
 *
 * @param name          Camera name, or NULL for the active camera.
 * @param worldUnitsHigh Visible world-unit height at zoom = 1.
 */
BGI_API void BGI_CALL wxbgi_cam2d_set_world_height(const char *name,
                                                    float worldUnitsHigh);

/** @} */

// =============================================================================
// Phase 2 — User Coordinate System (UCS) API
// =============================================================================

/** @defgroup wxbgi_ucs_api User Coordinate System (UCS) API
 *
 *  A UCS defines a named local coordinate frame inside the Z-up, right-handed
 *  world space.  It consists of an origin point and three orthonormal axes
 *  (@e X, @e Y, @e Z) expressed as world-space direction vectors.
 *
 *  A UCS named @c "world" (the identity frame) is created automatically on
 *  @ref initwindow() / @ref initgraph() and cannot be destroyed.
 *
 *  All functions accept @c NULL as @p name to target the currently active UCS.
 *  @{
 */

/**
 * @brief Creates a named UCS initialised to the world identity frame.
 *
 * Returns an error if a UCS with the same name already exists. The
 * reserved name @c "world" (created automatically by initwindow()) is
 * always rejected.
 *
 * @param name  UCS identifier string (must not be NULL or empty).
 * @return 1 on success, 0 on invalid input, -1 if no window is open,
 *         -2 if a UCS with that name already exists
 *         (sets @c graphresult() to @c grDuplicateName).
 */
BGI_API int BGI_CALL wxbgi_ucs_create(const char *name);

/**
 * @brief Removes a named UCS from the registry.
 *
 * The @c "world" UCS cannot be destroyed.
 */
BGI_API void BGI_CALL wxbgi_ucs_destroy(const char *name);

/**
 * @brief Sets the active UCS used when @c NULL is passed as the UCS name.
 */
BGI_API void BGI_CALL wxbgi_ucs_set_active(const char *name);

/**
 * @brief Returns the name of the currently active UCS.
 *
 * The returned pointer is valid until the next call to this function from the
 * same thread.
 */
BGI_API const char *BGI_CALL wxbgi_ucs_get_active(void);

/**
 * @brief Sets the origin of a UCS in world space.
 * @param name   UCS name, or NULL for the active UCS.
 */
BGI_API void BGI_CALL wxbgi_ucs_set_origin(const char *name,
                                            float ox, float oy, float oz);

/** @brief Retrieves the current UCS origin. */
BGI_API void BGI_CALL wxbgi_ucs_get_origin(const char *name,
                                            float *ox, float *oy, float *oz);

/**
 * @brief Sets all three axes of a UCS.
 *
 * The vectors are orthonormalised automatically (Gram-Schmidt), so it is
 * sufficient to supply two perpendicular vectors and let the library compute
 * the third.  The Z axis is recomputed as @p xAxis × @p yAxis.
 *
 * @param name  UCS name, or NULL for the active UCS.
 * @param xx,xy,xz  World-space direction of the local X axis.
 * @param yx,yy,yz  World-space direction of the local Y axis.
 * @param zx,zy,zz  World-space direction of the local Z axis
 *                  (overridden by the cross product after normalisation).
 */
BGI_API void BGI_CALL wxbgi_ucs_set_axes(const char *name,
                                          float xx, float xy, float xz,
                                          float yx, float yy, float yz,
                                          float zx, float zy, float zz);

/**
 * @brief Retrieves all three axes of a UCS as world-space unit vectors.
 *
 * Any of the output pointers may be NULL to skip that axis.
 */
BGI_API void BGI_CALL wxbgi_ucs_get_axes(const char *name,
                                          float *xx, float *xy, float *xz,
                                          float *yx, float *yy, float *yz,
                                          float *zx, float *zy, float *zz);

/**
 * @brief Aligns a UCS so that its local Z axis matches a given surface normal.
 *
 * Equivalent to calling @ref wxbgi_ucs_set_origin followed by an axis
 * assignment where Z = @p normal.  The X and Y axes are chosen automatically
 * to form a complete right-handed frame.
 *
 * @param name      UCS name, or NULL for the active UCS.
 * @param nx,ny,nz  Surface normal (need not be unit length).
 * @param ox,oy,oz  New UCS origin in world space.
 */
BGI_API void BGI_CALL wxbgi_ucs_align_to_plane(const char *name,
                                                float nx, float ny, float nz,
                                                float ox, float oy, float oz);

/**
 * @brief Writes the local-to-world matrix into @p out16 (column-major float[16]).
 * @param name UCS name, or NULL for the active UCS.
 */
BGI_API void BGI_CALL wxbgi_ucs_get_local_to_world_matrix(const char *name,
                                                           float *out16);

/**
 * @brief Writes the world-to-local matrix into @p out16 (column-major float[16]).
 * @param name UCS name, or NULL for the active UCS.
 */
BGI_API void BGI_CALL wxbgi_ucs_get_world_to_local_matrix(const char *name,
                                                           float *out16);

/**
 * @brief Transforms a world-space point into UCS local coordinates.
 *
 * @param name      UCS name, or NULL for the active UCS.
 * @param wx,wy,wz  World-space input point.
 * @param lx,ly,lz  Output UCS local coordinates (any may be NULL).
 */
BGI_API void BGI_CALL wxbgi_ucs_world_to_local(const char *name,
                                                float wx, float wy, float wz,
                                                float *lx, float *ly, float *lz);

/**
 * @brief Transforms a UCS local point into world-space coordinates.
 *
 * @param name      UCS name, or NULL for the active UCS.
 * @param lx,ly,lz  UCS local input point.
 * @param wx,wy,wz  Output world-space coordinates (any may be NULL).
 */
BGI_API void BGI_CALL wxbgi_ucs_local_to_world(const char *name,
                                                float lx, float ly, float lz,
                                                float *wx, float *wy, float *wz);

/** @} */

// =============================================================================
// Phase 2 — World Extents API
// =============================================================================

/** @defgroup wxbgi_world_api World Extents API
 *
 *  The world extents define an axis-aligned bounding box (AABB) in world space
 *  that represents the programmer's declared drawing area.  This is used by
 *  @ref wxbgi_cam_fit_to_extents() and can be queried at any time.
 *
 *  The extents start in an "empty" state (no data) and grow as points are added.
 *  @{
 */

/**
 * @brief Sets the world extents to an explicit AABB.
 *
 * Replaces any previously set extents.  After this call @c hasData is @c true.
 */
BGI_API void BGI_CALL wxbgi_set_world_extents(float minX, float minY, float minZ,
                                               float maxX, float maxY, float maxZ);

/**
 * @brief Retrieves the current world extents.
 *
 * Any output pointer may be NULL.  If no extents have been set, all values
 * are 0.  Check the return value: 1 = extents set, 0 = no data yet.
 */
BGI_API int BGI_CALL wxbgi_get_world_extents(float *minX, float *minY, float *minZ,
                                              float *maxX, float *maxY, float *maxZ);

/**
 * @brief Expands the world extents to include the point (@p x, @p y, @p z).
 *
 * If no extents have been set the point becomes the initial AABB.
 */
BGI_API void BGI_CALL wxbgi_expand_world_extents(float x, float y, float z);

/**
 * @brief Resets the world extents to the "empty / no data" state.
 */
BGI_API void BGI_CALL wxbgi_reset_world_extents(void);

/**
 * @brief Adjusts a camera's projection and position to frame the world extents.
 *
 * **Orthographic cameras** (including 2-D cameras): the ortho extents are
 * set so the full AABB is visible with 5 % padding on each side.  For 2-D
 * cameras the pan is centred on the AABB and the zoom is adjusted.
 *
 * **Perspective cameras**: the eye is moved along the current view direction
 * (keeping the existing target) to a distance where the AABB's bounding
 * sphere fits within the field of view.
 *
 * Does nothing if no world extents have been set.
 *
 * @param camName Camera name, or NULL for the active camera.
 * @return 1 on success, 0 if no extents are set, -1 if camera not found.
 */
BGI_API int BGI_CALL wxbgi_cam_fit_to_extents(const char *camName);

// =============================================================================
// Phase 3: UCS-to-screen projection + world-coordinate drawing wrappers
// =============================================================================

/**
 * @brief Projects a UCS-local point through the named UCS and camera to
 *        screen-pixel coordinates.
 *
 * Full pipeline: UCS-local -> world -> view -> NDC -> screen pixel.
 *
 * @param ucsName   UCS name, or NULL for the active UCS.
 * @param camName   Camera name, or NULL for the active camera.
 * @param ux,uy,uz  UCS-local input point.
 * @param screenX   Output screen X pixel (top-left origin, Y increasing down).
 * @param screenY   Output screen Y pixel.
 * @return          1 if the point is visible, 0 if clipped, -1 if not found.
 */
BGI_API int BGI_CALL wxbgi_ucs_to_screen(const char *ucsName, const char *camName,
                                          float ux, float uy, float uz,
                                          float *screenX, float *screenY);

// --- World-space drawing (uses the active camera) ----------------------------

/** @brief Draws a single pixel at a world-space position.
 *  @param x,y,z  World-space position.  @param color  BGI colour index. */
BGI_API void BGI_CALL wxbgi_world_point(float x, float y, float z, int color);

/** @brief Draws a line segment between two world-space points using the current colour/style. */
BGI_API void BGI_CALL wxbgi_world_line(float x1, float y1, float z1,
                                        float x2, float y2, float z2);

/** @brief Draws a circle (world-space centre, world-unit radius) via the active camera.
 *  The screen radius is estimated from a radial offset along world-X. */
BGI_API void BGI_CALL wxbgi_world_circle(float cx, float cy, float cz, float radius);

/** @brief Draws an ellipse arc (world-space centre, world-unit semi-axes rx/ry).
 *  @param startAngle  Arc start in degrees (0=right, CCW).  @param endAngle  Arc end. */
BGI_API void BGI_CALL wxbgi_world_ellipse(float cx, float cy, float cz,
                                           float rx, float ry,
                                           int startAngle, int endAngle);

/** @brief Draws a screen-space rectangle outline from two projected world-space corners. */
BGI_API void BGI_CALL wxbgi_world_rectangle(float x1, float y1, float z1,
                                             float x2, float y2, float z2);

/** @brief Draws an open polyline through world-space points.
 *  @param xyzTriplets  Flat (x,y,z) array; length = pointCount*3. */
BGI_API void BGI_CALL wxbgi_world_polyline(const float *xyzTriplets, int pointCount);

/** @brief Draws and fills a closed polygon through world-space points.
 *  Points behind the camera are dropped; needs >= 3 visible vertices. */
BGI_API void BGI_CALL wxbgi_world_fillpoly(const float *xyzTriplets, int pointCount);

/** @brief Draws text at a world-space position using current BGI font/colour settings. */
BGI_API void BGI_CALL wxbgi_world_outtextxy(float x, float y, float z, const char *text);

// --- UCS-local drawing (transform through named UCS, then project) -----------

/** @brief Draws a pixel at a UCS-local position.  NULL ucsName = active UCS. */
BGI_API void BGI_CALL wxbgi_ucs_point(const char *ucsName,
                                       float ux, float uy, float uz, int color);

/** @brief Draws a line between two UCS-local points.  NULL ucsName = active UCS. */
BGI_API void BGI_CALL wxbgi_ucs_line(const char *ucsName,
                                      float ux1, float uy1, float uz1,
                                      float ux2, float uy2, float uz2);

/** @brief Draws a circle at a UCS-local centre with a UCS-unit radius.  NULL ucsName = active UCS. */
BGI_API void BGI_CALL wxbgi_ucs_circle(const char *ucsName,
                                        float cx, float cy, float cz, float radius);

/** @brief Draws an open polyline through UCS-local points.  NULL ucsName = active UCS.
 *  @param xyzTriplets  Flat (x,y,z) array in UCS local space. */
BGI_API void BGI_CALL wxbgi_ucs_polyline(const char *ucsName,
                                          const float *xyzTriplets, int pointCount);

/** @brief Draws text at a UCS-local anchor.  NULL ucsName = active UCS. */
BGI_API void BGI_CALL wxbgi_ucs_outtextxy(const char *ucsName,
                                           float ux, float uy, float uz,
                                           const char *text);

/** @} */
