# Camera2D -- Code Map

A Camera2D in `wx_bgi_graphics` is **not a separate struct**.  It is a
`Camera3D` with the flag `is2D = true`.  This means every `wxbgi_cam_*`
function works on a 2D camera, and the `wxbgi_cam2d_*` convenience functions
are thin wrappers that manipulate only the 2D-specific fields.

For the complete `Camera3D` field reference see [Camera3D_Map.md](./Camera3D_Map.md).

---

## 1. The 2D-specific fields inside `Camera3D` (`src/bgi_types.h`)

```cpp
// --- 2-D convenience layer (active when is2D == true) ---
bool  is2D{false};          // Switch: enables 2D matrix math
float pan2dX{0.f};          // World X of the view centre
float pan2dY{0.f};          // World Y of the view centre
float zoom2d{1.f};          // Zoom level (1 = nominal, >1 = zoomed in)
float rot2dDeg{0.f};        // View rotation around Z axis, degrees
float worldHeight2d{2.f};   // World-units visible vertically at zoom = 1
```

The 3D eye/target/up, ortho extents, and fovY fields are **ignored** when
`is2D == true`.  The matrix math derives everything from the five 2D fields
above plus the screen viewport dimensions.

---

## 2. Matrix math -- `src/bgi_camera.cpp`

Two functions branch on `is2D`:

### `cameraViewMatrix(cam)` -- 2D path

```cpp
if (cam.is2D)
{
    const float cosR = std::cos(glm::radians(cam.rot2dDeg));
    const float sinR = std::sin(glm::radians(cam.rot2dDeg));
    return glm::lookAt(
        glm::vec3(cam.pan2dX, cam.pan2dY, 1.f),   // eye directly above pan target
        glm::vec3(cam.pan2dX, cam.pan2dY, 0.f),   // target on XY plane (Z = 0)
        glm::vec3(-sinR, cosR, 0.f));              // up = rotated by rot2dDeg
}
```

The eye is placed at `(pan2dX, pan2dY, 1)` looking straight down at
`(pan2dX, pan2dY, 0)` on the XY ground plane.  Rotation is encoded in the up
vector: `(-sin(rot), cos(rot), 0)`.

**Key insight:** panning moves the eye AND the target together, so the view
always looks straight down.  Rotation spins the horizon without tilting.

### `cameraProjMatrix(cam, aspect)` -- 2D path

```cpp
if (cam.is2D)
{
    const float halfH = (cam.worldHeight2d / cam.zoom2d) * 0.5f;
    const float halfW = halfH * aspectRatio;
    return glm::ortho(-halfW, halfW, -halfH, halfH,
                      cam.nearPlane, cam.farPlane);
}
```

The projection is **always orthographic** for a 2D camera.  Zoom is applied
by shrinking the half-extents: `zoom2d = 2` shows half as many world units.
The projection is centred at the origin; the view matrix handles the pan offset.

### Visible world area formula

```
visible_height (world units) = worldHeight2d / zoom2d
visible_width  (world units) = visible_height * (vpW / vpH)
```

---

## 3. Public C API -- `src/bgi_camera_api.cpp` / `src/wx_bgi_3d.h`

All `wxbgi_cam2d_*` functions:
1. Acquire `bgi::gMutex`.
2. Resolve the name (NULL = active camera) via `resolveName()`.
3. Look up `Camera3D *` via `findCamera()` (returns `nullptr` if not found).
4. Write or read one or more 2D fields.

### Lifecycle

| Function | Returns | Description |
|---|---|---|
| `wxbgi_cam2d_create(name)` | `1/-1/-2` | Create a 2D camera (`is2D = true`, ortho, Z range -10000..+10000). Fails with `grDuplicateName (-22)` if name already exists or equals `"default"`. |

Defaults set by `wxbgi_cam2d_create`:

