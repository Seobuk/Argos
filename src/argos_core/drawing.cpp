/****************************************************************************
** Argos - 2D orthographic drawing generator implementation (Qt-free, OCCT)
** SPDX-License-Identifier: BSD-2-Clause
****************************************************************************/

#include "drawing.h"

#include "../3rdparty/nlohmann/json.hpp"

#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <GCPnts_QuasiUniformDeflection.hxx>
#include <GeomAbs_CurveType.hxx>
#include <HLRAlgo_Projector.hxx>
#include <HLRBRep_Algo.hxx>
#include <HLRBRep_HLRToShape.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>
#include <gp.hxx>
#include <gp_Ax2.hxx>
#include <gp_Ax3.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace argos {

namespace {

// --- drawing constants (mm) -----------------------------------------------
constexpr double kDimOffset   = 10.0;  // view edge -> dimension line
constexpr double kExtGap      = 1.0;   // object -> start of extension line
constexpr double kExtBeyond   = 1.5;   // extension line past the dimension line
constexpr double kTextHeight  = 3.5;   // dimension text height
constexpr double kArrow       = 2.5;   // arrowhead length
constexpr double kDimReserve  = kDimOffset + kTextHeight + 4.0; // margin for dims

// Right-handed view frame: 'normal' points toward the viewer (out of the page),
// 'xdir' is the drawing's rightward axis. up = normal ^ xdir (gp_Ax2 convention).
struct ViewFrame { gp_Dir normal; gp_Dir xdir; };

ViewFrame frameFor(ViewKind v)
{
    switch (v) {
    case ViewKind::Front: return { gp_Dir(0, -1, 0), gp_Dir(1, 0, 0) }; // right +X, up +Z
    case ViewKind::Top:   return { gp_Dir(0, 0, 1),  gp_Dir(1, 0, 0) }; // right +X, up +Y
    case ViewKind::Right: return { gp_Dir(1, 0, 0),  gp_Dir(0, 1, 0) }; // right +Y, up +Z
    case ViewKind::Iso:   return { gp_Dir(1, 1, 1),  gp_Dir(-1, 1, 0) }; // pictorial
    }
    return { gp_Dir(0, -1, 0), gp_Dir(1, 0, 0) };
}

const char* koLabel(ViewKind v)
{
    switch (v) {
    case ViewKind::Front: return "정면도";
    case ViewKind::Top:   return "평면도";
    case ViewKind::Right: return "우측면도";
    case ViewKind::Iso:   return "등각도";
    }
    return "";
}

struct Bbox {
    double minx = 1e300, miny = 1e300, maxx = -1e300, maxy = -1e300;
    void add(double x, double y) {
        minx = std::min(minx, x); miny = std::min(miny, y);
        maxx = std::max(maxx, x); maxy = std::max(maxy, y);
    }
    bool valid() const { return maxx >= minx && maxy >= miny; }
    double w() const { return valid() ? maxx - minx : 0.0; }
    double h() const { return valid() ? maxy - miny : 0.0; }
};

// Discretise every edge of 'comp' into 2D polylines (dropping the depth Z), and
// accumulate their extent into 'bb'. Never throws (bad edges are skipped).
void collectEdges(const TopoDS_Shape& comp, double deflection,
                  std::vector<Polyline2D>& out, Bbox& bb)
{
    if (comp.IsNull())
        return;

    for (TopExp_Explorer ex(comp, TopAbs_EDGE); ex.More(); ex.Next()) {
        try {
            BRepAdaptor_Curve curve(TopoDS::Edge(ex.Current()));
            Polyline2D pl;

            if (curve.GetType() == GeomAbs_Line) {
                const gp_Pnt a = curve.Value(curve.FirstParameter());
                const gp_Pnt b = curve.Value(curve.LastParameter());
                pl.pts.push_back({ a.X(), a.Y() });
                pl.pts.push_back({ b.X(), b.Y() });
            }
            else {
                GCPnts_QuasiUniformDeflection sampler(curve, deflection);
                if (sampler.IsDone() && sampler.NbPoints() >= 2) {
                    for (int i = 1; i <= sampler.NbPoints(); ++i) {
                        const gp_Pnt p = sampler.Value(i);
                        pl.pts.push_back({ p.X(), p.Y() });
                    }
                }
                else {
                    const gp_Pnt a = curve.Value(curve.FirstParameter());
                    const gp_Pnt b = curve.Value(curve.LastParameter());
                    pl.pts.push_back({ a.X(), a.Y() });
                    pl.pts.push_back({ b.X(), b.Y() });
                }
            }

            if (pl.pts.size() >= 2) {
                for (const Pt2& p : pl.pts)
                    bb.add(p.x, p.y);
                out.push_back(std::move(pl));
            }
        }
        catch (const Standard_Failure&) { /* skip degenerate edge */ }
    }
}

// Project 'shape' into one view: returns a DrawingView with geometry normalised
// so its bottom-left corner sits at (0,0). 'ok' is false on HLR failure.
bool projectView(const TopoDS_Shape& shape, ViewKind kind, const DrawingOptions& opt,
                 DrawingView& view)
{
    view.name = toString(kind);
    view.label = koLabel(kind);

    const ViewFrame vf = frameFor(kind);
    const bool wantHidden = opt.hiddenLines && kind != ViewKind::Iso;

    try {
        // World -> view-local frame, so the projection is a plain look-down-(+Z).
        const gp_Ax3 ax3(gp_Pnt(0, 0, 0), vf.normal, vf.xdir);
        gp_Trsf trsf;
        trsf.SetTransformation(ax3);
        const TopoDS_Shape inView =
            BRepBuilderAPI_Transform(shape, trsf, /*copy*/ true).Shape();

        Handle(HLRBRep_Algo) hlr = new HLRBRep_Algo();
        hlr->Add(inView);
        HLRAlgo_Projector projector(gp_Ax2(gp::Origin(), gp::DZ(), gp::DX()));
        hlr->Projector(projector);
        hlr->Update();
        hlr->Hide();

        HLRBRep_HLRToShape toShape(hlr);
        Bbox bb;
        collectEdges(toShape.VCompound(),        opt.deflection, view.visible, bb);
        collectEdges(toShape.OutLineVCompound(), opt.deflection, view.visible, bb);
        collectEdges(toShape.Rg1LineVCompound(), opt.deflection, view.visible, bb);
        if (wantHidden) {
            collectEdges(toShape.HCompound(),        opt.deflection, view.hidden, bb);
            collectEdges(toShape.OutLineHCompound(), opt.deflection, view.hidden, bb);
        }

        if (!bb.valid() || (view.visible.empty() && view.hidden.empty()))
            return false;

        // Normalise so the view's lower-left corner is the origin.
        for (std::vector<Polyline2D>* set : { &view.visible, &view.hidden }) {
            for (Polyline2D& pl : *set) {
                for (Pt2& p : pl.pts) { p.x -= bb.minx; p.y -= bb.miny; }
            }
        }
        view.width = bb.w();
        view.height = bb.h();
        return true;
    }
    catch (const Standard_Failure&) {
        return false;
    }
}

std::string fmtNum(double v)
{
    // Up to 2 decimals, trailing zeros trimmed. Snap near-integers.
    if (std::abs(v - std::round(v)) < 5e-3)
        v = std::round(v);
    std::ostringstream os;
    os << std::fixed << std::setprecision(2) << v;
    std::string s = os.str();
    if (s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    return s;
}

// Offset every point of a view by (dx,dy) — moving normalised geometry into
// sheet coordinates.
void offsetView(DrawingView& v, double dx, double dy)
{
    for (std::vector<Polyline2D>* set : { &v.visible, &v.hidden }) {
        for (Polyline2D& pl : *set)
            for (Pt2& p : pl.pts) { p.x += dx; p.y += dy; }
    }
}

// Build overall width (below) + height (left) dimensions for an ortho view whose
// lower-left corner sits at (ox,oy) in sheet coordinates.
void buildDims(DrawingView& v, double ox, double oy)
{
    const double w = v.width, h = v.height;

    // Width dimension, below the view.
    {
        DimAnnotation d;
        d.value = w;
        d.text = fmtNum(w);
        const double y = oy - kDimOffset;
        d.lineStart = { ox, y };
        d.lineEnd   = { ox + w, y };
        d.extA0 = { ox, oy - kExtGap };
        d.extA1 = { ox, y - kExtBeyond };
        d.extB0 = { ox + w, oy - kExtGap };
        d.extB1 = { ox + w, y - kExtBeyond };
        d.textPos = { ox + w / 2.0, y + 1.2 };
        d.vertical = false;
        v.dims.push_back(d);
    }
    // Height dimension, to the left of the view.
    {
        DimAnnotation d;
        d.value = h;
        d.text = fmtNum(h);
        const double x = ox - kDimOffset;
        d.lineStart = { x, oy };
        d.lineEnd   = { x, oy + h };
        d.extA0 = { ox - kExtGap, oy };
        d.extA1 = { x - kExtBeyond, oy };
        d.extB0 = { ox - kExtGap, oy + h };
        d.extB1 = { x - kExtBeyond, oy + h };
        d.textPos = { x - 1.2, oy + h / 2.0 };
        d.vertical = true;
        v.dims.push_back(d);
    }
}

// XML-escape a UTF-8 string for SVG text.
std::string xmlEscape(const std::string& s)
{
    std::string o;
    o.reserve(s.size());
    for (char c : s) {
        switch (c) {
        case '&': o += "&amp;"; break;
        case '<': o += "&lt;"; break;
        case '>': o += "&gt;"; break;
        case '"': o += "&quot;"; break;
        default: o += c;
        }
    }
    return o;
}

} // namespace

const char* toString(ProjectionAngle a)
{
    return a == ProjectionAngle::First ? "first" : "third";
}

bool parseProjectionAngle(const std::string& s, ProjectionAngle& out)
{
    std::string k;
    for (char c : s) k.push_back(char(std::tolower((unsigned char)c)));
    if (k == "first" || k == "1" || k == "iso" || k == "1st") { out = ProjectionAngle::First; return true; }
    if (k == "third" || k == "3" || k == "asme" || k == "3rd") { out = ProjectionAngle::Third; return true; }
    return false;
}

const char* toString(ViewKind v)
{
    switch (v) {
    case ViewKind::Front: return "front";
    case ViewKind::Top:   return "top";
    case ViewKind::Right: return "right";
    case ViewKind::Iso:   return "iso";
    }
    return "front";
}

bool parseViewList(const std::string& s, std::vector<ViewKind>& out)
{
    out.clear();
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        std::string k;
        for (char c : tok) if (!std::isspace((unsigned char)c)) k.push_back(char(std::tolower((unsigned char)c)));
        if (k.empty()) continue;
        if (k == "front")      out.push_back(ViewKind::Front);
        else if (k == "top")   out.push_back(ViewKind::Top);
        else if (k == "right") out.push_back(ViewKind::Right);
        else if (k == "iso")   out.push_back(ViewKind::Iso);
        else return false;
    }
    return !out.empty();
}

