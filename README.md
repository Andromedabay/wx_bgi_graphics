# wx_BGI_Graphics

wx BGI Graphics is a C/C++ shared library that implements a large classic BGI-compatible API (Borland Graphics Interface) on top of OpenGL, GLFW and GLEW. The goal is to keep old Pascal/C/C++ graphics programs usable with minimal source changes while staying cross-platform on Windows, Linux, and macOS.

## Objectives
1. Strive to be as simple and beginner complete as the original BGI library.
1. Provide a portable graphics backend with a familiar BGI programming model, and some advanced user OpenGL capability.

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
   - Doxygen (for API documentation generation)

## Compilation Requirements

To build this project yourself, ensure you have the following tools and libraries:

- **CMake (Version 3.14 or later):** Essential for building and managing the project files.
- **C++ Compiler:** Compatible with Clang, GCC, or MSVC. Choose one based on your development environment.
- **OpenGL-capable system libraries:** Required by GLFW/GLEW on all supported platforms.

## Building the Project

- Updated FreePascal demo instructions are in examples/demoFreePascal/README.md.

## Build, Test, and Docs on Windows/Linux/macOS

Use the command sets below to run a complete local pipeline: configure, build, test, and generate Doxygen docs.

### Windows (MSVC, multi-config)

Debug pipeline:

```powershell
cmake -S . -B build
cmake --build build -j --config Debug
ctest --test-dir build -C Debug --output-on-failure
cmake --build build --target api_docs -j --config Debug
```

Release pipeline:

```powershell
cmake -S . -B build
cmake --build build -j --config Release
ctest --test-dir build -C Release --output-on-failure
cmake --build build --target api_docs -j --config Release
```

### Linux (single-config)

Debug pipeline:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
cmake --build build --target api_docs -j
```

Release pipeline:

```bash
cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release
cmake --build build-rel -j
ctest --test-dir build-rel --output-on-failure
cmake --build build-rel --target api_docs -j
```

### macOS (single-config)

Debug pipeline:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
cmake --build build --target api_docs -j
```

Release pipeline:

```bash
cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release
cmake --build build-rel -j
ctest --test-dir build-rel --output-on-failure
cmake --build build-rel --target api_docs -j
```

Generated API documentation output:

- `build/doxygen/html/index.html` (debug tree)
- `build-rel/doxygen/html/index.html` (release tree on Linux/macOS)

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
  
This repository is based on Open-Source Code from 4 different source:
    1. Hammad Rauf, rauf.hammad@gmail.com, MIT License
         - Supervised usage and prompting of: Github Copilot Pro (Claude-Sonnet 4.6), free Trial.
    2. CMake Template, from lszl84, MIT License
    3. glfw Library, from Marcus Geelnard & Camilla Löwy, Zlib license
    4. glew Library, from https://github.com/nigels-com/glew, Custom License
  
Depends on C/C++ Template created by Luke of devmindscapetutorilas:
 - [www.onlyfastcode.com](https://www.onlyfastcode.com)
 - [https://devmindscape.com/](https://devmindscape.com/)
 - [https://www.youtube.com/@devmindscapetutorials](https://www.youtube.com/@devmindscapetutorials)
