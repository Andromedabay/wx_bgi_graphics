# Drawing Description Data Structure (DDS)

## Acronym Glossary

| Acronym | Expanded Form | Meaning |
|---------|--------------|---------|
| **DDS** | Drawing Description Data Structure | The in-memory retained-mode scene graph that holds every drawing object created during a session. |
| **DDDS** | Drawing Description Data Structure (alternative form) | Interchangeable with DDS -- used in early design notes; the shorter form **DDS** is preferred in code and API names. |
| **CHDOP** | Class Hierarchy of Drawing Object Primitives | The C++ class/struct hierarchy that represents individual objects stored inside the DDS (points, lines, circles, cameras, UCS frames, 3D solids, etc.). |
| **DDJ** | Drawing Data JSON | A JSON-format serialisation of the full DDS scene, produced by `wxbgi_dds_to_json()` and consumed by `wxbgi_dds_from_json()`. |
| **DDY** | Drawing Data YAML | A YAML-format serialisation of the full DDS scene, produced by `wxbgi_dds_to_yaml()` and consumed by `wxbgi_dds_from_yaml()`. |

These acronyms appear throughout API function names (e.g. `wxbgi_dds_*`), internal type names, and code comments.

---

## What the DDS Adds

Every call to a BGI drawing function (e.g. `line`, `circle`, `rectangle`) and every world/UCS draw call (e.g. `wxbgi_world_line`, `wxbgi_ucs_circle`) **automatically appends a CHDOP entry** to the DDS while also performing the immediate pixel-buffer render. The DDS therefore always contains a complete, replayable description of the current scene.

Cameras (`wxbgi_cam_*` / `wxbgi_cam2d_*`), User Coordinate Systems (`wxbgi_ucs_*`), world extents, and 3D solid primitives are also stored as CHDOP nodes in the DDS.

Key design choices:

- **Collection:** `std::unordered_map` (O(1) direct access by string ID) combined with a `std::vector` insertion-order index -- giving both quick direct access and sequential traversal.
- **Default scene:** A default camera named `"default"` and a world UCS named `"world"` are created automatically on `initwindow()` / `initgraph()`.
- **Retained-mode render:** Call `wxbgi_render_dds(camName)` to replay the full scene through any camera. All three panels of the camera demo use this to show the same DDS from different viewpoints simultaneously.
- **Serialisation:** The scene can be exported to DDJ or DDY at any time and re-imported to restore or share it.

---

## Public API Header

```c
#include "wx_bgi_dds.h"
```

---

## Core DDS Functions

### Scene Inspection

| Function | Description |
|----------|-------------|
| `wxbgi_dds_object_count()` | Total number of CHDOP objects currently in the DDS. |
| `wxbgi_dds_get_id_at(index)` | String ID of the object at insertion-order position `index`. |
| `wxbgi_dds_get_type(id)` | Returns a `WXBGI_DDS_*` type constant (see table below). |
| `wxbgi_dds_get_coord_space(id)` | Coordinate space of the object (`BgiPixel`, `World3D`, or `UcsLocal`). |
| `wxbgi_dds_get_label(id)` | Human-readable label attached to the object. |
| `wxbgi_dds_set_label(id, label)` | Attach or change a label. |
| `wxbgi_dds_find_by_label(label)` | Look up an object ID by its label. |
| `wxbgi_dds_set_visible(id, visible)` | Show / hide an individual object without deleting it. |
| `wxbgi_dds_get_visible(id)` | Returns 1 if visible, 0 if hidden. |

### Scene Modification

| Function | Description |
|----------|-------------|
| `wxbgi_dds_remove(id)` | Remove one object by ID. |
| `wxbgi_dds_clear()` | Remove all drawing objects; keeps cameras and UCS frames. |
| `wxbgi_dds_clear_all()` | Remove every object including cameras and UCS frames. |

### Retained-Mode Rendering

