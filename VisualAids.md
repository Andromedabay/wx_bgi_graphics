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

---

## Visual Aid Types

### 1) Reference Grid (global)

Draws grid lines in the UCS XY plane and projects them through every camera
viewport.  The grid is visible simultaneously in all panels when enabled.
BGI viewport clipping applies so only the portion inside each panel is drawn.

- Enable/disable and query:
  - `wxbgi_overlay_grid_enable()`
  - `wxbgi_overlay_grid_disable()`
  - `wxbgi_overlay_grid_is_enabled()` -> `1` / `0`
- Configuration:
  - `wxbgi_overlay_grid_set_spacing(worldUnits)` -- distance between lines (> 0)
  - `wxbgi_overlay_grid_set_extent(halfExtentInLines)` -- lines each side of origin (>= 1)
  - `wxbgi_overlay_grid_set_colors(xAxisColor, yAxisColor, gridColor)`
  - `wxbgi_overlay_grid_set_ucs(ucsName)` -- `NULL`/`""` = active UCS

Defaults:

| Field | Default |
|---|---|
| Spacing | `25` world units |
| Half extent | `4` lines each side |
| X axis color | `RED` |
| Y axis color | `GREEN` |
| Grid line color | `DARKGRAY` |
| UCS binding | active UCS |

---

### 2) UCS Axes (global)

Draws labeled X (RED), Y (GREEN), Z (BLUE) axis arms in every camera viewport
for orientation.  Both the world UCS origin and the active UCS origin can be
shown simultaneously.

- Enable/disable and query:
  - `wxbgi_overlay_ucs_axes_enable()`
  - `wxbgi_overlay_ucs_axes_disable()`
  - `wxbgi_overlay_ucs_axes_is_enabled()` -> `1` / `0`
- Configuration:
  - `wxbgi_overlay_ucs_axes_show_world(show)` -- show world-origin axes
  - `wxbgi_overlay_ucs_axes_show_active(show)` -- show active-UCS axes
  - `wxbgi_overlay_ucs_axes_set_length(worldUnits)` -- arm length (> 0)

Defaults:

| Field | Default |
|---|---|
| World axes | shown |
| Active UCS axes | shown |
| Axis length | `80` world units |

---

### 3) Concentric Circles + Crosshair (per-camera)

Draws concentric world-space circles centered on the camera pan/target point.
Because radii are in world units, the visible pixel spacing changes with zoom,
providing immediate scale feedback.

- 2D cameras: crosshair rotates with the camera rotation angle.
- 3D cameras: crosshair is fixed (upward on screen).

- Enable/disable and query:
  - `wxbgi_overlay_concentric_enable(camName)` -- `NULL`/`""` = active camera
  - `wxbgi_overlay_concentric_disable(camName)`
  - `wxbgi_overlay_concentric_is_enabled(camName)` -> `1` / `0`
- Configuration:
  - `wxbgi_overlay_concentric_set_count(camName, count)` -- clamped to `1..8`
  - `wxbgi_overlay_concentric_set_radii(camName, innerRadius, outerRadius)`

Defaults:

| Field | Default |
|---|---|
| Circle count | `3` |
| Inner radius | `25` world units |
| Outer radius | `100` world units |

---

### 4) Selection Cursor Square (per-camera)

Draws a small square that tracks the mouse inside a camera's viewport.  The
square is drawn in the OpenGL pass (after the BGI pixel buffer) so it is always
visible on top of scene content.

- Border alternates every 2 seconds between a light and dark shade of the
  chosen color scheme.
- Color schemes: `0` = blue (default), `1` = green, `2` = red.
- Selection is triggered by a left-click anywhere inside the camera viewport.

- Enable/disable and query:
  - `wxbgi_overlay_cursor_enable(camName)` -- also activates click-to-select
  - `wxbgi_overlay_cursor_disable(camName)`
  - `wxbgi_overlay_cursor_is_enabled(camName)` -> `1` / `0`
