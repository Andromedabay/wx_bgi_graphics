#pragma once

/**
 * @file wx_bgi_dds.h
 * @brief Public C API for the Drawing Description Data Structure (DDS).
 *
 * The DDS is a retained-mode in-memory scene graph.  Every draw call in the
 * library both renders immediately to the pixel buffer AND appends a CHDOP
 * (Class Hierarchy of Drawing Object Primitives) object to the DDS.
 *
 * Cameras and UCS definitions are also first-class DDS objects; they are
 * serialized and restored as part of the scene.
 *
 * Usage pattern:
 * @code
 *   // Draw normally — DDS is populated automatically
 *   circle(400, 300, 80);
 *   wxbgi_world_line(0,0,0, 10,0,0);
 *
 *   // Inspect the scene
 *   int n = wxbgi_dds_object_count();
 *
 *   // Serialise
 *   const char *json = wxbgi_dds_to_json();
 *
 *   // Re-render from DDS through a different camera
 *   cleardevice();
 *   wxbgi_render_dds("cam_perspective");
 * @endcode
 *
 * Include this header in addition to @c wx_bgi.h and/or @c wx_bgi_3d.h.
 */

#include "bgi_types.h"  // BGI_API, BGI_CALL

// DDS object-type constants (returned by wxbgi_dds_get_type).
// Values match bgi::DdsObjectType; kept as plain ints for C compatibility.
#define WXBGI_DDS_CAMERA         0
#define WXBGI_DDS_UCS            1
#define WXBGI_DDS_WORLD_EXTENTS  2
#define WXBGI_DDS_POINT          3
#define WXBGI_DDS_LINE           4
#define WXBGI_DDS_CIRCLE         5
#define WXBGI_DDS_ARC            6
#define WXBGI_DDS_ELLIPSE        7
#define WXBGI_DDS_FILL_ELLIPSE   8
#define WXBGI_DDS_PIE_SLICE      9
#define WXBGI_DDS_SECTOR         10
#define WXBGI_DDS_RECTANGLE      11
#define WXBGI_DDS_BAR            12
#define WXBGI_DDS_BAR3D          13
#define WXBGI_DDS_POLYGON        14
#define WXBGI_DDS_FILL_POLY      15
#define WXBGI_DDS_TEXT           16
#define WXBGI_DDS_IMAGE          17
// Phase 4 — 3D Solid Primitives
#define WXBGI_DDS_BOX            18
#define WXBGI_DDS_SPHERE         19
#define WXBGI_DDS_CYLINDER       20
#define WXBGI_DDS_CONE           21
#define WXBGI_DDS_TORUS          22
// Phase 5 — 3D Surfaces
#define WXBGI_DDS_HEIGHTMAP      23
#define WXBGI_DDS_PARAM_SURFACE  24
// Phase 6 — 2D→3D Extrusion
#define WXBGI_DDS_EXTRUSION      25
#define WXBGI_DDS_TRANSFORM      26
#define WXBGI_DDS_SET_UNION      27
#define WXBGI_DDS_SET_INTERSECTION 28
#define WXBGI_DDS_SET_DIFFERENCE 29
#define WXBGI_DDS_UNKNOWN        -1

// Parametric surface formula constants
#define WXBGI_PARAM_SPHERE    0
#define WXBGI_PARAM_CYLINDER  1
#define WXBGI_PARAM_TORUS     2
#define WXBGI_PARAM_SADDLE    3
#define WXBGI_PARAM_MOBIUS    4

// Solid draw mode constants
#define WXBGI_SOLID_WIREFRAME  0
#define WXBGI_SOLID_SOLID      1   ///< Backward-compat alias for FLAT
#define WXBGI_SOLID_FLAT       1   ///< GL Phong flat shading + depth buffer
#define WXBGI_SOLID_SMOOTH     2   ///< GL Phong smooth (Gouraud) shading + depth buffer

// CoordSpace constants (returned by wxbgi_dds_get_coord_space).
#define WXBGI_COORD_BGI_PIXEL  0  ///< Classic BGI pixel coords stored as world (x, y, 0).
#define WXBGI_COORD_WORLD_3D   1  ///< World-space 3-D float coords.
#define WXBGI_COORD_UCS_LOCAL  2  ///< UCS-local float coords.

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Scene inspection and management
// =============================================================================

/**
 * @brief Returns the total number of non-deleted, visible + hidden objects in
 *        the DDS (cameras, UCS, drawing primitives).
 */
BGI_API int BGI_CALL wxbgi_dds_object_count(void);

