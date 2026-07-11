/****************************************************************************
** Argos - section-plane state & headless cross-section implementation
** SPDX-License-Identifier: BSD-2-Clause
****************************************************************************/

#include "section.h"

#include <BRepAlgoAPI_Section.hxx>
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>

#include "../3rdparty/nlohmann/json.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace argos {

namespace {

const auto kReplace = nlohmann::ordered_json::error_handler_t::replace;

Vec3 baseNormal(const SectionState& s)
{
    switch (s.plane) {
    case StandardPlane::XY: return Vec3{ 0, 0, 1 };
    case StandardPlane::YZ: return Vec3{ 1, 0, 0 };
    case StandardPlane::ZX: return Vec3{ 0, 1, 0 };
    case StandardPlane::Custom: return s.customNormal;
    }
    return Vec3{ 0, 0, 1 };
}

// Normalized base normal WITHOUT the flip applied. The flip only reverses the
// clipping side, it does not move the plane, so positioning must use this.
Vec3 baseUnitNormal(const SectionState& s)
{
    const Vec3 n = baseNormal(s);
    const double len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
    // !isfinite catches NaN/Inf components (comparisons with NaN are always false,
    // so a bare `len < 1e-12` would let them through).
    if (!std::isfinite(len) || len < 1e-12)
        return Vec3{ 0, 0, 1 };
    return Vec3{ n.x / len, n.y / len, n.z / len };
}

// Point the plane passes through: origin moved by 'offset' along the base normal.
Vec3 planePoint(const SectionState& s)
{
    const Vec3 bn = baseUnitNormal(s);
    return Vec3{ s.origin.x + s.offset * bn.x,
                 s.origin.y + s.offset * bn.y,
                 s.origin.z + s.offset * bn.z };
}

void addVec(nlohmann::ordered_json& j, const char* key, const std::optional<Vec3>& v)
{
    if (v)
        j[key] = { { "x", v->x }, { "y", v->y }, { "z", v->z } };
}

} // namespace

const char* toString(StandardPlane p)
{
    switch (p) {
    case StandardPlane::XY: return "XY";
    case StandardPlane::YZ: return "YZ";
    case StandardPlane::ZX: return "ZX";
    case StandardPlane::Custom: return "Custom";
    }
    return "XY";
}

bool parseStandardPlane(const std::string& s, StandardPlane& out)
{
    std::string u;
    u.reserve(s.size());
    for (char c : s)
        u.push_back(char(std::toupper(static_cast<unsigned char>(c))));

    if (u == "XY") { out = StandardPlane::XY; return true; }
    if (u == "YZ") { out = StandardPlane::YZ; return true; }
    if (u == "ZX") { out = StandardPlane::ZX; return true; }
    if (u == "CUSTOM") { out = StandardPlane::Custom; return true; }
    return false;
}

Vec3 planeNormal(const SectionState& s)
{
    Vec3 n = baseUnitNormal(s);
    if (s.flipped)
        n = Vec3{ -n.x, -n.y, -n.z };

    return n;
}

PlaneCoeffs planeCoefficients(const SectionState& s)
{
    const Vec3 n = planeNormal(s);   // flip reverses the normal...
    const Vec3 p = planePoint(s);    // ...but the plane stays at the same location
    PlaneCoeffs c;
    c.a = n.x;
    c.b = n.y;
    c.c = n.z;
    c.d = -(n.x * p.x + n.y * p.y + n.z * p.z);
    return c;
}

std::string to_json(const SectionState& s, int indent)
{
    const Vec3 n = planeNormal(s);
    const PlaneCoeffs c = planeCoefficients(s);
    nlohmann::ordered_json j;
    j["plane"] = toString(s.plane);
    j["normal"] = { { "x", n.x }, { "y", n.y }, { "z", n.z } };
    j["origin"] = { { "x", s.origin.x }, { "y", s.origin.y }, { "z", s.origin.z } };
    j["offset"] = s.offset;
    j["flipped"] = s.flipped;
    j["cappingOn"] = s.cappingOn;
    j["capColor"] = { { "r", s.capColor.x }, { "g", s.capColor.y }, { "b", s.capColor.z } };
    j["coefficients"] = { { "a", c.a }, { "b", c.b }, { "c", c.c }, { "d", c.d } };
    return j.dump(indent, ' ', false, kReplace);
}

