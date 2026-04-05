# wx_BGI_Graphics

wx BGI Graphics is a C/C++ shared library that implements a classic BGI-compatible API (Borland Graphics Interface) on top of OpenGL, GLFW, GLEW and wxWidgets. The goal is to keep old Pascal/C/C++ graphics programs usable with minimal source changes while staying cross-platform on Windows, Linux, and macOS.

## Objectives
1. Strive to be as simple and beginner complete as the original BGI library.
2. Provide a portable graphics backend with a familiar BGI programming model, and some advanced user OpenGL capability.
3. Provide a statically packaged single binary with no other runtime dependency.
4. Easy to use Graphics Library tool for Python, Pascal, C/C++, etc. and other language users.
5. Hide away OpenGL complexities, behind simple interface for Programmers/Developers.
6. C/C++ Developers can choose light weight GLEW or Full Windowing wxWidgets framework.

## Latest Downloads

- Latest release page: https://github.com/Andromedabay/wx_bgi_graphics/releases/latest
- Windows x64 binary zip: https://github.com/Andromedabay/wx_bgi_graphics/releases/latest/download/wx_bgi_opengl-windows-x64.zip
- Linux x64 binary tar.gz: https://github.com/Andromedabay/wx_bgi_graphics/releases/latest/download/wx_bgi_opengl-linux-x64.tar.gz
- macOS x64 binary tar.gz: https://github.com/Andromedabay/wx_bgi_graphics/releases/latest/download/wx_bgi_opengl-macos-x64.tar.gz
- API docs site: https://andromedabay.github.io/wx_bgi_graphics/
- API docs zip: https://github.com/Andromedabay/wx_bgi_graphics/releases/latest/download/api-docs.zip
- API docs tar.gz: https://github.com/Andromedabay/wx_bgi_graphics/releases/latest/download/api-docs.tar.gz

The download URLs above always point to the newest tagged GitHub Release asset with the matching filename.

## Documentation

| Document | Contents |
|----------|----------|
| **[Building.md](./Building.md)** | Dependencies, compile requirements, CMake flags, build commands for Windows / Linux / macOS, running examples |
| **[Tests.md](./Tests.md)** | All 22 CTest targets, how to run them, test categories, test seam security policy, CI integration |
| **[Tutorial.md](./Tutorial.md)** | BGI double-buffering deep dive: page buffers, `setactivepage`, `swapbuffers`, animation loops |
| **[WxWidgets.md](./WxWidgets.md)** | wxWidgets embedded canvas guide: `WxBgiCanvas`, standalone wx API, event routing, 3D in wx mode |
| **[DDS.md](./DDS.md)** | Drawing Description Data Structure: scene graph, CHDOP hierarchy, `wxbgi_dds_*` API, JSON/YAML serialization |
| **[Camera3D_Map.md](./Camera3D_Map.md)** | 3-D camera code map: `Camera3D` struct, GLM math, `wxbgi_cam_*` API |
| **[Camera2D_Map.md](./Camera2D_Map.md)** | 2-D overhead camera: pan/zoom/rotation, `wxbgi_cam2d_*` API |
| **[InputsProcessing.md](./InputsProcessing.md)** | Keyboard and mouse event handling, keyboard queue, DOS-style extended keys, input hooks |
| **[VisualAids.md](./VisualAids.md)** | Visual overlay system: reference grid, UCS axes, crosshair, selection cursor |
| **[ScreenShots.md](./ScreenShots.md)** | Annotated screenshots of example programs |

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
- retained-mode scene graph (DDS / DDDS) with JSON and YAML serialisation via `wxbgi_dds_*`
- 3D solid primitives (box, sphere, cylinder, cone, torus) with wireframe and filled draw modes via `wxbgi_solid_*`
- Phong lighting model (key + fill lights, ambient/diffuse/specular) configured via `wxbgi_solid_set_light_*`
- wxWidgets embedded canvas (`wx_bgi_wx`) with OpenGL 3.3 texture-quad compositing and automatic legacy fallback

Public API declarations are available in:

- `src/wx_bgi.h` (classic BGI API)
- `src/wx_bgi_ext.h` (modern extension helpers)
- `src/wx_bgi_3d.h` (camera, UCS, world-coordinate extension API)
- `src/wx_bgi_dds.h` (Drawing Description Data Structure -- DDS/CHDOP/DDJ/DDY)

See **[DDS.md](./DDS.md)** for a full explanation of the DDS acronyms, the CHDOP object hierarchy, all `wxbgi_dds_*` functions, and usage examples.

