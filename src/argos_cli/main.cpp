/****************************************************************************
** Argos - argos-cli : headless measurement command-line tool (Qt-free)
** SPDX-License-Identifier: BSD-2-Clause
**
** Links only argos_core (-> OpenCASCADE). This is the AI/automation entry
** point: it loads a CAD file, selects sub-shapes by index and prints a JSON
** measurement to stdout.
**
**   argos-cli measure part.step --vertex 12 --vertex 47
**   argos-cli measure part.step --face 3 --face 8 --pretty
**   argos-cli info part.step
**
** On Windows the entry point is wmain() so non-ASCII paths (e.g. Korean file
** names) reach OpenCASCADE as UTF-8.
****************************************************************************/

#include "../argos_core/io.h"
#include "../argos_core/measure.h"
#include "../argos_core/section.h"
#include "../3rdparty/nlohmann/json.hpp"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

int emitError(const std::string& msg, int indent = -1)
{
    nlohmann::ordered_json j;
    j["ok"] = false;
    j["error"] = msg;
    std::cout << j.dump(indent, ' ', false, nlohmann::ordered_json::error_handler_t::replace)
              << std::endl;
    return 1;
}

void printUsage()
{
    std::cerr <<
        "argos-cli - headless CAD measurement (JSON on stdout)\n\n"
        "Usage:\n"
        "  argos-cli measure <file> [selection...] [options]\n"
        "  argos-cli section <file> [--plane xy|yz|zx] [--offset N] [--flip] [options]\n"
        "  argos-cli props   <file> [--density N] [--urdf] [--pretty]\n"
        "  argos-cli info    <file> [--pretty]\n\n"
        "measure selection (order matters; mix freely, SolidWorks-style):\n"
        "  --vertex N      select the N-th vertex (1-based)\n"
        "  --edge   N      select the N-th edge\n"
        "  --face   N      select the N-th face\n"
        "  --point-to-point   force minimum point-to-point distance for 2 entities\n"
        "  --no-xyz           omit the dX/dY/dZ decomposition\n\n"
        "section options:\n"
        "  --plane xy|yz|zx|custom   datum plane (default xy)\n"
        "  --normal X Y Z            custom plane normal (implies --plane custom)\n"
        "  --origin X Y Z            point the plane passes through (default 0,0,0)\n"
        "  --offset N                signed distance moved along the normal\n"
        "  --flip                    reverse the cut side\n\n"
        "props options (mass / inertia for rigid-body dynamics):\n"
        "  --density N               material density in kg/m^3 (default 7850, steel)\n"
        "  --urdf                    print a ROS/URDF <inertial> block instead of JSON\n\n"
        "common:\n"
        "  --pretty           pretty-print the JSON\n\n"
        "Supported formats: STEP (.step/.stp), IGES (.iges/.igs), BREP (.brep).\n";
}

bool parseInt(const std::string& s, int& out)
{
    try {
        size_t pos = 0;
        const int v = std::stoi(s, &pos);
        if (pos != s.size())
            return false;
        out = v;
        return true;
    }
    catch (...) {
        return false;
    }
}

bool parseDouble(const std::string& s, double& out)
{
    try {
        size_t pos = 0;
        const double v = std::stod(s, &pos);
        if (pos != s.size() || !std::isfinite(v))   // reject "nan"/"inf"
            return false;
        out = v;
        return true;
    }
    catch (...) {
        return false;
    }
}

