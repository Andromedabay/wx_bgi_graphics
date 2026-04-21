\mainpage wx_BGI_Graphics

# wx_BGI_Graphics

`wx_BGI_Graphics` is a cross-platform shared library that keeps the classic
Borland BGI programming model usable on modern systems by implementing the API
on top of OpenGL, GLFW, GLEW, and wxWidgets.

It is designed for programmers who want a simple graphics API for C, C++,
Pascal, Python, and similar environments, while still gaining access to modern
features such as retained scenes, multiple cameras, world coordinates, 3D
solids, embedded wxWidgets canvases, and optional OpenLB-oriented live viewing.

## Project Objectives

1. Preserve the simple BGI programming model for modern platforms.
2. Keep legacy graphics code portable across Windows, Linux, and macOS.
3. Hide most OpenGL complexity behind a small and stable API surface.
4. Provide both classic BGI calls and optional modern extension APIs.
5. Support both lightweight standalone windows and richer wxWidgets embedding.
6. Support [OpenLB](https://www.openlb.net/) integration through DDS metadata, material-sampling helpers, and live visualization. OpenLB itself is not included in this shared-lib.

## Highlights

- Classic BGI-compatible 2D drawing API via `src/wx_bgi.h`
- Modern extension helpers via `src/wx_bgi_ext.h`
- 2D/3D cameras, UCS, and world-coordinate drawing via `src/wx_bgi_3d.h`
- Retained DDS/CHDOP scene graph with JSON/YAML serialization via `src/wx_bgi_dds.h`
- Retained affine transforms, set operations, and exact 3D Manifold booleans
- 3D solid primitives, surfaces, overlays, and wxWidgets embedded canvas support
- Optional OpenLB live-visualization and DDS material-bridge support

## Dependencies

Core dependencies used by the project:

| Dependency | Purpose |
|---|---|
| GLFW 3.4 | Standalone windowing and OpenGL context creation |
| GLEW 2.2.0 | OpenGL extension loading |
| GLM 1.0.1 | Camera, UCS, and transform math |
| wxWidgets 3.2.5+ | Embedded canvas and default windowed backend |
| nlohmann/json | DDS JSON serialization |
| yaml-cpp | DDS YAML serialization |
| Manifold | Exact retained 3D boolean evaluation |

Most C/C++ dependencies are fetched automatically by CMake. Optional external
tools include Doxygen, LaTeX/MiKTeX, FreePascal, and Python 3.

## Releases and Downloads

- Latest release: https://github.com/Andromedabay/wx_bgi_graphics/releases/latest
- Windows x64 binary zip: https://github.com/Andromedabay/wx_bgi_graphics/releases/latest/download/wx_bgi_opengl-windows-x64.zip
- Linux x64 binary tar.gz: https://github.com/Andromedabay/wx_bgi_graphics/releases/latest/download/wx_bgi_opengl-linux-x64.tar.gz
- macOS x64 binary tar.gz: https://github.com/Andromedabay/wx_bgi_graphics/releases/latest/download/wx_bgi_opengl-macos-x64.tar.gz
- Published API docs: https://andromedabay.github.io/wx_bgi_graphics/

## Documentation

The documentation is organized into two main hierarchies.


\section user-guide User-Guide

Start here if you want to build the library, run it, and use its public
features in applications.

- **[User Guide index](./docs/user-guide/README.md)**
- **[Getting Started](./docs/user-guide/getting-started.md)**


\section developer-guide Developer-Guide

Start here if you want to understand how the project is built, how the major
subsystems are organized, how tests are structured, and how the docs pipeline is
generated.

- **[Developer Guide index](./docs/developer-guide/README.md)**
- **[Architecture Overview](./docs/developer-guide/architecture-overview.md)**
- **[OpenLB Bridge Plan](./docs/developer-guide/openlb-bridge-plan.md)**
- **[Documentation Pipeline](./docs/developer-guide/documentation-pipeline.md)**


## Public Headers

| Header | Purpose |
|---|---|
| `src/wx_bgi.h` | Classic BGI C API |
| `src/wx_bgi_ext.h` | Modern `wxbgi_*` extension helpers |
| `src/wx_bgi_3d.h` | Cameras, UCS, world-coordinate drawing |
| `src/wx_bgi_dds.h` | Retained DDS/CHDOP scene graph |
| `src/wx_bgi_openlb.h` | OpenLB live-loop helpers and DDS material-query bridge entry points |
| `src/bgi_types.h` | Shared types, structs, and constants |

## Quick Orientation

- New users should begin with **[Getting Started](./docs/user-guide/getting-started.md)**.
- For animation/page-buffer workflow, read **[Tutorial.md](./docs/user-guide/Tutorial.md)**.
- For retained scenes and composition, read **[DDS.md](./docs/DDS.md)** and
  **[Object-Operation.md](./docs/Object-Operation.md)**.
- For DDS-to-OpenLB authoring and viewer integration, read
  **[OpenLB-Support.md](./docs/user-guide/OpenLB-Support.md)**.
- For build, testing, and design internals, use the **Developer Guide**.

## OpenLB Status

The repository now supports DDS external attributes plus material-query helpers
for OpenLB-oriented workflows. A DDS-only preview demo is included and works in
the standard build. A real OpenLB-coupled smoke target is also wired when
`WXBGI_ENABLE_OPENLB=ON`, but the current public OpenLB release does not compile
cleanly under native Windows/MSVC in this environment, so full coupled
validation currently requires a Linux, macOS, or WSL-style toolchain.

## Screenshot Gallery

See **[ScreenShots.md](./docs/user-guide/ScreenShots.md)** for example output and reference
images from the current demos.