| Field | Default |
|---|---|
| `is2D` | `true` |
| `projection` | `Orthographic` |
| `pan2dX`, `pan2dY` | `0, 0` |
| `zoom2d` | `1.0` |
| `rot2dDeg` | `0.0` |
| `worldHeight2d` | window pixel height (so 1 world unit = 1 pixel at zoom 1) |
| `nearPlane` | `-10000` |
| `farPlane` | `+10000` |

### Pan

| Function | Description |
|---|---|
| `wxbgi_cam2d_set_pan(name, x, y)` | Set the world-space centre of the view. |
| `wxbgi_cam2d_pan_by(name, dx, dy)` | Offset pan by `(dx, dy)` world units. |
| `wxbgi_cam2d_get_pan(name, *x, *y)` | Read current pan position. |

### Zoom

| Function | Description |
|---|---|
| `wxbgi_cam2d_set_zoom(name, zoom)` | Set absolute zoom (clamped to > 1e-6). |
| `wxbgi_cam2d_zoom_at(name, factor, pivotX, pivotY)` | Multiply zoom by `factor` while keeping world point `(pivotX, pivotY)` stationary on screen. |
| `wxbgi_cam2d_get_zoom(name, *zoom)` | Read current zoom level. |

**Zoom-at pivot math:**

```
newZoom = clamp(oldZoom * factor, 1e-6, inf)
ratio   = oldZoom / newZoom
pan_new = pivot + (pan_old - pivot) * ratio
```

This keeps the world point that was under the cursor stationary, matching
the behaviour of CAD and map applications.

### Rotation

| Function | Description |
|---|---|
| `wxbgi_cam2d_set_rotation(name, angleDeg)` | Rotate the view around Z by `angleDeg` degrees. |
| `wxbgi_cam2d_get_rotation(name, *angleDeg)` | Read current rotation. |

### World height

| Function | Description |
|---|---|
| `wxbgi_cam2d_set_world_height(name, h)` | Set how many world units are visible top-to-bottom at `zoom = 1`. |

### Inherited 3D API functions that also work on 2D cameras

| Function | Useful for 2D? | Notes |
|---|---|---|
| `wxbgi_cam_set_screen_viewport(name, x, y, w, h)` | Yes | Restrict camera to a sub-rectangle of the window. |
| `wxbgi_cam_get_view_matrix(name, float[16])` | Yes | Returns the pan/rot view matrix. |
| `wxbgi_cam_get_proj_matrix(name, float[16])` | Yes | Returns the zoom ortho matrix. |
| `wxbgi_cam_world_to_screen(name, wx, wy, wz, *sx, *sy)` | Yes | Project any world XYZ through the 2D camera to a screen pixel. |
| `wxbgi_cam_screen_to_ray(name, sx, sy, ...)` | Partial | Returns a ray perpendicular to the XY plane at the unprojected position. |
| `wxbgi_cam_set_eye/target/up` | No | Ignored when `is2D = true`. |
| `wxbgi_cam_set_perspective` | No | 2D cameras are always orthographic. |

---

## 4. Interaction with the DDS render pipeline

When `wxbgi_render_dds(cameraName)` is called:

1. `bgi_dds_render.cpp` looks up the camera in `gState.cameras`.
2. It retrieves `DdsCamera::camera` (a `Camera3D &`).
3. It passes that reference to `cameraWorldToScreen` / `cameraWorldToClip` /
   `cameraClipToScreen` from `bgi_camera.cpp`.
4. Because the math functions branch on `is2D`, the 2D pan/zoom/rot are
   automatically applied to every projected point.

No special DDS render path exists for 2D cameras -- the matrix math handles
the difference transparently.

---

## 5. Interaction with the solid renderer (`bgi_solid_render.cpp`)

The 3D solid renderer (cylinder, box, sphere, cone, torus) is also viewable
through a 2D camera.  The solids are represented as triangle meshes in 3D
world space; `cameraViewMatrix` and `cameraProjMatrix` project them through
the 2D ortho+pan+zoom transform, producing a correct top-down or oblique view.