- Configuration:
  - `wxbgi_overlay_cursor_set_size(camName, sizePx)` -- clamped to `2..16`
  - `wxbgi_overlay_cursor_set_color(camName, colorScheme)`

Default size: `8` px

---

## Selection System

The selection system lets the user pick DDS drawing objects by left-clicking
inside any camera viewport that has the Selection Cursor Square overlay enabled.
Selected objects flash in an alternate color each frame.

### Click Behavior

| Action | Result |
|---|---|
| Left-click on an object | Clear selection; select the clicked object |
| Left-click on an already-selected object | Deselect it (selection becomes empty) |
| Left-click on empty space | Clear selection |
| CTRL+click on an object | Add to existing selection |
| CTRL+click on already-selected object | Remove it from selection |
| CTRL+click on empty space | Selection unchanged |

### Depth Picking

When several objects overlap on screen the one **closest to the camera eye** is
picked.  Objects within a 0.5 world-unit depth band of each other are resolved
by screen-space distance to the click point.  All DDS drawing-object types are
supported:

- 2D primitives: line, circle, arc, rectangle, ellipse, text
- 3D world/UCS draw functions: world line, world circle, world arc, world rectangle, world ellipse
- 3D solids: sphere, cylinder, cone, torus (Sutherland-Hodgman projected outline)
- Parametric surfaces: sphere, cylinder, torus, saddle, Mobius (formula-aware probe points)

### Flash Visual Feedback

Selected objects are drawn each frame with a color override that alternates
between an orange and a darker orange tone (scheme `0`), or purple/dark-purple
(scheme `1`).

- 3D solids: `solidColorOverride` in `BgiState` overrides `tri.faceColor` /
  `tri.edgeColor` in `renderTriangleBatch`.
- All other primitives: `colorOverride` in `renderObject` replaces
  `gState.currentColor` / `gState.fillColor`.

Flash rate: 1 Hz (1-second on/off toggle based on `glfwGetTime()`).

### Selection API

```c
int         wxbgi_selection_count(void);
const char *wxbgi_selection_get_id(int index);       // 0-based; valid until next mutation
void        wxbgi_selection_clear(void);              // deselect all (no DDS change)
void        wxbgi_selection_delete_selected(void);    // remove from DDS + clear selection
void        wxbgi_selection_set_flash_scheme(int scheme);   // 0=orange (default), 1=purple
void        wxbgi_selection_set_pick_radius(int pixels);    // default 16, clamped 2..64
```

Iterating selected objects:

```c
for (int i = 0; i < wxbgi_selection_count(); ++i) {
    const char *id = wxbgi_selection_get_id(i);
    printf("selected: %s\n", id);
}
```

---

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

wxbgi_overlay_cursor_enable("cam2d");   /* also activates click-to-select */
wxbgi_overlay_cursor_set_size("cam2d", 12);
wxbgi_overlay_cursor_set_color("cam2d", 0); /* blue */

/* In your render loop */
wxbgi_render_dds();

/* Read selection after each frame */
for (int i = 0; i < wxbgi_selection_count(); ++i)
    printf("selected: %s\n", wxbgi_selection_get_id(i));
```

For a full interactive example, see `examples/cpp/wxbgi_camera_demo.cpp`.

---

# Visual Aids -- Code Map

This section describes where every piece of the Visual Aids system lives in the
source tree, from data structs through the drawing layer to the public C API.

---

## 1. Data -- structs in `src/bgi_types.h`

### Global overlay state (fields of `BgiState`)

```cpp
// ---- Global overlays -------------------------------------------------------
struct OverlayGridState
{
    bool        enabled    {false};
    float       spacing    {25.f};      // world units between adjacent lines
    int         halfExtent {4};         // grid spans +/-(halfExtentxspacing)
    int         xAxisColor {RED};
    int         yAxisColor {GREEN};
    int         gridColor  {DARKGRAY};
    std::string ucsName;                // empty = BgiState::activeUcs
} overlayGrid;

