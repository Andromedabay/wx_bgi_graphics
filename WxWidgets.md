# wxWidgets Embedded Canvas

`wx_bgi_wx` is an optional static library that lets you embed the BGI OpenGL
drawing surface -- cameras, viewports, DDS scene graph -- inside a **wxWidgets**
`wxFrame` alongside menus, toolbars, status bars, and other controls.

<!-- ------------------------------------------------------------------ -->
## Contents

1. [Quick Start — C++ with WxBgiCanvas](#quick-start)
2. [Standalone wx API (Python / Pascal / C)](#standalone-wx-api-python--pascal--c)
3. [CMake Usage](#cmake-usage)
4. [WxBgiCanvas API](#wxbgicanvas-api)
5. [BGI Drawing in wx Mode](#bgi-drawing-in-wx-mode)
6. [Event Routing Table](#event-routing-table)
7. [Scroll and Input Hooks in wx Mode](#scroll-and-input-hooks-in-wx-mode)
8. [3D Camera and Solid Primitives](#3d-camera-and-solid-primitives)
9. [Thread Safety](#thread-safety)
10. [Multi-Canvas (Future)](#multi-canvas-future)
11. [Automated Test](#automated-test)

---

## Quick Start — C++ with WxBgiCanvas

```cpp
#include <wx/wx.h>
#include "wx_bgi_wx.h"     // WxBgiCanvas
#include "wx_bgi.h"        // Classic BGI API
#include "wx_bgi_ext.h"    // Extension API

class MyFrame : public wxFrame {
public:
    MyFrame() : wxFrame(nullptr, wxID_ANY, "My App", wxDefaultPosition, wxSize(800,600)) {
        auto* canvas = new wxbgi::WxBgiCanvas(this);
        canvas->SetAutoRefreshHz(60);

        // Draw once -- canvas repaints automatically
        setcolor(YELLOW);
        circle(400, 300, 100);
    }
};
```

---

## Standalone wx API (Python / Pascal / C)

The library includes a **standalone wx window API** modelled on wxPython's
`wx.App` / `wx.Frame` / `wx.App.MainLoop()` pattern.  It lets Python, Pascal,
plain C, or any ctypes-compatible language open a wx window and draw with the
full BGI API — without writing a single line of wxWidgets C++.

> **Note:** Do **not** mix `wxbgi_wx_app_create()` with `wxIMPLEMENT_APP` or
> `wx_bgi_wx.lib` in the same program.  For C++ apps that want a custom
> `wxFrame`, use `wx_bgi_wx.lib` + `WxBgiCanvas` directly.

### API Reference

```c
// Initialise the wx application object (call once, before frame creation).
void wxbgi_wx_app_create(void);

// Create and show a BGI window of the given pixel dimensions.
void wxbgi_wx_frame_create(int width, int height, const char* title);

// Enter the wx event loop.  Does not return until the window is closed
// (or wxbgi_wx_close_frame / wxbgi_wx_close_after_ms fires).
void wxbgi_wx_app_main_loop(void);

// Close the window immediately.
void wxbgi_wx_close_frame(void);

// Schedule the window to close after `ms` milliseconds.
void wxbgi_wx_close_after_ms(int ms);

// Register a frame callback (C function pointer, cdecl) that is called
// by the refresh timer every tick before the canvas repaints.
// Signature: void myCallback(void);
void wxbgi_wx_set_idle_callback(WxbgiFrameCallback fn);

// Set the repaint rate (default 0 = no periodic repaint).
void wxbgi_wx_set_frame_rate(int fps);

// Force an immediate canvas repaint outside the timer.
void wxbgi_wx_refresh(void);
```

`WxbgiFrameCallback` is defined in `wx_bgi_ext.h`:
```c
typedef void (*WxbgiFrameCallback)(void);
```

### C Example

```c
#include "wx_bgi.h"
#include "wx_bgi_ext.h"

static int g_frame = 0;

void drawFrame(void) {
    cleardevice();
    setcolor(YELLOW);
    circle(320, 240, 40 + (g_frame++ % 80));
    outtextxy(10, 10, "wx standalone BGI!");
    swapbuffers();
}

int main(void) {
    wxbgi_wx_app_create();
    wxbgi_wx_frame_create(640, 480, "My BGI App");
    setbkcolor(BLACK);
    wxbgi_wx_set_idle_callback(drawFrame);
    wxbgi_wx_set_frame_rate(60);
    wxbgi_wx_app_main_loop();
    return 0;
}
```

### Python Example

```python
import ctypes, os, sys
from pathlib import Path

lib = ctypes.CDLL(str(Path(__file__).parent / "wx_bgi_opengl.dll"))
# ... configure argtypes/restypes ...

lib.wxbgi_wx_app_create()
lib.wxbgi_wx_frame_create(640, 480, b"Python BGI")
lib.setbkcolor(0)   # BLACK
lib.setcolor(14)    # YELLOW
lib.circle(320, 240, 100)
lib.outtextxy(10, 10, b"Hello from Python!")
lib.wxbgi_wx_close_after_ms(3000)
lib.wxbgi_wx_app_main_loop()
```

### FreePascal Example

```pascal
program PascalBGI;
uses SysUtils;

const BgiLib = 'wx_bgi_opengl.dll';
procedure wxbgi_wx_app_create; cdecl; external BgiLib;
procedure wxbgi_wx_frame_create(w,h:Integer;t:PChar); cdecl; external BgiLib;
procedure wxbgi_wx_app_main_loop; cdecl; external BgiLib;
procedure wxbgi_wx_close_after_ms(ms:Integer); cdecl; external BgiLib;
procedure setcolor(c:Integer); cdecl; external BgiLib;
procedure circle(x,y,r:Integer); cdecl; external BgiLib;

begin
  wxbgi_wx_app_create;
  wxbgi_wx_frame_create(640, 480, 'Pascal BGI');
  setcolor(14);
  circle(320, 240, 100);
  wxbgi_wx_close_after_ms(3000);
  wxbgi_wx_app_main_loop;
end.
```

### How It Works

1. `wxbgi_wx_app_create()` allocates a minimal `BgiStandaloneApp` (subclass of
   `wxApp`) and calls `wxInitialize()`.
2. `wxbgi_wx_frame_create()` creates a `BgiStandaloneFrame` hosting a single
   `WxBgiCanvas`, calls `wxbgi_wx_init_for_canvas(w, h)` to set up the BGI CPU
   page buffers, then shows the frame.  All BGI drawing calls made after this
   point write into the page buffer and are displayed on the next paint.
3. `wxbgi_wx_app_main_loop()` runs a Win32 message pump.  When the frame is
   closed (timer, user click, or `wxbgi_wx_close_frame()`), the pump exits and
   the function returns.

---

## CMake Usage

```cmake
# In your CMakeLists.txt
cmake_minimum_required(VERSION 3.22)
project(MyApp)

# Point to your wx_bgi installation
find_package(wx_bgi_opengl REQUIRED)

# Build with wx support enabled (ON by default)
option(WXBGI_ENABLE_WX "Enable wxWidgets canvas" ON)
add_subdirectory(path/to/wx_bgi_private)

add_executable(MyApp main.cpp)
target_link_libraries(MyApp PRIVATE wx_bgi_wx)
```

Or when building wx_bgi itself — wx is ON by default, no flag needed:

```sh
cmake -S . -B build
cmake --build build -j
```

To build **without** wxWidgets (GLFW-only, smaller DLL):

```sh
cmake -S . -B build -DWXBGI_ENABLE_WX=OFF
cmake --build build -j
```

When `WXBGI_ENABLE_WX=ON` the build system automatically downloads **wxWidgets
3.2.5** via `FetchContent` (requires network access at configure time, ~170 MB
git shallow clone).  No manual wxWidgets installation is needed.

### FetchContent Details

The `CMakeLists.txt` fetches wxWidgets with these settings:

```cmake
set(wxBUILD_SHARED  OFF CACHE BOOL "" FORCE)   # static wx, no extra DLLs
set(wxBUILD_TESTS   OFF CACHE STRING "" FORCE)
set(wxBUILD_SAMPLES OFF CACHE STRING "" FORCE)
set(wxBUILD_DEMOS   OFF CACHE STRING "" FORCE)
FetchContent_Declare(wxWidgets
    GIT_REPOSITORY https://github.com/wxWidgets/wxWidgets.git
    GIT_TAG        v3.2.5
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(wxWidgets)
```

CMake targets provided by wx: `wxcore`, `wxgl`, `wxbase`.

### Output Targets

| Target | Type | Purpose |
|---|---|---|
| `wx_bgi_wx` | STATIC lib | Link into your app |
| `wx_bgi_app` | Executable | Interactive 2-D demo (mouse crosshair, key feedback) |
| `wx_bgi_3d_app` | Executable | Interactive 3-D orbit demo (camera, solids) |
| `wx_bgi_solids_test` | Executable | 3-second automated test |

---

## WxBgiCanvas API

```cpp
namespace wxbgi {

class WxBgiCanvas : public wxGLCanvas {
public:
    // Construct and embed in any wxWindow-derived parent.
    explicit WxBgiCanvas(wxWindow* parent,
                         wxWindowID id    = wxID_ANY,
                         const wxPoint& pos  = wxDefaultPosition,
                         const wxSize&  size = wxDefaultSize,
                         long style      = 0,
                         const wxString& name = "WxBgiCanvas");

    ~WxBgiCanvas() override;

    // Immediately render the current BGI page to the GL surface.
    void Render();

    // Start/update automatic repaint timer (default: off, i.e. hz=0).
    // Call with hz=60 for smooth animation, hz=0 to disable.
    void SetAutoRefreshHz(int hz);
};

} // namespace wxbgi
```

### Initialization

`WxBgiCanvas` uses a lazy-initialization pattern following the official wxWidgets
OpenGL sample (`cube.cpp`):

1. **Constructor** -- no GL work, no wx introspection, no `wxGLContext` creation.
   Just sets up event bindings.
2. **First `Render()` call** (triggered by first `OnPaint`) -- creates
   `wxGLContext`, calls `glewInit()`, calls `wxbgi_wx_init_for_canvas(w, h)`.
   This sets `BgiState::wxEmbedded = true`, allocates CPU page buffers, creates
   the default `"default"` camera (pixel-space orthographic), and the default
   `"world"` UCS.  GLFW is **never** started in wx mode.

This pattern avoids the DLL-boundary wxWidgets initialisation crash described in
the [Architecture Note](#architecture-note) below.

### Rendering

`OnPaint` makes the `wxGLContext` current (via `SetCurrent(*m_glContext)`), calls
`wxbgi_wx_render_page_gl(w, h)` (pure GL, no GLFW calls, exported by the DLL),
then calls `SwapBuffers()`.

```
flushToScreen()               (GLFW mode -- standalone)
    +-- renderPageToCurrentGLContext(w, h)  <- shared GL work
    +-- glfwSwapBuffers()

WxBgiCanvas::OnPaint()        (wx mode -- embedded)
    +-- wxbgi_wx_render_page_gl(w, h)       <- same shared GL work via C API
    +-- SwapBuffers()
```

### Architecture Note

`WxBgiCanvas` is compiled into the **`wx_bgi_wx` STATIC library**, not the
`wx_bgi_opengl.dll`.  This is critical on Windows:

> When a DLL statically links wxWidgets it gets its **own private copy** of all
> wx globals (`wxTheApp`, GL factory, timer factory, ...).  That copy has no
> `wxApp` registered (only the EXE's `wxIMPLEMENT_APP` sets `wxTheApp`).  Any
> wx assertion from inside the DLL's message handler causes a
> `STATUS_FATAL_USER_CALLBACK_EXCEPTION` (0xC000041D) crash.

By placing `WxBgiCanvas` in the STATIC lib it compiles into the EXE's address
space where `wxTheApp` is valid.  The DLL exposes only plain C functions
(`wxbgi_wx_render_page_gl`, `wxbgi_wx_key_event`, ...) that `WxBgiCanvas` calls
through the normal import table -- no wx objects cross the DLL boundary.

### Resize

`OnSize` calls `wxbgi_wx_resize(w, h)` to reallocate page buffers while keeping
camera viewports intact.

---

## BGI Drawing in wx Mode

All classic BGI drawing functions work identically:

```cpp
setbkcolor(BLACK);
cleardevice();
setcolor(WHITE);
rectangle(10, 10, 200, 200);
setfillstyle(SOLID_FILL, RED);
fillellipse(400, 300, 80, 50);
outtextxy(10, 10, "wx_bgi rocks!");
```

**Difference from GLFW mode:**

| Behaviour | GLFW mode | wx mode |
|---|---|---|
| Event loop | `wxbgi_poll_events()` | wx event loop (automatic) |
| `wxbgi_poll_events()` | pumps GLFW | no-op, returns 0 |
| Page flush | `flushToScreen()` or `swapbuffers()` | `WxBgiCanvas::OnPaint()` |
| Double-buffering | `setactivepage()` / `setvisualpage()` | unchanged |
| `glfwSwapBuffers()` | called | never called |

Use `canvas->Render()` to force an immediate repaint, or rely on the timer set
with `SetAutoRefreshHz()`.

---

## Event Routing Table

`WxBgiCanvas` translates wx events into the same internal state updates that
the GLFW callbacks perform in standalone mode.

| wx Event | BGI Internal State | User Hook Fired |
|---|---|---|
| `wxEVT_KEY_DOWN` | `gState.keyDown[glfwKey] = 1`; key queue push | `userKeyHook` (action=PRESS) |
| `wxEVT_KEY_UP` | `gState.keyDown[glfwKey] = 0` | `userKeyHook` (action=RELEASE) |
| `wxEVT_CHAR` | `gState.keyQueue` push (unicode) | `userCharHook` |
| `wxEVT_MOTION` | `gState.mouseX/Y`; `mouseMoved=true` | `userCursorPosHook` |
| `wxEVT_LEFT_DOWN/UP` | -- | `userMouseButtonHook` (button=LEFT) |
| `wxEVT_RIGHT_DOWN/UP` | -- | `userMouseButtonHook` (button=RIGHT) |
| `wxEVT_MIDDLE_DOWN/UP` | -- | `userMouseButtonHook` (button=MIDDLE) |
| `wxEVT_MOUSEWHEEL` | `gState.scrollDeltaX/Y` accumulate | `userScrollHook` |

wx key codes are translated to GLFW key constants for `keyDown[]` indexing.
The mapping is defined in `WxBgiCanvas::WxKeyToGlfw()` inside
`src/wx/wx_bgi_canvas.cpp`.  GLFW headers are **not** available in
`wx_bgi_wx` -- constants are defined locally.

---

## Scroll and Input Hooks in wx Mode

All hook registration functions work identically in wx mode:

```cpp
// Register a scroll hook -- fires whenever the mouse wheel turns
wxbgi_set_scroll_hook([](double dx, double dy, int mods) {
    printf("Scroll: %.1f  %.1f\n", dx, dy);
});

// Bypass default scroll accumulation (handle it yourself)
wxbgi_set_input_defaults(wxbgi_get_input_defaults() & ~WXBGI_DEFAULT_SCROLL_ACCUM);
```

The `WXBGI_DEFAULT_*` bypass flags apply equally to wx mode and GLFW mode.
See [InputsProcessing.md](InputsProcessing.md#bypass-mechanism) for the full
bypass API.

---

## 3D Camera and Solid Primitives

All camera, UCS, world-draw, and solid APIs work in wx mode:

```cpp
// Create a perspective camera
wxbgi_cam_create("cam3d", WXBGI_CAM_PERSPECTIVE);
wxbgi_cam_set_perspective("cam3d", 55.f, 640.f/480.f, 0.1f, 500.f);
wxbgi_cam_set_eye   ("cam3d", 8.f, 6.f, 6.f);
wxbgi_cam_set_target("cam3d", 0.f, 0.f, 0.f);
wxbgi_cam_set_up    ("cam3d", 0.f, 0.f, 1.f);
wxbgi_cam_set_screen_viewport("cam3d", 0, 0, 640, 480);
wxbgi_cam_set_active("cam3d");

// Draw solid primitives
wxbgi_solid_set_draw_mode(WXBGI_SOLID_SOLID);
wxbgi_solid_set_face_color(CYAN);
wxbgi_solid_sphere(0.f, 0.f, 0.f, 2.f, 32, 16);

// Render the scene
wxbgi_render_dds("cam3d");
canvas->Render();
```

### Solid Draw Mode Constants

| Constant | Description |
|----------|-------------|
| `WXBGI_SOLID_WIREFRAME` | Edges only, using the current line colour. |
| `WXBGI_SOLID_SOLID` / `WXBGI_SOLID_FLAT` | Filled faces with flat Phong shading (GPU pass pending). |
| `WXBGI_SOLID_SMOOTH` | Smooth per-vertex Phong shading (GPU pass pending). |

### Phong Lighting API

Configure the key/fill lights and material parameters for solid shading
(effective once the GPU solid pass, GL-5, is enabled):

```cpp
wxbgi_solid_set_light_dir(1.f, 1.f, 2.f);           // key light direction
wxbgi_solid_set_light_space(1);                       // 1=world space, 0=eye space
wxbgi_solid_set_fill_light(-1.f, 0.5f, 0.5f, 0.3f); // fill light + strength
wxbgi_solid_set_ambient(0.2f);
wxbgi_solid_set_diffuse(0.7f);
wxbgi_solid_set_specular(0.4f, 32.f);
```

### GL Context Lifecycle

`WxBgiCanvas` automatically calls `wxbgi_gl_pass_destroy()` in its destructor.
If you manage a **custom** `wxGLContext`, call it with the context current before
destroying the context to avoid GPU driver crashes on Windows:

```cpp
canvas->SetCurrent(*myGlCtx);
wxbgi_gl_pass_destroy();
delete myGlCtx;
```

---

## Thread Safety

- All `wxbgi_*` and classic BGI functions acquire `gMutex` internally.
- `WxBgiCanvas` methods run on the wx main thread -- no additional locking needed.
- User hook callbacks fire **inside** the gMutex lock.  Do **not** call any
  `wxbgi_*` function from within a hook callback -- that will deadlock.
  Use flags or queues to communicate back to the main thread instead.

---

## Multi-Canvas (Future)

The `WxBgiCanvas` constructor accepts a `bgi::BgiState*` parameter (defaults
to `&bgi::gState`).  When per-instance state is added in a future phase, pass
a heap-allocated `BgiState` to each canvas for isolated drawing surfaces with
no API changes required.

---

## Automated Test

`examples/wx/wx_bgi_solids_test.cpp` is an automated CTest entry
(`wx_bgi_solids_test`, 15-second timeout).  It:

1. Creates a `SolidsFrame` with a `WxBgiCanvas` (640x480).
2. Sets up a perspective camera `"testcam"`.
3. Draws a solid **cube** (RED), **sphere** (CYAN), and **cylinder** (YELLOW).
4. Closes automatically after **3 seconds**.
5. Exits with code 0 on success.

Run it:

```sh
cmake -S . -B build -DWXBGI_ENABLE_WX=ON -DWXBGI_ENABLE_TEST_SEAMS=ON
cmake --build build -j
ctest --test-dir build -R wx_bgi_solids_test --output-on-failure
```

---

## Interactive Demos

### 2-D Demo (`wx_bgi_app`)

Source: `examples/wx/wx_bgi_app.cpp`

Demonstrates live mouse tracking and key feedback with the classic BGI 2-D API.

| Input | Effect |
|---|---|
| Mouse move | Magenta crosshair follows the cursor; coordinates shown in HUD |
| `R` `G` `B` `C` `Y` `W` | Change the triangle fill colour |
| Any other key | Key name shown in HUD |

```sh
# Windows
.\build\Debug\wx_bgi_app.exe
```

### 3-D Orbit Demo (`wx_bgi_3d_app`)

Source: `examples/wx/wx_bgi_3d_app.cpp`

Demonstrates an orbiting perspective camera with solid 3-D primitives:
box (red), sphere (cyan), cylinder (yellow), cone (magenta), torus (light green),
a ground grid, and XYZ axes (red/green/blue).

| Input | Effect |
|---|---|
| Arrow keys | Orbit camera (azimuth / elevation) |
| `+` / `-` | Zoom in / out |
| `W` | Toggle wireframe / solid |
| `R` | Reset camera to default position |
| Left-drag | Orbit (azimuth x elevation) |
| Mouse scroll | Zoom |

Camera state (azimuth, elevation, distance) is displayed in the status bar.

```sh
# Windows
.\build\Debug\wx_bgi_3d_app.exe
```

**Implementation pattern:** The scene is built once into the DDS (retained-mode
scene graph) via `wxbgi_solid_*` calls.  On each camera change only
`wxbgi_render_dds("cam3d")` is called after updating the camera eye position --
there is no need to re-submit geometry every frame.

---

*See also: [README.md](README.md) * [InputsProcessing.md](InputsProcessing.md) * [DDS.md](DDS.md)*