## Camera Reference

The library provides two camera modes, both built on the `Camera3D` struct:

- **[Camera3D_Map.md](./Camera3D_Map.md)** -- 3D perspective and orthographic cameras: data layout, GLM math layer, full `wxbgi_cam_*` API, serialization, and the file dependency diagram.
- **[Camera2D_Map.md](./Camera2D_Map.md)** -- 2D overhead cameras (`is2D = true`): pan/zoom/rotation math, `wxbgi_cam2d_*` API, zoom-at pivot formula, and usage example.

## Visual Aids Reference

- **[VisualAids.md](./VisualAids.md)** -- visual overlays for camera/UCS workflows: reference grid, UCS axes, concentric circles + crosshair, and per-camera selection cursor.

## Input Processing Reference

- **[InputsProcessing.md](./InputsProcessing.md)** -- complete guide to keyboard and mouse event handling: wx and GLFW backends, DOS-style extended key codes, keyboard queue, mouse position tracking, click-to-pick pipeline, user hook (callback chaining) system, thread safety rules, and full code map.

## Screenshots

See **[ScreenShots.md](./ScreenShots.md)** for annotated screenshots of the example programs.

![3D perspective camera view](images/thumb_cam3d_persp.png)

*[View all screenshots](ScreenShots.md)*


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
#include "wx_bgi_ext.h"

wxbgi_wx_app_create();
wxbgi_wx_frame_create(1024, 720, "Camera Demo");

wxbgi_cam2d_create("plan");
wxbgi_cam_set_active("plan");
wxbgi_cam2d_set_pan("plan", 0.0f, 0.0f);
wxbgi_cam2d_set_zoom("plan", 1.5f);

setcolor(WHITE);
wxbgi_world_line(-100.0f, 0.0f, 0.0f, 100.0f, 0.0f, 0.0f);
wxbgi_world_circle(0.0f, 0.0f, 0.0f, 50.0f);

wxbgi_wx_close_after_ms(5000);
wxbgi_wx_app_main_loop();
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

For the complete keyboard input reference, test seam security policy, and CI integration details see **[Tests.md](./Tests.md)**.

### Advanced API Usage Example

The snippet below shows a minimal frame loop using the wx standalone API (default mode):

```c
#include "wx_bgi.h"
#include "wx_bgi_ext.h"

static int g_frame = 0;

void drawFrame(void) {
    cleardevice();
    setcolor(WHITE);
    circle(320, 240, 50 + (g_frame++ % 100));
    outtextxy(10, 10, "wx_BGI standalone loop");
    swapbuffers();
}

int main(void) {
    wxbgi_wx_app_create();
    wxbgi_wx_frame_create(640, 480, "My Window");
    wxbgi_set_vsync(1);
    setbkcolor(0);   /* BLACK */
    wxbgi_wx_set_idle_callback(drawFrame);
    wxbgi_wx_set_frame_rate(60);
    wxbgi_wx_app_main_loop();
    return 0;
}
```

**GLFW-mode** (with `WXBGI_ENABLE_WX=OFF`) uses the classic `initwindow` / frame loop pattern:

```c
initwindow(640, 480, "My Window", 0, 0, 1, 1);
wxbgi_set_vsync(1);
while (wxbgi_should_close() == 0) {
    wxbgi_begin_advanced_frame(0.05f, 0.05f, 0.10f, 1.0f, 1, 0);
    setcolor(WHITE);
    circle(320, 240, 100);
    outtextxy(10, 10, "Hello BGI");
    wxbgi_end_advanced_frame(1);
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

## wxWidgets Embedded Canvas (Default Backend)

wxWidgets is the **default** rendering backend.  The DLL is built with wx support
automatically — no extra CMake flag needed:

```sh
cmake -S . -B build
cmake --build build -j
```

The library ships two wx integration options:

### 1. Standalone wx API (Python / Pascal / C / simple C++)

Open a wx window using only the C API — no wx C++ required:

```c
wxbgi_wx_app_create();
wxbgi_wx_frame_create(640, 480, "My BGI App");
setbkcolor(BLACK);
setcolor(YELLOW);
circle(320, 240, 100);
wxbgi_wx_close_after_ms(5000);
wxbgi_wx_app_main_loop();   // blocks until window closes
```

This is how the Python (`bgi_api_coverage.py`) and FreePascal programs work.

### 2. Embedded Canvas (C++ with wxFrame)

For C++ apps that need menus, status bars, or custom wx controls alongside the BGI surface,
use `wx_bgi_wx.lib` + `WxBgiCanvas`:

```cpp
#include <wx/wx.h>
#include "wx_bgi_wx.h"   // WxBgiCanvas
#include "wx_bgi.h"      // Classic BGI API