Drawing computeDrawing(const TopoDS_Shape& shape, const DrawingOptions& optIn,
                       const std::string& title)
{
    Drawing d;
    d.projection = toString(optIn.projection);
    d.title = title;

    if (shape.IsNull()) {
        d.error = "null shape";
        return d;
    }

    DrawingOptions opt = optIn;
    if (opt.views.empty())
        opt.views = { ViewKind::Front, ViewKind::Top, ViewKind::Right, ViewKind::Iso };

    // 1) Project each requested view.
    std::vector<DrawingView> views;
    std::vector<ViewKind> kinds;
    for (ViewKind k : opt.views) {
        DrawingView v;
        if (projectView(shape, k, opt, v)) {
            views.push_back(std::move(v));
            kinds.push_back(k);
        }
    }
    if (views.empty()) {
        d.error = "hidden-line projection produced no geometry";
        return d;
    }

    // 2) Lay out the views (math coords, Y-up). Front is the anchor; top/right
    //    are placed per the projection convention. Iso is a free pictorial.
    auto indexOf = [&](ViewKind k) -> int {
        for (size_t i = 0; i < kinds.size(); ++i) if (kinds[i] == k) return int(i);
        return -1;
    };
    const int iF = indexOf(ViewKind::Front);
    const int iT = indexOf(ViewKind::Top);
    const int iR = indexOf(ViewKind::Right);
    const int iI = indexOf(ViewKind::Iso);
    const double g = opt.gap;

    std::vector<Pt2> pos(views.size(), { 0, 0 });
    std::vector<bool> placed(views.size(), false);

    if (iF >= 0) {
        const double wF = views[iF].width, hF = views[iF].height;
        pos[iF] = { 0, 0 }; placed[iF] = true;
        const bool third = opt.projection == ProjectionAngle::Third;
        if (iT >= 0) {
            pos[iT] = third ? Pt2{ 0, hF + g }
                            : Pt2{ 0, -(views[iT].height + g) };
            placed[iT] = true;
        }
        if (iR >= 0) {
            pos[iR] = third ? Pt2{ wF + g, 0 }
                            : Pt2{ -(views[iR].width + g), 0 };
            placed[iR] = true;
        }
        if (iI >= 0) { pos[iI] = { wF + g, hF + g }; placed[iI] = true; }
    }
    // Fallback: tile any not-yet-placed views left to right along the baseline.
    double tileX = 0;
    for (size_t i = 0; i < views.size(); ++i) {
        if (placed[i]) continue;
        pos[i] = { tileX, 0 };
        tileX += views[i].width + g;
        placed[i] = true;
    }

    // 3) Global bounds of the placed view rectangles.
    Bbox content;
    for (size_t i = 0; i < views.size(); ++i) {
        content.add(pos[i].x, pos[i].y);
        content.add(pos[i].x + views[i].width, pos[i].y + views[i].height);
    }

    // Reserve space for dimensions (below + left) and a uniform margin.
    const double reserve = opt.dimensions ? kDimReserve : 0.0;
    const double padL = opt.margin + reserve;
    const double padB = opt.margin + reserve;
    const double padR = opt.margin;
    const double padT = opt.margin;
    const double shiftX = -content.minx + padL;
    const double shiftY = -content.miny + padB;

    // 4) Move geometry into sheet coordinates and build dimensions.
    for (size_t i = 0; i < views.size(); ++i) {
        const double ox = pos[i].x + shiftX;
        const double oy = pos[i].y + shiftY;
        offsetView(views[i], ox, oy);
        if (opt.dimensions && kinds[i] != ViewKind::Iso)
            buildDims(views[i], ox, oy);
    }

    d.sheetWidth  = content.w() + padL + padR;
    d.sheetHeight = content.h() + padB + padT;
    d.views = std::move(views);
    d.ok = true;
    return d;
}

