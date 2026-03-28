# Copilot Instructions for wx_bgi_graphics

## What This Project Is

A cross-platform shared library that implements the classic **Borland BGI (Graphics Interface) C API** on top of modern OpenGL + GLFW + GLEW. It allows legacy Pascal/C/C++ graphics programs to run unmodified on Windows, Linux, and macOS. The library exposes two API layers:

- **Classic BGI API** (`wx_bgi.h`): 100+ C functions like `initgraph()`, `circle()`, `fillpoly()`, etc.
- **Extension API** (`wx_bgi_ext.h`): Modern helpers prefixed with `wxbgi_` (event pump, keyboard queue, vsync, frame control)

## Build Commands

Dependencies (GLFW 3.4, GLEW 2.2.0, GLM 1.0.1) are auto-downloaded via CMake FetchContent — no manual install needed.

**Windows (MSVC):**
```powershell
cmake -S . -B build
cmake --build build -j --config Debug
cmake --build build -j --config Release
```

**Linux / macOS:**
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

**Run tests:**
```powershell
# Windows
ctest --test-dir build -C Debug --output-on-failure

# Linux/macOS
ctest --test-dir build --output-on-failure
```

**Generate API docs** (requires Doxygen):
```bash
cmake --build build --target api_docs        # HTML
cmake --build build --target api_docs_pdf    # PDF (requires LaTeX)
```

**Enable test seams** (for CI keyboard injection):
```bash
cmake -S . -B build -DWXBGI_ENABLE_TEST_SEAMS=ON
```

## Architecture

```
src/
├── wx_bgi.h           # Public: classic BGI C API declarations
├── wx_bgi_ext.h       # Public: wxbgi_* extension API declarations
├── wx_bgi_3d.h        # Public: wxbgi_cam_* / wxbgi_cam2d_* / wxbgi_ucs_* / wxbgi_world_* camera API
├── bgi_types.h        # Public: shared types, structs, BGI constants, Camera3D
├── bgi_api.cpp        # Implements the classic BGI C functions
├── bgi_modern_api.cpp # Implements wxbgi_* extension functions
├── bgi_camera.h/cpp   # Internal: GLM-based camera math (view/proj matrices, ray-cast)
├── bgi_camera_api.cpp # Implements wxbgi_cam_* / wxbgi_cam2d_* C wrapper
├── bgi_ucs.h/cpp      # Internal: UCS frame math (local↔world transforms, AABB helpers)
├── bgi_ucs_api.cpp    # Implements wxbgi_ucs_* / wxbgi_*_world_extents / wxbgi_cam_fit_to_extents
├── bgi_world_api.cpp  # Implements wxbgi_world_* / wxbgi_ucs_point/line/circle… drawing wrappers
├── bgi_state.{cpp,h}  # Global BgiState singleton + mutex
├── bgi_draw.{cpp,h}   # Drawing primitives via OpenGL
├── bgi_font.{cpp,h}   # Vector stroke font rendering (5 profiles)
└── bgi_image.{cpp,h}  # getimage()/putimage() buffer operations
```

**Data flow:** All BGI API calls → read/write `bgi::gState` (protected by `gMutex`) → issue OpenGL draw commands → rasterize to active page buffer.

**Double buffering:** `setactivepage()` sets the draw target; `setvisualpage()` sets what's displayed. `swapbuffers()` flips them. This is the primary animation mechanism.

**Thread safety:** A single `std::mutex gMutex` in `bgi_state.h` guards all state mutations. Acquire it before touching `gState`.

## Key Conventions

### Naming
| Context | Convention | Example |
|---|---|---|
| Classic BGI C API | snake_case, no prefix | `circle()`, `fillpoly()` |
| Extension API | `wxbgi_` prefix | `wxbgi_read_key()`, `wxbgi_poll_events()` |
| Internal (namespace `bgi`) | camelCase | `setPixel()`, `drawCircleInternal()` |
| Global variables | `g` prefix | `gState`, `gMutex` |
| Constants / enums | ALL_CAPS | `BLACK`, `SOLID_LINE`, `LEFT_TEXT` |
| Public structs | PascalCase or BGI-compatible snake_case | `BgiState`, `fillsettingstype` |

### API Export Macro
All public functions must be declared with the export macro in the relevant header:
```cpp
#define BGI_API extern "C" __declspec(dllexport)  // Windows; Linux/macOS handled separately
#define BGI_CALL __cdecl

BGI_API void BGI_CALL circle(int x, int y, int radius);
```

### Header Convention
- `#pragma once` (not `#ifndef` guards)
- Internal headers (`bgi_draw.h`, `bgi_font.h`, etc.) are not installed — only `wx_bgi.h`, `wx_bgi_ext.h`, and `bgi_types.h` are public

### C++20, Static MSVC Runtime
- Use C++20 features freely (`std::vector`, `std::queue`, `std::mutex`, `std::array`, etc.)
- On Windows, the runtime is statically linked (`/MT` or `/MTd`) — the resulting DLL has no MSVC Redistributable dependency

### Camera System (`wx_bgi_3d.h`)

The camera system is **purely additive** — classic BGI drawing functions ignore it entirely.