The eye for a 2D camera is at `(pan2dX, pan2dY, 1)`, so objects at Z > 0
appear above the ground plane and objects at Z < 0 appear below it -- a
natural top-down rendering.

---

## 6. Serialization -- `src/bgi_dds_serial.cpp`

2D cameras are serialized identically to 3D cameras.  The `is2D` flag
distinguishes them.  Only the meaningful 2D fields are round-tripped:

```json
{
  "type": "Camera",
  "name": "cam2d",
  "is2D": true,
  "projection": "orthographic",
  "pan": [0.0, 0.0],
  "zoom": 1.0,
  "rotation": 0.0,
  "worldHeight2d": 480.0,
  "nearPlane": -10000.0,
  "farPlane": 10000.0,
  "viewport": [480, 0, 480, 480]
}
```

---

## 7. Typical usage example (C++)

```cpp
#include "wx_bgi.h"
#include "wx_bgi_3d.h"
#include "wx_bgi_ext.h"

int main()
{
    initwindow(960, 480, "2D Camera Demo");

    // Create a 2D camera occupying the right half of the window.
    wxbgi_cam2d_create("top");
    wxbgi_cam_set_screen_viewport("top", 480, 0, 480, 480);
    wxbgi_cam2d_set_world_height("top", 200.f);  // 200 world units tall
    wxbgi_cam2d_set_pan("top", 0.f, 0.f);        // centred on origin

    // Build the scene in the DDS.
    setcolor(15);
    wxbgi_world_rectangle(-50.f, -40.f, 0.f, 50.f, 40.f, 0.f);

    // Render through the 2D camera.
    setviewport(480, 0, 959, 479, 1);
    wxbgi_render_dds("top");
    setviewport(0, 0, 959, 479, 0);

    // Interactive pan / zoom loop.
    while (!wxbgi_should_close())
    {
        wxbgi_poll_events();

        if (wxbgi_is_key_down('W')) wxbgi_cam2d_pan_by("top",  0.f,  5.f);
        if (wxbgi_is_key_down('S')) wxbgi_cam2d_pan_by("top",  0.f, -5.f);
        if (wxbgi_is_key_down('A')) wxbgi_cam2d_pan_by("top", -5.f,  0.f);
        if (wxbgi_is_key_down('D')) wxbgi_cam2d_pan_by("top",  5.f,  0.f);
        if (wxbgi_is_key_down('+')) wxbgi_cam2d_set_zoom("top",
            [&]{ float z; wxbgi_cam2d_get_zoom("top",&z); return z*1.05f; }());
        if (wxbgi_is_key_down('Q')) wxbgi_cam2d_set_rotation("top",
            [&]{ float r; wxbgi_cam2d_get_rotation("top",&r); return r+2.f; }());

        cleardevice();
        setviewport(480, 0, 959, 479, 1);
        wxbgi_render_dds("top");
        setviewport(0, 0, 959, 479, 0);
        swapbuffers();
    }

    closegraph();
}
```

---

## 8. File dependency diagram

```
wx_bgi_3d.h               (public declarations: wxbgi_cam2d_*)
    |
    v
bgi_camera_api.cpp         (wxbgi_cam2d_* implementations)
    |---- bgi_camera.h/cpp  (cameraViewMatrix 2D branch, cameraProjMatrix 2D branch)
    |---- bgi_dds.h          (DdsCamera::camera field = Camera3D)
    |---- bgi_state.h        (gState.cameras, gMutex)
    |---- bgi_types.h        (Camera3D with is2D fields, GraphStatus)
```

---

## See also

- [Camera3D_Map.md](./Camera3D_Map.md) -- 3D camera math, full API, serialization
- [DDS.md](../DDS.md) -- DDS / CHDOP / DDJ / DDY scene graph documentation
- [Tutorial.md](../user-guide/Tutorial.md) -- Animation loop and double-buffering
