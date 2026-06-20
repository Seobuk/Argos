/****************************************************************************
** Argos - measurement engine implementation (Qt-free, OpenCASCADE only)
** SPDX-License-Identifier: BSD-2-Clause
****************************************************************************/

#include "measure.h"

#include "../measure/measure_tool_brep.h"   // Mayo::MeasureToolBRep + result structs

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <GProp_PrincipalProps.hxx>
#include <Precision.hxx>
#include <gp_Mat.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Circ.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Dir.hxx>
#include <gp_Lin.hxx>
#include <gp_Pln.hxx>

#include "../3rdparty/nlohmann/json.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace argos {

namespace {

constexpr double kPi = 3.14159265358979323846;

Vec3 toVec3(const gp_Pnt& p) { return Vec3{ p.X(), p.Y(), p.Z() }; }

double radToDeg(double rad) { return rad * 180.0 / kPi; }

// Sub-shape category used by the dispatch matrix.
enum class Cat {
    Vertex,
    EdgeLine,
    EdgeCircle,   // circle / ellipse(==circle) edge
    EdgeOther,    // generic / polygon edge
    FacePlane,
    FaceCylinder,
    FaceOther,
    Solidish,     // solid / shell / compound / compsolid / wire
    Unknown
};

Cat classify(const TopoDS_Shape& s)
{
    if (s.IsNull())
        return Cat::Unknown;

    switch (s.ShapeType()) {
    case TopAbs_VERTEX:
        return Cat::Vertex;
    case TopAbs_EDGE:
        try {
            const BRepAdaptor_Curve curve(TopoDS::Edge(s));
            switch (curve.GetType()) {
            case GeomAbs_Line:    return Cat::EdgeLine;
            case GeomAbs_Circle:
            case GeomAbs_Ellipse: return Cat::EdgeCircle;
            default:              return Cat::EdgeOther;
            }
        }
        catch (const Standard_Failure&) {
            return Cat::EdgeOther; // non-geometric (mesh/polygon) edge
        }
    case TopAbs_FACE:
        try {
            const BRepAdaptor_Surface surf(TopoDS::Face(s));
            switch (surf.GetType()) {
            case GeomAbs_Plane:    return Cat::FacePlane;
            case GeomAbs_Cylinder: return Cat::FaceCylinder;
            default:               return Cat::FaceOther;
            }
        }
        catch (const Standard_Failure&) {
            return Cat::FaceOther;
        }
    default:
        return Cat::Solidish;
    }
}

bool isEdge(Cat c)  { return c == Cat::EdgeLine || c == Cat::EdgeCircle || c == Cat::EdgeOther; }
bool isFace(Cat c)  { return c == Cat::FacePlane || c == Cat::FaceCylinder || c == Cat::FaceOther; }

gp_Dir planeNormal(const TopoDS_Shape& face)
{
    return BRepAdaptor_Surface(TopoDS::Face(face)).Plane().Axis().Direction();
}

gp_Dir lineDir(const TopoDS_Shape& edge)
{
    return BRepAdaptor_Curve(TopoDS::Edge(edge)).Line().Direction();
}

// Runs a measurement lambda, translating any exception into result.error.
template<class Fn>
void guarded(MeasureResult& r, Fn&& fn)
{
    try {
        fn();
        r.ok = true;
    }
    catch (const Mayo::IMeasureError& e) {
        r.ok = false;
        r.error = std::string(e.message());
    }
    catch (const Standard_Failure& e) {
        r.ok = false;
        r.error = e.GetMessageString() ? e.GetMessageString() : "OpenCASCADE failure";
    }
    catch (const std::exception& e) {
        r.ok = false;
        r.error = e.what();
    }
    catch (...) {
        r.ok = false;
        r.error = "unknown measurement error";
    }
}

void fillDistance(MeasureResult& r, const Mayo::MeasureDistance& d, bool showXyz)
{
    r.value = d.value.value();   // mm
    r.unit = "mm";
    r.point = toVec3(d.pnt1);
    r.point2 = toVec3(d.pnt2);
    if (showXyz) {
        r.delta = Vec3{
            std::abs(d.pnt2.X() - d.pnt1.X()),
            std::abs(d.pnt2.Y() - d.pnt1.Y()),
            std::abs(d.pnt2.Z() - d.pnt1.Z())
        };
    }
}

// --- single sub-shape ------------------------------------------------------

MeasureResult dispatchOne(const TopoDS_Shape& s, Cat cat, const MeasureOptions&)
{
    MeasureResult r;
    r.inputCount = 1;

    if (cat == Cat::Vertex) {
        r.kind = MeasureKind::VertexPosition;
        guarded(r, [&]{ r.point = toVec3(Mayo::MeasureToolBRep::brepVertexPosition(s)); });
        return r;
    }

    if (cat == Cat::EdgeCircle) {
        r.kind = MeasureKind::Circle;
        guarded(r, [&]{
            const Mayo::MeasureCircle c = Mayo::MeasureToolBRep::brepCircle(s);
            const double radius = c.value.Radius();
            r.radius = radius;
            r.diameter = 2.0 * radius;
            r.value = 2.0 * radius;        // primary = diameter
            r.unit = "mm";
            r.point = toVec3(c.value.Location());  // center
            r.isArc = c.isArc;
        });
        return r;
    }

    if (isEdge(cat)) {
        r.kind = MeasureKind::Length;
        guarded(r, [&]{
            const Mayo::MeasureLength len = Mayo::MeasureToolBRep::brepLength(s);
            r.value = len.value.value();
            r.unit = "mm";
            r.length = r.value;
            r.point = toVec3(len.middlePnt);
        });
        return r;
    }

    if (cat == Cat::FaceCylinder) {
        r.kind = MeasureKind::Circle;   // cylinder -> diameter + area
        guarded(r, [&]{
            const double radius = BRepAdaptor_Surface(TopoDS::Face(s)).Cylinder().Radius();
            r.radius = radius;
            r.diameter = 2.0 * radius;
            const Mayo::MeasureArea a = Mayo::MeasureToolBRep::brepArea(s);
            r.area = a.value.value();
            r.value = 2.0 * radius;        // primary = diameter
            r.unit = "mm";
            r.point = toVec3(a.middlePnt);
        });
        return r;
    }

    if (isFace(cat)) {
        r.kind = MeasureKind::Area;
        guarded(r, [&]{
            const Mayo::MeasureArea a = Mayo::MeasureToolBRep::brepArea(s);
            r.value = a.value.value();
            r.unit = "mm^2";
            r.area = r.value;
            r.point = toVec3(a.middlePnt);
            // perimeter = sum of bounding edge lengths
            GProp_GProps lp;
            BRepGProp::LinearProperties(s, lp);
            r.length = lp.Mass();
        });
        return r;
    }

    // solid / shell / compound -> bounding box
    r.kind = MeasureKind::BoundingBox;
    guarded(r, [&]{
        const Mayo::MeasureBoundingBox bb = Mayo::MeasureToolBRep::brepBoundingBox(s);
        r.bboxMin = toVec3(bb.cornerMin);
        r.bboxMax = toVec3(bb.cornerMax);
        r.bboxSize = Vec3{ bb.xLength.value(), bb.yLength.value(), bb.zLength.value() };
        r.value = bb.volume.value();
        r.unit = "mm^3";
    });
    return r;
}

// --- pair of sub-shapes ----------------------------------------------------

MeasureResult dispatchPair(const TopoDS_Shape& a, Cat ca,
                           const TopoDS_Shape& b, Cat cb,
                           const MeasureOptions& opt)
{
    MeasureResult r;
    r.inputCount = 2;

    auto asMinDistance = [&]{
        r.kind = MeasureKind::MinDistance;
        guarded(r, [&]{
            fillDistance(r, Mayo::MeasureToolBRep::brepMinDistance(a, b), opt.showXyz);
        });
    };

    if (opt.pointToPoint) {
        asMinDistance();
        return r;
    }

    // two circular edges -> center-to-center distance
    if (ca == Cat::EdgeCircle && cb == Cat::EdgeCircle) {
        r.kind = MeasureKind::CenterDistance;
        guarded(r, [&]{
            fillDistance(r, Mayo::MeasureToolBRep::brepCenterDistance(a, b), opt.showXyz);
        });
        return r;
    }

    // two linear edges -> angle (unless parallel)
    if (ca == Cat::EdgeLine && cb == Cat::EdgeLine) {
        bool parallel = false;
        try { parallel = lineDir(a).IsParallel(lineDir(b), Precision::Angular()); }
        catch (const Standard_Failure&) { parallel = false; }
        if (!parallel) {
            r.kind = MeasureKind::Angle;
            guarded(r, [&]{
                const Mayo::MeasureAngle ang = Mayo::MeasureToolBRep::brepAngle(a, b);
                r.value = radToDeg(ang.value.value());
                r.unit = "deg";
                r.point = toVec3(ang.pnt1);
                r.point2 = toVec3(ang.pnt2);
            });
            return r;
        }
        asMinDistance();
        return r;
    }

    // two planar faces -> acute angle between planes (unless parallel -> distance)
    if (ca == Cat::FacePlane && cb == Cat::FacePlane) {
        bool computed = false;
        try {
            const gp_Dir n1 = planeNormal(a);
            const gp_Dir n2 = planeNormal(b);
            if (!n1.IsParallel(n2, Precision::Angular())) {
                double ang = n1.Angle(n2);     // angle between normals, in [0, pi]
                // The angle between two outward normals is the supplement of the
                // dihedral angle; report the acute angle between the planes, which
                // matches the SolidWorks / Mayo edge-angle convention.
                if (ang > kPi / 2.0)
                    ang = kPi - ang;
                r.kind = MeasureKind::Angle;
                r.value = radToDeg(ang);
                r.unit = "deg";
                r.ok = true;
                computed = true;
            }
        }
        catch (const Standard_Failure&) {
            // normal extraction failed -> fall back to a distance measure
        }
        if (!computed)
            asMinDistance();
        return r;
    }

    // any other pair (vertex/vertex, mixed, etc.) -> minimum distance
    asMinDistance();
    return r;
}

// --- many sub-shapes -------------------------------------------------------

MeasureResult dispatchMany(const std::vector<TopoDS_Shape>& shapes,
                           const std::vector<Cat>& cats,
                           const MeasureOptions&)
{
    MeasureResult r;
    r.inputCount = int(shapes.size());

    const bool allEdges = std::all_of(cats.begin(), cats.end(), [](Cat c){ return isEdge(c); });
    const bool allFaces = std::all_of(cats.begin(), cats.end(), [](Cat c){ return isFace(c); });

    if (allEdges) {
        r.kind = MeasureKind::SumLength;
        guarded(r, [&]{
            double total = 0.0;
            for (const TopoDS_Shape& s : shapes)
                total += Mayo::MeasureToolBRep::brepLength(s).value.value();
            r.value = total;
            r.unit = "mm";
            r.length = total;
        });
        return r;
    }

    if (allFaces) {
        r.kind = MeasureKind::SumArea;
        guarded(r, [&]{
            double total = 0.0;
            for (const TopoDS_Shape& s : shapes)
                total += Mayo::MeasureToolBRep::brepArea(s).value.value();
            r.value = total;
            r.unit = "mm^2";
            r.area = total;
        });
        return r;
    }

    r.ok = false;
    r.kind = MeasureKind::None;
    r.error = "unsupported selection set: select only edges (total length) or only faces (total area)";
    return r;
}

void addVec(nlohmann::ordered_json& j, const char* key, const std::optional<Vec3>& v)
{
    if (v)
        j[key] = { {"x", v->x}, {"y", v->y}, {"z", v->z} };
}

template<class T>
void addOpt(nlohmann::ordered_json& j, const char* key, const std::optional<T>& v)
{
    if (v)
        j[key] = *v;
}

} // namespace

