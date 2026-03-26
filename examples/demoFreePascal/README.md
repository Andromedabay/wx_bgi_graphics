# FreePascal Demo for wx_bgi Shared Library

This folder contains a minimal FreePascal program that loads the shared library and exercises the BGI-like API.

## Files

- `demo_bgi_wrapper.pas`
- `demo_bgi_wrapper_gui.pas`

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

4. Run:

   ```bash
   demoFreePascal/demo_bgi_wrapper.exe
   ```

   GUI-subsystem variant:

   ```bash
   demoFreePascal/demo_bgi_wrapper_gui.exe
   ```

## Troubleshooting

- Error `0xc000007b` almost always means 32-bit and 64-bit binaries are mixed.
- Both console and GUI-subsystem demo variants are provided; subsystem choice does not cause `0xc000007b`.

## Linux/macOS

- The program already maps library names automatically:
  - `libwx_bgi_opengl.so` (Linux)
  - `libwx_bgi_opengl.dylib` (macOS)
- Ensure the library can be found via rpath or runtime library path (for example, `LD_LIBRARY_PATH` on Linux, `DYLD_LIBRARY_PATH` on macOS).