/**
 * @brief Returns the auto-assigned ID string of the object at position @p index
 *        (0-based insertion order, excluding deleted entries).
 *
 * Returns a pointer to an internal thread-local buffer valid until the next
 * call from the same thread.  Returns "" if @p index is out of range.
 */
BGI_API const char *BGI_CALL wxbgi_dds_get_id_at(int index);

/**
 * @brief Returns the @c WXBGI_DDS_* type constant for the object identified by
 *        @p id, or @c WXBGI_DDS_UNKNOWN if not found.
 */
BGI_API int BGI_CALL wxbgi_dds_get_type(const char *id);

/**
 * @brief Returns the CoordSpace (@c WXBGI_COORD_*) of a drawing object.
 *        Returns -1 for config objects (Camera, Ucs, WorldExtents) or
 *        if not found.
 */
BGI_API int BGI_CALL wxbgi_dds_get_coord_space(const char *id);

/**
 * @brief Returns the user label of the object identified by @p id.
 *
 * Returns a pointer to an internal thread-local buffer valid until the next
 * call from the same thread.  Returns "" if the object has no label or is not
 * found.
 */
BGI_API const char *BGI_CALL wxbgi_dds_get_label(const char *id);

/**
 * @brief Sets the user label of the object identified by @p id.
 *
 * Labels are optional and non-unique; they are a convenience for
 * application-level bookkeeping.
 */
BGI_API void BGI_CALL wxbgi_dds_set_label(const char *id, const char *label);

/**
 * @brief Set one generic external metadata key/value pair on a DDS object.
 *
 * Passing NULL or empty @p value stores an empty string. Returns 1 on success,
 * 0 if @p id or @p key is invalid or the object does not exist.
 */
BGI_API int BGI_CALL wxbgi_dds_set_external_attr(const char *id, const char *key, const char *value);

/**
 * @brief Get one external metadata value from a DDS object.
 *
 * Returns a pointer to an internal thread-local buffer valid until the next
 * DDS string-returning API call from the same thread. Returns "" if the object
 * or key is not found.
 */
BGI_API const char *BGI_CALL wxbgi_dds_get_external_attr(const char *id, const char *key);

/**
 * @brief Remove one external metadata key from a DDS object.
 *
 * Returns 1 if the key existed and was removed, otherwise 0.
 */
BGI_API int BGI_CALL wxbgi_dds_clear_external_attr(const char *id, const char *key);

/**
 * @brief Return the number of external metadata entries stored on a DDS object.
 *
 * Returns 0 if the object is missing.
 */
BGI_API int BGI_CALL wxbgi_dds_external_attr_count(const char *id);

/**
 * @brief Return the key at @p index within a DDS object's external metadata map.
 *
 * Iteration order is unspecified. Returns "" if @p id is invalid or @p index is
 * out of range.
 */
BGI_API const char *BGI_CALL wxbgi_dds_get_external_attr_key_at(const char *id, int index);

/**
 * @brief Return the value at @p index within a DDS object's external metadata map.
 *
 * Iteration order is unspecified. Returns "" if @p id is invalid or @p index is
 * out of range.
 */
BGI_API const char *BGI_CALL wxbgi_dds_get_external_attr_value_at(const char *id, int index);

/**
 * @brief Returns the ID of the first non-deleted object with the given @p label,
 *        or "" if not found.
 *
 * Returns a pointer to an internal thread-local buffer.
 */
BGI_API const char *BGI_CALL wxbgi_dds_find_by_label(const char *label);

/**
 * @brief Sets the visibility flag of the object identified by @p id.
 *
 * @p visible  Non-zero = visible (default); 0 = hidden (not rendered by
 *             wxbgi_render_dds).
 */
BGI_API void BGI_CALL wxbgi_dds_set_visible(const char *id, int visible);

/**
 * @brief Returns 1 if the object exists and is visible, 0 otherwise.
 */
BGI_API int BGI_CALL wxbgi_dds_get_visible(const char *id);

/**
 * @brief Sets the face fill colour for an existing DDS object identified by @p id.
 *
 * Currently supported for 3D solid leaf objects and retained set-operation
 * nodes (`SetUnion`, `SetIntersection`, `SetDifference`). Returns 1 on
 * success, 0 if the object is missing or does not expose a face colour.
 */
BGI_API int BGI_CALL wxbgi_object_set_face_color(const char *id, int color);

/**
 * @brief Soft-deletes the object identified by @p id.
 *
 * The object is marked deleted and excluded from future traversals; its memory
 * is freed lazily on the next compact operation (automatic at clearDrawing /
 * clearAll).
 *
 * @return 1 on success, 0 if not found.
 */
BGI_API int BGI_CALL wxbgi_dds_remove(const char *id);