int doMeasure(const std::vector<std::string>& args)
{
    std::string file;
    std::vector<std::pair<TopAbs_ShapeEnum, int>> selection;
    argos::MeasureOptions opt;
    int indent = -1;
    bool parseFailed = false;

    for (size_t i = 2; i < args.size(); ++i) {
        const std::string& a = args[i];
        auto wantIndex = [&](TopAbs_ShapeEnum type) -> bool {
            if (i + 1 >= args.size()) {
                emitError("missing index after " + a, indent);
                return false;
            }
            int idx = 0;
            if (!parseInt(args[++i], idx) || idx < 1) {
                emitError("invalid index after " + a + ": " + args[i], indent);
                return false;
            }
            selection.emplace_back(type, idx);
            return true;
        };

        if (a == "--vertex")              { if (!wantIndex(TopAbs_VERTEX)) { parseFailed = true; break; } }
        else if (a == "--edge")           { if (!wantIndex(TopAbs_EDGE))   { parseFailed = true; break; } }
        else if (a == "--face")           { if (!wantIndex(TopAbs_FACE))   { parseFailed = true; break; } }
        else if (a == "--point-to-point") opt.pointToPoint = true;
        else if (a == "--no-xyz")         opt.showXyz = false;
        else if (a == "--pretty")         indent = 2;
        else if (!a.empty() && a[0] == '-') return emitError("unknown option: " + a, indent);
        else if (file.empty())            file = a;
        else return emitError("unexpected argument: " + a, indent);
    }

    if (parseFailed)
        return 1; // wantIndex already emitted a JSON error

    if (file.empty())
        return emitError("no input file given", indent);

    std::string loadErr;
    const TopoDS_Shape shape = argos::loadShape(file, &loadErr);
    if (shape.IsNull())
        return emitError(loadErr.empty() ? "failed to load file" : loadErr, indent);

    if (selection.empty())
        return emitError("no sub-shape selected (use --vertex/--edge/--face)", indent);

    std::vector<TopoDS_Shape> shapes;
    shapes.reserve(selection.size());
    for (const auto& sel : selection) {
        const TopoDS_Shape sub = argos::subShape(shape, sel.first, sel.second);
        if (sub.IsNull()) {
            const char* kind = sel.first == TopAbs_VERTEX ? "vertex"
                             : sel.first == TopAbs_EDGE   ? "edge" : "face";
            const int total = argos::countSubShapes(shape, sel.first);
            return emitError(std::string(kind) + " index " + std::to_string(sel.second) +
                             " out of range (1.." + std::to_string(total) + ")", indent);
        }
        shapes.push_back(sub);
    }

    const argos::MeasureResult res = argos::dispatch(shapes, opt);
    std::cout << argos::to_json(res, indent) << std::endl;
    return res.ok ? 0 : 1;
}

int doInfo(const std::vector<std::string>& args)
{
    std::string file;
    int indent = -1;
    for (size_t i = 2; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "--pretty") indent = 2;
        else if (file.empty() && !(a.size() && a[0] == '-')) file = a;
        else return emitError("unexpected argument: " + a, indent);
    }
    if (file.empty())
        return emitError("no input file given", indent);

    std::string loadErr;
    const TopoDS_Shape shape = argos::loadShape(file, &loadErr);
    if (shape.IsNull())
        return emitError(loadErr.empty() ? "failed to load file" : loadErr, indent);

    nlohmann::ordered_json j;
    j["ok"] = true;
    j["file"] = file;
    j["vertices"] = argos::countSubShapes(shape, TopAbs_VERTEX);
    j["edges"] = argos::countSubShapes(shape, TopAbs_EDGE);
    j["faces"] = argos::countSubShapes(shape, TopAbs_FACE);
    j["solids"] = argos::countSubShapes(shape, TopAbs_SOLID);

    const argos::MeasureResult bb = argos::dispatch({ shape });
    if (bb.ok) {
        try { j["boundingBox"] = nlohmann::ordered_json::parse(argos::to_json(bb)); }
        catch (...) {}
    }

    std::cout << j.dump(indent, ' ', false, nlohmann::ordered_json::error_handler_t::replace)
              << std::endl;
    return 0;
}

