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
#define WXBGI_DDS_UNKNOWN        -1

// Parametric surface formula constants
#define WXBGI_PARAM_SPHERE    0
#define WXBGI_PARAM_CYLINDER  1
#define WXBGI_PARAM_TORUS     2
#define WXBGI_PARAM_SADDLE    3
#define WXBGI_PARAM_MOBIUS    4

// Solid draw mode constants
#define WXBGI_SOLID_WIREFRAME  0
#define WXBGI_SOLID_SOLID      1

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

#ifdef __cplusplus
} // extern "C"
#endif
