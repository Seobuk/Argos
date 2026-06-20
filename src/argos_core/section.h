/****************************************************************************
** Argos - section-plane state & headless cross-section (Qt-free, OCCT only)
** SPDX-License-Identifier: BSD-2-Clause
**
** SectionState is the SolidWorks-style cutting-plane state (standard/custom
** plane + offset + flip + capping). It is a JSON-serializable POD so the GUI
** can bind it to a Graphic3d_ClipPlane and the CLI/MCP can drive sections
** headlessly. computeSection() actually slices a shape with the plane.
****************************************************************************/

#pragma once

#include "measure.h"   // argos::Vec3, TopoDS_Shape

#include <optional>
#include <string>

namespace argos {

// Standard datum planes (normal axis in parentheses) or an arbitrary normal.
enum class StandardPlane { XY, YZ, ZX, Custom };

const char* toString(StandardPlane p);
bool parseStandardPlane(const std::string& s, StandardPlane& out);

// State of a single cutting plane.
struct SectionState {
    StandardPlane plane = StandardPlane::XY;
    Vec3 customNormal{ 0.0, 0.0, 1.0 };  // used only when plane == Custom
    Vec3 origin{ 0.0, 0.0, 0.0 };        // a point the (un-offset) plane passes through
    double offset = 0.0;                 // signed distance along the BASE (un-flipped) normal
    bool flipped = false;                // reverse the kept side only; does NOT move the plane
    bool cappingOn = true;               // fill the cut section (SolidWorks look)
    Vec3 capColor{ 0.78, 0.78, 0.78 };   // RGB in [0,1]
};

// Resolved unit normal of the plane (flip applied). Returns (0,0,1) if the
// custom normal is degenerate.
Vec3 planeNormal(const SectionState& s);

// Implicit plane coefficients a*x + b*y + c*z + d = 0 for the offset plane.
struct PlaneCoeffs { double a = 0, b = 0, c = 0, d = 0; };
PlaneCoeffs planeCoefficients(const SectionState& s);

std::string to_json(const SectionState& s, int indent = -1);

// Result of slicing a shape with the section plane.
struct SectionResult {
    bool ok = false;
    std::string error;
    int edgeCount = 0;             // number of section curve segments
    double outlineLength = 0.0;    // total length of the section outline (mm)
    std::optional<Vec3> bboxMin;
    std::optional<Vec3> bboxMax;
    std::optional<Vec3> bboxSize;
};

// Slice 'shape' with the section plane; never throws (errors via ok/error).
SectionResult computeSection(const TopoDS_Shape& shape, const SectionState& s);

std::string to_json(const SectionResult& r, int indent = -1);

} // namespace argos