const char* toString(MeasureKind kind)
{
    switch (kind) {
    case MeasureKind::None:           return "None";
    case MeasureKind::VertexPosition: return "VertexPosition";
    case MeasureKind::Length:         return "Length";
    case MeasureKind::Circle:         return "Circle";
    case MeasureKind::Area:           return "Area";
    case MeasureKind::MinDistance:    return "MinDistance";
    case MeasureKind::CenterDistance: return "CenterDistance";
    case MeasureKind::Angle:          return "Angle";
    case MeasureKind::BoundingBox:    return "BoundingBox";
    case MeasureKind::SumLength:      return "SumLength";
    case MeasureKind::SumArea:        return "SumArea";
    }
    return "None";
}

MeasureResult dispatch(const std::vector<TopoDS_Shape>& shapes, const MeasureOptions& opt)
{
    MeasureResult r;

    if (shapes.empty()) {
        r.ok = false;
        r.error = "empty selection";
        r.kindName = toString(r.kind);
        return r;
    }

    std::vector<Cat> cats;
    cats.reserve(shapes.size());
    for (const TopoDS_Shape& s : shapes)
        cats.push_back(classify(s));

    if (shapes.size() == 1)
        r = dispatchOne(shapes[0], cats[0], opt);
    else if (shapes.size() == 2)
        r = dispatchPair(shapes[0], cats[0], shapes[1], cats[1], opt);
    else
        r = dispatchMany(shapes, cats, opt);

    r.kindName = toString(r.kind);

    // Degenerate geometry can yield a non-finite scalar; keep the result (and its
    // JSON) self-consistent by reporting failure rather than emitting NaN/Inf.
    if (r.ok && r.value && !std::isfinite(*r.value)) {
        r.ok = false;
        r.error = "non-finite measurement result";
    }

    return r;
}

