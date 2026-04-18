# OpenLB Support

`wx_bgi_graphics` does **not** turn OpenLB into a built-in GUI toolkit. Instead,
it provides a practical way to use `wx_bgi` as the **interactive viewer shell**
around an OpenLB simulation.

OpenLB remains the simulation owner:

1. OpenLB advances the solver.
2. Your code extracts scalar/vector snapshots from the current state.
3. `wx_bgi` renders those snapshots live in a wx-backed window.

---

## What is OpenLB?

[OpenLB](https://www.openlb.net/) is an open-source C++ framework for
Lattice Boltzmann Method (LBM) simulation. It is aimed at Computational Fluid Dynamics (CFD) and related
transport problems, involving multiple physics(physical) systems, with strong emphasis on:

- batch / HPC workflows
- MPI / OpenMP / GPU-capable simulation backends
- offline output such as VTK, images, CSV, and plots

OpenLB itself is primarily a **simulation framework**, not a live interactive
windowing system. That is where `wx_bgi_graphics` fits in.

---

## What support is provided here?

The shared library now includes an **optional OpenLB-oriented live-view path**
with these parts:

### 1. Generic field-visualization helpers

Declared in `src/wx_bgi_ext.h`:

- `wxbgi_field_draw_scalar_grid(...)`
- `wxbgi_field_draw_vector_grid(...)`
- `wxbgi_field_draw_scalar_legend(...)`

These are generic solver-view helpers, not OpenLB-only APIs. They are intended
for live rendering of:

- velocity magnitude
- pressure / density
- vector arrows
- legends and HUD overlays

### 2. Header-only live-loop wrappers and bridge entry points

Declared in `src/wx_bgi_openlb.h`:

- `wxbgi_openlb_begin_session(...)`
- `wxbgi_openlb_pump()`
- `wxbgi_openlb_present()`
- `wxbgi_openlb_classify_point_material(...)`
- `wxbgi_openlb_sample_materials_2d(...)`

These wrappers support an **OpenLB-style non-blocking main loop** where the
simulation remains in charge and `wx_bgi` only handles rendering and event
pumping.

The two material-query functions form the current DDS-to-OpenLB bridge MVP:

- `wxbgi_openlb_classify_point_material(...)` classifies one world-space sample
  point against the retained DDS scene and returns the resolved material id
- `wxbgi_openlb_sample_materials_2d(...)` samples a regular 2D grid of world
  points and writes one material id per sample

The bridge reads generic DDS metadata from `externalAttributes` rather than
hard-coding OpenLB-only fields into the core DDS type.

### 3. Interactive demos

The repository now includes:

- `examples/cpp/wxbgi_openlb_live_demo.cpp`
- `examples/cpp/wxbgi_openlb_material_preview_demo.cpp`

`wxbgi_openlb_live_demo.cpp` uses a mock live field, but it demonstrates the
intended integration pattern for a real OpenLB solver:

```cpp
while (wxbgi_openlb_pump()) {
    stepSolverChunk();

    cleardevice();
    wxbgi_field_draw_scalar_grid(...);
    wxbgi_field_draw_vector_grid(...);
    wxbgi_field_draw_scalar_legend(...);

    wxbgi_openlb_present();
}
```

`wxbgi_openlb_material_preview_demo.cpp` demonstrates the authoring side of the
workflow:

1. build retained DDS solids and set-operations
2. attach OpenLB-facing metadata such as `openlb.material`
3. render the exact retained 3D scene through a perspective camera
4. sample a 2D material grid and preview it with `wxbgi_field_draw_scalar_grid`

### 4. Optional build and staging support

OpenLB support is **opt-in**:

```text
WXBGI_ENABLE_OPENLB=ON
OPENLB_ROOT=<path to local OpenLB release tree>
```

When enabled, the build adds:

- OpenLB option validation (`OPENLB_ROOT\src\olb.h` must exist)
- `openlb_bridge_package` target
- an optional OpenLB-coupled smoke target for local validation

That staging target collects the current shared library, headers, and demo into
`build/openlb_bridge` so they can be referenced from an OpenLB checkout or demo
workflow.

---

## DDS metadata convention for OpenLB

Every DDS object now has a generic `externalAttributes` map that can store
OpenLB-facing labels. The bridge currently uses these keys:

| Key | Meaning |
|---|---|
| `openlb.role` | semantic role such as `fluid`, `solid`, or `boundary` |
| `openlb.material` | explicit integer material id encoded as a string |
| `openlb.boundary` | optional boundary name such as `wall` or `inlet` |
| `openlb.priority` | integer tie-breaker when multiple solids overlap |
| `openlb.enabled` | `0`/`1` style on-off switch for export |

Useful DDS metadata APIs:

- `wxbgi_dds_set_external_attr(id, key, value)`
- `wxbgi_dds_get_external_attr(id, key)`
- `wxbgi_dds_clear_external_attr(id, key)`
- `wxbgi_dds_external_attr_count(id)`
- `wxbgi_dds_get_external_attr_key_at(id, index)`
- `wxbgi_dds_get_external_attr_value_at(id, index)`

## Supported bridge geometry subset

The current material-classification bridge supports retained:

- box
- sphere
- cylinder
- cone
- torus
- transform chains
- set-union, set-intersection, and set-difference nodes

Unsupported DDS object types are currently treated as non-exportable solver
geometry rather than being approximated silently.

## What is not provided?

The current support intentionally does **not** do the following:

- bundle OpenLB into the main shared library as a hard dependency
- expose OpenLB template internals through the stable DLL ABI
- replace OpenLB's own solver setup, meshing, or physics code
- provide direct built-in 3D iso-surface extraction from OpenLB fields
- export every DDS primitive type to solver geometry yet

In other words: this repo currently provides the **viewer/runtime bridge**, not
a full OpenLB binding layer.

---

## Recommended integration model

Use `wx_bgi` as the interactive visualization frontend:

1. author retained solver geometry in DDS
2. tag solids and set-operation nodes with `openlb.*` metadata
3. classify lattice cell centres from DDS using the bridge helpers
4. advance OpenLB in your own control loop
5. publish reduced scalar/vector data for display
6. draw using the `wxbgi_field_*` helpers
7. keep the GUI responsive with `wxbgi_openlb_pump()` / `wxbgi_openlb_present()`

This matches the current wx rendering model well:

- rendering stays on the GUI side
- the simulation loop stays under user control
- no blocking `wxIMPLEMENT_APP`-managed application loop is required

## Windows / MSVC status

Local validation now includes a real OpenLB checkout at:

`C:\Users\hamma\AppData\Local\Temp\openlb-release`

An older checkout was also tested at:

`C:\Users\hamma\AppData\Local\Temp\openlb-release-1.6.0`

That is sufficient for `OPENLB_ROOT`, and the repository now wires a local
OpenLB-coupled smoke target when `WXBGI_ENABLE_OPENLB=ON`. However, the current
public OpenLB release still does **not** compile cleanly under native
Windows/MSVC in this environment. The observed failures are in OpenLB headers
and templates, not in the DDS bridge or viewer code in this repository.

Observed results so far:

- OpenLB `1.9.0` fails early on native MSVC because the current release assumes
  compiler builtins and feature detection that do not map cleanly to this toolchain
- OpenLB `1.6.0` gets much further, and this repository now includes both
  `external/tinyxml` and `external/tinyxml2` in the optional OpenLB include
  path so older releases are wired correctly
- even with that fix, OpenLB `1.6.0` still fails under native MSVC because
  OpenLB itself directly includes Unix-only headers such as `unistd.h`

Practical consequence:

- DDS metadata, bridge helpers, and the DDS-side preview demo work on Windows
- the real OpenLB-coupled smoke build is currently blocked on upstream/toolchain
  compatibility
- for full coupled validation today, prefer a Linux, macOS, or WSL-based OpenLB
  toolchain

---

## Demo screenshot

> `examples/cpp/wxbgi_openlb_live_demo.cpp`

![OpenLB live demo](../../images/screenshot_openlb_live_demo.png)

The demo shows the current viewer pattern: a live false-colour scalar field,
decimated vector arrows, and a scalar legend rendered in real time inside the
wx-backed `wx_bgi` window.
