#include "bgi_dds.h"
#include "bgi_state.h"

#include "wx_bgi_overlay.h"

#include <algorithm>
#include <mutex>
#include <string>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace
{

/// Resolve NULL or "" camera name to the active camera name.
static const std::string &resolveCamName(const char *name)
{
    if (name != nullptr && name[0] != '\0')
    {
        static thread_local std::string s;
        s = name;
        return s;
    }
    return bgi::gState.activeCamera;
}

/// Return a pointer to Camera3D for @p name, or nullptr if not found.
static bgi::Camera3D *findCam(const std::string &name)
{
    auto it = bgi::gState.cameras.find(name);
    return (it != bgi::gState.cameras.end()) ? &it->second->camera : nullptr;
}

} // anonymous namespace

// =============================================================================
// Reference Grid
// =============================================================================

BGI_API void BGI_CALL wxbgi_overlay_grid_enable(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.overlayGrid.enabled = true;
}

BGI_API void BGI_CALL wxbgi_overlay_grid_disable(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.overlayGrid.enabled = false;
}

BGI_API int BGI_CALL wxbgi_overlay_grid_is_enabled(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    return bgi::gState.overlayGrid.enabled ? 1 : 0;
}

BGI_API void BGI_CALL wxbgi_overlay_grid_set_spacing(float worldUnits)
{
    if (worldUnits <= 0.f) return;
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.overlayGrid.spacing = worldUnits;
}

BGI_API void BGI_CALL wxbgi_overlay_grid_set_extent(int halfExtentInLines)
{
    if (halfExtentInLines < 1) return;
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.overlayGrid.halfExtent = halfExtentInLines;
}

BGI_API void BGI_CALL wxbgi_overlay_grid_set_colors(int xAxisColor, int yAxisColor, int gridColor)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.overlayGrid.xAxisColor = xAxisColor;
    bgi::gState.overlayGrid.yAxisColor = yAxisColor;
    bgi::gState.overlayGrid.gridColor  = gridColor;
}

BGI_API void BGI_CALL wxbgi_overlay_grid_set_ucs(const char *ucsName)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.overlayGrid.ucsName = (ucsName != nullptr) ? ucsName : "";
}

// =============================================================================
// UCS Axes
// =============================================================================

BGI_API void BGI_CALL wxbgi_overlay_ucs_axes_enable(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.overlayUcsAxes.enabled = true;
}

BGI_API void BGI_CALL wxbgi_overlay_ucs_axes_disable(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.overlayUcsAxes.enabled = false;
}

BGI_API int BGI_CALL wxbgi_overlay_ucs_axes_is_enabled(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    return bgi::gState.overlayUcsAxes.enabled ? 1 : 0;
}

BGI_API void BGI_CALL wxbgi_overlay_ucs_axes_show_world(int show)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.overlayUcsAxes.showWorld = (show != 0);
}

BGI_API void BGI_CALL wxbgi_overlay_ucs_axes_show_active(int show)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.overlayUcsAxes.showActive = (show != 0);
}

BGI_API void BGI_CALL wxbgi_overlay_ucs_axes_set_length(float worldUnits)
{
    if (worldUnits <= 0.f) return;
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.overlayUcsAxes.axisLength = worldUnits;
}

// =============================================================================
// Concentric Circles + Crosshair  (per-camera)
// =============================================================================

BGI_API void BGI_CALL wxbgi_overlay_concentric_enable(const char *camName)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (auto *cam = findCam(resolveCamName(camName)))
        cam->concentricOverlay.enabled = true;
}

BGI_API void BGI_CALL wxbgi_overlay_concentric_disable(const char *camName)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (auto *cam = findCam(resolveCamName(camName)))
        cam->concentricOverlay.enabled = false;
}

BGI_API int BGI_CALL wxbgi_overlay_concentric_is_enabled(const char *camName)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (const auto *cam = findCam(resolveCamName(camName)))
        return cam->concentricOverlay.enabled ? 1 : 0;
    return 0;
}

BGI_API void BGI_CALL wxbgi_overlay_concentric_set_count(const char *camName, int count)
{
    if (count < 1 || count > 8) return;
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (auto *cam = findCam(resolveCamName(camName)))
        cam->concentricOverlay.count = count;
}

BGI_API void BGI_CALL wxbgi_overlay_concentric_set_radii(const char *camName,
                                                         float innerRadius,
                                                         float outerRadius)
{
    if (innerRadius < 0.f || outerRadius < innerRadius) return;
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (auto *cam = findCam(resolveCamName(camName)))
    {
        cam->concentricOverlay.innerRadius = innerRadius;
        cam->concentricOverlay.outerRadius = outerRadius;
    }
}

// =============================================================================
// Selection Cursor Square  (per-camera)
// =============================================================================

BGI_API void BGI_CALL wxbgi_overlay_cursor_enable(const char *camName)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (auto *cam = findCam(resolveCamName(camName)))
        cam->selCursorOverlay.enabled = true;
}

BGI_API void BGI_CALL wxbgi_overlay_cursor_disable(const char *camName)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (auto *cam = findCam(resolveCamName(camName)))
        cam->selCursorOverlay.enabled = false;
}

BGI_API int BGI_CALL wxbgi_overlay_cursor_is_enabled(const char *camName)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (const auto *cam = findCam(resolveCamName(camName)))
        return cam->selCursorOverlay.enabled ? 1 : 0;
    return 0;
}

BGI_API void BGI_CALL wxbgi_overlay_cursor_set_size(const char *camName, int sizePx)
{
    // Clamp to valid range 2..16
    sizePx = std::clamp(sizePx, 2, 16);
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (auto *cam = findCam(resolveCamName(camName)))
        cam->selCursorOverlay.sizePx = sizePx;
}

BGI_API void BGI_CALL wxbgi_overlay_cursor_set_color(const char *camName, int colorScheme)
{
    // 0 = blue, 1 = green, 2 = red
    colorScheme = std::clamp(colorScheme, 0, 2);
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    if (auto *cam = findCam(resolveCamName(camName)))
        cam->selCursorOverlay.colorScheme = colorScheme;
}

// =============================================================================
// DDS Object Selection
// =============================================================================

BGI_API int BGI_CALL wxbgi_selection_count(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    return static_cast<int>(bgi::gState.selectedObjectIds.size());
}

BGI_API const char *BGI_CALL wxbgi_selection_get_id(int index)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    const auto &ids = bgi::gState.selectedObjectIds;
    if (index < 0 || index >= static_cast<int>(ids.size()))
        return "";
    // Return a pointer to the stored string — valid until the next selection mutation.
    return ids[static_cast<std::size_t>(index)].c_str();
}

BGI_API void BGI_CALL wxbgi_selection_clear(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.selectedObjectIds.clear();
}

BGI_API void BGI_CALL wxbgi_selection_delete_selected(void)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    for (const auto &id : bgi::gState.selectedObjectIds)
        bgi::gState.dds->remove(id);
    bgi::gState.dds->compact();
    bgi::gState.selectedObjectIds.clear();
}

BGI_API void BGI_CALL wxbgi_selection_set_flash_scheme(int scheme)
{
    // 0 = orange (252), 1 = purple (253)
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.selectionFlashScheme = std::clamp(scheme, 0, 1);
}

BGI_API void BGI_CALL wxbgi_selection_set_pick_radius(int pixels)
{
    std::lock_guard<std::mutex> lock(bgi::gMutex);
    bgi::gState.selectionPickRadiusPx = std::max(2, std::min(pixels, 64));
}