// ---------------------------------------------------------------------------
// SVG writer
// ---------------------------------------------------------------------------

std::string to_svg(const Drawing& d)
{
    std::ostringstream o;
    o << std::fixed << std::setprecision(3);
    const double W = d.sheetWidth, H = d.sheetHeight;

    // Paper is Y-up; SVG is Y-down. Flip every Y as sy = H - y so text stays upright.
    auto Y = [H](double y) { return H - y; };

    o << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    o << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << W << "mm\" height=\""
      << H << "mm\" viewBox=\"0 0 " << W << " " << H << "\">\n";
    o << "<rect x=\"0\" y=\"0\" width=\"" << W << "\" height=\"" << H
      << "\" fill=\"white\"/>\n";

    // Border, inset 5 mm.
    const double b = 5.0;
    o << "<rect x=\"" << b << "\" y=\"" << b << "\" width=\"" << (W - 2 * b)
      << "\" height=\"" << (H - 2 * b)
      << "\" fill=\"none\" stroke=\"black\" stroke-width=\"0.5\"/>\n";

    auto polyline = [&](const Polyline2D& pl, const char* extra) {
        o << "<polyline fill=\"none\" " << extra << " points=\"";
        for (size_t i = 0; i < pl.pts.size(); ++i) {
            if (i) o << " ";
            o << pl.pts[i].x << "," << Y(pl.pts[i].y);
        }
        o << "\"/>\n";
    };

    for (const DrawingView& v : d.views) {
        o << "<g>\n";
        for (const Polyline2D& pl : v.hidden)
            polyline(pl, "stroke=\"#444\" stroke-width=\"0.25\" stroke-dasharray=\"2,1.2\"");
        for (const Polyline2D& pl : v.visible)
            polyline(pl, "stroke=\"black\" stroke-width=\"0.35\"");

        // Dimensions.
        for (const DimAnnotation& dm : v.dims) {
            auto line = [&](Pt2 a, Pt2 c) {
                o << "<line x1=\"" << a.x << "\" y1=\"" << Y(a.y) << "\" x2=\"" << c.x
                  << "\" y2=\"" << Y(c.y) << "\" stroke=\"#1030c0\" stroke-width=\"0.18\"/>\n";
            };
            line(dm.extA0, dm.extA1);
            line(dm.extB0, dm.extB1);
            line(dm.lineStart, dm.lineEnd);
            // Arrowheads (small filled triangles) at both ends of the dim line.
            auto arrow = [&](Pt2 tip, Pt2 toward) {
                const double dx = toward.x - tip.x, dy = toward.y - tip.y;
                const double len = std::sqrt(dx * dx + dy * dy);
                if (len < 1e-9) return;
                const double ux = dx / len, uy = dy / len;
                const double px = -uy, py = ux;   // perpendicular
                const Pt2 p1{ tip.x + ux * kArrow + px * kArrow * 0.28,
                              tip.y + uy * kArrow + py * kArrow * 0.28 };
                const Pt2 p2{ tip.x + ux * kArrow - px * kArrow * 0.28,
                              tip.y + uy * kArrow - py * kArrow * 0.28 };
                o << "<polygon fill=\"#1030c0\" points=\"" << tip.x << "," << Y(tip.y) << " "
                  << p1.x << "," << Y(p1.y) << " " << p2.x << "," << Y(p2.y) << "\"/>\n";
            };
            arrow(dm.lineStart, dm.lineEnd);
            arrow(dm.lineEnd, dm.lineStart);
            // Text.
            const double tx = dm.textPos.x, ty = Y(dm.textPos.y);
            o << "<text x=\"" << tx << "\" y=\"" << ty << "\" font-size=\"" << kTextHeight
              << "\" font-family=\"sans-serif\" fill=\"#1030c0\" text-anchor=\"middle\"";
            if (dm.vertical)
                o << " transform=\"rotate(-90 " << tx << " " << ty << ")\"";
            o << ">" << xmlEscape(dm.text) << "</text>\n";
        }

        // View caption, just below the view's lower-left area.
        o << "</g>\n";
    }

    // Title block: bottom-right corner inside the border.
    {
        const double tbw = 80.0, tbh = 24.0;
        const double x0 = W - b - tbw, y0 = b;   // paper coords (y-up)
        auto tline = [&](double y) {
            o << "<line x1=\"" << x0 << "\" y1=\"" << Y(y) << "\" x2=\"" << (x0 + tbw)
              << "\" y2=\"" << Y(y) << "\" stroke=\"black\" stroke-width=\"0.3\"/>\n";
        };
        o << "<rect x=\"" << x0 << "\" y=\"" << Y(y0 + tbh) << "\" width=\"" << tbw
          << "\" height=\"" << tbh << "\" fill=\"none\" stroke=\"black\" stroke-width=\"0.5\"/>\n";
        tline(y0 + tbh * 2.0 / 3.0);
        tline(y0 + tbh / 3.0);
        auto txt = [&](double x, double y, double size, const std::string& s) {
            o << "<text x=\"" << x << "\" y=\"" << Y(y) << "\" font-size=\"" << size
              << "\" font-family=\"sans-serif\" fill=\"black\">" << xmlEscape(s) << "</text>\n";
        };
        txt(x0 + 2, y0 + tbh - 6, 4.5, d.title.empty() ? std::string("Argos Drawing") : d.title);
        const std::string proj = (d.projection == "third") ? "3rd angle" : "1st angle";
        txt(x0 + 2, y0 + tbh / 3.0 + 2, 3.0, "Projection: " + proj);
        txt(x0 + 2, y0 + 2, 3.0, "Unit: " + d.unit + "   Scale: 1:1");
    }

    o << "</svg>\n";
    return o.str();
}

