# wx_BGI_Graphics

wx BGI Graphics is a C/C++ shared library that implements a classic BGI-compatible API (Borland Graphics Interface) on top of OpenGL, GLFW and GLEW. The goal is to keep old Pascal/C/C++ graphics programs usable with minimal source changes while staying cross-platform on Windows, Linux, and macOS.

## Objectives
1. Strive to be as simple and beginner complete as the original BGI library.
1. Provide a portable graphics backend with a familiar BGI programming model, and some advanced user OpenGL capability.
1. Provide a staticaly packaged single binary with no other runtime dependency.
1. Easy to use Graphics Library tool for Python, Pascal, C/C++, etc. and other language users.
1. Hide away OpenGL complexities, behind simple interface for Programmers/Developers.

## Latest Downloads

- Latest release page: https://github.com/Andromedabay/wx_bgi_graphics/releases/latest
- Windows x64 binary zip: https://github.com/Andromedabay/wx_bgi_graphics/releases/latest/download/wx_bgi_opengl-windows-x64.zip
- Linux x64 binary tar.gz: https://github.com/Andromedabay/wx_bgi_graphics/releases/latest/download/wx_bgi_opengl-linux-x64.tar.gz
- macOS x64 binary tar.gz: https://github.com/Andromedabay/wx_bgi_graphics/releases/latest/download/wx_bgi_opengl-macos-x64.tar.gz
- API docs site: https://andromedabay.github.io/wx_bgi_graphics/
- API docs zip: https://github.com/Andromedabay/wx_bgi_graphics/releases/latest/download/api-docs.zip
- API docs tar.gz: https://github.com/Andromedabay/wx_bgi_graphics/releases/latest/download/api-docs.tar.gz

The download URLs above always point to the newest tagged GitHub Release asset with the matching filename.

## Current Compatibility

The library now includes classic BGI-style support for:

- graphics lifecycle and status functions such as `initgraph`, `closegraph`, `graphresult`, `graphdefaults`
- drawing primitives such as `line`, `lineto`, `linerel`, `rectangle`, `bar`, `bar3d`, `circle`, `arc`, `ellipse`, `pieslice`, `sector`
- filled drawing operations such as `floodfill`, `fillpoly`, `fillellipse`, `setfillstyle`, `setfillpattern`
- viewport, palette, and state accessors such as `setviewport`, `getviewsettings`, `setpalette`, `setrgbpalette`, `setbkcolor`, `setwritemode`
- text rendering and measurement such as `outtext`, `outtextxy`, `settextstyle`, `settextjustify`, `textwidth`, `textheight`
- image buffer helpers such as `imagesize`, `getimage`, `putimage`
- double-buffer style page selection via `setactivepage`, `setvisualpage`, `getactivepage`, `getvisualpage`, `swapbuffers`
- additive 3-D/2-D camera + UCS + world-coordinate helpers via `wxbgi_cam_*`, `wxbgi_cam2d_*`, `wxbgi_ucs_*`, and `wxbgi_world_*`

Public API declarations are available in:

- `src/wx_bgi.h` (classic BGI API)
- `src/wx_bgi_ext.h` (modern extension helpers)
- `src/wx_bgi_3d.h` (camera, UCS, world-coordinate extension API)


## Turbo Pascal/Turbo C/C++ - Drop in Replacement (Well Almost...)

Please see [./examples/bgidemo-pascal/README.md](./examples/bgidemo-pascal/README.md).


## Advanced OpenGL Extension API

In addition to classic BGI, the library now exports an optional non-BGI extension API for modern control paths such as explicit event pumping, context control, swap interval, size queries, frame begin/end, and pixel readback.

- Header: `src/wx_bgi_ext.h`
- Naming: all extension functions use the `wxbgi_` prefix
- Typical use cases:
  - integrating custom OpenGL draws between BGI calls
  - querying OpenGL/GLFW runtime details
  - controlled frame loops and framebuffer capture

This extension API is intended to complement BGI compatibility, not replace it.

## Camera, UCS, and World-Coordinate API (`wx_bgi_3d.h`)

The project now includes a dedicated camera/UCS extension layer for engineering-style world coordinates while keeping classic BGI behavior unchanged.

Highlights:

- Coordinate system is right-handed, Z-up (XY = ground plane).
- A default camera named `"default"` is created by `initwindow` / `initgraph`.
- 3-D camera controls are exposed as `wxbgi_cam_*`.
- 2-D overhead camera controls (pan/zoom/rotate) are exposed as `wxbgi_cam2d_*`.
- User coordinate systems are exposed as `wxbgi_ucs_*`.
- World extents and fit-to-view helpers are exposed via `wxbgi_set_world_extents`, `wxbgi_expand_world_extents`, and `wxbgi_cam_fit_to_extents`.
- World/UCS drawing wrappers such as `wxbgi_world_line`, `wxbgi_world_circle`, `wxbgi_ucs_line`, and `wxbgi_ucs_outtextxy` project through the active camera into the classic BGI pixel pipeline.

