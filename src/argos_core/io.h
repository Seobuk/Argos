/****************************************************************************
** Argos - headless CAD loading (Qt-free, OpenCASCADE only)
** SPDX-License-Identifier: BSD-2-Clause
**
** Minimal BRep loaders so argos_core can be driven without Qt or Mayo's
** document model (the CLI / MCP path). Mesh formats (STL) are intentionally
** out of scope here: sub-shape (vertex/edge/face) measurement is BRep-only.
****************************************************************************/

#pragma once

#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS_Shape.hxx>

#include <string>

namespace argos {

// Load a BRep CAD file into a single shape (a compound of all roots).
// Supported by extension: .step/.stp, .iges/.igs, .brep/.brp.
// Returns a null shape on failure; *error (if given) receives a message.
TopoDS_Shape loadShape(const std::string& path, std::string* error = nullptr);

// Write a shape to a STEP file (mm units). Returns false and sets *error on
// failure. ponytail: plain STEPControl roundtrip, so colours / names /
// assembly tree are dropped; switch to STEPCAFControl if those must survive.
bool writeStep(const std::string& path, const TopoDS_Shape& shape, std::string* error = nullptr);

// Number of sub-shapes of the given type, using the stable TopExp ordering.
int countSubShapes(const TopoDS_Shape& shape, TopAbs_ShapeEnum type);

// 1-based sub-shape accessor matching countSubShapes()/TopExp ordering.
// Returns a null shape if index is out of range.
TopoDS_Shape subShape(const TopoDS_Shape& shape, TopAbs_ShapeEnum type, int index1Based);

} // namespace argos
