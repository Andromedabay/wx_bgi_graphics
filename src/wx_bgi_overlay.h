#pragma once

#include "bgi_types.h"

/**
 * @file wx_bgi_overlay.h
 * @brief Visual Aids Overlay API for wx_bgi_graphics.
 *
 * Visual overlays are non-DDS rendering aids drawn fresh every frame.  They
 * are not stored in the scene graph, cannot be selected or serialised, and
 * default to **disabled** so that existing programs see no change.
 *
 * All overlays are drawn automatically as part of @ref wxbgi_render_dds(),
 * so no extra call is needed — just enable the desired overlay and call
 * @c wxbgi_render_dds() as usual.
 *
 * ---
 *
 * ## Overlay types
 *
 * ### 1 — Reference Grid
 * Drawn in the XY plane of the active (or a named) UCS, projected in 3-D
 * through every camera viewport.  Visible simultaneously in all panels when
 * enabled.  BGI viewport clipping applies.
 *
 * ### 2 — UCS Axes
 * Labelled X/Y/Z axis arms drawn at the world UCS origin and/or the active
 * UCS origin, projected through every camera viewport.
 *
 * ### 3 — Concentric Circles + Crosshair  (per-camera)
 * World-space concentric circles centred at the camera's pan/target centre.
 * Because the radii are in world units, zooming in spreads the circles apart
 * giving a real sense of zoom scale.  The crosshair rotates with the camera
 * for 2-D cameras; it is fixed for 3-D cameras.
 *
 * ### 4 — Selection Cursor Square  (per-camera)
 * A small square (2-16 px) that follows the mouse within the camera's
 * viewport.  The border colour alternates every 2 seconds between the light
 * and dark shade of the chosen colour scheme (blue / green / red).
 * Drawn in the OpenGL pass on top of all scene content.
 *
 * @note Thread safety: all functions acquire the internal library mutex.
 */

/** @defgroup wxbgi_overlay_api Visual Overlay API
 *  @{
 */

// =============================================================================
// Reference Grid
// =============================================================================

/**
 * @brief Enable the reference grid overlay.
 *
 * The grid is drawn in the XY plane of the active UCS (or the UCS set by
 * @ref wxbgi_overlay_grid_set_ucs()) and is visible in every camera viewport.
 */
BGI_API void BGI_CALL wxbgi_overlay_grid_enable(void);

/** @brief Disable the reference grid overlay. */
BGI_API void BGI_CALL wxbgi_overlay_grid_disable(void);

/** @brief Return 1 if the grid overlay is enabled, 0 otherwise. */
BGI_API int  BGI_CALL wxbgi_overlay_grid_is_enabled(void);

/**
 * @brief Set the spacing between adjacent grid lines.
 *
 * @param worldUnits  Distance between lines in world units (must be > 0).
 *                    Default: 25.
 */
BGI_API void BGI_CALL wxbgi_overlay_grid_set_spacing(float worldUnits);

/**
 * @brief Set the grid extent.
 *
 * The grid spans from @c -(halfExtentInLines * spacing) to
 * @c +(halfExtentInLines * spacing) from the UCS origin in both UCS X and Y.
 *
 * @param halfExtentInLines  Number of lines on each side of the origin
 *                           (must be >= 1).  Default: 4.
 */
BGI_API void BGI_CALL wxbgi_overlay_grid_set_extent(int halfExtentInLines);

/**
 * @brief Set the three colours used by the grid overlay.
 *
 * @param xAxisColor  Colour for the line along UCS local X (default: RED).
 * @param yAxisColor  Colour for the line along UCS local Y (default: GREEN).
 * @param gridColor   Colour for all other grid lines (default: DARKGRAY).
 */
BGI_API void BGI_CALL wxbgi_overlay_grid_set_colors(int xAxisColor,
                                                     int yAxisColor,
                                                     int gridColor);

/**
 * @brief Bind the grid to a specific UCS.
 *
 * @param ucsName  Name of the UCS whose XY plane the grid follows.
 *                 Pass @c NULL or @c "" to use the active UCS (default).
 */
BGI_API void BGI_CALL wxbgi_overlay_grid_set_ucs(const char *ucsName);

// =============================================================================
// UCS Axes
// =============================================================================

/**
 * @brief Enable the UCS axes overlay.
 *
 * Draws labelled X (RED), Y (GREEN), Z (BLUE) axis arms at the world origin
 * and/or the active UCS origin in every camera viewport.
 */
BGI_API void BGI_CALL wxbgi_overlay_ucs_axes_enable(void);

/** @brief Disable the UCS axes overlay. */
BGI_API void BGI_CALL wxbgi_overlay_ucs_axes_disable(void);

/** @brief Return 1 if the UCS axes overlay is enabled, 0 otherwise. */
BGI_API int  BGI_CALL wxbgi_overlay_ucs_axes_is_enabled(void);

/**
 * @brief Show or hide the world UCS axes (at the world origin).
 *
 * @param show  Non-zero to show, 0 to hide.  Default: shown.
 */
BGI_API void BGI_CALL wxbgi_overlay_ucs_axes_show_world(int show);

/**
 * @brief Show or hide the active UCS axes (at the active UCS origin).
 *
 * @param show  Non-zero to show, 0 to hide.  Default: shown.
 */
BGI_API void BGI_CALL wxbgi_overlay_ucs_axes_show_active(int show);

/**
 * @brief Set the length of each axis arm.
 *
 * @param worldUnits  Length from the UCS origin to the axis tip (must be > 0).
 *                    Default: 80.
 */