Minimal camera setup example:

```c
#include "wx_bgi.h"
#include "wx_bgi_3d.h"

initwindow(1024, 720, "Camera Demo", 0, 0, 1, 1);

wxbgi_cam2d_create("plan");
wxbgi_cam_set_active("plan");
wxbgi_cam2d_set_pan("plan", 0.0f, 0.0f);
wxbgi_cam2d_set_zoom("plan", 1.5f);

setcolor(WHITE);
wxbgi_world_line(-100.0f, 0.0f, 0.0f, 100.0f, 0.0f, 0.0f);
wxbgi_world_circle(0.0f, 0.0f, 0.0f, 50.0f);
```

### Keyboard Queue Helpers

The extension API also exposes a small keyboard input layer for GUI-style event waits that do not rely on terminal or console input.

- `wxbgi_key_pressed()` reports whether a translated key event is waiting in the internal queue.
- `wxbgi_read_key()` consumes the next queued key code.
- `wxbgi_is_key_down(glfwKey)` checks raw key-down state for a GLFW key code such as `GLFW_KEY_ESCAPE` or `GLFW_KEY_LEFT`.

Input is captured from the graphics window through GLFW callbacks.

- Printable keys are queued as normal character codes.
- Special keys like `Esc`, `Enter`, and `Tab` are queued as their classic ASCII control values.
- Extended keys use DOS-style compatibility semantics: `wxbgi_read_key()` returns `0` first, then returns the scan code on the next read.

This makes it practical to port old Pascal and BGI programs that used `KeyPressed` and `ReadKey` style control flow without falling back to `Crt`.

### Internal Test Seam Security Policy

The project uses two distinct mechanisms to support deterministic CI testing without a live GUI.

#### Keyboard injection test seams (`WXBGI_ENABLE_TEST_SEAMS`)

The following APIs allow CI to inject synthetic key events into the keyboard queue:

- `wxbgi_test_clear_key_queue()`
- `wxbgi_test_inject_key_code()`
- `wxbgi_test_inject_extended_scan()`

Security policy:

- These APIs are compiled only when `WXBGI_ENABLE_TEST_SEAMS=ON`.
- Default is `OFF` so release/public binaries do not expose synthetic input injection.
- CI/system tests can enable them explicitly for deterministic keyboard queue checks.
- Exercised in `examples/cpp/bgi_api_coverage.cpp` under the `#ifdef WXBGI_ENABLE_TEST_SEAMS` guard.

Example test-only configure:

```powershell
cmake -S . -B build -DWXBGI_ENABLE_TEST_SEAMS=ON
```

Do not enable this option for production distribution artifacts.

#### Camera demo headless mode (`--test` flag)

The camera demo (`wxbgi_camera_demo_cpp`) uses a different, always-available mechanism: passing `--test` on the command line causes it to draw exactly one frame and exit cleanly. This approach requires **no** compile-time option and exposes no synthetic input surface.

```powershell
# Run manually in headless test mode:
.\build\Debug\wxbgi_camera_demo_cpp.exe --test
```

CTest invokes this automatically — see the `wxbgi_camera_demo_cpp` test entry in `CMakeLists.txt`.

### Advanced API Usage Example

The snippet below shows a minimal frame loop that mixes classic BGI drawing with the extension API:

```c
#include "wx_bgi.h"
#include "wx_bgi_ext.h"

/* open a double-buffered window the normal BGI way */
initwindow(640, 480, "My Window", 0, 0, 1, 1);
wxbgi_set_vsync(1);
wxbgi_set_window_title("Running");

while (wxbgi_should_close() == 0) {
    /* pump OS events and clear to a dark background */
    wxbgi_begin_advanced_frame(0.05f, 0.05f, 0.10f, 1.0f, /*clearColor=*/1, /*clearDepth=*/0);

    /* classic BGI drawing */
    setcolor(WHITE);
    circle(320, 240, 100);
    outtextxy(10, 10, "Hello BGI");

    /* flush OpenGL and swap buffers */
    wxbgi_end_advanced_frame(/*swapBuffers=*/1);
}
closegraph();
```

You can also query runtime information independently of the draw loop:

```c
printf("Vendor:   %s\n", wxbgi_get_gl_string(0));  /* 0=vendor, 1=renderer, 2=version */
printf("Renderer: %s\n", wxbgi_get_gl_string(1));
printf("Elapsed:  %.3f s\n", wxbgi_get_time_seconds());

int w, h;
wxbgi_get_framebuffer_size(&w, &h);
printf("Framebuffer: %d x %d\n", w, h);
```

