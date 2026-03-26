# wx_BGI_Graphics
wx BGI Graphics, is a C/C++ shared-lib - Borland Graphics Interface implementation on OpenGL and wxWidgets. Purpose is to make old Pascal/C/C++ Graphics Code (Written near about 1985-1996) usable with minimal changes. It can also be useful for novice programmers to start using Computer Graphics and OpenGL.

## Objectives
1. Strive to be as simple and beginner complete as the original BGI library.
1. Facilitate use of OpenGL and Mouse with ease.

## Usage Examples
The examples are under the ./examples/ folder.
### Example - demoFreePascal
```pascal:examples/demoFreePascal/demo_bgi_wrapper_gui.pas

```

## Compile-time Dependencies
   - WxWidgets 
   - OpenGL 
   - CMake 

## Compilation Requirements

To build this project yourself, ensure you have the following tools and libraries:

- **CMake (Version 3.14 or later):** Essential for building and managing the project files.
- **C++ Compiler:** Compatible with Clang, GCC, or MSVC. Choose one based on your development environment.
- **GTK3 Development Libraries (for Linux users):** Necessary for GUI development on Linux platforms.

## Building the Project
- Updated Build Instructions are in [./examples/demoFreePascal/README.md](./examples/demoFreePascal/README.md).
  
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

This will create a `build` directory and compile all necessary artifacts there. The main executable will be located in `build/`.

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

MIT License. Can be used in closed-source commercial products.
  
Depends on C/C++ Template created by Luke of @devmindscapetutorilas:
 - [www.onlyfastcode.com](https://www.onlyfastcode.com)
 - [https://devmindscape.com/](https://devmindscape.com/)
 - [https://www.youtube.com/@devmindscapetutorials](https://www.youtube.com/@devmindscapetutorials)
