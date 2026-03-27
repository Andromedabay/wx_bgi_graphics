# FreePascal Demo for wx_bgi Shared Library

This folder contains a minimal FreePascal program that loads the shared library and exercises the BGI-like API.

## Files

- `demo_bgi_wrapper.pas`
- `demo_bgi_wrapper_gui.pas`
- `demo_bgi_api_coverage.pas`
- `demo_wxbgi_keyboard_queue.pas`

`demo_bgi_api_coverage.pas` is the broader API coverage example. It exercises the expanded classic BGI export surface from FreePascal rather than only drawing a minimal scene.
`demo_wxbgi_keyboard_queue.pas` is a small direct extension-API sample for `wxbgi_key_pressed`, `wxbgi_read_key`, and `wxbgi_is_key_down`.

## Build and Run (Windows)

1. Build the C++ library first.

   Important: DLL architecture must match the Pascal EXE architecture.

   For 32-bit Pascal (`i386`):

   ```bash
   cmake -S . -B build32 -A Win32 -DCMAKE_BUILD_TYPE=Debug
   cmake --build build32 -j
   ```

   For 64-bit Pascal (`x86_64`):

   ```bash
   cmake -S . -B build64 -A x64 -DCMAKE_BUILD_TYPE=Debug
   cmake --build build64 -j
   ```

2. Make sure `wx_bgi_opengl.dll` is next to `demo_bgi_wrapper.exe` (or available on `PATH`).

   The DLL is generated at:

   - `build32/Debug/wx_bgi_opengl.dll`
   - `build64/Debug/wx_bgi_opengl.dll`

3. Compile demo.

   Default FPC may produce `i386` Win32:

   ```bash
   fpc demoFreePascal/demo_bgi_wrapper.pas
   ```

   GUI-subsystem variant (no console window):

   ```bash
   fpc demoFreePascal/demo_bgi_wrapper_gui.pas
   ```

   If you have 64-bit FPC installed, use:

   ```bash
   ppcx64 demoFreePascal/demo_bgi_wrapper.pas
   ```

   GUI-subsystem variant with 64-bit FPC:

   ```bash
   ppcx64 demoFreePascal/demo_bgi_wrapper_gui.pas
   ```

   Coverage example with 64-bit FPC:

   ```bash
   ppcx64 demoFreePascal/demo_bgi_api_coverage.pas
   ```

   Keyboard queue example with 64-bit FPC:

   ```bash
   ppcx64 demoFreePascal/demo_wxbgi_keyboard_queue.pas
   ```

4. Run:

   ```bash
   demoFreePascal/demo_bgi_wrapper.exe
   ```

   GUI-subsystem variant:

   ```bash
   demoFreePascal/demo_bgi_wrapper_gui.exe
   ```

   Coverage example:

   ```bash
   demoFreePascal/demo_bgi_api_coverage.exe
   ```

## Troubleshooting

- Error `0xc000007b` almost always means 32-bit and 64-bit binaries are mixed.
- Both console and GUI-subsystem demo variants are provided; subsystem choice does not cause `0xc000007b`.
- The coverage example should be compiled with the same architecture as the `wx_bgi_opengl` library. On Windows, a 32-bit FreePascal executable cannot load a 64-bit DLL, and vice versa.

## Linux/macOS

- The program already maps library names automatically:
  - `libwx_bgi_opengl.so` (Linux)
  - `libwx_bgi_opengl.dylib` (macOS)
- Ensure the library can be found via rpath or runtime library path (for example, `LD_LIBRARY_PATH` on Linux, `DYLD_LIBRARY_PATH` on macOS).
