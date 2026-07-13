/****************************************************************************
** Argos - argos_core unit tests (Qt-free)
** SPDX-License-Identifier: BSD-2-Clause
**
** Links ONLY argos_core (-> MayoCore -> OpenCASCADE). No Qt. This both tests
** the dispatch() matrix and proves the core is usable headlessly (the AI path).
****************************************************************************/

#include "argos_core/measure.h"
#include "argos_core/section.h"
#include "3rdparty/nlohmann/json.hpp"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRep_Tool.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Pln.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

int g_checks = 0;
int g_failures = 0;

void check(bool cond, const std::string& msg, const std::string& detail = {})
{
    ++g_checks;
    if (cond) {
        std::cout << "  ok   : " << msg << "\n";
    }
    else {
        ++g_failures;
        std::cout << "  FAIL : " << msg;
        if (!detail.empty())
            std::cout << "  (" << detail << ")";
        std::cout << "\n";
    }
}

void checkNear(double a, double b, double tol, const std::string& msg)
{
    check(std::abs(a - b) <= tol, msg,
          "got=" + std::to_string(a) + " expected~" + std::to_string(b));
}

bool oneOf(double v, std::initializer_list<double> opts, double tol = 1e-6)
{
    for (double o : opts) {
        if (std::abs(v - o) <= tol)
            return true;
    }
    return false;
}

std::vector<TopoDS_Shape> mapShapes(const TopoDS_Shape& s, TopAbs_ShapeEnum type)
{
    TopTools_IndexedMapOfShape map;
    TopExp::MapShapes(s, type, map);
    std::vector<TopoDS_Shape> out;
    for (int i = 1; i <= map.Extent(); ++i)
        out.push_back(map(i));
    return out;
}

gp_Dir faceNormal(const TopoDS_Shape& f)
{
    return BRepAdaptor_Surface(TopoDS::Face(f)).Plane().Axis().Direction();
}

} // namespace