std::string to_json(const MeasureResult& res, int indent)
{
    nlohmann::ordered_json j;
    j["ok"] = res.ok;
    j["kind"] = res.kindName.empty() ? toString(res.kind) : res.kindName;
    j["inputCount"] = res.inputCount;

    // Use the "replace" error handler so non-UTF-8 bytes in OCCT/std error
    // messages (e.g. embedded file paths) become U+FFFD instead of throwing.
    const auto onBadUtf8 = nlohmann::ordered_json::error_handler_t::replace;

    if (!res.ok) {
        j["error"] = res.error;
        return j.dump(indent, ' ', false, onBadUtf8);
    }

    if (res.value) {
        j["value"] = *res.value;
        if (!res.unit.empty())
            j["unit"] = res.unit;
    }

    addOpt(j, "radius", res.radius);
    addOpt(j, "diameter", res.diameter);
    addOpt(j, "area", res.area);
    addOpt(j, "length", res.length);
    addOpt(j, "isArc", res.isArc);
    addVec(j, "point", res.point);
    addVec(j, "point2", res.point2);
    addVec(j, "delta", res.delta);
    addVec(j, "bboxMin", res.bboxMin);
    addVec(j, "bboxMax", res.bboxMax);
    addVec(j, "bboxSize", res.bboxSize);

    return j.dump(indent, ' ', false, onBadUtf8);
}