SectionResult computeSection(const TopoDS_Shape& shape, const gp_Pln& plane)
{
    SectionResult r;
    if (shape.IsNull()) {
        r.error = "null shape";
        return r;
    }

    try {
        BRepAlgoAPI_Section section(shape, plane, Standard_False);
        // Pcurves-on-faces are pure cost here (nothing downstream reads them;
        // measured >2x the whole slice time on assemblies). Approximation stays
        // ON: it compresses walking-line intersections into compact B-splines,
        // which is measurably FASTER end-to-end than the raw dense polylines
        // (both to build and to measure/display) and keeps the perimeter exact
        // on curved sections.
        section.ComputePCurveOn1(Standard_False);
        section.Approximation(Standard_True);
        // Intersect the faces on all cores.
        section.SetRunParallel(Standard_True);
        // Never touch the input shapes (boolean ops may otherwise bump
        // sub-shape tolerances): callers slice geometry that is concurrently
        // displayed, possibly from a worker thread.
        section.SetNonDestructive(Standard_True);
        section.Build();
        if (!section.IsDone()) {
            r.error = "section algorithm failed";
            return r;
        }

        const TopoDS_Shape secShape = section.Shape();

        TopTools_IndexedMapOfShape edges;
        TopExp::MapShapes(secShape, TopAbs_EDGE, edges);
        r.edgeCount = edges.Extent();

        if (r.edgeCount == 0) {
            // Plane does not intersect the shape: a valid, empty section.
            r.ok = true;
            r.outlineLength = 0.0;
            return r;
        }

        r.shape = secShape;

        GProp_GProps lprops;
        BRepGProp::LinearProperties(secShape, lprops);
        r.outlineLength = lprops.Mass();

        Bnd_Box box;
        BRepBndLib::Add(secShape, box);
        if (!box.IsVoid()) {
            const gp_Pnt lo = box.CornerMin();
            const gp_Pnt hi = box.CornerMax();
            r.bboxMin = Vec3{ lo.X(), lo.Y(), lo.Z() };
            r.bboxMax = Vec3{ hi.X(), hi.Y(), hi.Z() };
            r.bboxSize = Vec3{ std::abs(hi.X() - lo.X()),
                               std::abs(hi.Y() - lo.Y()),
                               std::abs(hi.Z() - lo.Z()) };
        }

        r.ok = true;
        return r;
    }
    catch (const Standard_Failure& e) {
        r.ok = false;
        r.shape.Nullify();   // keep the invariant: shape is non-null only when ok
        r.error = e.GetMessageString() ? e.GetMessageString() : "OpenCASCADE section failure";
        return r;
    }
    catch (const std::exception& e) {
        r.ok = false;
        r.shape.Nullify();
        r.error = e.what();
        return r;
    }
}

SectionResult computeSection(const TopoDS_Shape& shape, const SectionState& s)
{
    if (!std::isfinite(s.offset)
        || !std::isfinite(s.origin.x) || !std::isfinite(s.origin.y) || !std::isfinite(s.origin.z)
        || !std::isfinite(s.customNormal.x) || !std::isfinite(s.customNormal.y) || !std::isfinite(s.customNormal.z)) {
        SectionResult r;
        r.error = "non-finite section parameters";
        return r;
    }

    const Vec3 n = planeNormal(s);
    const Vec3 p = planePoint(s);
    return computeSection(shape, gp_Pln(gp_Pnt(p.x, p.y, p.z), gp_Dir(n.x, n.y, n.z)));
}

std::string to_json(const SectionResult& r, int indent)
{
    nlohmann::ordered_json j;
    j["ok"] = r.ok;
    if (!r.ok) {
        j["error"] = r.error;
        return j.dump(indent, ' ', false, kReplace);
    }
    j["edgeCount"] = r.edgeCount;
    j["outlineLength"] = r.outlineLength;
    addVec(j, "bboxMin", r.bboxMin);
    addVec(j, "bboxMax", r.bboxMax);
    addVec(j, "bboxSize", r.bboxSize);
    return j.dump(indent, ' ', false, kReplace);
}

} // namespace argos
