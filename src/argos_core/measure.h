/****************************************************************************
** Argos - measurement engine (Qt-free, OpenCASCADE only)
** SPDX-License-Identifier: BSD-2-Clause
**
** This is the heart of argos_core: a headless, JSON-serializable measurement
** dispatcher that auto-infers the measurement type from a *set* of selected
** sub-shapes (SolidWorks-style), then computes it by reusing Mayo's Qt-free
** BRep measurement primitives (Mayo::MeasureToolBRep).
**
** No Qt types are allowed in this header or its implementation.
****************************************************************************/

#pragma once

#include <TopoDS_Shape.hxx>

#include <optional>
#include <string>
#include <vector>

namespace argos {

// Kind of measurement that dispatch() resolved the selection set into.
enum class MeasureKind {
    None,
    VertexPosition,   // 1 vertex -> X,Y,Z
    Length,           // 1 linear/poly edge -> length
    Circle,           // 1 circular edge or cylindrical face -> radius/diameter/center
    Area,             // 1 face -> area
    MinDistance,      // 2 entities -> shortest distance (+ dX/dY/dZ)
    CenterDistance,   // 2 circles -> center-to-center distance
    Angle,            // 2 non-parallel linear edges / planar faces -> angle
    BoundingBox,      // 1 solid/compound -> AABB
    SumLength,        // N edges -> total length
    SumArea           // N faces -> total area
};

const char* toString(MeasureKind kind);

// Plain 3D point/vector, JSON-friendly.
struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

// Result of a measurement. Pure POD (no Qt, no OCCT handles) so it can be
// serialized to JSON and shared by GUI / CLI / MCP. Canonical units are:
//   length -> millimeter, area -> square millimeter, angle -> degree.
struct MeasureResult {
    bool ok = false;
    MeasureKind kind = MeasureKind::None;
    std::string kindName;          // toString(kind), convenience for consumers
    std::string error;             // human-readable reason when ok == false
    int inputCount = 0;            // number of input sub-shapes

    // Primary scalar value + its unit ("mm", "mm^2", "deg"). Empty when n/a.
    std::optional<double> value;
    std::string unit;

    // Structured extras; only the fields relevant to 'kind' are populated.
    std::optional<Vec3> point;     // vertex coord / circle center / distance pnt1
    std::optional<Vec3> point2;    // distance pnt2 / angle 2nd point
    std::optional<Vec3> delta;     // |dX|,|dY|,|dZ| decomposition of a distance
    std::optional<double> radius;
    std::optional<double> diameter;
    std::optional<double> area;
    std::optional<double> length;
    std::optional<bool>   isArc;
    std::optional<Vec3>   bboxMin;
    std::optional<Vec3>   bboxMax;
    std::optional<Vec3>   bboxSize;
};

// Options that influence dispatch behaviour.
struct MeasureOptions {
    // For a 2-entity selection: force a point-to-point minimum distance even
    // when the pair would otherwise resolve to angle/center-distance.
    bool pointToPoint = false;
    // Compute the |dX|,|dY|,|dZ| decomposition for distance results.
    bool showXyz = true;
    // Hint for human-facing display (GUI panel / CLI): number of decimals to
    // show. NOTE: to_json() intentionally emits full-precision raw values so AI/
    // tooling consumers can round as needed; this field does not affect to_json().
    int precision = 4;
};

// Auto-infer the measurement type from the selected sub-shapes and compute it.
// Never throws: failures are reported via MeasureResult::ok / ::error.
MeasureResult dispatch(const std::vector<TopoDS_Shape>& shapes, const MeasureOptions& opt = {});

// Serialize a result to a JSON string (single line by default; pass indent >= 0
// for pretty-printing).
std::string to_json(const MeasureResult& res, int indent = -1);

} // namespace argos
