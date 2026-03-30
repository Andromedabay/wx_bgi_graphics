#pragma once

#include "bgi_types.h"

#include <string>

/**
 * @file bgi_overlay.h
 * @brief Internal declarations for the visual overlay system.
 *
 * Visual overlays are non-DDS, non-selectable rendering aids drawn fresh every
 * frame.  They are not serialised to DDJ/DDY and cannot be picked by the
 * selection cursor.
 *
 * **Grid and UCS axes** are global (visible in every camera viewport when
 * enabled) and are drawn by @ref drawOverlaysForCamera(), which is called from
 * @c wxbgi_render_dds() after the DDS scene traversal and before
 * @c flushToScreen().
 *
 * **Concentric circles + crosshair** are per-camera and are also drawn by
 * @ref drawOverlaysForCamera().
 *
 * **Selection cursor square** is drawn as an OpenGL @c GL_LINE_LOOP by
 * @ref drawSelectionCursorsGL(), which is called from @c flushToScreen() after
 * the pixel-buffer rendering pass and before @c glFlush(), so it always
 * appears on top of everything else.
 */

namespace bgi
{

    /**
     * Draw all active overlays for one camera viewport.
     *
     * Called from @c wxbgi_render_dds() while @c gMutex is held, after the
     * DDS scene traversal and before @c flushToScreen().  The BGI viewport is
     * already set to the camera panel, so overlay drawing is clipped correctly.
     *
     * @param camName  Name of the camera being rendered.
     * @param cam      Camera3D data for this camera.
     */
    void drawOverlaysForCamera(const std::string &camName, const Camera3D &cam);

    /**
     * Draw selection cursor squares for all cameras that have the cursor enabled.
     *
     * Called from @c flushToScreen() in the OpenGL pass (after the pixel-buffer
     * @c GL_POINTS loop, before @c glFlush).  Draws using immediate-mode OpenGL
     * in the same pixel-space ortho projection as the page-buffer rendering.
     *
     * Does NOT acquire @c gMutex — the caller may already hold it.
     */
    void drawSelectionCursorsGL();

    /**
     * Handle a left-click pick event.
     *
     * Called from the GLFW mouse-button callback while @c gMutex is held.
     * Finds the nearest visible DDS drawing object within
     * @c gState.selectionPickRadiusPx screen pixels of (@p screenX, @p screenY)
     * for any camera whose selection-cursor overlay is enabled, and updates
     * @c gState.selectedObjectIds.
     *
     * @param screenX    Absolute window X of the click.
     * @param screenY    Absolute window Y of the click.
     * @param multiSelect  If true (CTRL held), add/toggle; otherwise replace selection.
     */
    void overlayPerformPick(int screenX, int screenY, bool multiSelect);

} // namespace bgi
