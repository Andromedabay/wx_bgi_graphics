# wx_BGI_Graphics

wx BGI Graphics is a C/C++ shared library that implements a large classic BGI-compatible API (Borland Graphics Interface) on top of OpenGL, GLFW and GLEW. The goal is to keep old Pascal/C/C++ graphics programs usable with minimal source changes while staying cross-platform on Windows, Linux, and macOS.

## Objectives
1. Strive to be as simple and beginner complete as the original BGI library.
1. Provide a portable graphics backend with a familiar BGI programming model.

## Current Compatibility

The library now includes classic BGI-style support for:

- graphics lifecycle and status functions such as `initgraph`, `closegraph`, `graphresult`, `graphdefaults`
- drawing primitives such as `line`, `lineto`, `linerel`, `rectangle`, `bar`, `bar3d`, `circle`, `arc`, `ellipse`, `pieslice`, `sector`
- filled drawing operations such as `floodfill`, `fillpoly`, `fillellipse`, `setfillstyle`, `setfillpattern`
- viewport, palette, and state accessors such as `setviewport`, `getviewsettings`, `setpalette`, `setrgbpalette`, `setbkcolor`, `setwritemode`
- text rendering and measurement such as `outtext`, `outtextxy`, `settextstyle`, `settextjustify`, `textwidth`, `textheight`
- image buffer helpers such as `imagesize`, `getimage`, `putimage`
- double-buffer style page selection via `setactivepage`, `setvisualpage`, `getactivepage`, `getvisualpage`, `swapbuffers`

The exported declarations are available in `src/wx_bgi.h`.

## Advanced OpenGL Extension API

In addition to classic BGI, the library now exports an optional non-BGI extension API for modern control paths such as explicit event pumping, context control, swap interval, size queries, frame begin/end, and pixel readback.

- Header: `src/wx_bgi_ext.h`
- Naming: all extension functions use the `wxbgi_` prefix
- Typical use cases:
  - integrating custom OpenGL draws between BGI calls
  - querying OpenGL/GLFW runtime details
  - controlled frame loops and framebuffer capture

This extension API is intended to complement BGI compatibility, not replace it.

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

## Usage Examples
The examples are under the ./examples/ folder.
### Example - demoFreePascal

- `examples/demoFreePascal/demo_bgi_wrapper_gui.pas`
- `examples/demoFreePascal/demo_bgi_wrapper.pas`

### Coverage Examples

- `examples/cpp/bgi_api_coverage.cpp`
- `examples/python/bgi_api_coverage.py`
- `examples/demoFreePascal/demo_bgi_api_coverage.pas`

## Compile-time Dependencies
   - GLFW
   - GLEW
   - OpenGL 
   - CMake 

## Compilation Requirements

To build this project yourself, ensure you have the following tools and libraries:

- **CMake (Version 3.14 or later):** Essential for building and managing the project files.
- **C++ Compiler:** Compatible with Clang, GCC, or MSVC. Choose one based on your development environment.
- **OpenGL-capable system libraries:** Required by GLFW/GLEW on all supported platforms.

## Building the Project

- Updated FreePascal demo instructions are in [examples/demoFreePascal/README.md](./examples/demoFreePascal/README.md).
  
## Debug

Follow these steps to build the project:

1. **Create a build directory & configure the build:**
   ```bash
   cmake -S. -Bbuild
   ```

2. **Build the project:**
   ```bash
   cmake --build build -j
   ```

3. **Run automated coverage examples:**
   ```bash
   ctest --test-dir build -C Debug --output-on-failure
   ```

4. **Generate API reference documentation:**
   ```bash
   cmake --build build --target api_docs -j
   ```

This will create a `build` directory and compile all necessary artifacts there. The main executable will be located in `build/`.

In the current Windows environment, the C++ and Python coverage examples are exercised automatically. The FreePascal coverage source is included as an example as well, and automated execution is enabled when a matching FreePascal compiler is available for the built DLL architecture.

Generated API reference output:

- `docs/API_REFERENCE.md`

The shared library output is `wx_bgi_opengl` with platform-specific extension:

- `build/Debug/wx_bgi_opengl.dll` on Windows debug builds
- `build/libwx_bgi_opengl.so` on Linux
- `build/libwx_bgi_opengl.dylib` on macOS

## Release

For release build use `--config Release` on Windows:

```bash
cmake -S. -Bbuild
cmake --build build -j --config Release
```

Artifacts for both configurations will be generated in the `build` directory.

On Mac or Linux you'll need to maintain two build trees:

```bash
cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
cmake -S. -Bbuild-rel -DCMAKE_BUILD_TYPE=Release
cmake --build build-rel -j
```

## License

This code is Open Source and Free to use with no warranties or claims.
  
This repository is based on Open-Source Code from 4 different source:
    1. Hammad Rauf, rauf.hammad@gmail.com, MIT License
         - Supervised usage and prompting of: Github Copilot Pro (Claude-Sonnet 4.6), free Trial.
    2. CMake Template, from lszl84, MIT License
    3. glfw Library, from Marcus Geelnard & Camilla Löwy, Zlib license
    4. glew Library, from https://github.com/nigels-com/glew, Custom License
  
Depends on C/C++ Template created by Luke of @devmindscapetutorilas:
 - [www.onlyfastcode.com](https://www.onlyfastcode.com)
 - [https://devmindscape.com/](https://devmindscape.com/)
 - [https://www.youtube.com/@devmindscapetutorials](https://www.youtube.com/@devmindscapetutorials)
