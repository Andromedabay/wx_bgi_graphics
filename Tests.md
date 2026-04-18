# wx_BGI_Graphics — Test Suite

This document describes the automated test suite for wx_BGI_Graphics.  
All tests are registered with CTest and run automatically in CI.

See **[Building.md](./Building.md)** for build commands.  
See **[WxWidgets.md](./WxWidgets.md)** for the embedded canvas integration guide.

---

## Test Summary

The current suite contains **28** CTest targets.

| # | Test Name | Category | Source File | Notes |
|---|-----------|----------|-------------|-------|
| 1 | `bgi_api_coverage_cpp` | C++ | `examples/cpp/bgi_api_coverage.cpp` | Classic BGI API coverage |
| 2 | `wxbgi_camera_demo_cpp` | C++ | `examples/cpp/wxbgi_camera_demo.cpp` | Camera/UCS/world demo — `--test` flag |
| 3 | `test_field_vis` | C++ / field vis | `examples/cpp/test_field_vis.cpp` | Scalar grid, vector glyphs, legend helpers |
| 4 | `wxbgi_openlb_live_demo` | C++ / wx standalone | `examples/cpp/wxbgi_openlb_live_demo.cpp` | OpenLB-style non-blocking live loop — `--test` flag |
| 5 | `wxbgi_set_operations_demo_cpp` | C++ / DDS | `examples/cpp/wxbgi_set_operations_demo.cpp` | Demo smoke test for retained translate/union/intersection/difference scene construction |
| 6 | `test_dds_serialize` | C++ / DDS | `examples/cpp/` | DDS JSON + YAML serialization |
| 7 | `test_dds_deserialize` | C++ / DDS | `examples/cpp/` | DDS JSON + YAML deserialization |
| 8 | `test_dds_clear` | C++ / DDS | `examples/cpp/` | DDS scene clear |
| 9 | `test_dds_cam2d_yz` | C++ / DDS | `examples/cpp/` | 2-D camera in YZ plane |
| 10 | `test_dds_cam3d_persp` | C++ / DDS | `examples/cpp/` | 3-D perspective camera |
| 11 | `test_dds_csg` | C++ / DDS | `examples/cpp/test_dds_csg.cpp` | Retained Transform / SetUnion / SetIntersection / SetDifference nodes, draw-mode propagation, ordered difference, and exact 3D CSG regressions |
| 12 | `test_multi_scene` | C++ / DDS | `examples/cpp/test_multi_scene.cpp` | Multi-CHDOP: scene lifecycle, camera→scene assignment, JSON round-trip |
| 13 | `wxbgi_multi_scene_demo` | C++ / DDS | `examples/cpp/wxbgi_multi_scene_demo.cpp` | 3-panel multi-scene demo — `--test` flag |
| 14 | `test_solids` | C++ / DDS | `examples/cpp/` | 3-D solid primitives (box, sphere, cylinder, cone, torus) |
| 15 | `test_input_hooks` | C++ | `examples/cpp/` | Keyboard hook / input-inject (requires `WXBGI_ENABLE_TEST_SEAMS`) |
| 16 | `test_input_bypass` | C++ | `examples/cpp/` | Input bypass + scroll hook |
| 17 | `bgi_api_coverage_python` | Python | `examples/python/bgi_api_coverage.py` | ctypes BGI API coverage |
| 18 | `bgi_api_coverage_pascal_build` | Pascal | `examples/demoFreePascal/demo_bgi_api_coverage.pas` | FPC compile |
| 19 | `bgi_api_coverage_pascal_run` | Pascal | same | GLFW → wx-standalone path |
| 20 | `bgi_canvas_coverage_pascal_build` | Pascal | `examples/demoFreePascal/demo_bgi_canvas_coverage.pas` | FPC compile |
| 21 | `bgi_canvas_coverage_pascal_run` | Pascal | same | wx-only path (no GLFW) |
| 22 | `test_input_hooks_pascal_build` | Pascal | `examples/demoFreePascal/test_input_hooks.pas` | FPC compile |
| 23 | `test_input_hooks_pascal_run` | Pascal | same | Keyboard hook — Pascal |
| 24 | `test_input_bypass_pascal_build` | Pascal | `examples/demoFreePascal/test_input_bypass.pas` | FPC compile |
| 25 | `test_input_bypass_pascal_run` | Pascal | same | Input bypass — Pascal |
| 26 | `wx_bgi_solids_test` | wxWidgets | `examples/wx/wx_bgi_solids_test.cpp` | 3-D solids in embedded canvas |
| 27 | `wx_bgi_3d_orbit_test` | wxWidgets | `examples/wx/wx_bgi_3d_orbit_test.cpp` | 3-D orbit animation in embedded canvas |
| 28 | `wx_bgi_canvas_coverage_test` | wxWidgets | `examples/wx/wx_bgi_canvas_coverage_test.cpp` | Full BGI API coverage — C++ embedded `WxBgiCanvas` |