// --- Mass / inertia properties ----------------------------------------------

MassProperties massProperties(const TopoDS_Shape& shape, double densityKgPerM3)
{
    MassProperties r;
    r.density = densityKgPerM3;
    if (shape.IsNull()) {
        r.error = "null shape";
        return r;
    }

    try {
        // Volume properties (implicit density 1): values are in model units (mm).
        GProp_GProps vol;
        BRepGProp::VolumeProperties(shape, vol);
        const double V = vol.Mass(); // volume in mm^3 (density 1)
        if (!std::isfinite(V) || V <= 1.0e-9) {
            r.error = "shape has no volume (not a solid)";
            return r;
        }

        GProp_GProps surf;
        BRepGProp::SurfaceProperties(shape, surf);
        r.area_mm2 = surf.Mass();
        r.volume_mm3 = V;

        const gp_Pnt c = vol.CentreOfMass();
        r.com_mm = toVec3(c);

        // SI mass
        const double volume_m3 = V * 1.0e-9;
        r.mass_kg = volume_m3 * densityKgPerM3;

        // OCCT's MatrixOfInertia (from VolumeProperties) is expressed about the
        // centre of mass, so use it directly (density 1, units mm^5). No shift.
        const gp_Mat Ic = vol.MatrixOfInertia();
        const double Ic_xx = Ic(1, 1);
        const double Ic_yy = Ic(2, 2);
        const double Ic_zz = Ic(3, 3);
        const double Ic_xy = Ic(1, 2);
        const double Ic_xz = Ic(1, 3);
        const double Ic_yz = Ic(2, 3);

        // Convert (density-1, mm^5) -> SI kg*m^2: multiply by density * 1e-15.
        // [ rho(kg/mm^3)=rho_si*1e-9 ] * [ mm^5 ] = kg*mm^2; *1e-6 -> kg*m^2.
        const double k = densityKgPerM3 * 1.0e-15;
        r.ixx = Ic_xx * k;
        r.iyy = Ic_yy * k;
        r.izz = Ic_zz * k;
        r.ixy = Ic_xy * k;
        r.ixz = Ic_xz * k;
        r.iyz = Ic_yz * k;

        // Principal moments about the COM (density 1, mm^5) -> SI.
        GProp_PrincipalProps pp = vol.PrincipalProperties();
        double pm1 = 0, pm2 = 0, pm3 = 0;
        pp.Moments(pm1, pm2, pm3);
        r.i1 = pm1 * k;
        r.i2 = pm2 * k;
        r.i3 = pm3 * k;

        r.ok = true;
    }
    catch (const Standard_Failure& e) {
        r.error = e.GetMessageString() ? e.GetMessageString() : "OpenCASCADE failure";
    }
    catch (...) {
        r.error = "unknown error computing mass properties";
    }

    return r;
}

