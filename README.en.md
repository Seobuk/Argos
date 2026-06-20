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

# model summary
argos-cli info part.step
```

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

## 📄 License

Argos keeps Mayo's permissive **BSD-2-Clause** license ([LICENSE.txt](LICENSE.txt)).
It is a fork of [fougue/mayo](https://github.com/fougue/mayo).