BGI_API void BGI_CALL wxbgi_overlay_ucs_axes_set_length(float worldUnits);

// =============================================================================
// Concentric Circles + Crosshair  (per-camera)
// =============================================================================

/**
 * @brief Enable the concentric circles overlay for a camera.
 *
 * World-space circles are drawn centred at the camera's pan centre (2-D
 * cameras) or target point (3-D cameras).  Because the radii are in world
 * units, the pixel spacing grows/shrinks as you zoom in/out.  A crosshair
 * through the centre rotates with the camera for 2-D cameras.
 *
 * @param camName  Camera name; @c NULL or @c "" = active camera.
 */
BGI_API void BGI_CALL wxbgi_overlay_concentric_enable(const char *camName);

/**
 * @brief Disable the concentric circles overlay for a camera.
 *
 * @param camName  Camera name; @c NULL or @c "" = active camera.
 */
BGI_API void BGI_CALL wxbgi_overlay_concentric_disable(const char *camName);

/**
 * @brief Return 1 if the concentric circles overlay is enabled for a camera.
 *
 * @param camName  Camera name; @c NULL or @c "" = active camera.
 */
BGI_API int  BGI_CALL wxbgi_overlay_concentric_is_enabled(const char *camName);

/**
 * @brief Set the number of concentric circles.
 *
 * @param camName  Camera name; @c NULL or @c "" = active camera.
 * @param count    Number of circles (clamped to 1..8).  Default: 3.
 */
BGI_API void BGI_CALL wxbgi_overlay_concentric_set_count(const char *camName,
                                                          int count);

/**
 * @brief Set the inner and outer world-unit radii for the circles.
 *
 * @param camName      Camera name; @c NULL or @c "" = active camera.
 * @param innerRadius  World-unit radius of the innermost circle (>= 0).
 *                     Default: 25.
 * @param outerRadius  World-unit radius of the outermost circle
 *                     (>= innerRadius).  Default: 100.
 */
BGI_API void BGI_CALL wxbgi_overlay_concentric_set_radii(const char *camName,
                                                          float innerRadius,
                                                          float outerRadius);

// =============================================================================
// Selection Cursor Square  (per-camera)
// =============================================================================

/**
 * @brief Enable the selection cursor square overlay for a camera.
 *
 * A small square follows the mouse within the camera's viewport.  Its border
 * alternates every 2 seconds between the light and dark shade of the chosen
 * colour scheme.
 *
 * @param camName  Camera name; @c NULL or @c "" = active camera.
 */
BGI_API void BGI_CALL wxbgi_overlay_cursor_enable(const char *camName);

/**
 * @brief Disable the selection cursor square overlay for a camera.
 *
 * @param camName  Camera name; @c NULL or @c "" = active camera.
 */
BGI_API void BGI_CALL wxbgi_overlay_cursor_disable(const char *camName);

/**
 * @brief Return 1 if the selection cursor is enabled for a camera.
 *
 * @param camName  Camera name; @c NULL or @c "" = active camera.
 */
BGI_API int  BGI_CALL wxbgi_overlay_cursor_is_enabled(const char *camName);

/**
 * @brief Set the side length of the selection cursor square.
 *
 * @param camName  Camera name; @c NULL or @c "" = active camera.
 * @param sizePx   Side length in pixels (clamped to 2..16).  Default: 8.
 */
BGI_API void BGI_CALL wxbgi_overlay_cursor_set_size(const char *camName,
                                                     int sizePx);

/**
 * @brief Set the colour scheme of the selection cursor.
 *
 * The cursor alternates between a light and dark shade of the chosen colour.
 *
 * @param camName      Camera name; @c NULL or @c "" = active camera.
 * @param colorScheme  0 = blue (default), 1 = green, 2 = red.
 */
BGI_API void BGI_CALL wxbgi_overlay_cursor_set_color(const char *camName,
                                                      int colorScheme);

// =============================================================================
// DDS Object Selection
// =============================================================================

/**
 * @brief Return the number of currently selected DDS objects.
 */
BGI_API int BGI_CALL wxbgi_selection_count(void);

/**
 * @brief Return the ID string of the @p index-th selected object (0-based).
 *
 * Returns "" if @p index is out of range.  The returned pointer is valid only
 * until the next selection mutation or library call.
 */
BGI_API const char *BGI_CALL wxbgi_selection_get_id(int index);

/** @brief Clear all currently selected objects (does not delete them from the DDS). */
BGI_API void BGI_CALL wxbgi_selection_clear(void);

/**
 * @brief Delete all currently selected objects from the DDS and clear the selection.
 *
 * Each object is soft-deleted via @c DdsScene::remove() and the scene is
 * compacted.  This is irreversible without resetting the DDS.
 */
BGI_API void BGI_CALL wxbgi_selection_delete_selected(void);

/**
 * @brief Set the selection flash colour scheme.
 *
 * @param scheme  0 = orange (default), 1 = purple.
 */
BGI_API void BGI_CALL wxbgi_selection_set_flash_scheme(int scheme);

/**
 * @brief Set the screen-pixel pick radius for object selection.
 *
 * A left-click selects the nearest DDS object whose projected geometry falls
 * within @p pixels screen pixels of the cursor centre.  Default: 12.
 * Clamped to the range [2, 64].
 */
BGI_API void BGI_CALL wxbgi_selection_set_pick_radius(int pixels);

/** @} */ // wxbgi_overlay_api