std::string to_json(const MassProperties& m, int indent)
{
    const auto onBadUtf8 = nlohmann::ordered_json::error_handler_t::replace;
    nlohmann::ordered_json j;
    j["ok"] = m.ok;
    if (!m.ok) {
        j["error"] = m.error;
        return j.dump(indent, ' ', false, onBadUtf8);
    }

    j["density_kg_m3"] = m.density;
    j["volume_mm3"] = m.volume_mm3;
    j["area_mm2"] = m.area_mm2;
    j["mass_kg"] = m.mass_kg;
    j["com_mm"] = { { "x", m.com_mm.x }, { "y", m.com_mm.y }, { "z", m.com_mm.z } };
    // Inertia tensor about COM (kg*m^2)
    j["inertia_com_kg_m2"] = {
        { "ixx", m.ixx }, { "iyy", m.iyy }, { "izz", m.izz },
        { "ixy", m.ixy }, { "ixz", m.ixz }, { "iyz", m.iyz }
    };
    j["principal_moments_kg_m2"] = { m.i1, m.i2, m.i3 };
    return j.dump(indent, ' ', false, onBadUtf8);
}

std::string toUrdfInertial(const MassProperties& m)
{
    if (!m.ok)
        return std::string("<!-- mass properties unavailable: ") + m.error + " -->";

    // COM in metres (model unit assumed mm).
    const double x = m.com_mm.x * 1.0e-3;
    const double y = m.com_mm.y * 1.0e-3;
    const double z = m.com_mm.z * 1.0e-3;

    char buf[1024];
    std::snprintf(
        buf, sizeof(buf),
        "<inertial>\n"
        "  <origin xyz=\"%.6g %.6g %.6g\" rpy=\"0 0 0\"/>\n"
        "  <mass value=\"%.6g\"/>\n"
        "  <inertia ixx=\"%.6g\" ixy=\"%.6g\" ixz=\"%.6g\""
        " iyy=\"%.6g\" iyz=\"%.6g\" izz=\"%.6g\"/>\n"
        "</inertial>",
        x, y, z, m.mass_kg,
        m.ixx, m.ixy, m.ixz, m.iyy, m.iyz, m.izz
    );
    return std::string(buf);
}

} // namespace argos