class MyFrame : public wxFrame {
public:
    MyFrame() : wxFrame(nullptr, wxID_ANY, "My App", wxDefaultPosition, wxSize(800,600)) {
        auto* canvas = new wxbgi::WxBgiCanvas(this);
        canvas->SetAutoRefreshHz(60);
        setcolor(YELLOW);
        circle(400, 300, 100);
    }
};
```

To build **without** wxWidgets (GLFW-only):
```sh
cmake -S . -B build -DWXBGI_ENABLE_WX=OFF
```

See **[WxWidgets.md](./WxWidgets.md)** for the full integration guide, standalone API reference,
event routing table, 3D camera setup in wx mode, and automated tests.

## 3D Solid Primitives and Shading

The `wx_bgi_dds.h` header exposes retained-mode 3D solid primitives that are
stored in the DDS scene graph and rendered through any `Camera3D` via
`wxbgi_render_dds()`.

### Draw Modes

| Constant | Description |
|----------|-------------|
| `WXBGI_SOLID_WIREFRAME` | Edge-only rendering using the current line colour. |
| `WXBGI_SOLID_SOLID` / `WXBGI_SOLID_FLAT` | Filled faces; flat Phong shading via GPU depth pass (pending GL-5). |
| `WXBGI_SOLID_SMOOTH` | Smooth per-vertex Gouraud/Phong shading (pending GL-5). |

```c
wxbgi_solid_set_draw_mode(WXBGI_SOLID_SOLID);
wxbgi_solid_set_face_color(CYAN);
wxbgi_solid_sphere(0.f, 0.f, 0.f, 2.f, 32, 16);
wxbgi_solid_box(-3.f, -1.f, -1.f, 3.f, 1.f, 1.f);
```

### Phong Lighting API

Configure the key and fill lights plus material properties:

```c
#include "wx_bgi_dds.h"