int main()
{
    using namespace argos;

    std::cout << "== argos_core measurement tests ==\n";

    // Box 10 x 20 x 30 -> 8 vertices, 12 edges (len 10/20/30), 6 faces (200/300/600)
    const TopoDS_Shape box = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();
    const auto vertices = mapShapes(box, TopAbs_VERTEX);
    const auto edges    = mapShapes(box, TopAbs_EDGE);
    const auto faces    = mapShapes(box, TopAbs_FACE);

    check(vertices.size() == 8, "box has 8 vertices", "got=" + std::to_string(vertices.size()));
    check(edges.size() == 12, "box has 12 edges", "got=" + std::to_string(edges.size()));
    check(faces.size() == 6, "box has 6 faces", "got=" + std::to_string(faces.size()));

    // --- 1 vertex -> VertexPosition ---------------------------------------
    {
        const MeasureResult r = dispatch({ vertices[0] });
        check(r.ok && r.kind == MeasureKind::VertexPosition, "1 vertex -> VertexPosition", r.error);
        check(r.point.has_value(), "vertex result has a point");
        if (r.point) {
            const gp_Pnt p = BRep_Tool::Pnt(TopoDS::Vertex(vertices[0]));
            checkNear(r.point->x, p.X(), 1e-9, "vertex X matches");
            checkNear(r.point->y, p.Y(), 1e-9, "vertex Y matches");
            checkNear(r.point->z, p.Z(), 1e-9, "vertex Z matches");
        }
    }

    // --- 2 vertices -> MinDistance (+ delta) ------------------------------
    {
        const MeasureResult r = dispatch({ vertices[0], vertices[1] });
        check(r.ok && r.kind == MeasureKind::MinDistance, "2 vertices -> MinDistance", r.error);
        const gp_Pnt p1 = BRep_Tool::Pnt(TopoDS::Vertex(vertices[0]));
        const gp_Pnt p2 = BRep_Tool::Pnt(TopoDS::Vertex(vertices[1]));
        if (r.value)
            checkNear(*r.value, p1.Distance(p2), 1e-6, "2-vertex distance matches gp_Pnt distance");
        check(r.delta.has_value(), "distance has dX/dY/dZ (Show XYZ)");
    }

    // --- 1 edge -> Length --------------------------------------------------
    {
        const MeasureResult r = dispatch({ edges[0] });
        check(r.ok && r.kind == MeasureKind::Length, "1 edge -> Length", r.error);
        check(r.value && oneOf(*r.value, {10.0, 20.0, 30.0}, 1e-6),
              "edge length is one of 10/20/30",
              r.value ? std::to_string(*r.value) : "none");
    }

    // --- 1 face -> Area ----------------------------------------------------
    {
        const MeasureResult r = dispatch({ faces[0] });
        check(r.ok && r.kind == MeasureKind::Area, "1 face -> Area", r.error);
        check(r.value && oneOf(*r.value, {200.0, 300.0, 600.0}, 1e-6),
              "face area is one of 200/300/600",
              r.value ? std::to_string(*r.value) : "none");
    }

    // --- 2 parallel faces -> MinDistance ; 2 perpendicular faces -> Angle --
    {
        int pi = -1, pj = -1, ki = -1, kj = -1;
        for (size_t i = 0; i < faces.size() && (pi < 0 || ki < 0); ++i) {
            for (size_t j = i + 1; j < faces.size(); ++j) {
                const gp_Dir ni = faceNormal(faces[i]);
                const gp_Dir nj = faceNormal(faces[j]);
                if (pi < 0 && ni.IsParallel(nj, 1e-7)) { pi = int(i); pj = int(j); }
                if (ki < 0 && ni.IsNormal(nj, 1e-7))   { ki = int(i); kj = int(j); }
            }
        }
        check(pi >= 0, "found a parallel face pair");
        if (pi >= 0) {
            const MeasureResult r = dispatch({ faces[pi], faces[pj] });
            check(r.ok && r.kind == MeasureKind::MinDistance, "2 parallel faces -> MinDistance", r.error);
            check(r.value && oneOf(*r.value, {10.0, 20.0, 30.0}, 1e-6),
                  "parallel-face distance is one of 10/20/30",
                  r.value ? std::to_string(*r.value) : "none");
        }
        check(ki >= 0, "found a perpendicular face pair");
        if (ki >= 0) {
            const MeasureResult r = dispatch({ faces[ki], faces[kj] });
            check(r.ok && r.kind == MeasureKind::Angle, "2 perpendicular faces -> Angle", r.error);
            if (r.value)
                checkNear(*r.value, 90.0, 1e-6, "perpendicular-face angle ~ 90 deg");
        }
    }

    // --- point-to-point override: 2 parallel faces -> MinDistance ---------
    {
        // any two faces, forced to min point-to-point distance
        MeasureOptions opt;
        opt.pointToPoint = true;
        const MeasureResult r = dispatch({ faces[0], faces[1] }, opt);
        check(r.ok && r.kind == MeasureKind::MinDistance, "pointToPoint forces MinDistance", r.error);
    }

    // --- cylinder: face diameter + circular edge --------------------------
    {
        const double R = 5.0;
        const TopoDS_Shape cyl = BRepPrimAPI_MakeCylinder(R, 20.0).Shape();
        // cylindrical face
        TopoDS_Shape cylFace;
        for (const auto& f : mapShapes(cyl, TopAbs_FACE)) {
            if (BRepAdaptor_Surface(TopoDS::Face(f)).GetType() == GeomAbs_Cylinder) { cylFace = f; break; }
        }
        check(!cylFace.IsNull(), "found cylindrical face");
        if (!cylFace.IsNull()) {
            const MeasureResult r = dispatch({ cylFace });
            check(r.ok && r.kind == MeasureKind::Circle, "cylinder face -> Circle/diameter", r.error);
            if (r.diameter)
                checkNear(*r.diameter, 2 * R, 1e-6, "cylinder face diameter ~ 10");
        }
        // circular edge
        TopoDS_Shape circEdge;
        for (const auto& e : mapShapes(cyl, TopAbs_EDGE)) {
            if (BRepAdaptor_Curve(TopoDS::Edge(e)).GetType() == GeomAbs_Circle) { circEdge = e; break; }
        }
        check(!circEdge.IsNull(), "found circular edge");
        if (!circEdge.IsNull()) {
            const MeasureResult r = dispatch({ circEdge });
            check(r.ok && r.kind == MeasureKind::Circle, "circular edge -> Circle", r.error);
            if (r.radius)
                checkNear(*r.radius, R, 1e-6, "circular edge radius ~ 5");
        }
    }

    // --- two circles -> center / min / max distance (SolidWorks dropdown) --
    {
        // Coplanar circles (z=0): A r=5 @ origin, B r=3 @ (20,0,0). Centres 20
        // apart on X, so: center-to-center=20, min rim=20-5-3=12, max rim=20+5+3=28.
        const gp_Circ cA(gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), 5.0);
        const gp_Circ cB(gp_Ax2(gp_Pnt(20, 0, 0), gp_Dir(0, 0, 1)), 3.0);
        const TopoDS_Shape eA = BRepBuilderAPI_MakeEdge(cA).Edge();
        const TopoDS_Shape eB = BRepBuilderAPI_MakeEdge(cB).Edge();

        // default -> center-to-center
        {
            const MeasureResult r = dispatch({ eA, eB });
            check(r.ok && r.kind == MeasureKind::CenterDistance,
                  "2 circles (default) -> CenterDistance", r.error);
            if (r.value)
                checkNear(*r.value, 20.0, 1e-6, "circle center-to-center == 20");
        }
        // minimum rim-to-rim
        {
            MeasureOptions o;
            o.circleMode = CircleDistanceMode::Minimum;
            const MeasureResult r = dispatch({ eA, eB }, o);
            check(r.ok && r.kind == MeasureKind::MinDistance,
                  "2 circles (min) -> MinDistance", r.error);
            if (r.value)
                checkNear(*r.value, 12.0, 1e-3, "circle min rim distance == 12");
        }
        // maximum rim-to-rim (sampled; allow a small discretisation tolerance)
        {
            MeasureOptions o;
            o.circleMode = CircleDistanceMode::Maximum;
            const MeasureResult r = dispatch({ eA, eB }, o);
            check(r.ok && r.kind == MeasureKind::MaxDistance,
                  "2 circles (max) -> MaxDistance", r.error);
            if (r.value)
                checkNear(*r.value, 28.0, 0.05, "circle max rim distance == 28");
        }
        // parse helper round-trips
        {
            CircleDistanceMode m;
            check(parseCircleDistanceMode("max", m) && m == CircleDistanceMode::Maximum,
                  "parseCircleDistanceMode(\"max\")");
            check(parseCircleDistanceMode("CENTER", m) && m == CircleDistanceMode::CenterToCenter,
                  "parseCircleDistanceMode(\"CENTER\")");
            check(!parseCircleDistanceMode("bogus", m),
                  "parseCircleDistanceMode rejects unknown");
        }
    }

    // --- two planar faces at a non-90 angle -> acute dihedral -------------
    {
        // Plane normals 120 deg apart -> acute angle between planes is 60 deg.
        const double pi = 3.14159265358979323846;
        const gp_Pln pln1(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1));
        const gp_Pln pln2(gp_Pnt(0, 0, 0), gp_Dir(std::sin(120.0 * pi / 180.0), 0,
                                                   std::cos(120.0 * pi / 180.0)));
        const TopoDS_Shape f1 = BRepBuilderAPI_MakeFace(pln1, -5, 5, -5, 5).Shape();
        const TopoDS_Shape f2 = BRepBuilderAPI_MakeFace(pln2, -5, 5, -5, 5).Shape();
        const MeasureResult r = dispatch({ f1, f2 });
        check(r.ok && r.kind == MeasureKind::Angle, "two non-90 planar faces -> Angle", r.error);
        if (r.value)
            checkNear(*r.value, 60.0, 1e-6, "planar-face angle reported as acute 60 deg (not 120)");
    }

    // --- N edges -> SumLength ; N faces -> SumArea ------------------------
    {
        const MeasureResult r = dispatch({ edges[0], edges[1], edges[2] });
        check(r.ok && r.kind == MeasureKind::SumLength, "3 edges -> SumLength", r.error);

        const MeasureResult r2 = dispatch({ faces[0], faces[1], faces[2] });
        check(r2.ok && r2.kind == MeasureKind::SumArea, "3 faces -> SumArea", r2.error);
    }

    // --- SectionState: plane coefficients + headless cut of the box -------
    {
        // XY plane through z = 5 cuts the 10x20x30 box -> rectangular outline.
        SectionState st;
        st.plane = StandardPlane::XY;
        st.offset = 5.0;   // origin (0,0,0) + 5 along +Z
        const PlaneCoeffs c = planeCoefficients(st);
        checkNear(c.a, 0.0, 1e-9, "XY plane coeff a==0");
        checkNear(c.b, 0.0, 1e-9, "XY plane coeff b==0");
        checkNear(c.c, 1.0, 1e-9, "XY plane coeff c==1");
        checkNear(c.d, -5.0, 1e-9, "XY plane coeff d==-5 (z=5)");

        const SectionResult sr = computeSection(box, st);
        check(sr.ok, "section of box by z=5 plane succeeds", sr.error);
        check(sr.edgeCount >= 4, "section outline has >=4 edges",
              "got=" + std::to_string(sr.edgeCount));
        // rectangle 10 (x) by 20 (y) -> perimeter 60
        checkNear(sr.outlineLength, 60.0, 1e-6, "section outline length == 60 (10x20 rect)");

        // flip negates the normal -> coefficients invert sign
        SectionState stf = st;
        stf.flipped = true;
        const PlaneCoeffs cf = planeCoefficients(stf);
        checkNear(cf.c, -1.0, 1e-9, "flipped XY plane coeff c==-1");
        checkNear(cf.d, 5.0, 1e-9, "flipped XY plane coeff d==5");

        // plane outside the shape -> valid empty section
        SectionState stOut;
        stOut.plane = StandardPlane::XY;
        stOut.offset = 999.0;
        const SectionResult srOut = computeSection(box, stOut);
        check(srOut.ok && srOut.edgeCount == 0, "non-intersecting plane -> empty section");

        // non-finite inputs must not produce garbage geometry
        SectionState stNaN;
        stNaN.plane = StandardPlane::Custom;
        stNaN.customNormal = Vec3{ std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0 };
        const Vec3 nn = planeNormal(stNaN);
        check(std::isfinite(nn.x) && std::isfinite(nn.y) && std::isfinite(nn.z),
              "NaN custom normal -> finite fallback normal");

        SectionState stInf;
        stInf.offset = std::numeric_limits<double>::infinity();
        const SectionResult srInf = computeSection(box, stInf);
        check(!srInf.ok, "non-finite offset -> section reports error (not garbage)");

        // to_json round-trips
        bool secJsonOk = false;
        try {
            auto j = nlohmann::json::parse(to_json(st));
            secJsonOk = j.contains("plane") && j.contains("coefficients") && j.contains("normal");
        } catch (...) { secJsonOk = false; }
        check(secJsonOk, "SectionState to_json valid with plane/normal/coefficients");
    }

    // --- buildSectionFaces: fill the cut outline into a measurable face ----
    {
        // Solid box cut at z=5 -> one 10x20 rectangular cross-section face.
        const gp_Pln plnZ5(gp_Pnt(0, 0, 5), gp_Dir(0, 0, 1));
        const SectionResult srf = computeSection(box, plnZ5);
        const TopoDS_Shape faces = buildSectionFaces(srf.shape, plnZ5);
        check(!faces.IsNull(), "buildSectionFaces returns a face for a solid cut");
        TopTools_IndexedMapOfShape fmap;
        TopExp::MapShapes(faces, TopAbs_FACE, fmap);
        check(fmap.Extent() == 1, "box cut -> exactly one cross-section face",
              "got=" + std::to_string(fmap.Extent()));
        if (fmap.Extent() >= 1) {
            const MeasureResult ar = dispatch({ fmap(1) });
            check(ar.ok && ar.kind == MeasureKind::Area, "cut face -> Area", ar.error);
            checkNear(ar.value.value_or(0), 200.0, 1e-6, "box cut face area == 200 (10x20)");
        }

        // Tube (outer R=10 minus inner R=5, h=20) cut at z=10 -> annulus; the
        // hole must be subtracted so the face area is pi*(100-25) = 75*pi.
        const TopoDS_Shape outer = BRepPrimAPI_MakeCylinder(10.0, 20.0).Shape();
        const TopoDS_Shape inner = BRepPrimAPI_MakeCylinder(5.0, 20.0).Shape();
        const TopoDS_Shape tube = BRepAlgoAPI_Cut(outer, inner).Shape();
        const gp_Pln plnZ10(gp_Pnt(0, 0, 10), gp_Dir(0, 0, 1));
        const SectionResult srt = computeSection(tube, plnZ10);
        const TopoDS_Shape tfaces = buildSectionFaces(srt.shape, plnZ10);
        TopTools_IndexedMapOfShape tfmap;
        TopExp::MapShapes(tfaces, TopAbs_FACE, tfmap);
        check(tfmap.Extent() == 1, "tube cut -> single annulus face (hole resolved)",
              "got=" + std::to_string(tfmap.Extent()));
        if (tfmap.Extent() >= 1) {
            const MeasureResult ar = dispatch({ tfmap(1) });
            checkNear(ar.value.value_or(0), 75.0 * std::acos(-1.0), 1e-2,
                      "annulus cut area == 75*pi (hole subtracted)");
        }
    }

    // --- empty selection -> error ----------------------------------------
    {
        const MeasureResult r = dispatch({});
        check(!r.ok && !r.error.empty(), "empty selection reported as error");
    }

    // --- to_json is valid JSON and round-trips ----------------------------
    {
        const MeasureResult r = dispatch({ vertices[0], vertices[1] });
        const std::string js = to_json(r);
        std::cout << "  json : " << js << "\n";
        bool parsed = false;
        try {
            auto j = nlohmann::json::parse(js);
            parsed = j.contains("ok") && j.contains("kind") && j.contains("value");
        }
        catch (...) { parsed = false; }
        check(parsed, "to_json() produces valid JSON with ok/kind/value");
    }

    // --- Mass / inertia properties (box 10x20x30, steel 7850 kg/m^3) ---------
    {
        const MassProperties mp = massProperties(box, 7850.0);
        check(mp.ok, "massProperties(box).ok");
        checkNear(mp.volume_mm3, 6000.0, 1e-2, "box volume = 6000 mm^3");
        checkNear(mp.area_mm2, 2200.0, 1e-2, "box area = 2200 mm^2");
        checkNear(mp.mass_kg, 0.04710, 1e-5, "box mass = 0.0471 kg (steel)");
        checkNear(mp.com_mm.x, 5.0, 1e-6, "box COM x = 5 mm");
        checkNear(mp.com_mm.y, 10.0, 1e-6, "box COM y = 10 mm");
        checkNear(mp.com_mm.z, 15.0, 1e-6, "box COM z = 15 mm");
        // analytic box inertia about COM: m/12 * (sum of the two other dims^2), in m
        checkNear(mp.ixx, 5.1025e-6, 1e-8, "box Ixx about COM");
        checkNear(mp.iyy, 3.9250e-6, 1e-8, "box Iyy about COM");
        checkNear(mp.izz, 1.9625e-6, 1e-8, "box Izz about COM");
        check(std::abs(mp.ixy) < 1e-9 && std::abs(mp.ixz) < 1e-9 && std::abs(mp.iyz) < 1e-9,
              "box products of inertia ~ 0");
        check(oneOf(mp.i1, { 5.1025e-6, 3.9250e-6, 1.9625e-6 }, 1e-8)
              && oneOf(mp.i2, { 5.1025e-6, 3.9250e-6, 1.9625e-6 }, 1e-8)
              && oneOf(mp.i3, { 5.1025e-6, 3.9250e-6, 1.9625e-6 }, 1e-8),
              "box principal moments match the diagonal");

        // density scaling: doubling density doubles mass and inertia
        const MassProperties mp2 = massProperties(box, 15700.0);
        checkNear(mp2.mass_kg, 2.0 * mp.mass_kg, 1e-9, "mass scales linearly with density");
        checkNear(mp2.ixx, 2.0 * mp.ixx, 1e-10, "inertia scales linearly with density");

        const std::string urdf = toUrdfInertial(mp);
        check(urdf.find("<inertial>") != std::string::npos
              && urdf.find("mass value") != std::string::npos
              && urdf.find("ixx") != std::string::npos,
              "URDF <inertial> snippet generated");

        // a face (no volume) must fail cleanly
        const MassProperties mpFace = massProperties(faces.front(), 7850.0);
        check(!mpFace.ok, "massProperties(face) fails cleanly (no volume)");
    }

    std::cout << "\n== " << (g_checks - g_failures) << "/" << g_checks
              << " checks passed, " << g_failures << " failed ==\n";
    return g_failures == 0 ? 0 : 1;
}
