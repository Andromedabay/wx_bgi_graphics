# Camera3D -- Code Map

This document describes where every piece of the 3D camera system lives in
the `wx_bgi_graphics` source tree, from the raw data struct through the math
layer to the public C API.

---

## 1. Data -- `Camera3D` struct (`src/bgi_types.h`)

`Camera3D` is a plain-old-data struct.  All fields have in-class default values.

```cpp
struct Camera3D
{
    // Eye / target / up (Z-up, right-handed coordinate system)
    float eyeX{0.f},    eyeY{-10.f},  eyeZ{5.f};
    float targetX{0.f}, targetY{0.f}, targetZ{0.f};
    float upX{0.f},     upY{0.f},     upZ{1.f};

    // Projection
    CameraProjection projection{CameraProjection::Orthographic};
    float fovYDeg{45.f};        // Perspective only
    float nearPlane{0.1f};
    float farPlane{10000.f};

    // Orthographic extents (all-zero triggers worldHeight2d auto-fit)
    float orthoLeft{0.f},   orthoRight{0.f};
    float orthoBottom{0.f}, orthoTop{0.f};

    // Screen-space viewport override in pixels (all-zero = full window)
    int vpX{0}, vpY{0}, vpW{0}, vpH{0};

    // 2-D convenience layer -- see Camera2D_Map.md for details
    bool  is2D{false};
    float pan2dX{0.f}, pan2dY{0.f};
    float zoom2d{1.f};
    float rot2dDeg{0.f};
    float worldHeight2d{2.f};   // Used by 2-D cameras and ortho auto-fit
};
```

**Ownership:** `Camera3D` is never heap-allocated standalone.  It lives as the
`camera` field inside a `DdsCamera` object (`src/bgi_dds.h`), which is held by
a `std::shared_ptr<DdsCamera>`.  The DDS scene graph owns every `DdsCamera`.
`BgiState::cameras` (`src/bgi_types.h`) is a `std::unordered_map<std::string,
std::shared_ptr<DdsCamera>>` -- a name-indexed look-up table into that graph.

---

## 2. Math layer -- `src/bgi_camera.h` / `src/bgi_camera.cpp`

Pure functions; no global state, no mutex.  All take `const Camera3D &`.
Depends on GLM (fetched automatically via CMake FetchContent).