- **Coordinate system**: Z-up, right-handed (AutoCAD/engineering). XY = ground plane, +Z = up.
- **Registry**: `BgiState::cameras` is a `std::unordered_map<std::string, Camera3D>`. A camera named `"default"` (pixel-space orthographic) is created automatically by `initwindow()`/`initgraph()`.
- **Active camera**: `BgiState::activeCamera` (string). Pass `NULL` as the name to any `wxbgi_cam_*` function to target the active camera.
- **Camera2D**: A `Camera3D` with `is2D = true`. View/projection matrices are computed on demand from `pan2dX/Y`, `zoom2d`, `rot2dDeg`, `worldHeight2d`. Zoom pivot math is in `wxbgi_cam2d_zoom_at()`.
- **Internal math**: `bgi_camera.h/cpp` contain GLM-based functions (`cameraViewMatrix`, `cameraProjMatrix`, `cameraWorldToScreen`, `cameraScreenToRay`). These are **not** part of the public API — add new camera math here.
- **`worldToScreen` Y convention**: NDC +1 → screen pixel row 0 (top-left), matching BGI. The Y-flip is: `screenY = (1 - ndcY) / 2 * vpH`.
- **Default camera ortho**: `orthoBottom = height, orthoTop = 0` (flipped) maps `(0,0)` world → top-left screen pixel.
- **Adding a new camera function**: declare in `wx_bgi_3d.h` with `BGI_API`/`BGI_CALL`, implement in `bgi_camera_api.cpp` (lock `gMutex`, call `findCamera(resolveName(name))`), add math helpers to `bgi_camera.h/cpp` if GLM is needed.

### UCS System (`wx_bgi_3d.h`)

- **Registry**: `BgiState::ucsSystems` is a `std::unordered_map<std::string, CoordSystem>`. A UCS named `"world"` (identity frame) is created automatically on `initwindow()` and cannot be destroyed.
- **Active UCS**: `BgiState::activeUcs` (string). Pass `NULL` to any `wxbgi_ucs_*` function to target the active UCS.
- **Axes are auto-orthonormalised** on `wxbgi_ucs_set_axes()` via `ucsOrthonormalise()` (Gram-Schmidt: Z = X × Y, then Y = Z × X).
- **Internal math**: `bgi_ucs.h/cpp` contain `ucsLocalToWorldMatrix`, `ucsWorldToLocalMatrix`, `ucsFromNormal`, `ucsOrthonormalise`, `worldExtentsExpand`. Not part of the public API.
- **World extents**: `BgiState::worldExtents` is a `WorldExtents` AABB. Set with `wxbgi_set_world_extents()`, expanded point-by-point with `wxbgi_expand_world_extents()`. `wxbgi_cam_fit_to_extents()` auto-adjusts any camera to frame the extents.
- **Adding a new UCS function**: declare in `wx_bgi_3d.h`, implement in `bgi_ucs_api.cpp` (lock `gMutex`, call `findUcs(resolveUcsName(name))`), add math to `bgi_ucs.h/cpp` if needed.

### World-Coordinate Drawing (`wx_bgi_3d.h` — Phase 3)

The world drawing API projects floating-point world/UCS coordinates through the **active camera** into the BGI pixel buffer. Classic BGI drawing functions are unaffected.

- **Active camera**: set with `wxbgi_cam_set_active(name)` before calling `wxbgi_world_*` / `wxbgi_ucs_*` draw functions.
- **Coordinate flow**: world point → `cameraWorldToScreen` → device pixel → subtract `viewport.left/top` → pass to `drawLineInternal` / `drawCircleInternal` / `bgi::drawText`. The net effect is the correct device pixel; the BGI viewport clips world drawing the same way it clips classic drawing.
- **UCS variants** (`wxbgi_ucs_line`, `wxbgi_ucs_circle`, etc.): first call `ucsLocalToWorld`, then project. For circle radius the UCS X-axis is used to estimate the world-space radius.
- **`wxbgi_ucs_to_screen`**: combined UCS-local → world → camera → screen pixel pipeline in one call.
- **Implementing a new world draw function**: acquire `gMutex`, call `findCam(resolveActiveCam())`, call `projectToVpRel()` (in `bgi_world_api.cpp` anonymous namespace), call the relevant `bgi::draw*Internal`, call `bgi::flushToScreen()`.
- **All implementations** live in `src/bgi_world_api.cpp` — do not put world draw logic in `bgi_camera_api.cpp` or `bgi_ucs_api.cpp`.

### Extended Key Codes (DOS-style)
The keyboard queue emulates DOS BGI behavior: special keys produce a `0` byte followed by a scancode. Use `wxbgi_read_key()` and `wxbgi_is_key_down()` from `wx_bgi_ext.h` for modern key handling.

### Color Palette
The library maps the 16-color classic BGI palette. When adding drawing features, use `gState.palette[]` for color resolution — do not hardcode RGB values.

### Pascal Compatibility
The `Graph.pas` shim in `examples/bgidemo-pascal/` bridges FreePascal's `Graph` unit to this library. Pascal examples must compile with FreePascal (FPC); CMake detects FPC at configure time and skips Pascal targets if absent.

## Examples

- `examples/cpp/bgi_api_coverage.cpp` — exercises almost every BGI API function; use as a reference implementation test
- `examples/cpp/wxbgi_keyboard_queue.cpp` — demonstrates the extension keyboard API
- `examples/python/bgi_api_coverage.py` — ctypes bindings; shows the calling convention from Python
