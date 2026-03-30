# Visual Aids

The Visual Aids system provides non-DDS overlays for camera/UCS workflows.
These overlays are designed for orientation, scale awareness, and interactive
selection feedback without changing the DDS scene data.

Key behavior:

- Visual Aids are drawn fresh each frame and are not stored in DDS.
- They are not serialized to DDJ/DDY.
- They are not selectable as DDS objects.
- They are disabled by default to keep legacy behavior unchanged.
- They are rendered automatically during `wxbgi_render_dds()`.

Primary API header: `src/wx_bgi_overlay.h`

## Visual Aid Types

### 1) Reference Grid (global)

Draws grid lines in the UCS XY plane and projects them through every camera
viewport.

- Enable/disable and query:
  - `wxbgi_overlay_grid_enable()`
  - `wxbgi_overlay_grid_disable()`
  - `wxbgi_overlay_grid_is_enabled()`
- Configuration:
  - `wxbgi_overlay_grid_set_spacing(worldUnits)`
  - `wxbgi_overlay_grid_set_extent(halfExtentInLines)`
  - `wxbgi_overlay_grid_set_colors(xAxisColor, yAxisColor, gridColor)`
  - `wxbgi_overlay_grid_set_ucs(ucsName)`

Defaults:

- Spacing: `25` world units
- Half extent: `4` lines each side of the origin
- Colors: X axis `RED`, Y axis `GREEN`, other lines `DARKGRAY`
- UCS binding: active UCS

### 2) UCS Axes (global)

Draws labeled X/Y/Z axis arms in every camera viewport for orientation.

- Enable/disable and query:
  - `wxbgi_overlay_ucs_axes_enable()`
  - `wxbgi_overlay_ucs_axes_disable()`
  - `wxbgi_overlay_ucs_axes_is_enabled()`
- Configuration:
  - `wxbgi_overlay_ucs_axes_show_world(show)`
  - `wxbgi_overlay_ucs_axes_show_active(show)`
  - `wxbgi_overlay_ucs_axes_set_length(worldUnits)`

Defaults:

- World axes: shown
- Active UCS axes: shown
- Axis length: `80` world units

### 3) Concentric Circles + Crosshair (per-camera)

Draws concentric world-space circles centered on camera pan/target.

- 2D cameras: crosshair rotates with camera rotation.
- 3D cameras: crosshair orientation is fixed.

Because radii are world-space, visible pixel spacing changes with zoom,
providing immediate scale feedback.

- Enable/disable and query:
  - `wxbgi_overlay_concentric_enable(camName)`
  - `wxbgi_overlay_concentric_disable(camName)`
  - `wxbgi_overlay_concentric_is_enabled(camName)`
- Configuration:
  - `wxbgi_overlay_concentric_set_count(camName, count)` (clamped to 1..8)
  - `wxbgi_overlay_concentric_set_radii(camName, innerRadius, outerRadius)`

Defaults:

- Circle count: `3`
- Inner radius: `25`
- Outer radius: `100`

### 4) Selection Cursor Square (per-camera)

Draws a small square following the mouse inside a camera viewport.

- Border alternates every 2 seconds between light/dark shades.
- Color scheme options: blue (0), green (1), red (2).
- Drawn in the OpenGL pass on top of scene content.

- Enable/disable and query:
  - `wxbgi_overlay_cursor_enable(camName)`
  - `wxbgi_overlay_cursor_disable(camName)`
  - `wxbgi_overlay_cursor_is_enabled(camName)`
- Configuration:
  - `wxbgi_overlay_cursor_set_size(camName, sizePx)` (clamped to 2..16)
  - `wxbgi_overlay_cursor_set_color(camName, colorScheme)`

Default size: `8` px

## Selection Companion APIs

These APIs complement the visual selection cursor by managing DDS object
selection state:

- `wxbgi_selection_count()`
- `wxbgi_selection_get_id(index)`
- `wxbgi_selection_clear()`
- `wxbgi_selection_delete_selected()`
- `wxbgi_selection_set_flash_scheme(scheme)`
- `wxbgi_selection_set_pick_radius(pixels)`

## Minimal Setup Example

```c
#include "wx_bgi.h"
#include "wx_bgi_3d.h"
#include "wx_bgi_dds.h"
#include "wx_bgi_overlay.h"

initwindow(1280, 720, "Visual Aids", 0, 0, 1, 1);

/* Global aids */
wxbgi_overlay_grid_enable();
wxbgi_overlay_grid_set_spacing(25.0f);
wxbgi_overlay_grid_set_extent(4);

wxbgi_overlay_ucs_axes_enable();
wxbgi_overlay_ucs_axes_set_length(110.0f);

/* Per-camera aids */
wxbgi_overlay_concentric_enable("cam2d");
wxbgi_overlay_concentric_set_count("cam2d", 3);
wxbgi_overlay_concentric_set_radii("cam2d", 30.0f, 90.0f);

wxbgi_overlay_cursor_enable("cam2d");
wxbgi_overlay_cursor_set_size("cam2d", 12);
wxbgi_overlay_cursor_set_color("cam2d", 0); /* blue */

/* In your render loop */
wxbgi_render_dds();
```

For a full interactive example, see `examples/cpp/wxbgi_camera_demo.cpp`.
