/****************************************************************************
** Argos - 2D orthographic drawing generator (Qt-free, OpenCASCADE only)
** SPDX-License-Identifier: BSD-2-Clause
**
** Turns a 3D shape into a 2D engineering drawing: standard orthographic views
** (front / top / right + an isometric pictorial) produced by OpenCASCADE's
** hidden-line-removal (HLRBRep), laid out on a sheet per the chosen projection
** convention (first- or third-angle), with overall bounding-box dimensions.
**
** The result is a JSON-friendly, unit-free geometry model (millimetres, Y-up)
** that can be serialised to SVG (viewable) or DXF R12 (CAD-editable). No Qt
** types appear here, so the GUI, CLI and (future) MCP share the same engine.
****************************************************************************/

#pragma once

#include <TopoDS_Shape.hxx>

#include <string>
#include <vector>

namespace argos {

// Orthographic projection convention: where the projected views land.
//   ThirdAngle (ASME / US, SolidWorks default): top ABOVE front, right to the RIGHT.
//   FirstAngle (ISO / Korea / EU):              top BELOW front, right to the LEFT.
enum class ProjectionAngle { First, Third };

const char* toString(ProjectionAngle a);
bool parseProjectionAngle(const std::string& s, ProjectionAngle& out);

// Which standard views to generate. Names map to the projection directions in a
// Z-up model: front looks along +Y, top looks down +Z, right looks along +X.
enum class ViewKind { Front, Top, Right, Iso };

const char* toString(ViewKind v);
// Parse a comma-separated list ("front,top,right,iso"); false on any bad token.
bool parseViewList(const std::string& s, std::vector<ViewKind>& out);

struct DrawingOptions {
    ProjectionAngle projection = ProjectionAngle::First;
    std::vector<ViewKind> views;          // empty -> {Front, Top, Right, Iso}
    bool dimensions = true;               // overall width/height per ortho view
    bool hiddenLines = true;              // include hidden (dashed) edges
    double deflection = 0.05;             // curve sampling deflection (mm)
    double margin = 20.0;                 // sheet margin (mm)
    double gap = 32.0;                    // spacing between views (mm)
};

// A 2D point in sheet coordinates (mm, Y-up, origin at sheet bottom-left).
struct Pt2 { double x = 0.0; double y = 0.0; };

// A connected 2D polyline (>= 2 points).
struct Polyline2D { std::vector<Pt2> pts; };

// A single dimension annotation: a value with the paper geometry needed to draw
// it (dimension line, two extension lines, and the text anchor).
struct DimAnnotation {
    double value = 0.0;                   // measured length (mm)
    std::string text;                     // formatted label, e.g. "30"
    Pt2 lineStart, lineEnd;               // dimension line endpoints
    Pt2 extA0, extA1;                     // 1st extension line
    Pt2 extB0, extB1;                     // 2nd extension line
    Pt2 textPos;                          // text anchor (centre)
    bool vertical = false;                // text rotated 90 deg when true
};

// One placed view: its geometry already offset into sheet coordinates.
struct DrawingView {
    std::string name;                     // "front" / "top" / "right" / "iso"
    std::string label;                    // Korean caption ("정면도"...)
    std::vector<Polyline2D> visible;      // solid outlines
    std::vector<Polyline2D> hidden;       // dashed (hidden) outlines
    std::vector<DimAnnotation> dims;       // overall dimensions (empty for iso)
    double width = 0.0;                   // model extent shown horizontally (mm)
    double height = 0.0;                  // model extent shown vertically (mm)
};

// The complete laid-out drawing.
struct Drawing {
    bool ok = false;
    std::string error;
    std::vector<DrawingView> views;
    double sheetWidth = 0.0;              // mm (incl. margins)
    double sheetHeight = 0.0;             // mm
    std::string projection;               // "first" / "third"
    std::string title;                    // title-block title (e.g. file stem)
    std::string unit = "mm";
};

// Build the drawing from a shape. Never throws: failures are reported via
// Drawing::ok / ::error. 'title' fills the title block (pass the file stem).
Drawing computeDrawing(const TopoDS_Shape& shape,
                       const DrawingOptions& opt = {},
                       const std::string& title = {});

// Serialise the laid-out drawing to a self-contained SVG document.
std::string to_svg(const Drawing& d);

// Serialise to an AutoCAD DXF R12 (AC1009) document: LINE + TEXT entities on
// VISIBLE / HIDDEN / DIM / TEXT layers (hidden edges use a DASHED linetype).
std::string to_dxf(const Drawing& d);

// Compact JSON summary (views, sizes, dimension values) for tooling/AI.
std::string to_json(const Drawing& d, int indent = -1);

} // namespace argos