struct OverlayUcsAxesState
{
    bool  enabled    {false};
    bool  showWorld  {true};            // draw world UCS at (0,0,0)
    bool  showActive {true};            // draw active UCS at its origin
    float axisLength {80.f};            // world units origin->tip
} overlayUcsAxes;

// ---- Selection state -------------------------------------------------------
std::vector<std::string> selectedObjectIds;   // IDs of currently selected objects
int selectionFlashScheme {0};                 // 0 = orange, 1 = purple
int selectionPickRadiusPx{16};                // screen-pixel pick threshold [2..64]

// ---- Solid color override (used by selection flash) -----------------------
int solidColorOverride{-1};   // if >= 0, overrides tri.faceColor/edgeColor
```

### Per-camera overlay state (anonymous structs inside `Camera3D`)

```cpp
// Inside Camera3D (src/bgi_types.h) -- not serialised to DDS/DDJ

struct {
    bool  enabled     {false};
    int   count       {3};           // circles: 1..8
    float innerRadius {25.f};        // world-unit radius of innermost circle
    float outerRadius {100.f};       // world-unit radius of outermost circle
} concentricOverlay;

struct {
    bool  enabled     {false};
    int   sizePx      {8};           // square side length, pixels: 2..16
    int   colorScheme {0};           // 0 = blue, 1 = green, 2 = red
} selCursorOverlay;
```

**Ownership:** Both overlay structs are value members of `Camera3D`, which is a
value member of `DdsCamera`.  They live and die with the camera.  They are
**not** serialized -- the scene file does not store overlay state.

---

## 2. Drawing layer -- `src/bgi_overlay.cpp`

Pure drawing routines; no public API symbols.  All functions operate under
`gMutex` (the mutex is held by the caller -- do not re-acquire inside).

### Internal drawing functions (anonymous namespace)

| Function | What it draws |
|---|---|
| `drawGridOverlay(cam)` | One grid-line pass for a camera.  Iterates rows and columns; each line passes through `cameraWorldToScreen` and `drawLineInternal`. |
| `drawUcsAxesOverlay(cam)` | Six axis arms (+/-X, +/-Y, +/-Z) for world UCS and/or active UCS.  Text labels placed at the positive tip via `bgi::drawText`. |
| `drawConcentricOverlay(camName, cam)` | Up to 8 concentric circles and a crosshair.  Radius stepping: `innerRadius + i * (outerRadius - innerRadius) / (count - 1)`.  Each circle drawn with `circle()` in projected pixel coordinates. |

### Selection cursor GL pass

```cpp
void drawSelectionCursorsGL();
```

Called from `flushToScreen()` in the **OpenGL pass** -- after the BGI pixel
buffer is uploaded but before `glFlush()`.  Uses immediate-mode `GL_LINE_LOOP`
so the square renders on top of everything.  Does not acquire `gMutex` (caller
holds it).

### Pick handler

```cpp
void overlayPerformPick(int screenX, int screenY, bool multiSelect);
```

Called from `mouseButtonCallback` in `bgi_api.cpp`.  Logic:

1. For each camera that has `selCursorOverlay.enabled`:
   - Check if click coordinates lie inside the camera's screen viewport.
   - For every DDS drawing object, call `screenDistToObject()` to get the
     screen-pixel distance and camera depth.
   - Collect all objects within `selectionPickRadiusPx` pixels.
   - Sort by depth (closest first); break ties by screen distance within 0.5
     world-unit depth band.
   - Take the nearest candidate as `nearestId`.
2. Update `selectedObjectIds` according to the toggle rules:

```
wasSelected = nearestId in selectedObjectIds

if NOT multiSelect:
    selectedObjectIds.clear()

if NOT wasSelected:
    selectedObjectIds.push_back(nearestId)   // select