wxbgi_solid_set_light_dir(1.f, 1.f, 2.f);            // key light direction
wxbgi_solid_set_light_space(1);                        // 1=world-space, 0=eye-space
wxbgi_solid_set_fill_light(-1.f, 0.5f, 0.5f, 0.3f);  // fill direction + strength
wxbgi_solid_set_ambient(0.2f);
wxbgi_solid_set_diffuse(0.7f);
wxbgi_solid_set_specular(0.4f, 32.f);
```

> **Note:** Phong-shaded GPU solid rendering (depth buffer, correct
> occlusion) is infrastructure-complete (shaders in `bgi_gl_shaders.h`,
> `LightState` in `BgiState`) but the wiring to the draw pipeline (GL-5
> through GL-8) is pending.  Current solid rendering uses the CPU-side
> projected-triangle path; depth-ordering artefacts may be visible.

### GL Rendering Path (wx Mode)

In wx mode the BGI pixel buffer is composited via an OpenGL 3.3 texture quad.
The library detects the available GL version at runtime and falls back to the
legacy `GL_POINTS` path automatically on older hardware.  To force the legacy
path explicitly:

```c
wxbgi_set_legacy_gl_render(1);  // 1 = legacy, 0 = texture quad (default)
```

## Text Rendering

The text renderer now uses a scalable stroke-font implementation rather than the earlier fixed 5x7 bitmap glyphs. That change improves compatibility in a few important ways:

- `settextstyle` now affects font family/profile instead of only storing metadata
- glyphs scale continuously with `charsize` and `setusercharsize` instead of enlarging a tiny bitmap
- vertical text uses the same vector glyph data rather than rotating a coarse bitmap block
- text metrics from `textwidth` and `textheight` now reflect the actual scaled stroke layout more closely

The current stroke-font implementation is still lightweight, but it is materially closer to classic BGI behavior than the old bitmap-only path.
  
The library has one shared glyph dataset used by all 5 font profiles (DEFAULT_FONT, TRIPLEX_FONT, SMALL_FONT, SANS_SERIF_FONT, GOTHIC_FONT). The profiles apply different scale, thickness, and slant on top of the same strokes -- so adding glyphs once covers all fonts. All printable ASCII Glyphs are included now with Release v1.1.0.

## BGI Double Buffering Tutorial
Please read more about this at **[Tutorial.md](./Tutorial.md)**.

## Usage Examples

The examples are under the `./examples/` folder.

### Coverage Examples

| Source | Language | What it exercises |
|--------|----------|-------------------|
| `examples/cpp/bgi_api_coverage.cpp` | C++ | Classic BGI API (standalone GLFW/wx) |
| `examples/python/bgi_api_coverage.py` | Python | ctypes BGI API coverage |
| `examples/demoFreePascal/demo_bgi_api_coverage.pas` | FreePascal | BGI API — GLFW → wx-standalone path |
| `examples/demoFreePascal/demo_bgi_canvas_coverage.pas` | FreePascal | BGI API — wx-only path (no GLFW) |
| `examples/wx/wx_bgi_canvas_coverage_test.cpp` | C++ / wx | BGI API — embedded `WxBgiCanvas` (timer-based phases) |

### Other Examples

| Source | Language | Description |
|--------|----------|-------------|
| `examples/cpp/wxbgi_camera_demo.cpp` | C++ | Camera/UCS/world-coordinate interactive demo |
| `examples/cpp/wxbgi_keyboard_queue.cpp` | C++ | DOS-style keyboard queue |
| `examples/demoFreePascal/demo_bgi_wrapper.pas` | FreePascal | Minimal BGI wrapper demo |
| `examples/demoFreePascal/demo_bgi_wrapper_gui.pas` | FreePascal | GUI subsystem BGI demo |
| `examples/demoFreePascal/demo_wxbgi_keyboard_queue.pas` | FreePascal | Keyboard queue from Pascal |
| `examples/wx/wx_bgi_3d_orbit_test.cpp` | C++ / wx | 3-D orbit animation in embedded canvas |
| `examples/wx/wx_bgi_solids_test.cpp` | C++ / wx | 3-D solids in embedded canvas |
| `examples/wx/wx_bgi_app.cpp` | C++ / wx | Interactive 2-D mouse/keyboard demo |
| `examples/wx/wx_bgi_3d_app.cpp` | C++ / wx | Interactive 3-D orbiting camera with solids |

### Running Examples and Tests

See **[Building.md](./Building.md)** for interactive run commands on every platform.  
See **[Tests.md](./Tests.md)** for CTest commands, test categories, and CI details.

## Compile-time Dependencies

See **[Building.md](./Building.md)** for full dependency details.

- GLFW 3.4, GLEW 2.2.0, GLM 1.0.1, wxWidgets 3.2.5, nlohmann/json, yaml-cpp — all auto-fetched by CMake `FetchContent`.
- Optional: Doxygen (API docs), LaTeX / MiKTeX (PDF docs), FreePascal (Pascal examples), Python 3 (Python test).

## Building the Project

See **[Building.md](./Building.md)** for full build and test instructions.

Quick start (Windows):

```powershell
cmake -S . -B build
cmake --build build -j --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Quick start (Linux / macOS):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## CI and Releases

See **[Tests.md — CI Integration](./Tests.md#ci-integration)** for the full CI setup, release tagging instructions, and GitHub Pages deployment guide.

## License

This code is Open Source and Free to use with no warranties or claims.
  
This repository is based on Open-Source Code from 8 different sources:  
    1. Hammad Rauf, rauf.hammad@gmail.com, MIT License  
         - Supervised usage and prompting of: Github Copilot Pro (Claude-Sonnet 4.6), free Trial.  
         - Techniques mentioned at URL [https://lisyarus.github.io/blog/posts/implementing-a-tiny-cpu-rasterizer-part-5.html](https://lisyarus.github.io/blog/posts/implementing-a-tiny-cpu-rasterizer-part-5.html).  
    2. CMake Template, from lszl84, MIT License  
    3. glfw Library, from Marcus Geelnard & Camilla Lowy, Zlib license  
    4. glew Library, from https://github.com/nigels-com/glew, Custom License  
    5. glm Library, from https://github.com/g-truc/glm, Happy Bunny License (Custom MIT License)  
    6. JSON Library, from citation.APA="Lohmann, N. (2025). JSON for Modern C++ (Version 3.12.0) [Computer software]. https://github.com/nlohmann", MIT License  
    7. YAML Library, from https://github.com/jbeder/yaml-cpp , MIT License  
    8. wxWidgets, from https://github.com/wxWidgets/wxWidgets, wxWindows Library Licence, Version 3.1  

Depends on C/C++ Template created by Luke of devmindscapetutorilas:
 - [www.onlyfastcode.com](https://www.onlyfastcode.com)
 - [https://devmindscape.com/](https://devmindscape.com/)
 - [https://www.youtube.com/@devmindscapetutorials](https://www.youtube.com/@devmindscapetutorials)
