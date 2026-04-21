# OpenLB Bridge Plan and Linux Handoff

This page captures the current DDS-to-OpenLB plan and status in a repo-tracked
location so work can continue on another computer or a later time.

## Current status

Implemented:

- DDS `externalAttributes` on retained objects
- public DDS metadata API for setting, reading, clearing, and enumerating
  external attributes
- DDJ/DDY serialization for external attributes
- analytic DDS-to-material bridge MVP for retained:
  - box
  - sphere
  - cylinder
  - cone
  - torus
  - transform chains
  - set-union, set-intersection, set-difference
- exported bridge entry points in `src/wx_bgi_openlb.h`
- C++-only OpenLB helper layer in `src/wx_bgi_openlb.h` for materializing DDS
  bridge results directly onto OpenLB `SuperGeometry<T,2>` and
  `SuperGeometry<T,3>` cells
- DDS-side material preview demo
- real OpenLB-coupled smoke demo on Linux using a DDS-authored 2D obstacle,
  OpenLB flow solve, and live `wxbgi_field_*` visualization
- real OpenLB-coupled 3D duct demo on Linux using DDS-authored retained
  geometry, OpenLB flow solve, a longitudinal slice view, and an orbiting 3D
  geometry panel with `--wireframe` / `--flat` / `--smooth` rendering and
  runtime `1` / `2` / `3` mode switches
- DDS/OpenLB bridge tests
- user-facing docs for the new workflow

## Linux validation

Following design strategy works on Linux against the latest public OpenLB
release repository checkout.

Validated flows:

1. custome designed and retained DDS geometry and tag it with `openlb.*` metadata
2. materialize that retained geometry onto an OpenLB `SuperGeometry<T,2>` using
   `wxbgi_openlb_materialize_super_geometry_2d(...)`
3. run a compact 2D channel + obstacle case in
   `examples/cpp/wxbgi_openlb_coupled_smoke.cpp`
4. materialize retained DDS sieve geometry onto an OpenLB
    `SuperGeometry<T,3>` using `wxbgi_openlb_materialize_super_geometry_3d(...)`
5. run a compact 3D duct + vent case in
   `examples/cpp/openlb-demo/wxbgi_openlb_pipe_3d_demo.cpp`
6. render live scalar/vector fields and DDS geometry with the existing
    `wxbgi_field_*` helpers plus the retained 3D DDS renderer

Observed results:

| Platform / OpenLB tree | Result |
|---|---|
| latest public GitLab release tree | Linux build + focused OpenLB bridge tests pass |
| `1.9.0` native Windows/MSVC | fails early on compiler builtin / feature-detection assumptions |
| `1.6.0` native Windows/MSVC | gets much further, but still fails in OpenLB itself on Unix-only headers such as `unistd.h` |

Conclusion: the DDS bridge MVP and the fully coupled 2D Linux path are working
in this repository, but native Windows/MSVC remains blocked by upstream/toolchain
compatibility.


## Important files

| File | Purpose |
|---|---|
| `src/bgi_openlb_bridge.h` / `src/bgi_openlb_bridge.cpp` | internal analytic DDS material bridge |
| `src/bgi_openlb_bridge_api.cpp` | exported bridge wrappers |
| `src/wx_bgi_openlb.h` | public OpenLB-facing helper header plus C++ materialization helpers |
| `examples/cpp/wxbgi_openlb_material_preview_demo.cpp` | DDS-side authoring and preview demo |
| `examples/cpp/wxbgi_openlb_coupled_smoke.cpp` | real OpenLB-coupled 2D channel + obstacle demo |
| `examples/cpp/openlb-demo/wxbgi_openlb_pipe_3d_demo.cpp` | real OpenLB-coupled 3D duct + orbit preview demo |
| `examples/cpp/openlb-demo/run_openlb_pipe_3d_demo.sh` | Debian/Ubuntu/WSL2 bootstrap script for the 3D OpenLB demo |
| `examples/cpp/openlb-demo/run_openlb_pipe_3d_demo_macos.sh` | macOS/Homebrew bootstrap script for the 3D OpenLB demo |
| `examples/cpp/test_openlb_bridge_materialize_2d.cpp` | current bridge regression test |
| `docs/user-guide/OpenLB-Support.md` | user-facing workflow and platform status |

## Linux continuation checklist

On the Linux machine:

1. clone this repository and checkout the working branch
2. clone the latest public OpenLB release repository
3. point `OPENLB_ROOT` at that checkout
4. configure with `-DWXBGI_ENABLE_OPENLB=ON`
5. build the focused OpenLB demos
6. run the focused OpenLB bridge tests

Suggested commands:

```bash
git clone git@github.com:Andromedabay/wx_bgi_graphics.git
cd wx_bgi_graphics

git clone https://gitlab.com/openlb/release.git ../openlb-release

cmake -S . -B build_openlb \
  -DCMAKE_BUILD_TYPE=Debug \
  -DWXBGI_ENABLE_OPENLB=ON \
  -DOPENLB_ROOT=../openlb-release

cmake --build build_openlb -j --target \
  wxbgi_openlb_coupled_smoke \
  wxbgi_openlb_pipe_3d_demo \
  test_dds_external_attrs \
  test_openlb_bridge_materialize_2d \
  wxbgi_openlb_material_preview_demo

ctest --test-dir build_openlb --output-on-failure -R \
  "test_dds_external_attrs|test_openlb_bridge_materialize_2d|wxbgi_openlb_material_preview_demo|wxbgi_openlb_coupled_smoke|wxbgi_openlb_pipe_3d_demo"
```

Or use the repo-tracked bootstrap script:

```bash
examples/cpp/openlb-demo/run_openlb_pipe_3d_demo.sh --test
```

On macOS, use the Homebrew-oriented companion script:

```bash
examples/cpp/openlb-demo/run_openlb_pipe_3d_demo_macos.sh --test
```

If one coupled OpenLB target needs focused iteration, build it directly:

```bash
cmake --build build_openlb --target wxbgi_openlb_coupled_smoke -j
cmake --build build_openlb --target wxbgi_openlb_pipe_3d_demo -j
```
