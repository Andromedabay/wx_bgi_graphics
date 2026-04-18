# Getting Started

This page gives a quick path from source tree to a working build.

## What the Project Provides

`wx_BGI_Graphics` combines:

- classic BGI-style immediate drawing
- retained DDS scene replay and serialization
- 2D/3D cameras and world-coordinate drawing
- 3D solids and retained affine composition
- standalone windows and wxWidgets embedded canvases

## Supported Platforms

- Windows
- Linux
- macOS

## Main Dependencies

Most C/C++ dependencies are fetched automatically by CMake:

- GLFW 3.4
- GLEW 2.2.0
- GLM 1.0.1
- wxWidgets 3.2.5+
- nlohmann/json
- yaml-cpp
- Manifold

Optional external tools:

- Doxygen
- LaTeX / MiKTeX
- FreePascal
- Python 3

## Build Commands

### Windows

```powershell
cmake -S . -B build
cmake --build build -j --config Debug
cmake --build build -j --config Release
```

### Linux

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

### macOS

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

For detailed platform notes, optional flags, OpenLB staging, and macOS-specific
workarounds, see **[Building.md](../developer-guide/Building.md)**.

## Run Tests

### Windows

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

### Linux / macOS

```bash
ctest --test-dir build --output-on-failure
```

For the full test catalog and platform-specific notes, see
**[Tests.md](../developer-guide/Tests.md)**.

## Suggested Reading Path

1. **[Tutorial.md](./Tutorial.md)** for classic buffering and animation
2. **[DDS.md](../DDS.md)** for retained scenes and serialization
3. **[Object-Operation.md](../Object-Operation.md)** for transforms and set operations
4. **[WxWidgets.md](../WxWidgets.md)** if you want embedded GUI integration
5. **[OpenLB-Support.md](./OpenLB-Support.md)** if you want solver/live-view integration

## Next

- Go back to the **[User Guide index](./README.md)**
- Jump to the **[Developer Guide](../developer-guide/README.md)** for build and design internals