/**
 * @brief Removes all drawing-primitive objects from the DDS.
 *
 * Camera and UCS objects are retained.  The screen is NOT cleared;
 * call cleardevice() separately if required.
 */
BGI_API void BGI_CALL wxbgi_dds_clear(void);

/**
 * @brief Removes ALL objects from the DDS including cameras and UCS.
 *
 * Recreates the default pixel-space camera and the world identity UCS so the
 * library remains in a usable state.  The screen is NOT cleared.
 */
BGI_API void BGI_CALL wxbgi_dds_clear_all(void);

// =============================================================================
// Retained composition helpers
// =============================================================================

/**
 * @brief Creates a retained transform node that translates the object/subtree
 *        identified by @p id by (@p dx, @p dy, @p dz).
 *
 * The original object is left unchanged. The returned ID belongs to the new
 * Transform object stored in the active DDS scene.
 *
 * Returns a pointer to an internal thread-local buffer. Returns "" on error.
 */
BGI_API const char *BGI_CALL wxbgi_dds_translate(const char *id, float dx, float dy, float dz);

/**
 * @brief Creates a retained transform node that rotates @p id about the world X axis.
 *
 * The rotation is applied about the world origin. Compose with translate nodes
 * to rotate about another pivot.
 */
BGI_API const char *BGI_CALL wxbgi_dds_rotate_x_deg(const char *id, float angleDeg);
BGI_API const char *BGI_CALL wxbgi_dds_rotate_y_deg(const char *id, float angleDeg);
BGI_API const char *BGI_CALL wxbgi_dds_rotate_z_deg(const char *id, float angleDeg);
BGI_API const char *BGI_CALL wxbgi_dds_rotate_x_rad(const char *id, float angleRad);
BGI_API const char *BGI_CALL wxbgi_dds_rotate_y_rad(const char *id, float angleRad);
BGI_API const char *BGI_CALL wxbgi_dds_rotate_z_rad(const char *id, float angleRad);

/**
 * @brief Creates a retained transform node that rotates @p id around an arbitrary axis.
 *
 * The axis vector is normalized internally. If it is zero-length the call
 * returns an empty ID.
 */
BGI_API const char *BGI_CALL wxbgi_dds_rotate_axis_deg(const char *id,
                                                        float axisX, float axisY, float axisZ,
                                                        float angleDeg);
BGI_API const char *BGI_CALL wxbgi_dds_rotate_axis_rad(const char *id,
                                                        float axisX, float axisY, float axisZ,
                                                        float angleRad);

/**
 * @brief Creates a retained transform node that scales @p id by the given factor(s).
 *
 * Scaling is applied about the world origin. Use translate/scale/translate to
 * scale about another pivot.
 */
BGI_API const char *BGI_CALL wxbgi_dds_scale_uniform(const char *id, float factor);
BGI_API const char *BGI_CALL wxbgi_dds_scale_xyz(const char *id, float sx, float sy, float sz);

/**
 * @brief Creates a retained transform node with a 3D shear/skew matrix.
 *
 * The factors are applied as:
 *   x' = x + xy*y + xz*z
 *   y' = y + yx*x + yz*z
 *   z' = z + zx*x + zy*y
 */
BGI_API const char *BGI_CALL wxbgi_dds_skew(const char *id,
                                             float xy, float xz,
                                             float yx, float yz,
                                             float zx, float zy);

/**
 * @brief Creates a retained SetUnion node over @p count operand IDs.
 *
 * Operands are referenced by ID; they are not copied. Returns the new DDS ID,
 * or "" on error.
 */
BGI_API const char *BGI_CALL wxbgi_dds_union(int count, const char *const *ids);

/**
 * @brief Creates a retained SetIntersection node over @p count operand IDs.
 *
 * Operands are referenced by ID; they are not copied. Returns the new DDS ID,
 * or "" on error.
 */
BGI_API const char *BGI_CALL wxbgi_dds_intersection(int count, const char *const *ids);

/**
 * @brief Creates a retained SetDifference node over @p count operand IDs.
 *
 * Difference is ordered: the first operand is used as the base and each
 * remaining operand is subtracted from it in sequence. Operands are referenced
 * by ID; they are not copied. Returns the new DDS ID, or "" on error.
 */
BGI_API const char *BGI_CALL wxbgi_dds_difference(int count, const char *const *ids);

/**
 * @brief Returns the number of direct child IDs referenced by a retained
 *        Transform / SetUnion / SetIntersection / SetDifference object.
 *
 * Returns 0 for leaf objects and for invalid IDs.
 */
BGI_API int BGI_CALL wxbgi_dds_get_child_count(const char *id);