else if multiSelect:
    selectedObjectIds.erase(nearestId)       // CTRL+click toggle off
// plain click on already-selected: cleared above -> deselected
```

3. Break -- first matching camera viewport wins.

### Distance probing -- `screenDistToObject()`

Returns the minimum screen-pixel distance from a screen point to any part of
a DDS object, and sets an output `depth` (camera-space -Z, positive = in
front).  Per-type logic:

| DDS Object Type | Probe method |
|---|---|
| `Line`, `WorldLine`, `UcsLine` | Closest point on the projected line segment |
| `Circle`, `WorldCircle`, `UcsCircle` | Closest point on projected circle perimeter |
| `Arc`, `WorldArc`, `UcsArc` | Sweep 8 arc sample points |
| `Rectangle`, `WorldRect`, `UcsRect` | 4 corners |
| `Ellipse`, `WorldEllipse`, `UcsEllipse` | 8 perimeter points |
| `Text`, `WorldText` | Closest point on projected text bounding box |
| `Sphere3D`, `Cylinder3D`, `Cone3D`, `Torus3D` | Projected outline vertices of the triangle mesh |
| `ParamSurface` | Formula-aware poles/rim/corner probe points |

---

## 3. Rendering pass integration -- `src/bgi_dds_render.cpp`

`wxbgi_render_dds()` calls the overlay system in two places:

```
for each camera:
    render DDS scene objects
    drawOverlaysForCamera(camName, cam)   <- grid, UCS axes, concentric circles
    flushToScreen()
        +-- drawSelectionCursorsGL()      <- selection cursor (OpenGL pass)
```

Flash rendering for selected objects:

```cpp
// In renderObject(), bgi_dds_render.cpp
const bool isSelected = std::find(gState.selectedObjectIds.begin(),
                                   gState.selectedObjectIds.end(), obj.id)
                          != gState.selectedObjectIds.end();
int colorOverride = -1;
if (isSelected) {
    const int flashPhase = static_cast<int>(glfwGetTime()) & 1;
    colorOverride = (gState.selectionFlashScheme == 0)
        ? (flashPhase ? 252 : 214)   // orange / dark-orange
        : (flashPhase ? 253 : 93);   // purple / dark-purple
}