// ---------------------------------------------------------------------------
// DXF R12 (AC1009) writer
// ---------------------------------------------------------------------------

namespace {

void dxfPair(std::ostringstream& o, int code, const std::string& val) { o << code << "\n" << val << "\n"; }
void dxfPair(std::ostringstream& o, int code, double val)
{
    std::ostringstream v; v << std::fixed << std::setprecision(4) << val;
    o << code << "\n" << v.str() << "\n";
}
void dxfPair(std::ostringstream& o, int code, int val) { o << code << "\n" << val << "\n"; }

void dxfLine(std::ostringstream& o, const std::string& layer,
             double x1, double y1, double x2, double y2)
{
    dxfPair(o, 0, std::string("LINE"));
    dxfPair(o, 8, layer);
    dxfPair(o, 10, x1); dxfPair(o, 20, y1); dxfPair(o, 30, 0.0);
    dxfPair(o, 11, x2); dxfPair(o, 21, y2); dxfPair(o, 31, 0.0);
}

void dxfPolyAsLines(std::ostringstream& o, const std::string& layer, const Polyline2D& pl)
{
    for (size_t i = 1; i < pl.pts.size(); ++i)
        dxfLine(o, layer, pl.pts[i - 1].x, pl.pts[i - 1].y, pl.pts[i].x, pl.pts[i].y);
}

void dxfText(std::ostringstream& o, const std::string& layer, double x, double y,
             double height, double rotationDeg, const std::string& s)
{
    dxfPair(o, 0, std::string("TEXT"));
    dxfPair(o, 8, layer);
    dxfPair(o, 10, x); dxfPair(o, 20, y); dxfPair(o, 30, 0.0);
    dxfPair(o, 40, height);
    dxfPair(o, 1, s);
    if (std::abs(rotationDeg) > 1e-9)
        dxfPair(o, 50, rotationDeg);
    dxfPair(o, 72, 1);                 // horizontal centre
    dxfPair(o, 11, x); dxfPair(o, 21, y); dxfPair(o, 31, 0.0);
}

} // namespace