int doSection(const std::vector<std::string>& args)
{
    std::string file;
    argos::SectionState st;
    int indent = -1;

    for (size_t i = 2; i < args.size(); ++i) {
        const std::string& a = args[i];
        auto nextNum = [&](double& d) -> bool {
            if (i + 1 >= args.size()) { emitError("missing value after " + a, indent); return false; }
            if (!parseDouble(args[++i], d)) { emitError("invalid number after " + a + ": " + args[i], indent); return false; }
            return true;
        };

        if (a == "--plane") {
            if (i + 1 >= args.size()) return emitError("missing value after --plane", indent);
            if (!argos::parseStandardPlane(args[++i], st.plane))
                return emitError("invalid plane (xy|yz|zx|custom): " + args[i], indent);
        }
        else if (a == "--offset") { double d; if (!nextNum(d)) return 1; st.offset = d; }
        else if (a == "--flip")   { st.flipped = true; }
        else if (a == "--normal") {
            double x, y, z;
            if (!nextNum(x) || !nextNum(y) || !nextNum(z)) return 1;
            st.plane = argos::StandardPlane::Custom;
            st.customNormal = argos::Vec3{ x, y, z };
        }
        else if (a == "--origin") {
            double x, y, z;
            if (!nextNum(x) || !nextNum(y) || !nextNum(z)) return 1;
            st.origin = argos::Vec3{ x, y, z };
        }
        else if (a == "--pretty")  indent = 2;
        else if (!a.empty() && a[0] == '-') return emitError("unknown option: " + a, indent);
        else if (file.empty())     file = a;
        else return emitError("unexpected argument: " + a, indent);
    }

    if (file.empty())
        return emitError("no input file given", indent);

    std::string loadErr;
    const TopoDS_Shape shape = argos::loadShape(file, &loadErr);
    if (shape.IsNull())
        return emitError(loadErr.empty() ? "failed to load file" : loadErr, indent);

    const argos::SectionResult res = argos::computeSection(shape, st);

    nlohmann::ordered_json j;
    j["ok"] = res.ok;
    try { j["state"] = nlohmann::ordered_json::parse(argos::to_json(st)); } catch (...) {}
    try { j["section"] = nlohmann::ordered_json::parse(argos::to_json(res)); } catch (...) {}
    if (!res.ok)
        j["error"] = res.error;

    std::cout << j.dump(indent, ' ', false, nlohmann::ordered_json::error_handler_t::replace)
              << std::endl;
    return res.ok ? 0 : 1;
}

int doProps(const std::vector<std::string>& args)
{
    std::string file;
    double density = 7850.0; // mild steel
    bool urdf = false;
    int indent = -1;

    for (size_t i = 2; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "--density") {
            double d = 0;
            if (i + 1 >= args.size() || !parseDouble(args[++i], d) || d <= 0.0)
                return emitError("invalid value after --density", indent);
            density = d;
        }
        else if (a == "--urdf")        urdf = true;
        else if (a == "--pretty")      indent = 2;
        else if (!a.empty() && a[0] == '-') return emitError("unknown option: " + a, indent);
        else if (file.empty())         file = a;
        else return emitError("unexpected argument: " + a, indent);
    }

    if (file.empty())
        return emitError("no input file given", indent);

    std::string loadErr;
    const TopoDS_Shape shape = argos::loadShape(file, &loadErr);
    if (shape.IsNull())
        return emitError(loadErr.empty() ? "failed to load file" : loadErr, indent);

    const argos::MassProperties mp = argos::massProperties(shape, density);
    if (urdf) {
        std::cout << argos::toUrdfInertial(mp) << std::endl;
        return mp.ok ? 0 : 1;
    }

    std::cout << argos::to_json(mp, indent) << std::endl;
    return mp.ok ? 0 : 1;
}

int runMain(const std::vector<std::string>& args)
{
    if (args.size() < 2) {
        printUsage();
        return 2;
    }

    const std::string& cmd = args[1];
    if (cmd == "measure")
        return doMeasure(args);
    if (cmd == "info")
        return doInfo(args);
    if (cmd == "section")
        return doSection(args);
    if (cmd == "props")
        return doProps(args);
    if (cmd == "-h" || cmd == "--help" || cmd == "help") {
        printUsage();
        return 0;
    }

    std::cerr << "unknown command: " << cmd << "\n\n";
    printUsage();
    return 2;
}

} // namespace

#ifdef _WIN32

// Convert a wide string to UTF-8 via std::filesystem (no <windows.h> needed,
// avoids clashes with OCCT's partial Windows includes). Works whether
// path::u8string() returns std::string (C++17) or std::u8string (C++20).
static std::string wideToUtf8(const wchar_t* w)
{
    if (!w || !*w)
        return {};

    const auto u8 = std::filesystem::path(w).u8string();
    return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
}

int wmain(int argc, wchar_t** argv)
{
    std::vector<std::string> args;
    args.reserve(static_cast<size_t>(argc));
    for (int i = 0; i < argc; ++i)
        args.push_back(wideToUtf8(argv[i]));

    return runMain(args);
}

#else

int main(int argc, char** argv)
{
    return runMain(std::vector<std::string>(argv, argv + argc));
}

#endif
