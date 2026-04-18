# OpenLB Bridge Plan and Linux Handoff

This page captures the current DDS-to-OpenLB plan and status in a repo-tracked
location so work can continue on another computer.

## Current status

Implemented in this branch:

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
- DDS-side material preview demo
- DDS/OpenLB bridge tests
- user-facing docs for the new workflow

## Current blocker

The remaining gap is the **real OpenLB-coupled demo/helper layer** on native
Windows/MSVC.

Observed results:

| OpenLB release | Native Windows/MSVC result |
|---|---|
| `1.9.0` | fails early on compiler builtin / feature-detection assumptions |
| `1.6.0` | gets much further, but still fails in OpenLB itself on Unix-only headers such as `unistd.h` |

Conclusion: the DDS bridge MVP in this repository is working, but the fully
coupled OpenLB path should be continued on **Linux, macOS, or WSL** rather than
native Windows/MSVC.

## Remaining stages

### Stage 3 — OpenLB integration helper layer

Finish the example-side helper utilities that take DDS-authored geometry plus
lattice parameters and apply materials into an OpenLB geometry / superGeometry
setup.

Target outcome:

- compile a real OpenLB-backed example using the bridge
- keep OpenLB internals in demos/examples rather than in the stable C ABI

### Stage 4 — Fully coupled demo

Finish the real OpenLB live simulation demo:

1. derive lattice materials from DDS-authored geometry
2. run a compact benchmark case
3. visualize scalar/vector fields live with existing `wxbgi_field_*` helpers

Recommended benchmark:

- start with a 2D channel + obstacle case
- only move to a 3D case after the 2D coupled loop is stable

### Stage 5 — Follow-on validation

Optional but recommended follow-up work:

- add a 3D materialization test
- add extra coupled smoke coverage once the Linux/macOS/WSL path is running

## Important files

| File | Purpose |
|---|---|
| `src/bgi_openlb_bridge.h` / `src/bgi_openlb_bridge.cpp` | internal analytic DDS material bridge |
| `src/bgi_openlb_bridge_api.cpp` | exported bridge wrappers |
| `src/wx_bgi_openlb.h` | public OpenLB-facing helper header |
| `examples/cpp/wxbgi_openlb_material_preview_demo.cpp` | DDS-side authoring and preview demo |
| `examples/cpp/wxbgi_openlb_coupled_smoke.cpp` | OpenLB-coupled smoke target to continue on Linux |
| `examples/cpp/test_openlb_bridge_materialize_2d.cpp` | current bridge regression test |
| `docs/user-guide/OpenLB-Support.md` | user-facing workflow and platform status |

## Linux continuation checklist

On the Linux machine:

1. clone this repository and checkout branch `OpenLBTaskA`
2. read this file first
3. point `OPENLB_ROOT` at a local OpenLB checkout on Linux
4. configure with `-DWXBGI_ENABLE_OPENLB=ON`
5. build `wxbgi_openlb_coupled_smoke`
6. if that succeeds, continue Stage 3 / Stage 4 there

Suggested commands:

```bash
git clone git@github.com:Andromedabay/wx_bgi_graphics.git
cd wx_bgi_graphics
git checkout OpenLBTaskA

cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DWXBGI_ENABLE_OPENLB=ON -DOPENLB_ROOT=/path/to/openlb
cmake --build build -j
ctest --test-dir build --output-on-failure -R "test_dds_external_attrs|test_openlb_bridge_materialize_2d|wxbgi_openlb_material_preview_demo"
```

If the coupled OpenLB target still needs focused iteration, build it directly:

```bash
cmake --build build --target wxbgi_openlb_coupled_smoke -j
```