/**
 * @brief Returns the direct child ID at @p index for a retained Transform /
 *        SetUnion / SetIntersection / SetDifference object.
 *
 * Returns "" if @p id is invalid, the object is not a retained composition
 * node, or @p index is out of range.
 */
BGI_API const char *BGI_CALL wxbgi_dds_get_child_at(const char *id, int index);

// =============================================================================
// Serialization — JSON (DDJ) and YAML (DDY)
// =============================================================================

/**
 * @brief Serialises the full DDS scene to a JSON string (DDJ format).
 *
 * Returns a pointer to an internal thread-local buffer valid until the next
 * call to any wxbgi_dds_to_* function from the same thread.
 * The JSON is pretty-printed (2-space indent).
 */
BGI_API const char *BGI_CALL wxbgi_dds_to_json(void);

/**
 * @brief Serialises the full DDS scene to a YAML string (DDY format).
 *
 * Returns a pointer to an internal thread-local buffer valid until the next
 * call to any wxbgi_dds_to_* function from the same thread.
 */
BGI_API const char *BGI_CALL wxbgi_dds_to_yaml(void);

/**
 * @brief Replaces the current DDS scene with the data in @p jsonString.
 *
 * Cameras and UCS objects in the JSON are restored into @c BgiState::cameras
 * and @c BgiState::ucsSystems.  All existing scene data is discarded first.
 *
 * @return 0 on success, -1 on parse error (scene is left empty on error).
 */
BGI_API int BGI_CALL wxbgi_dds_from_json(const char *jsonString);

/**
 * @brief Replaces the current DDS scene with the data in @p yamlString.
 *
 * @return 0 on success, -1 on parse error.
 */
BGI_API int BGI_CALL wxbgi_dds_from_yaml(const char *yamlString);

/**
 * @brief Serialises the DDS to a JSON file at @p filePath.
 * @return 0 on success, -1 on I/O error.
 */
BGI_API int BGI_CALL wxbgi_dds_save_json(const char *filePath);

/**
 * @brief Loads and restores a DDS from a JSON file at @p filePath.
 * @return 0 on success, -1 on I/O or parse error.
 */
BGI_API int BGI_CALL wxbgi_dds_load_json(const char *filePath);

/**
 * @brief Serialises the DDS to a YAML file at @p filePath.
 * @return 0 on success, -1 on I/O error.
 */
BGI_API int BGI_CALL wxbgi_dds_save_yaml(const char *filePath);

/**
 * @brief Loads and restores a DDS from a YAML file at @p filePath.
 * @return 0 on success, -1 on I/O or parse error.
 */
BGI_API int BGI_CALL wxbgi_dds_load_yaml(const char *filePath);

// =============================================================================
// DDS-based rendering  (Phase D — placeholder declaration)
// =============================================================================

/**
 * @brief Renders all visible DDS drawing objects through the named camera into
 *        the BGI pixel buffer.
 *
 * ALL objects (BgiPixel, World3D, UcsLocal) are reprojected through the
 * specified camera:
 *  - @c BgiPixel coords are treated as world point (x, y, 0) and projected.
 *  - @c World3D coords are projected directly.
 *  - @c UcsLocal coords are transformed to world via their UCS, then projected.
 *
 * Pass @p camName = NULL to use the active camera.
 *
 * This function is fully implemented in Phase D.  In Phase A–C it is a no-op
 * (stub) so the header is stable from the start.
 */
BGI_API void BGI_CALL wxbgi_render_dds(const char *camName);

// =============================================================================
// GL rendering mode control
// =============================================================================

/**
 * @brief Enable or disable the legacy per-pixel GL_POINTS rendering path.
 *
 * When @p enable is non-zero, the page buffer is rendered using the old
 * GL_POINTS loop (one glVertex2i per non-background pixel).  When zero (default),
 * a much faster texture-based path is used.  The legacy path is provided only
 * for backward-compatibility diagnostics.
 */
BGI_API void BGI_CALL wxbgi_set_legacy_gl_render(int enable);

/**
 * @brief Release all OpenGL objects managed by the GL render pass.
 *
 * Must be called with the GL context current and before destroying that
 * context (e.g., from WxBgiCanvas destructor before deleting the wxGLContext).
 * Safe to call when no GL pass was ever initialised (no-op in that case).
 */
BGI_API void BGI_CALL wxbgi_gl_pass_destroy(void);

// =============================================================================
// GL Phong lighting API
// =============================================================================

/** @brief Set the primary (key) light direction.
 *  The vector is automatically normalised.  Default: (-0.577, 0.577, 0.577). */
