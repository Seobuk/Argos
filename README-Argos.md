# Argos — STEP Viewer with SolidWorks-style Measure & Section

Argos is a Windows-native CAD viewer built as a fork of
[Mayo](https://github.com/fougue/mayo) (Qt 6 + OpenCASCADE). It adds:

1. **SolidWorks-style Measure** — turn the tool on and just click vertices, edges
   and faces in any mix (no mode switching). The measurement type is inferred
   automatically from the selection set.
2. **Section View** — standard datum-plane (XY/YZ/ZX) clipping with an offset
   slider, flip, and a filled (capped) section, reusing Mayo's clip-plane engine.
3. **`argos_core`** — a **Qt-free**, OpenCASCADE-only measurement/section engine
   that returns **JSON**, so the same logic is shared by the GUI, the CLI and
   (later) an MCP server. This is the path by which an AI/LLM can drive Argos.
4. **`argos-cli`** — a headless console tool for measurement & sectioning.

Argos keeps Mayo's permissive **BSD-2-Clause** license (see `LICENSE.txt`).

---

## Architecture (3 layers)

```
argos_core   (Qt-free, OpenCASCADE only)   <-- measurement/section engine + JSON + loaders
   ^                ^
   |                |
Qt GUI (Argos)   argos-cli / (future) MCP
```

`argos_core` (`src/argos_core/`) never includes or links Qt. It is proven Qt-free:
the unit-test and CLI executables import only OpenCASCADE + the C runtime (verified
with `dumpbin /dependents`).

| Piece | Location |
|-------|----------|
| Measurement dispatch + JSON | `src/argos_core/measure.{h,cpp}` |
| Section-plane state + headless cut | `src/argos_core/section.{h,cpp}` |
| Headless STEP/IGES/BREP loader | `src/argos_core/io.{h,cpp}` |
| GUI measure panel (SolidWorks-style) | `src/app/widget_measure.{h,cpp}` |
| GUI clip/section panel | `src/app/widget_clip_planes.{h,cpp}` |
| Unit tests (GUI-free) | `tests/argos/test_measure.cpp` |
| CLI | `src/argos_cli/main.cpp` |

---

## Build (Windows)

Prerequisites:

- **Visual Studio 2022 Build Tools** (C++ workload incl. CMake & Ninja)
- **vcpkg** (e.g. `C:\vcpkg`) — provides OpenCASCADE. The version is pinned to
  **7.9.0** by `vcpkg.json` (Mayo's HEAD does not yet support OCCT 8.x).
- **Qt 6** MSVC build (e.g. `C:\Qt\6.8.3\msvc2022_64`, installable headlessly via
  `pip install aqtinstall` then `aqt install-qt windows desktop 6.8.3 win64_msvc2022_64`).

```powershell
# configure + build mayo (GUI) + argos-cli + tests, then run the tests
pwsh -File scripts/argos-build.ps1 -Tests `
     -VcpkgRoot C:/vcpkg -QtDir C:/Qt/6.8.3/msvc2022_64
```

Outputs land in `build/Release/` (`mayo.exe`, `argos-cli.exe`, `argos_core_test.exe`).

### Package a redistributable

```powershell
pwsh -File scripts/argos-package.ps1
# -> dist/Argos/ (Argos.exe + argos-cli.exe + all DLLs + Qt plugins) and
#    dist/Argos-win64.zip
```

To bundle a Korean UI font (optional, for non-Korean Windows), run
`scripts/argos-fetch-font.ps1` before packaging; Argos loads any `.ttf/.otf` in a
`fonts/` folder next to the executable at startup.

---

## argos-cli (headless, JSON on stdout)

```
argos-cli measure <file> [selection...] [options]
argos-cli section <file> [--plane xy|yz|zx] [--offset N] [--flip] [options]
argos-cli info    <file> [--pretty]
```

Selection (order matters; mix freely):
`--vertex N`, `--edge N`, `--face N` (1-based, stable ordering).
Options: `--point-to-point`, `--no-xyz`, `--pretty`.

Examples:

```jsonc
# distance between two vertices, with dX/dY/dZ
> argos-cli measure cube.step --vertex 1 --vertex 7
{"ok":true,"kind":"MinDistance","inputCount":2,"value":14.142135623730951,
 "unit":"mm","point":{...},"point2":{...},"delta":{"x":10.0,"y":10.0,"z":0.0}}

# cross-section by the XY plane at z = 5
> argos-cli section cube.step --plane xy --offset 5
{"ok":true,"state":{...},"section":{"ok":true,"edgeCount":4,"outlineLength":40.0,...}}

# model summary
> argos-cli info cube.step
{"ok":true,"vertices":8,"edges":12,"faces":6,"solids":1,"boundingBox":{...}}
```

Supported formats for the CLI: STEP (`.step/.stp`), IGES (`.iges/.igs`), BREP
(`.brep`). STL is mesh-based and is supported by the **GUI** viewer but not by the
sub-shape CLI measure. Non-ASCII (e.g. Korean) file paths are handled.

---

## Measurement dispatch matrix

| Selection | Result |
|-----------|--------|
| vertex ×1 | X, Y, Z |
| vertex ×2 | distance (+ dX/dY/dZ) |
| edge ×1 (line) | length |
| edge ×1 (circle) | radius / diameter / center |
| face ×1 (plane) | area (+ perimeter) |
| face ×1 (cylinder) | diameter + area |
| face ×2 (parallel) | distance |
| edge/face ×2 (non-parallel) | angle |
| circle ×2 | center-to-center distance |
| many edges / faces | total length / total area |

`--point-to-point` forces a minimum point-to-point distance for any 2 entities.