std::string to_dxf(const Drawing& d)
{
    std::ostringstream o;

    // HEADER
    dxfPair(o, 0, std::string("SECTION"));
    dxfPair(o, 2, std::string("HEADER"));
    dxfPair(o, 9, std::string("$ACADVER")); dxfPair(o, 1, std::string("AC1009"));
    dxfPair(o, 9, std::string("$INSUNITS")); dxfPair(o, 70, 4);   // millimetres
    dxfPair(o, 9, std::string("$EXTMIN"));
    dxfPair(o, 10, 0.0); dxfPair(o, 20, 0.0); dxfPair(o, 30, 0.0);
    dxfPair(o, 9, std::string("$EXTMAX"));
    dxfPair(o, 10, d.sheetWidth); dxfPair(o, 20, d.sheetHeight); dxfPair(o, 30, 0.0);
    dxfPair(o, 0, std::string("ENDSEC"));

    // TABLES: linetypes + layers.
    dxfPair(o, 0, std::string("SECTION"));
    dxfPair(o, 2, std::string("TABLES"));

    dxfPair(o, 0, std::string("TABLE")); dxfPair(o, 2, std::string("LTYPE")); dxfPair(o, 70, 2);
    dxfPair(o, 0, std::string("LTYPE")); dxfPair(o, 2, std::string("CONTINUOUS"));
    dxfPair(o, 70, 0); dxfPair(o, 3, std::string("Solid line"));
    dxfPair(o, 72, 65); dxfPair(o, 73, 0); dxfPair(o, 40, 0.0);
    dxfPair(o, 0, std::string("LTYPE")); dxfPair(o, 2, std::string("DASHED"));
    dxfPair(o, 70, 0); dxfPair(o, 3, std::string("Dashed __ __ __"));
    dxfPair(o, 72, 65); dxfPair(o, 73, 2); dxfPair(o, 40, 0.6);
    dxfPair(o, 49, 0.4); dxfPair(o, 49, -0.2);
    dxfPair(o, 0, std::string("ENDTAB"));

    dxfPair(o, 0, std::string("TABLE")); dxfPair(o, 2, std::string("LAYER")); dxfPair(o, 70, 4);
    auto layer = [&](const std::string& name, int color, const std::string& ltype) {
        dxfPair(o, 0, std::string("LAYER")); dxfPair(o, 2, name); dxfPair(o, 70, 0);
        dxfPair(o, 62, color); dxfPair(o, 6, ltype);
    };
    layer("VISIBLE", 7, "CONTINUOUS");
    layer("HIDDEN", 8, "DASHED");
    layer("DIM", 5, "CONTINUOUS");
    layer("TEXT", 3, "CONTINUOUS");
    dxfPair(o, 0, std::string("ENDTAB"));
    dxfPair(o, 0, std::string("ENDSEC"));

    // ENTITIES
    dxfPair(o, 0, std::string("SECTION"));
    dxfPair(o, 2, std::string("ENTITIES"));
    for (const DrawingView& v : d.views) {
        for (const Polyline2D& pl : v.hidden)  dxfPolyAsLines(o, "HIDDEN", pl);
        for (const Polyline2D& pl : v.visible) dxfPolyAsLines(o, "VISIBLE", pl);
        for (const DimAnnotation& dm : v.dims) {
            dxfLine(o, "DIM", dm.extA0.x, dm.extA0.y, dm.extA1.x, dm.extA1.y);
            dxfLine(o, "DIM", dm.extB0.x, dm.extB0.y, dm.extB1.x, dm.extB1.y);
            dxfLine(o, "DIM", dm.lineStart.x, dm.lineStart.y, dm.lineEnd.x, dm.lineEnd.y);
            dxfText(o, "TEXT", dm.textPos.x, dm.textPos.y, kTextHeight,
                    dm.vertical ? 90.0 : 0.0, dm.text);
        }
    }
    dxfPair(o, 0, std::string("ENDSEC"));

    dxfPair(o, 0, std::string("EOF"));
    return o.str();
}

