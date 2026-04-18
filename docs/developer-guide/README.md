# Developer Guide

This guide is for contributors and programmers who want to understand how the
project is built, tested, documented, and organized internally.

## Build, Tooling, and Quality

| Topic | Description |
|---|---|
| **[Building.md](./Building.md)** | Dependencies, CMake flags, platform-specific build notes, output paths, and documentation build targets |
| **[Tests.md](./Tests.md)** | Test catalog, categories, platform notes, and security/test-seam rules |
| **[Documentation Pipeline](./documentation-pipeline.md)** | How markdown, Doxygen, HTML, and PDF outputs are wired together |

## Architecture and Design

| Topic | Description |
|---|---|
| **[Architecture Overview](./architecture-overview.md)** | High-level map of the major subsystems and source files |
| **[DDS.md](../DDS.md)** | Retained scene graph design, CHDOP types, serialization, and composition behavior |
| **[OpenLB Bridge Plan](./openlb-bridge-plan.md)** | Current DDS-to-OpenLB plan, status, blocker, and Linux continuation checklist |
| **[Camera3D_Map.md](./Camera3D_Map.md)** | Camera internals and 3D API/code map |
| **[Camera2D_Map.md](./Camera2D_Map.md)** | 2D camera behavior and implementation map |
| **[InputsProcessing.md](./InputsProcessing.md)** | Input model, queue semantics, hooks, and backend behavior |
| **[WxWidgets.md](../WxWidgets.md)** | wx integration details including embedded canvas architecture |

## Related Guides

- Return to the **[project README](../../README.md)**
- For public feature usage and tutorials, see the
  **[User Guide](../user-guide/README.md)**