| Function | Purpose |
|---|---|
| `cameraEffectiveViewport(cam, winW, winH, x, y, w, h)` | Resolve the screen-pixel viewport for a camera.  Falls back to full window when `vpW == 0`. |
| `cameraAspectRatio(cam, winW, winH)` | Aspect ratio derived from `cameraEffectiveViewport`. |
| `cameraViewMatrix(cam)` | Build GLM `mat4` view matrix from eye/target/up. Branches into 2-D pan/zoom/rot math when `is2D`. |
| `cameraProjMatrix(cam, aspectRatio)` | Build GLM `mat4` projection (perspective or ortho). Auto-fit ortho when all extents are zero. |
| `cameraVPMatrix(cam, aspectRatio)` | Combined `proj * view`. |
| `cameraWorldToScreen(cam, winW, winH, wx, wy, wz, sx, sy)` | Project world point to screen pixel.  Returns `false` when outside the view frustum. |
| `cameraWorldToScreenForced(cam, winW, winH, wx, wy, wz, sx, sy)` | Same but no frustum test (used by solid renderer for painter's-algorithm depth sort). |
| `cameraWorldToClip(cam, winW, winH, wx, wy, wz)` | Returns the raw 4D clip-space `glm::vec4` (`xyzw`). |
| `cameraClipToScreen(cam, winW, winH, clip, sx, sy)` | Convert a clip-space `vec4` to a screen pixel (NDC divide + viewport transform). |
| `clipPolyByPlane(poly, plane)` | Sutherland-Hodgman: clip a polygon (vec4 vertices) against one half-space. |
| `clipPolyZPlanes(poly)` | Clip against the near (`Z+W > 0`) and far (`-Z+W > 0`) planes. |
| `clipLineByPlane(A, B, plane)` | Clip a line segment against one half-space; returns `false` when fully outside. |
| `clipLineZPlanes(A, B)` | Clip a line segment against near/far Z planes. |
| `cameraScreenToRay(cam, winW, winH, sx, sy, ...)` | Unproject a screen pixel back to a world-space ray (origin + direction). |
| `matToFloatArray(m, out16)` | Write a `glm::mat4` column-major into a `float[16]` (for the public matrix getters). |

### View matrix (3D path)

```
V = lookAt(eye, target, up)
```

### Projection matrix paths

```
Perspective:   proj = perspective(fovYDeg, aspect, nearPlane, farPlane)
Ortho auto:    halfH = worldHeight2d/2;  halfW = halfH * aspect
               proj = ortho(-halfW, halfW, -halfH, halfH, near, far)
Ortho explicit: proj = ortho(orthoLeft, orthoRight, orthoBottom, orthoTop, near, far)
```

The default `"default"` camera uses explicit extents with `orthoBottom = windowHeight`,
`orthoTop = 0` -- this Y-flip maps world (0,0) to the top-left screen pixel.

---

## 3. Public C API -- `src/bgi_camera_api.cpp` / `src/wx_bgi_3d.h`

All functions are `BGI_API` / `BGI_CALL` (C linkage, dllexport on Windows).
Each acquires `bgi::gMutex` before touching state, resolves the camera name
(NULL means active camera), retrieves a raw `Camera3D *` via `findCamera()`,
and calls the math layer.

### Lifecycle

| Function | Returns | Description |
|---|---|---|
| `wxbgi_cam_create(name, type)` | `1/-1/-2` | Create ortho or perspective camera.  Fails with `grDuplicateName (-22)` if name exists or equals `"default"`. |
| `wxbgi_cam_destroy(name)` | void | Remove from registry and DDS.  `"default"` cannot be destroyed. |
| `wxbgi_cam_set_active(name)` | void | Set the active camera used when `NULL` is passed to other functions. |
| `wxbgi_cam_get_active()` | `const char *` | Return the active camera name. |

### Eye / target / up

| Function | Description |
|---|---|
| `wxbgi_cam_set_eye(name, x, y, z)` | Set camera position in world space. |
| `wxbgi_cam_set_target(name, x, y, z)` | Set look-at target point. |
| `wxbgi_cam_set_up(name, x, y, z)` | Set up vector (default `(0, 0, 1)` = Z-up). |
| `wxbgi_cam_get_eye(name, *x, *y, *z)` | Read camera position. |
| `wxbgi_cam_get_target(name, *x, *y, *z)` | Read target point. |
| `wxbgi_cam_get_up(name, *x, *y, *z)` | Read up vector. |

### Projection

| Function | Description |
|---|---|
| `wxbgi_cam_set_perspective(name, fovY, near, far)` | Switch to perspective; set fovY in degrees. |
| `wxbgi_cam_set_ortho(name, l, r, b, t, near, far)` | Explicit ortho extents. |
| `wxbgi_cam_set_ortho_auto(name, worldUnitsHigh, near, far)` | Auto-fit ortho: set `worldHeight2d`, zero the explicit extents. |

### Screen viewport

| Function | Description |
|---|---|
| `wxbgi_cam_set_screen_viewport(name, x, y, w, h)` | Restrict the camera to a pixel sub-rectangle.  Pass zeros to use the full window. |

### Matrix getters (for custom OpenGL rendering)

| Function | Output |
|---|---|
| `wxbgi_cam_get_view_matrix(name, float[16])` | Column-major view matrix. |
| `wxbgi_cam_get_proj_matrix(name, float[16])` | Column-major projection matrix. |
| `wxbgi_cam_get_vp_matrix(name, float[16])` | Column-major combined `proj * view`. |

### Coordinate utilities

| Function | Returns | Description |
|---|---|---|
| `wxbgi_cam_world_to_screen(name, wx, wy, wz, *sx, *sy)` | `1/0` | Project world point to screen pixel. Returns 0 when outside frustum. |
| `wxbgi_cam_screen_to_ray(name, sx, sy, *ox, *oy, *oz, *dx, *dy, *dz)` | void | Unproject screen pixel to world-space ray. |

---

## 4. DDS ownership -- `src/bgi_dds.h`

```cpp
class DdsCamera : public DdsObject
{
public:
    std::string name;   // Registry key in BgiState::cameras
    Camera3D    camera; // Full camera data lives here
    DdsCamera() { type = DdsObjectType::Camera; }
};
```

`BgiState::cameras` is an `unordered_map<string, shared_ptr<DdsCamera>>`.
The DDS tree (`BgiState::dds`) is the true owner; the map is a fast-lookup
index.  Both are wiped and recreated on `initwindow()` / `closegraph()`.

---

## 5. Serialization -- `src/bgi_dds_serial.cpp`

`DdsCamera` is serialized to/from **DDJ** (JSON) and **DDY** (YAML) as part of
the full DDS scene.

JSON example:

```json
{
  "type": "Camera",
  "name": "cam3d",
  "is2D": false,
  "projection": "perspective",
  "fovYDeg": 60.0,
  "nearPlane": 0.5,
  "farPlane": 5000.0,
  "eye":    [300.0, 0.0, 120.0],
  "target": [  0.0, 0.0,   0.0],
  "up":     [  0.0, 0.0,   1.0],
  "viewport": [960, 0, 480, 480]
}
```

All `Camera3D` fields are persisted; `is2D` determines which fields are
meaningful when the scene is reloaded.

---

## 6. Where Camera3D fields are accessed

| File | How Camera3D is used |
|---|---|
| `bgi_camera_api.cpp` | Direct field writes (setters) and reads (getters). |
| `bgi_state.cpp` | Direct field writes to initialise the `"default"` camera in `resetStateForWindow()`. |
| `bgi_dds_serial.cpp` | Read all fields to serialise; write all fields to deserialise. |
| `bgi_camera.cpp` | Reads fields to compute matrices, viewport, projections. |
| `bgi_dds_render.cpp` | Passes `Camera3D &` to math functions only -- no direct field access. |
| `bgi_solid_render.cpp` | Passes `Camera3D &` to `cameraViewMatrix`, `cameraWorldToClip` -- no direct field access. |
| `bgi_world_api.cpp` | Passes `Camera3D *` to `cameraWorldToScreen` -- no direct field access. |
| `bgi_export.cpp` | Passes `Camera3D &` to `cameraEffectiveViewport` -- no direct field access. |

The rule: **only `bgi_camera_api.cpp`, `bgi_state.cpp`, and
`bgi_dds_serial.cpp` may touch `Camera3D` fields directly.**  Everything else
treats it as an opaque token passed to the math layer.

---

## 7. File dependency diagram

```
wx_bgi_3d.h          (public declarations)
    |
    v
bgi_camera_api.cpp   (public C API implementation)
    |---- bgi_camera.h/cpp   (GLM math: matrices, projections, clip)
    |---- bgi_dds.h           (DdsCamera owner)
    |---- bgi_state.h         (gState.cameras map, gMutex)
    |---- bgi_types.h         (Camera3D, GraphStatus enum)
```

---

## See also

- [Camera2D_Map.md](./Camera2D_Map.md) -- 2D camera convenience layer
- [DDS.md](../DDS.md) -- DDS / CHDOP / DDJ / DDY scene graph documentation
- [Tutorial.md](../user-guide/Tutorial.md) -- Double-buffering and animation loop
