# Pascal BGIDemo Port for wx_BGI

This folder contains a port of Borland's old `BGIDemo` Pascal sample to the wx_BGI shared library.

## Files

- `bgidemo.pas`: the original demo logic, adapted to use the local compatibility layer
- `Graph.pas`: a FreePascal compatibility shim that maps classic Borland `Graph` unit calls onto `wx_bgi_opengl`

## Build with CMake

Build the C++ library first, then build the Pascal example target.

### Windows

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug --target bgidemo_pascal_build
```

Outputs:

- `build/bgidemo_pascal/bgidemo.exe`
- `build/bgidemo_pascal/wx_bgi_opengl.dll`

### Linux

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target bgidemo_pascal_build
```

Outputs:

- `build/bgidemo_pascal/bgidemo`
- `build/bgidemo_pascal/libwx_bgi_opengl.so`

### macOS

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target bgidemo_pascal_build
```

Outputs:

- `build/bgidemo_pascal/bgidemo`
- `build/bgidemo_pascal/libwx_bgi_opengl.dylib`

## Run

Run the executable from the `bgidemo_pascal` output directory so the shared library is next to it.

### Windows

```powershell
.\build\bgidemo_pascal\bgidemo.exe
```

### Linux/macOS

```bash
./build/bgidemo_pascal/bgidemo
```

## Reusing `Graph.pas` with Other Old Pascal Programs

`Graph.pas` is intended as a drop-in compatibility layer for old Pascal code that used Borland's `Graph` unit.

Basic reuse flow:

1. Copy `Graph.pas` into your legacy Pascal project, or add `examples/bgidemo-pascal` to your FreePascal unit search path with `-Fu`.
2. Keep `uses Graph;` in the old source, or replace the Borland `Graph` unit reference with this local one.
3. Compile the Pascal program with FreePascal.
4. Put the matching wx_BGI shared library next to the executable, or make it available through the runtime library search path.

Example compile commands:

```powershell
C:\FPC\3.2.2\bin\i386-Win32\ppcrossx64.exe -Fuexamples\bgidemo-pascal path\to\old_program.pas
```

```bash
ppcx64 -Fuexamples/bgidemo-pascal path/to/old_program.pas
```

What this shim helps with:

- classic `Graph` constants, types, and procedures used by many Borland graphics demos
- BGI drawing, text, viewport, palette, and image APIs that already exist in wx_BGI
- `InitGraph`, `CloseGraph`, `GraphResult`, and similar Borland-era calls

What it does not automatically solve:

- old `Dos` unit logic unrelated to graphics
- CRT-dependent console UX beyond what your program already does
- direct hardware assumptions or code that expects `.BGI` driver files to exist on disk

If your old program uses only the `Graph` unit heavily, the port is usually small. If it also relies on `Dos`, BIOS behavior, or direct text-mode tricks, expect additional cleanup.

## `KeyPressed` / `ReadKey` Portability

The demo now uses portable keyboard input provided by wx_BGI itself.

- wx_BGI installs GLFW keyboard callbacks and keeps a translated key queue inside the library.
- `Graph.pas` uses that queue for `KeyPressed` and `ReadKey`, so the waits no longer depend on FreePascal `Crt` input behavior.
- Printable keys are delivered as normal characters.
- Extended keys follow classic DOS-style behavior by returning `#0` first and the extended scan code on the next `ReadKey` call.

This is substantially more portable for GUI-style waits on Windows, Linux, and macOS because the input comes from the graphics window rather than the launching terminal.