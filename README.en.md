<div align="center">

<img src="images/appicon_256.png" width="116" alt="Argos"/>

# Argos

**Windows-native 3D CAD measure viewer** · STEP · IGES · STL

A [Mayo](https://github.com/fougue/mayo) fork with SolidWorks-style **Measure** /
**Section** tools and a Qt-free measurement engine.

[한국어](README.md) · **[English](README.en.md)**

</div>

---

![Argos](docs/images/hero.png)

## ✨ Features

Argos keeps Mayo's solid viewer and adds **measurement, sectioning, an automation engine, and a Korean UI** on top — additively, removing nothing.

### 📐 SolidWorks-style Measure
Turn the tool on and **click vertices, edges and faces in any mix (no mode switching)** — the measurement type is inferred automatically.
Just **hovering** an entity shows its length / area / diameter live, with **zero clicks**.

- Color-coded selection chips, a **large value card**, a colored ΔX/ΔY/ΔZ grid, secondary info (radius / center / perimeter…)
- Copy value / copy JSON / clear selection, measurement history, shortcuts (`M` · `Esc` · `Ctrl+C`)

![Measure](docs/images/measure.png)

### ✂️ Section tool
Cut by standard planes (XY / YZ / ZX) with an offset slider, flip, **capping (fill)** and cap colors.
Section **perimeter / edge count** are computed by the Qt-free engine.

![Section](docs/images/section.png)

### ⚙️ `argos_core` — Qt-free engine + CLI
Measurement / section / loaders live in a **Qt-free**, OpenCASCADE-only layer that returns JSON.
The same engine is shared by the GUI, the CLI and (later) an MCP server — the path by which AI/automation can drive Argos.

```
argos_core (no Qt, OpenCASCADE only)
   ├── Argos GUI (Qt)
   └── argos-cli  → JSON
```

## ⬇️ Download

Grab the latest build from **[Releases](https://github.com/Seobuk/Argos/releases/latest)**.
Unzip `Argos-win64.zip` and run **`Argos.exe`** (Windows 10/11, 64-bit).

> This is a private repository, so downloading requires repo access.

## 🖥 Command line (`argos-cli`)

Headless measure / section with **JSON on stdout**.

```bash
# distance between two vertices (with dX/dY/dZ)
argos-cli measure part.step --vertex 1 --vertex 7

# cross-section by the XY plane at z = 5
argos-cli section part.step --plane xy --offset 5

# mass / centre-of-mass / inertia tensor (robot-link dynamics; density kg/m^3)
argos-cli props part.step --density 2700

# emit a ROS/URDF <inertial> block (paste straight into a robot link)
argos-cli props part.step --urdf

# model summary / one-shot digest (dimensions, mass, diameters)
argos-cli info part.step
argos-cli digest part.step --pretty
```

### 📑 STEP measurement report (auto PowerPoint)
Drop in a STEP and Argos builds a folder with a **photo + dimension report**:
overall width/length/height, **multi-angle (isometric) photos**, and a
**dimensioned drawing** with the size drawn right on each view. The 3D images are
rendered **fully offscreen** via `mayo-conv` — no window pops up.

```powershell
pip install python-pptx pillow
py -3.12 scripts/argos_report.py part.step --out part_report
# add the mass / inertia / URDF slides too: --with-mass
```

See [examples/planetary_gear](examples/planetary_gear) for a real report.

> 🤖 **Mass & inertia for humanoids**: `props` (or the report with `--with-mass`)
> computes volume, centre of mass, the **inertia tensor about the COM** and
> principal moments in SI units (kg, kg·m²) and exports a URDF `<inertial>` block.

## 🔨 Build (Windows)

- Visual Studio 2022 Build Tools (C++) · vcpkg (OpenCASCADE pinned to **7.9.0**) · Qt 6

```powershell
powershell -File scripts/argos-build.ps1 -Tests
powershell -File scripts/argos-package.ps1   # produces dist/Argos-win64.zip
```

Outputs in `build/Release/`: `mayo.exe` (shipped as `Argos.exe`), `argos-cli.exe`, `argos_core_test.exe`.

## 📚 Docs

- [Argos vs Mayo](docs/argos-vs-mayo.md) — per-feature comparison (done / partial / planned)
- [Product spec](docs/argos-spec.md)
- [UI design handoff](docs/ui-design/)

## 🙏 Open-source acknowledgements

Argos is built on the open source below. See **[THIRD-PARTY-NOTICES](docs/THIRD-PARTY-NOTICES.md)** for the full list with versions, copyright and links.

| Component | License |
|---|---|
| [Mayo](https://github.com/fougue/mayo) — fork base | BSD-2-Clause |
| [Qt](https://www.qt.io) 6.8.3 | LGPL-3.0 |
| [Open CASCADE](https://dev.opencascade.org) 7.9.0 | LGPL-2.1 + exception |
| [nlohmann/json](https://github.com/nlohmann/json) 3.11.3 | MIT |
| [fmt](https://github.com/fmtlib/fmt) · [Microsoft GSL](https://github.com/microsoft/GSL) · [magic_enum](https://github.com/Neargye/magic_enum) · [KDBindings](https://github.com/KDAB/KDBindings) · [miniply](https://github.com/vilya/miniply) | MIT |
| [fast_float](https://github.com/fastfloat/fast_float) | Apache-2.0 / MIT / BSL-1.0 |
| [Noto Sans KR](https://fonts.google.com/noto/specimen/Noto+Sans+KR) · [Pretendard](https://github.com/orioncactus/pretendard) | SIL OFL 1.1 |

## 📄 License

Argos keeps Mayo's permissive **BSD-2-Clause** license ([LICENSE.txt](LICENSE.txt)).
It is a fork of [fougue/mayo](https://github.com/fougue/mayo).