## Text Rendering

The text renderer now uses a scalable stroke-font implementation rather than the earlier fixed 5x7 bitmap glyphs. That change improves compatibility in a few important ways:

- `settextstyle` now affects font family/profile instead of only storing metadata
- glyphs scale continuously with `charsize` and `setusercharsize` instead of enlarging a tiny bitmap
- vertical text uses the same vector glyph data rather than rotating a coarse bitmap block
- text metrics from `textwidth` and `textheight` now reflect the actual scaled stroke layout more closely

The current stroke-font implementation is still lightweight, but it is materially closer to classic BGI behavior than the old bitmap-only path.
  
The library has one shared glyph dataset used by all 5 font profiles (DEFAULT_FONT, TRIPLEX_FONT, SMALL_FONT, SANS_SERIF_FONT, GOTHIC_FONT). The profiles apply different scale, thickness, and slant on top of the same strokes — so adding glyphs once covers all fonts. All printable ASCII Glyphs are included now with Release v1.1.0.

## BGI Double Buffering Tutorial
Please read more about this at [./Tutorial.md](./Tutorial.md)

## Usage Examples
The examples are under the ./examples/ folder.

### Running Examples Interactively

Commands to launch each example in an interactive session after a Debug build.

#### Windows

```powershell
# C++ API coverage (opens a window, draws, then closes automatically)
.\build\Debug\bgi_api_coverage_cpp.exe

# Keyboard queue demo (interactive — press keys, close window to exit)
.\build\Debug\wxbgi_keyboard_queue_cpp.exe

# Camera demo (interactive — W/A/S/D pan, +/- zoom, arrow keys orbit, Esc to exit)
.\build\Debug\wxbgi_camera_demo_cpp.exe

# Pascal BGI demo (interactive — requires FreePascal build step first)
.\build\bgidemo_pascal\bgidemo.exe

# Pascal keyboard queue demo (interactive — requires FreePascal build step first)
.\build\keyboard_queue_pascal\demo_wxbgi_keyboard_queue.exe

# Python API coverage (exits automatically; pass DLL path as argument)
python examples\python\bgi_api_coverage.py build\Debug\wx_bgi_opengl.dll
```

#### Linux / macOS

```bash
# C++ API coverage
./build/bgi_api_coverage_cpp

# Keyboard queue demo
./build/wxbgi_keyboard_queue_cpp

# Camera demo
./build/wxbgi_camera_demo_cpp

# Pascal BGI demo
./build/bgidemo_pascal/bgidemo

# Pascal keyboard queue demo
./build/keyboard_queue_pascal/demo_wxbgi_keyboard_queue

# Python API coverage
python3 examples/python/bgi_api_coverage.py build/libwx_bgi_opengl.so
```

> **Note:** The camera demo also accepts a `--test` flag (`wxbgi_camera_demo_cpp --test`) which draws one frame and exits immediately — this is the mode used by CTest.

### Example - demoFreePascal

- `examples/demoFreePascal/demo_bgi_wrapper_gui.pas`
- `examples/demoFreePascal/demo_bgi_wrapper.pas`
- `examples/demoFreePascal/demo_wxbgi_keyboard_queue.pas`

### Example - direct keyboard queue

- `examples/cpp/wxbgi_keyboard_queue.cpp`
- `examples/demoFreePascal/demo_wxbgi_keyboard_queue.pas`

### Example - camera and world-coordinate API

- `examples/cpp/wxbgi_camera_demo.cpp`

### Coverage Examples

- `examples/cpp/bgi_api_coverage.cpp`
- `examples/python/bgi_api_coverage.py`
- `examples/demoFreePascal/demo_bgi_api_coverage.pas`

## Compile-time Dependencies
   - GLFW
   - GLEW
   - GLM
   - OpenGL
   - CMake
   - Doxygen (for API documentation generation)

## Compilation Requirements

To build this project yourself, ensure you have the following tools and libraries:

- **CMake (Version 3.14 or later):** Essential for building and managing the project files.
- **C++20 Compiler:** Compatible with Clang, GCC, or MSVC.
- **OpenGL-capable system libraries:** Required by GLFW/GLEW on all supported platforms.

Dependency note:

- GLFW 3.4, GLEW 2.2.0, and GLM 1.0.1 are fetched automatically by CMake `FetchContent`; no manual dependency installation is required.

## Building the Project

- Updated FreePascal demo instructions are in examples/demoFreePascal/README.md.
- Pascal BGIDemo port instructions are in examples/bgidemo-pascal/README.md.