Pascal tests (18–25) run only when a matching-architecture FreePascal compiler (`fpc`) is found at CMake configure time.

---

## Test Categories

### C++ Core Tests (1–16)

These tests link directly against `wx_bgi_opengl` (the DLL/shared library) and exercise the full API surface through a real graphics window.

| Test | What it checks |
|------|----------------|
| `bgi_api_coverage_cpp` | All classic BGI drawing functions: `line`, `circle`, `arc`, `bar`, `fillpoly`, `outtextxy`, `getimage`/`putimage`, page switching, palette, viewport |
| `wxbgi_camera_demo_cpp` | Camera creation and orbiting (`wxbgi_cam_*`), UCS frames (`wxbgi_ucs_*`), world-coordinate drawing (`wxbgi_world_*`), camera fit-to-extents |
| `test_field_vis` | Scalar-grid, vector-grid, and legend helpers used for solver visualisation |
| `wxbgi_openlb_live_demo` | OpenLB-style non-blocking render loop using the field-visualisation helpers |
| `wxbgi_set_operations_demo_cpp` | Builds the retained translate / union / intersection / difference showcase scene and exits via `--test` |
| `test_dds_serialize` | `wxbgi_dds_save_json` / `wxbgi_dds_save_yaml` round-trip correctness |
| `test_dds_deserialize` | `wxbgi_dds_load_json` / `wxbgi_dds_load_yaml` round-trip correctness |
| `test_dds_clear` | `wxbgi_dds_clear` removes all scene objects |
| `test_dds_cam2d_yz` | 2-D orthographic camera in the YZ plane; coordinate projection |
| `test_dds_cam3d_persp` | 3-D perspective projection; worldToScreen math |
| `test_dds_csg` | `wxbgi_dds_translate`, `wxbgi_dds_union`, `wxbgi_dds_intersection`, `wxbgi_dds_difference`, retained child references, JSON tokens for Transform / SetUnion / SetIntersection / SetDifference, draw-mode propagation to retained set-op nodes, a 2D ordered-difference case, and exact 3D union/intersection/difference regressions |
| `test_multi_scene` | Multi-CHDOP scene lifecycle: `wxbgi_dds_scene_create`, `wxbgi_dds_scene_exists`, `wxbgi_dds_scene_set_active`, `wxbgi_cam_set_scene`/`wxbgi_cam_get_scene`, scene isolation (objects in "secondary" do not appear in "default"), `wxbgi_dds_scene_destroy` (cameras fall back to "default"), JSON round-trip preserving scene assignment |
| `wxbgi_multi_scene_demo` | Full 3-panel multi-scene demo in `--test` mode: builds "main" and "secondary" scene graphs, creates three cameras (two perspective + one pixel-space ortho), renders one frame via `wxbgi_render_dds`, exits cleanly |
| `test_solids` | `wxbgi_solid_box`, `wxbgi_solid_sphere`, `wxbgi_solid_cylinder`, `wxbgi_solid_cone`, `wxbgi_solid_torus` — all draw modes |
| `test_input_hooks` | Keyboard queue injection via test seams; hook ordering (see [Test Seam Policy](#keyboard-injection-test-seams-wxbgi_enable_test_seams)) |
| `test_input_bypass` | Scroll bypass, input hook chaining, `wxbgi_set_input_hook` |

### Python Test (17)

```bash
python examples/python/bgi_api_coverage.py build/Debug/wx_bgi_opengl.dll    # Windows
python3 examples/python/bgi_api_coverage.py build/libwx_bgi_opengl.so       # Linux
python3 examples/python/bgi_api_coverage.py build/libwx_bgi_opengl.dylib    # macOS
```

Exercises the full BGI API surface via Python `ctypes` — the same functions as the C++ coverage test.

### FreePascal Tests (18–25)

Each Pascal test has a **build** step (FPC compile) and a **run** step.  
The build step copies the up-to-date DLL into the output directory automatically.

| Tests | Source | What it checks |
|-------|--------|----------------|
| 18–19 `bgi_api_coverage_pascal_*` | `demo_bgi_api_coverage.pas` | Full BGI API surface: GLFW `initgraph` / `closegraph` → `wxbgi_wx_frame_create`; all classic drawing functions + extension API |
| 20–21 `bgi_canvas_coverage_pascal_*` | `demo_bgi_canvas_coverage.pas` | wx-only path (no GLFW): `wxbgi_wx_app_create` + `wxbgi_wx_frame_create`; colorful primitives phase + blue-screen phase + extension API |
| 22–23 `test_input_hooks_pascal_*` | `test_input_hooks.pas` | Pascal keyboard hook API |
| 24–25 `test_input_bypass_pascal_*` | `test_input_bypass.pas` | Pascal input bypass / scroll hooks |

> **Note:** `demo_bgi_canvas_coverage.pas` (tests 20–21) is the **wx-only** version — it does not call `initgraph` or `closegraph`.  It uses a `YieldMs()` helper to keep the wx event loop alive between draw phases.

#### Linux / macOS: FreePascal FPU Exception Mask

On Linux and macOS, FreePascal enables strict x87 FPU exception masking by default.
GTK's SVG icon renderer (`librsvg` → `libxml2`) performs floating-point operations
(NaN / denormal arithmetic) that violate this strict mask and raise `SIGFPE`,
appearing as:

```
An unhandled exception occurred at $...:
EInvalidOp: Invalid floating point operation
```

All Pascal test programs that open a wx window already include the fix at the top
of their `begin` block:

```pascal
uses SysUtils, Math;

begin
  SetExceptionMask([exInvalidOp, exDenormalized, exZeroDivide,
                    exOverflow, exUnderflow, exPrecision]);
  // ... rest of program
```

**When writing new Pascal programs** on Linux / macOS that use `wxbgi_wx_app_create`
or any GTK-backed wx function, add `Math` to the `uses` clause and call
`SetExceptionMask` as the very first statement.

#### Linux / macOS: GLEW Initialisation Before `wxbgi_wx_app_main_loop`

`WxBgiCanvas::Render()` performs lazy GLEW initialisation the first time the
canvas paints.  In the C++ tests this paint occurs inside the wx event loop.
Pascal (and Python / C) programs that call GL-dependent extension functions such
as `wxbgi_write_pixels_rgba8` *before* `wxbgi_wx_app_main_loop` would previously
crash with `EAccessViolation` (null `glWindowPos2i` function pointer).

`wxbgi_wx_frame_create` now calls `wxApp::Yield()` followed by an explicit
`WxBgiCanvas::Render()` on Linux / macOS after `Show(true)`, so that the GL
context is fully initialised — including the DLL-local GLEW function pointer
table — before the function returns.  This makes GL extension calls safe
immediately after `wxbgi_wx_frame_create` on all platforms.

### wxWidgets Canvas Tests (26–28)

These link against `wx_bgi_wx` (the static wx integration library) and use `WxBgiCanvas` embedded inside a `wxFrame`.  
They are the C++ equivalents of the standalone wx demos.

| Test | Duration | What it checks |
|------|----------|----------------|
| `wx_bgi_solids_test` | < 1 s | 3-D solids render without crash; depth buffer works |
| `wx_bgi_3d_orbit_test` | ~5 s | Camera orbit animation; solid occlusion across 360 ° |
| `wx_bgi_canvas_coverage_test` | ~4 s | Full BGI API coverage in embedded `WxBgiCanvas`; timer-based two-phase flow |

---

## Running the Tests

### Windows (MSVC, multi-config)

```powershell
# Debug
cmake -S . -B build
cmake --build build -j --config Debug
ctest --test-dir build -C Debug --output-on-failure

# Release
cmake -S . -B build
cmake --build build -j --config Release
ctest --test-dir build -C Release --output-on-failure

# Run a single test
ctest --test-dir build -C Debug -R bgi_api_coverage_cpp --output-on-failure

# Run a category by regex
ctest --test-dir build -C Debug -R pascal --output-on-failure
ctest --test-dir build -C Debug -R wx_bgi --output-on-failure
```

### Linux (single-config)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure

# Single test
ctest --test-dir build -R bgi_api_coverage_cpp --output-on-failure
```

### macOS (single-config, Apple Silicon)

First-time setup — install system GLFW and wxWidgets if not already done:

```bash
brew install glfw wxwidgets
```

Configure and build with the system-package flags:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
  -DWXBGI_SYSTEM_GLFW=ON -DWXBGI_SYSTEM_WX=ON \
  -DWXBGI_ENABLE_TEST_SEAMS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

> **FreePascal tests (18–25) on macOS:** Pascal CTest targets require a working
> FPC Homebrew install with correct linker paths.  If FPC is installed to a
> non-standard Homebrew prefix the linker may report `ld: library 'c' not found`.
> All C++, Python, and wxWidgets tests (1–17, 26–28) pass regardless.

### Single test

```bash
ctest --test-dir build -R bgi_api_coverage_cpp --output-on-failure
```

### Enabling Test Seams (keyboard injection)

```powershell
cmake -S . -B build -DWXBGI_ENABLE_TEST_SEAMS=ON
cmake --build build -j --config Debug
ctest --test-dir build -C Debug -R "input" --output-on-failure
```

---

## Test Seam Security Policy

### Keyboard Injection Test Seams (`WXBGI_ENABLE_TEST_SEAMS`)

These APIs allow CI to inject synthetic key events into the keyboard queue deterministically:

- `wxbgi_test_clear_key_queue()` — empties the keyboard queue
- `wxbgi_test_inject_key_code(code)` — enqueues a translated key code
- `wxbgi_test_inject_extended_scan(scan)` — enqueues a DOS-style extended scan code

**Security rules:**

- Compiled **only** when `-DWXBGI_ENABLE_TEST_SEAMS=ON` (default is `OFF`).
- Release and public distribution builds **must not** enable this option.
- CI test jobs that require keyboard injection set this flag explicitly.
- The guard `#ifdef WXBGI_ENABLE_TEST_SEAMS` is present in all usage sites.

```powershell
# CI / test-only configure — never use for release artifacts
cmake -S . -B build -DWXBGI_ENABLE_TEST_SEAMS=ON
```

### Multi-Scene Demo Headless Mode (`--test` flag)

`wxbgi_multi_scene_demo` (test #9) uses the same always-available `--test` mechanism:
passing it on the command line causes the demo to build both scene graphs, render
one frame with all three cameras, and exit cleanly.

- No compile-time option required.
- No synthetic input surface exposed.

```powershell
# Manual headless run
.\build\Debug\wxbgi_multi_scene_demo.exe --test
```

### Camera Demo Headless Mode (`--test` flag)

`wxbgi_camera_demo_cpp` uses a separate, always-available mechanism: passing `--test` on the command line causes it to render exactly one frame and exit cleanly.

- No compile-time option required.
- No synthetic input surface exposed.
- CTest invokes this flag automatically via the `COMMAND` in `CMakeLists.txt`.

```powershell
# Manual headless run
.\build\Debug\wxbgi_camera_demo_cpp.exe --test
```

---

## CI Integration

The repository workflow file is `.github/workflows/CI.yml`.

**Normal push / pull request:** matrix CI in Debug mode on Windows, Linux, and macOS.

**Tag push (`v1.2.3`):** Release build → tests → docs generation → packaging:

| Artifact | Description |
|----------|-------------|
| `wx_bgi_opengl-windows-x64.zip` | Windows x64 DLL + headers |
| `wx_bgi_opengl-linux-x64.tar.gz` | Linux x64 shared library + headers |
| `wx_bgi_opengl-macos-x64.tar.gz` | macOS x64 dylib + headers |
| `api-docs.zip` / `api-docs.tar.gz` | Doxygen HTML documentation |

Doxygen HTML docs are published to **GitHub Pages** on every release tag.

### Creating a Release

```bash
git tag v1.2.3
git push origin v1.2.3
```

### Enabling GitHub Pages (first time)

1. Repository **Settings → Pages → Source** → set to **GitHub Actions**.
2. **Settings → Environments → github-pages** → allow deployments from `v*` tags  
   (or remove the branch/tag restriction).