BGI_API void BGI_CALL wxbgi_solid_set_light_dir(float x, float y, float z);

/** @brief Select light coordinate space.
 *  @param worldSpace  1 = world-space light (default), 0 = view-space light. */
BGI_API void BGI_CALL wxbgi_solid_set_light_space(int worldSpace);

/** @brief Set the secondary (fill) light direction and relative strength.
 *  Default direction: (0, -1, 0); default strength: 0.3. */
BGI_API void BGI_CALL wxbgi_solid_set_fill_light(float x, float y, float z, float strength);

/** @brief Set the ambient light intensity (0–1).  Default: 0.20. */
BGI_API void BGI_CALL wxbgi_solid_set_ambient(float a);

/** @brief Set the diffuse light intensity (0–1).  Default: 0.70. */
BGI_API void BGI_CALL wxbgi_solid_set_diffuse(float d);

/** @brief Set the specular intensity and shininess exponent.
 *  Defaults: specular = 0.30, shininess = 32. */
BGI_API void BGI_CALL wxbgi_solid_set_specular(float s, float shininess);

// =============================================================================
// Multi-Scene (CHDOP) Management
// =============================================================================
//
// The library maintains a registry of named DDS scene graphs. A "default"
// scene always exists and cannot be destroyed.  Immediate-mode draw calls
// (circle, box, wxbgi_world_line, etc.) always append to the *active* scene.
// Each camera renders its *assigned* scene (see wxbgi_cam_set_scene).
//
// Typical usage:
//   wxbgi_dds_scene_create("secondary");         // create a second scene
//   wxbgi_dds_scene_set_active("secondary");     // route draws to it
//   wxbgi_world_box(0,0,0, 5,5,5);              // appends to "secondary"
//   wxbgi_dds_scene_set_active("default");       // switch back
//   wxbgi_cam_set_scene("cam_side", "secondary");// cam_side renders "secondary"
//   wxbgi_render_dds("cam_main");               // renders "default" scene
//   wxbgi_render_dds("cam_side");               // renders "secondary" scene

/**
 * @brief Creates a new named DDS scene graph.
 * @param name  Non-empty name for the new scene.  Must not already exist.
 * @return 0 on success, -1 if the name is empty, already exists, or is "default".
 */
BGI_API int BGI_CALL wxbgi_dds_scene_create(const char *name);

/**
 * @brief Destroys a named DDS scene graph and all its objects.
 *
 * Any cameras assigned to the destroyed scene fall back to rendering the
 * "default" scene.  Attempting to destroy "default" is a no-op.
 * @param name  Name of the scene to destroy.
 */
BGI_API void BGI_CALL wxbgi_dds_scene_destroy(const char *name);

/**
 * @brief Sets the active scene for immediate-mode draw calls.
 *
 * All subsequent draw calls (circle, box, wxbgi_world_line, etc.) append
 * objects to this scene until changed again.  Passing NULL or an empty
 * string selects "default".
 * @param name  Name of an existing scene, or NULL/"" for "default".
 */
BGI_API void BGI_CALL wxbgi_dds_scene_set_active(const char *name);

/**
 * @brief Returns the name of the currently active scene.
 * @return Pointer to a static string; valid until the next call to this function.
 */
BGI_API const char *BGI_CALL wxbgi_dds_scene_get_active(void);

/**
 * @brief Tests whether a named scene exists in the registry.
 * @param name  Scene name to test.
 * @return 1 if the scene exists, 0 otherwise.
 */
BGI_API int BGI_CALL wxbgi_dds_scene_exists(const char *name);

/**
 * @brief Clears all drawing-primitive objects from a named scene.
 *
 * Camera and UCS objects in the scene are retained.
 * @param name  Scene name, or NULL/"" for the active scene.
 */
BGI_API void BGI_CALL wxbgi_dds_scene_clear(const char *name);

/**
 * @brief Assigns a DDS scene to a camera.
 *
 * After this call, wxbgi_render_dds(camName) will render the specified scene
 * through that camera.  If @p sceneName does not exist the call is a no-op.
 * @param camName    Camera name, or NULL/"" for the active camera.
 * @param sceneName  Name of the scene to assign.  NULL/"" selects "default".
 */
BGI_API void BGI_CALL wxbgi_cam_set_scene(const char *camName, const char *sceneName);

/**
 * @brief Returns the name of the scene currently assigned to a camera.
 * @param camName  Camera name, or NULL/"" for the active camera.
 * @return Scene name string, or NULL if the camera is not found.
 */
BGI_API const char *BGI_CALL wxbgi_cam_get_scene(const char *camName);

#ifdef __cplusplus
} // extern "C"
#endif