// ---------------------------------------------------------------------------
// JSON summary
// ---------------------------------------------------------------------------

std::string to_json(const Drawing& d, int indent)
{
    const auto onBadUtf8 = nlohmann::ordered_json::error_handler_t::replace;
    nlohmann::ordered_json j;
    j["ok"] = d.ok;
    if (!d.ok) {
        j["error"] = d.error;
        return j.dump(indent, ' ', false, onBadUtf8);
    }
    j["projection"] = d.projection;
    j["unit"] = d.unit;
    j["sheet"] = { { "width", d.sheetWidth }, { "height", d.sheetHeight } };
    j["views"] = nlohmann::ordered_json::array();
    for (const DrawingView& v : d.views) {
        nlohmann::ordered_json jv;
        jv["name"] = v.name;
        jv["width"] = v.width;
        jv["height"] = v.height;
        jv["visibleCurves"] = int(v.visible.size());
        jv["hiddenCurves"] = int(v.hidden.size());
        if (!v.dims.empty()) {
            jv["dimensions"] = nlohmann::ordered_json::array();
            for (const DimAnnotation& dm : v.dims)
                jv["dimensions"].push_back(dm.value);
        }
        j["views"].push_back(jv);
    }
    return j.dump(indent, ' ', false, onBadUtf8);
}

} // namespace argos
