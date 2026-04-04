# wx_BGI_Graphics — Test Suite

This document describes the automated test suite for wx_BGI_Graphics.  
All tests are registered with CTest and run automatically in CI.

See **[Building.md](./Building.md)** for build commands.  
See **[WxWidgets.md](./WxWidgets.md)** for the embedded canvas integration guide.

---

## Test Summary

| # | Test Name | Category | Source File | Notes |
|---|-----------|----------|-------------|-------|
| 1 | `bgi_api_coverage_cpp` | C++ | `examples/cpp/bgi_api_coverage.cpp` | Classic BGI API coverage |
| 2 | `wxbgi_camera_demo_cpp` | C++ | `examples/cpp/wxbgi_camera_demo.cpp` | Camera/UCS/world demo — `--test` flag |
| 3 | `test_dds_serialize` | C++ / DDS | `examples/cpp/` | DDS JSON + YAML serialization |
| 4 | `test_dds_deserialize` | C++ / DDS | `examples/cpp/` | DDS JSON + YAML deserialization |
| 5 | `test_dds_clear` | C++ / DDS | `examples/cpp/` | DDS scene clear |
| 6 | `test_dds_cam2d_yz` | C++ / DDS | `examples/cpp/` | 2-D camera in YZ plane |
| 7 | `test_dds_cam3d_persp` | C++ / DDS | `examples/cpp/` | 3-D perspective camera |
| 8 | `test_solids` | C++ / DDS | `examples/cpp/` | 3-D solid primitives (box, sphere, cylinder, …) |
| 9 | `test_input_hooks` | C++ | `examples/cpp/` | Keyboard hook / input-inject (requires `WXBGI_ENABLE_TEST_SEAMS`) |
| 10 | `test_input_bypass` | C++ | `examples/cpp/` | Input bypass + scroll hook |
| 11 | `bgi_api_coverage_python` | Python | `examples/python/bgi_api_coverage.py` | ctypes BGI API coverage |
| 12 | `bgi_api_coverage_pascal_build` | Pascal | `examples/demoFreePascal/demo_bgi_api_coverage.pas` | FPC compile |
| 13 | `bgi_api_coverage_pascal_run` | Pascal | same | GLFW → wx-standalone path |
| 14 | `bgi_canvas_coverage_pascal_build` | Pascal | `examples/demoFreePascal/demo_bgi_canvas_coverage.pas` | FPC compile |
| 15 | `bgi_canvas_coverage_pascal_run` | Pascal | same | wx-only path (no GLFW) |
| 16 | `test_input_hooks_pascal_build` | Pascal | `examples/demoFreePascal/test_input_hooks.pas` | FPC compile |
| 17 | `test_input_hooks_pascal_run` | Pascal | same | Keyboard hook — Pascal |
| 18 | `test_input_bypass_pascal_build` | Pascal | `examples/demoFreePascal/test_input_bypass.pas` | FPC compile |
| 19 | `test_input_bypass_pascal_run` | Pascal | same | Input bypass — Pascal |
| 20 | `wx_bgi_solids_test` | wxWidgets | `examples/wx/wx_bgi_solids_test.cpp` | 3-D solids in embedded canvas |
| 21 | `wx_bgi_3d_orbit_test` | wxWidgets | `examples/wx/wx_bgi_3d_orbit_test.cpp` | 3-D orbit animation in embedded canvas |
| 22 | `wx_bgi_canvas_coverage_test` | wxWidgets | `examples/wx/wx_bgi_canvas_coverage_test.cpp` | Full BGI API coverage — C++ embedded `WxBgiCanvas` |

Pascal tests (12–19) run only when a matching-architecture FreePascal compiler (`fpc`) is found at CMake configure time.

---

## Test Categories

### C++ Core Tests (1–10)

These tests link directly against `wx_bgi_opengl` (the DLL/shared library) and exercise the full API surface through a real graphics window.

| Test | What it checks |
|------|----------------|
| `bgi_api_coverage_cpp` | All classic BGI drawing functions: `line`, `circle`, `arc`, `bar`, `fillpoly`, `outtextxy`, `getimage`/`putimage`, page switching, palette, viewport |
| `wxbgi_camera_demo_cpp` | Camera creation and orbiting (`wxbgi_cam_*`), UCS frames (`wxbgi_ucs_*`), world-coordinate drawing (`wxbgi_world_*`), camera fit-to-extents |
| `test_dds_serialize` | `wxbgi_dds_save_json` / `wxbgi_dds_save_yaml` round-trip correctness |
| `test_dds_deserialize` | `wxbgi_dds_load_json` / `wxbgi_dds_load_yaml` round-trip correctness |
| `test_dds_clear` | `wxbgi_dds_clear` removes all scene objects |
| `test_dds_cam2d_yz` | 2-D orthographic camera in the YZ plane; coordinate projection |
| `test_dds_cam3d_persp` | 3-D perspective projection; worldToScreen math |
| `test_solids` | `wxbgi_solid_box`, `wxbgi_solid_sphere`, `wxbgi_solid_cylinder`, `wxbgi_solid_cone`, `wxbgi_solid_torus` — all draw modes |
| `test_input_hooks` | Keyboard queue injection via test seams; hook ordering (see [Test Seam Policy](#keyboard-injection-test-seams-wxbgi_enable_test_seams)) |
| `test_input_bypass` | Scroll bypass, input hook chaining, `wxbgi_set_input_hook` |

### Python Test (11)

```bash
python examples/python/bgi_api_coverage.py build/Debug/wx_bgi_opengl.dll   # Windows
python3 examples/python/bgi_api_coverage.py build/libwx_bgi_opengl.so      # Linux
```

Exercises the full BGI API surface via Python `ctypes` — the same functions as the C++ coverage test.

### FreePascal Tests (12–19)

Each Pascal test has a **build** step (FPC compile) and a **run** step.  
The build step copies the up-to-date DLL into the output directory automatically.

| Tests | Source | What it checks |
|-------|--------|----------------|
| 12–13 `bgi_api_coverage_pascal_*` | `demo_bgi_api_coverage.pas` | Full BGI API surface: GLFW `initgraph` / `closegraph` → `wxbgi_wx_frame_create`; all classic drawing functions + extension API |
| 14–15 `bgi_canvas_coverage_pascal_*` | `demo_bgi_canvas_coverage.pas` | wx-only path (no GLFW): `wxbgi_wx_app_create` + `wxbgi_wx_frame_create`; colorful primitives phase + blue-screen phase + extension API |
| 16–17 `test_input_hooks_pascal_*` | `test_input_hooks.pas` | Pascal keyboard hook API |
| 18–19 `test_input_bypass_pascal_*` | `test_input_bypass.pas` | Pascal input bypass / scroll hooks |

> **Note:** `demo_bgi_canvas_coverage.pas` (tests 14–15) is the **wx-only** version — it does not call `initgraph` or `closegraph`.  It uses a `YieldMs()` helper to keep the wx event loop alive between draw phases.

### wxWidgets Canvas Tests (20–22)

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

### macOS (single-config)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

### Enabling Test Seams (keyboard injection)

Tests 9–10 and 16–17 (`test_input_hooks`, `test_input_bypass`) require the keyboard injection test seam API to be compiled in:

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
