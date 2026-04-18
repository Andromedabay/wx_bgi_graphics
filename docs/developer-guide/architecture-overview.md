# Architecture Overview

This page gives a high-level map of the main subsystems in `wx_BGI_Graphics`.

## Public API Layers

| Header | Role |
|---|---|
| `src/wx_bgi.h` | Classic BGI-compatible C API |
| `src/wx_bgi_ext.h` | Modern extension helpers (`wxbgi_*`) |
| `src/wx_bgi_3d.h` | Cameras, UCS, world-coordinate drawing |
| `src/wx_bgi_dds.h` | DDS/CHDOP retained scene API |
| `src/wx_bgi_openlb.h` | Header-only OpenLB live-loop helpers |
| `src/bgi_types.h` | Shared types and constants |

## Core Runtime Areas

| Area | Main Files | Notes |
|---|---|---|
| Global state | `src/bgi_state.cpp`, `src/bgi_state.h` | Owns global state, buffers, and lifecycle helpers |
| Immediate drawing | `src/bgi_api.cpp`, `src/bgi_draw.cpp`, `src/bgi_font.cpp`, `src/bgi_image.cpp` | Classic BGI drawing path and pixel-buffer rendering |
| Modern extension API | `src/bgi_modern_api.cpp` | Polling, frame control, wx helpers, and advanced runtime features |
| Cameras | `src/bgi_camera.cpp`, `src/bgi_camera_api.cpp` | View/projection math and camera API wrappers |
| UCS/world | `src/bgi_ucs.cpp`, `src/bgi_ucs_api.cpp`, `src/bgi_world_api.cpp` | Coordinate systems, extents, and world/UCS draw wrappers |
| DDS retained scene | `src/bgi_dds.cpp`, `src/bgi_dds_api.cpp`, `src/bgi_dds_render.cpp`, `src/bgi_dds_serial.cpp` | Scene graph, traversal, rendering, and JSON/YAML round-trips |
| 3D solids | `src/bgi_solid_api.cpp`, `src/bgi_solid_render.cpp` | Solid primitive API and tessellation/rendering |
| Overlays | `src/bgi_overlay.cpp`, `src/bgi_overlay_api.cpp` | Grid, UCS axes, cursors, and visual aids |
| wx integration | `src/wx/wx_bgi_canvas.cpp`, `src/wx/wx_bgi_canvas.h` | Embedded `WxBgiCanvas` rendering surface |

## Key Design Themes

- **Immediate mode + retained mode together:** most draw calls both render
  immediately and append a replayable DDS object.
- **Camera additive model:** classic BGI behavior remains intact while cameras,
  UCS, and world coordinates add a separate engineering/3D layer.
- **Retained composition:** retained transform and set-operation nodes wrap
  existing DDS IDs instead of mutating source objects.
- **Exact retained 3D booleans:** supported solids use the Manifold backend for
  retained union/intersection/difference.
- **Single shared state mutex:** internal state changes are guarded centrally.

## Where to Read Next

- **[DDS.md](../DDS.md)** for retained scene design
- **[Camera3D_Map.md](./Camera3D_Map.md)** and
  **[Camera2D_Map.md](./Camera2D_Map.md)** for camera internals
- **[InputsProcessing.md](./InputsProcessing.md)** for input/event behavior
- **[Tests.md](./Tests.md)** for regression coverage and expectations