| Function | Description |
|----------|-------------|
| `wxbgi_render_dds(camName)` | Traverse the full DDS and render every visible object through the named camera into the active BGI pixel buffer. Pass `NULL` to use the active camera. |

### Serialisation -- DDJ (JSON)

| Function | Description |
|----------|-------------|
| `wxbgi_dds_to_json()` | Serialise scene to a DDJ string (valid until next serialisation call). |
| `wxbgi_dds_from_json(jsonString)` | Deserialise a DDJ string into the DDS. Returns 0 on success. |
| `wxbgi_dds_save_json(filePath)` | Save DDJ to a file. Returns 0 on success. |
| `wxbgi_dds_load_json(filePath)` | Load DDJ from a file. Returns 0 on success. |

### Serialisation -- DDY (YAML)

| Function | Description |
|----------|-------------|
| `wxbgi_dds_to_yaml()` | Serialise scene to a DDY string (valid until next serialisation call). |
| `wxbgi_dds_from_yaml(yamlString)` | Deserialise a DDY string into the DDS. Returns 0 on success. |
| `wxbgi_dds_save_yaml(filePath)` | Save DDY to a file. Returns 0 on success. |
| `wxbgi_dds_load_yaml(filePath)` | Load DDY from a file. Returns 0 on success. |

---

## CHDOP Object Type Constants (`WXBGI_DDS_*`)

The following constants identify the concrete CHDOP sub-type of each DDS entry:

| Constant | Value | Description |
|----------|-------|-------------|
| `WXBGI_DDS_CAMERA` | 0 | Camera (2D or 3D) |
| `WXBGI_DDS_UCS` | 1 | User Coordinate System frame |
| `WXBGI_DDS_WORLD_EXTENTS` | 2 | World bounding extents |
| `WXBGI_DDS_POINT` | 3 | `putpixel` / point |
| `WXBGI_DDS_LINE` | 4 | `line` / `lineto` / `linerel` |
| `WXBGI_DDS_CIRCLE` | 5 | `circle` |
| `WXBGI_DDS_ARC` | 6 | `arc` |
| `WXBGI_DDS_ELLIPSE` | 7 | `ellipse` |
| `WXBGI_DDS_FILL_ELLIPSE` | 8 | `fillellipse` |
| `WXBGI_DDS_PIE_SLICE` | 9 | `pieslice` |
| `WXBGI_DDS_SECTOR` | 10 | `sector` |
| `WXBGI_DDS_RECTANGLE` | 11 | `rectangle` |
| `WXBGI_DDS_BAR` | 12 | `bar` |
| `WXBGI_DDS_BAR3D` | 13 | `bar3d` |
| `WXBGI_DDS_POLYGON` | 14 | `drawpoly` |
| `WXBGI_DDS_FILL_POLY` | 15 | `fillpoly` |
| `WXBGI_DDS_TEXT` | 16 | `outtextxy` / `outtext` |
| `WXBGI_DDS_IMAGE` | 17 | `putimage` |
| `WXBGI_DDS_BOX` | 18 | `wxbgi_solid_box` (3D solid) |
| `WXBGI_DDS_SPHERE` | 19 | `wxbgi_solid_sphere` (3D solid) |
| `WXBGI_DDS_CYLINDER` | 20 | `wxbgi_solid_cylinder` (3D solid) |
| `WXBGI_DDS_CONE` | 21 | `wxbgi_solid_cone` (3D solid) |
| `WXBGI_DDS_TORUS` | 22 | `wxbgi_solid_torus` (3D solid) |
| `WXBGI_DDS_HEIGHTMAP` | 23 | `wxbgi_surface_heightmap` (surface) |
| `WXBGI_DDS_PARAM_SURFACE` | 24 | `wxbgi_surface_parametric` (surface) |
| `WXBGI_DDS_EXTRUSION` | 25 | `wxbgi_extrude_polygon` (extrusion) |

---

## Minimal DDS Usage Example

