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
 * If a camera with @p name already exists it is replaced.
 * Newly created cameras have the following defaults:
 * - Orthographic projection; eye at (0,−10,5) looking at (0,0,0); Z-up.
 * - Auto-fit ortho extents (2 world-units tall).
 * - Screen viewport: full window.
 *
 * @param name Camera identifier string (UTF-8, must not be NULL).
 * @param type @ref WXBGI_CAM_ORTHO or @ref WXBGI_CAM_PERSPECTIVE.
 * @return 1 on success, 0 on invalid input, −1 if no window is open.
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
 *         −1 if the camera is not found or no window is open.
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
 * The initial view is centred on world origin (0,0), zoom = 1, no rotation,
 * and 720 world-units visible vertically.
 *
 * @param name Camera identifier string (must not be NULL).
 * @return 1 on success, 0 on invalid input, −1 if no window is open.
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