## Build, Test, and Docs on Windows/Linux/macOS

Use the command sets below to run a complete local pipeline: configure, build, test, and generate Doxygen docs.

### Windows (MSVC, multi-config)

Debug pipeline:

```powershell
cmake -S . -B build
cmake --build build -j --config Debug
ctest --test-dir build -C Debug --output-on-failure
cmake --build build --target api_docs -j --config Debug
cmake --build build --target api_docs_pdf -j --config Debug
```

Release pipeline:

```powershell
cmake -S . -B build
cmake --build build -j --config Release
ctest --test-dir build -C Release --output-on-failure
cmake --build build --target api_docs -j --config Release
cmake --build build --target api_docs_pdf -j --config Release
```

### Linux (single-config)

Debug pipeline:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
cmake --build build --target api_docs -j
cmake --build build --target api_docs_pdf -j
```

Release pipeline:

```bash
cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release
cmake --build build-rel -j
ctest --test-dir build-rel --output-on-failure
cmake --build build-rel --target api_docs -j
cmake --build build-rel --target api_docs_pdf -j
```

### macOS (single-config)

Debug pipeline:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
cmake --build build --target api_docs -j
cmake --build build --target api_docs_pdf -j
```

Release pipeline:

```bash
cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release
cmake --build build-rel -j
ctest --test-dir build-rel --output-on-failure
cmake --build build-rel --target api_docs -j
cmake --build build-rel --target api_docs_pdf -j
```

Generated API documentation output:

- `build/doxygen/html/index.html` (debug tree)
- `build-rel/doxygen/html/index.html` (release tree on Linux/macOS)
- `build/doxygen/latex/refman.pdf` (debug tree PDF)
- `build-rel/doxygen/latex/refman.pdf` (release tree PDF on Linux/macOS)

Shared library output name is `wx_bgi_opengl` with platform-specific extension:

- `build/Debug/wx_bgi_opengl.dll` on Windows Debug
- `build/Release/wx_bgi_opengl.dll` on Windows Release
- `build/libwx_bgi_opengl.so` on Linux
- `build/libwx_bgi_opengl.dylib` on macOS

In the current Windows environment, the C++ and Python coverage examples are exercised automatically. The FreePascal coverage source is included as an example as well, and automated execution is enabled when a matching FreePascal compiler is available for the built DLL architecture.

## CI Example (GitHub Actions)

  Actual repository workflow file:

  - `.github/workflows/CI.yml`

  Release behavior in that workflow (free-tier friendly):

  1. Normal `push`/`pull_request` runs matrix CI in Debug mode on Windows, Linux, and macOS.
  2. Tag pushes like `v1.2.3` run Release builds, tests, docs generation, and packaging.
  3. Release artifacts uploaded to GitHub Release:
    - Windows binary zip
    - Linux binary tar.gz
    - macOS binary tar.gz
    - API docs zip
    - API docs tar.gz
  4. The generated Doxygen HTML docs are published to GitHub Pages.
  5. Release notes are generated from git commit messages between the previous tag and current tag.

  To enable GitHub Pages deployment for the first time:

  1. Open repository Settings -> Pages.
  2. Under Build and deployment, set Source to GitHub Actions.
  3. Open repository Settings -> Environments -> github-pages.
  4. If deployment protection rules are enabled there, allow deployments from release tags such as `v*`, or remove the branch/tag restriction for the `github-pages` environment.

  To create a release from CI:

  ```bash
  git tag v1.2.3
  git push origin v1.2.3
  ```

## License

This code is Open Source and Free to use with no warranties or claims.
  
This repository is based on Open-Source Code from 7 different source:  
    1. Hammad Rauf, rauf.hammad@gmail.com, MIT License  
         - Supervised usage and prompting of: Github Copilot Pro (Claude-Sonnet 4.6), free Trial.  
    2. CMake Template, from lszl84, MIT License  
    3. glfw Library, from Marcus Geelnard & Camilla Löwy, Zlib license  
    4. glew Library, from https://github.com/nigels-com/glew, Custom License  
    5. glm Library, from https://github.com/g-truc/glm, Happy Bunny License (Custom MIT License)
    6. JSON Library, from citation.APA="Lohmann, N. (2025). JSON for Modern C++ (Version 3.12.0) [Computer software]. https://github.com/nlohmann", MIT License
    7. YAML Library, from https://github.com/jbeder/yaml-cpp , MIT License

  
Depends on C/C++ Template created by Luke of devmindscapetutorilas:
 - [www.onlyfastcode.com](https://www.onlyfastcode.com)
 - [https://devmindscape.com/](https://devmindscape.com/)
 - [https://www.youtube.com/@devmindscapetutorials](https://www.youtube.com/@devmindscapetutorials)