```c
#include "wx_bgi.h"
#include "wx_bgi_dds.h"
#include "wx_bgi_3d.h"

initwindow(800, 600, "DDS Demo", 0, 0, 1, 1);

// Classic BGI calls -- each one also writes a CHDOP entry to the DDS.
setcolor(WHITE);
circle(400, 300, 80);
rectangle(200, 150, 600, 450);

// Render the same scene through any named camera.
wxbgi_render_dds("default");

// Export to DDJ and DDY.
wxbgi_dds_save_json("scene.ddj");
wxbgi_dds_save_yaml("scene.ddy");

// Query the scene.
printf("Objects in DDS: %d\n", wxbgi_dds_object_count());

// Reload from JSON into a fresh session.
wxbgi_dds_clear();
wxbgi_dds_load_json("scene.ddj");
```

---

## GL Rendering Pipeline (wx Mode)

When using `wx_bgi_wx`, the BGI pixel buffer is composited to the screen via an
OpenGL texture quad pass rather than the legacy per-pixel `GL_POINTS` path.
The modern path is selected automatically when the driver supports **OpenGL 3.3**
(virtually all hardware since 2010).  On older drivers the library falls back
silently to the legacy path.

### Explicit GL Cleanup

When the application destroys its `wxGLContext`, call `wxbgi_gl_pass_destroy()`
**with the context still current** to release all internal GL objects (textures,
VAOs, shader programs).  Omitting this can cause GPU driver crashes during
process shutdown on some Windows configurations.

The `WxBgiCanvas` destructor handles this automatically, so user code only needs
this call when managing a custom `wxGLContext` lifecycle:

```cpp
// Custom GL context teardown -- make context current first.
canvas->SetCurrent(*myGlContext);
wxbgi_gl_pass_destroy();   // release VAOs, VBOs, textures, programs
delete myGlContext;
```

### Diagnostic: Legacy GL Fallback

Force the legacy `GL_POINTS` rendering path at runtime (useful for comparing
output or isolating GL driver issues):

```c
wxbgi_set_legacy_gl_render(1);   // 1 = legacy GL_POINTS, 0 = texture quad (default)
```

---

## 3D Solid Shading Modes and Lighting API

3D solid primitives support two draw modes and a Phong-style lighting model.

### Draw Mode Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `WXBGI_SOLID_WIREFRAME` | 0 | Edges only, current line colour |
| `WXBGI_SOLID_SOLID` | 1 | Filled faces, current face colour |
| `WXBGI_SOLID_FLAT` | 1 | Alias for `WXBGI_SOLID_SOLID` -- flat-shaded Phong pass (future GL-5) |
| `WXBGI_SOLID_SMOOTH` | 2 | Smooth (Gouraud-interpolated) Phong shading per vertex (future GL-5) |

### Lighting API (`wx_bgi_dds.h`)

These functions configure the Phong lighting model used when `WXBGI_SOLID_FLAT`
or `WXBGI_SOLID_SMOOTH` is active.  The parameters are stored in `BgiState` and
will be applied by the GPU solid-render pass (GL-5 phase, pending).

| Function | Description |
|----------|-------------|
| `wxbgi_solid_set_light_dir(x, y, z)` | Primary (key) light direction vector (will be normalised). |
| `wxbgi_solid_set_light_space(worldSpace)` | `1` = light direction in world space; `0` = view/eye space. |
| `wxbgi_solid_set_fill_light(x, y, z, strength)` | Secondary fill light direction + intensity scalar. |
| `wxbgi_solid_set_ambient(a)` | Ambient light intensity `[0..1]`. |
| `wxbgi_solid_set_diffuse(d)` | Diffuse reflection coefficient `[0..1]`. |
| `wxbgi_solid_set_specular(s, shininess)` | Specular intensity and Phong shininess exponent. |

Default lighting:
- Key light: `(1, 1, 2)` world-space (upper-right-forward).
- Fill light: `(-1, 0.5, 0.5)` at strength `0.3`.
- Ambient `0.2`, diffuse `0.7`, specular `0.4` / shininess `32`.