// 2D/world primitives: apply colorOverride to gState.currentColor / fillColor
// 3D solids:           set gState.solidColorOverride, call renderSolid3D, clear
```

---

## 4. Public C API -- `src/wx_bgi_overlay.h` / `src/bgi_overlay_api.cpp`

All functions are `BGI_API` / `BGI_CALL` (C linkage, dllexport on Windows).
Each acquires `bgi::gMutex`, reads/writes `gState` or a named camera's overlay
struct, then releases the mutex.

### Reference Grid

| Function | Description |
|---|---|
| `wxbgi_overlay_grid_enable()` | `overlayGrid.enabled = true` |
| `wxbgi_overlay_grid_disable()` | `overlayGrid.enabled = false` |
| `wxbgi_overlay_grid_is_enabled()` | Returns `overlayGrid.enabled` |
| `wxbgi_overlay_grid_set_spacing(f)` | Sets `overlayGrid.spacing` (> 0 only) |
| `wxbgi_overlay_grid_set_extent(n)` | Sets `overlayGrid.halfExtent` (>= 1) |
| `wxbgi_overlay_grid_set_colors(x,y,g)` | Sets X/Y/grid colors |
| `wxbgi_overlay_grid_set_ucs(name)` | Sets `overlayGrid.ucsName` |

### UCS Axes

| Function | Description |
|---|---|
| `wxbgi_overlay_ucs_axes_enable()` | `overlayUcsAxes.enabled = true` |
| `wxbgi_overlay_ucs_axes_disable()` | `overlayUcsAxes.enabled = false` |
| `wxbgi_overlay_ucs_axes_is_enabled()` | Returns `overlayUcsAxes.enabled` |
| `wxbgi_overlay_ucs_axes_show_world(n)` | Sets `overlayUcsAxes.showWorld` |
| `wxbgi_overlay_ucs_axes_show_active(n)` | Sets `overlayUcsAxes.showActive` |
| `wxbgi_overlay_ucs_axes_set_length(f)` | Sets `overlayUcsAxes.axisLength` (> 0) |

### Concentric Circles (per-camera, `camName` = `NULL`/`""` -> active camera)

| Function | Description |
|---|---|
| `wxbgi_overlay_concentric_enable(cam)` | `concentricOverlay.enabled = true` |
| `wxbgi_overlay_concentric_disable(cam)` | `concentricOverlay.enabled = false` |
| `wxbgi_overlay_concentric_is_enabled(cam)` | Returns `concentricOverlay.enabled` |
| `wxbgi_overlay_concentric_set_count(cam, n)` | `count` clamped to `1..8` |
| `wxbgi_overlay_concentric_set_radii(cam, r1, r2)` | Sets `innerRadius`/`outerRadius` |

### Selection Cursor (per-camera)

| Function | Description |
|---|---|
| `wxbgi_overlay_cursor_enable(cam)` | `selCursorOverlay.enabled = true` (also activates click-to-select) |
| `wxbgi_overlay_cursor_disable(cam)` | `selCursorOverlay.enabled = false` |
| `wxbgi_overlay_cursor_is_enabled(cam)` | Returns `selCursorOverlay.enabled` |
| `wxbgi_overlay_cursor_set_size(cam, px)` | `sizePx` clamped to `2..16` |
| `wxbgi_overlay_cursor_set_color(cam, s)` | `colorScheme`: 0=blue, 1=green, 2=red |

### Selection

| Function | Description |
|---|---|
| `wxbgi_selection_count()` | Number of currently selected objects |
| `wxbgi_selection_get_id(i)` | ID of the i-th selected object (0-based) |
| `wxbgi_selection_clear()` | Deselect all (no DDS change) |
| `wxbgi_selection_delete_selected()` | Remove selected objects from DDS + clear |
| `wxbgi_selection_set_flash_scheme(s)` | `0` = orange (default), `1` = purple |
| `wxbgi_selection_set_pick_radius(px)` | Pick radius clamped to `2..64` (default 16) |

---

## 5. File dependency diagram

```
wx_bgi_overlay.h        (public declarations -- include in user code)
     |
     v
bgi_overlay_api.cpp     (public C API implementation: lock gMutex, mutate gState)
     |
     +---> bgi_overlay.h/cpp  (internal: drawOverlaysForCamera, drawSelectionCursorsGL,
     |                          overlayPerformPick, screenDistToObject)
     |         |
     |         +---> bgi_camera.h/cpp   (cameraWorldToScreen, cameraEffectiveViewport)
     |         +---> bgi_draw.h/cpp     (drawLineInternal, drawCircleInternal, stampPixel)
     |         +---> bgi_font.h/cpp     (drawText for axis labels)
     |         +---> bgi_ucs.h/cpp      (ucsLocalToWorldMatrix for grid UCS transform)
     |
     +---> bgi_types.h   (OverlayGridState, OverlayUcsAxesState, Camera3D overlay structs,
                          BgiState::selectedObjectIds / selectionFlashScheme / selectionPickRadiusPx)

bgi_dds_render.cpp  (calls drawOverlaysForCamera + renderObject flash logic)
bgi_api.cpp         (mouseButtonCallback -> overlayPerformPick)
bgi_draw.cpp        (flushToScreen -> drawSelectionCursorsGL)
```

---

## See also

- [Camera3D_Map.md](Camera3D_Map.md) -- camera data, math, and API
- [Camera2D_Map.md](Camera2D_Map.md) -- 2D camera convenience layer
- [DDS.md](DDS.md) -- DDS / CHDOP / DDJ / DDY scene graph documentation
