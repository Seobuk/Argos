/****************************************************************************
** Argos - headless CAD loading implementation (Qt-free, OpenCASCADE only)
** SPDX-License-Identifier: BSD-2-Clause
****************************************************************************/

#include "io.h"

#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <IGESControl_Reader.hxx>
#include <Interface_Static.hxx>
#include <STEPControl_Reader.hxx>
#include <STEPControl_Writer.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>

#include <algorithm>
#include <cctype>

namespace argos {

namespace {

std::string toLowerExt(const std::string& path)
{
    // Only consider a dot that belongs to the file name, not to a directory
    // component (e.g. "C:/my.models/part" has no extension).
    const auto slash = path.find_last_of("/\\");
    const std::string::size_type base = (slash == std::string::npos) ? 0 : slash + 1;
    const auto dot = path.find_last_of('.');
    if (dot == std::string::npos || dot < base || dot + 1 >= path.size())
        return {};

    std::string ext = path.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    return ext;
}

void setError(std::string* error, const std::string& msg)
{
    if (error)
        *error = msg;
}

} // namespace

TopoDS_Shape loadShape(const std::string& path, std::string* error)
{
    const std::string ext = toLowerExt(path);
    try {
        if (ext == "step" || ext == "stp") {
            STEPControl_Reader reader;
            if (reader.ReadFile(path.c_str()) != IFSelect_RetDone) {
                setError(error, "failed to read STEP file: " + path);
                return {};
            }
            reader.TransferRoots();
            const TopoDS_Shape shape = reader.OneShape();
            if (shape.IsNull())
                setError(error, "STEP file contains no transferable shape: " + path);
            return shape;
        }
        else if (ext == "iges" || ext == "igs") {
            IGESControl_Reader reader;
            if (reader.ReadFile(path.c_str()) != IFSelect_RetDone) {
                setError(error, "failed to read IGES file: " + path);
                return {};
            }
            reader.TransferRoots();
            const TopoDS_Shape shape = reader.OneShape();
            if (shape.IsNull())
                setError(error, "IGES file contains no transferable shape: " + path);
            return shape;
        }
        else if (ext == "brep" || ext == "brp") {
            TopoDS_Shape shape;
            BRep_Builder builder;
            if (!BRepTools::Read(shape, path.c_str(), builder) || shape.IsNull()) {
                setError(error, "failed to read BREP file: " + path);
                return {};
            }
            return shape;
        }
        else if (ext.empty()) {
            setError(error, "cannot determine format (no file extension): " + path);
            return {};
        }
        else {
            setError(error, "unsupported format '." + ext +
                            "' (argos-cli measures BRep formats: step/iges/brep)");
            return {};
        }
    }
    catch (const Standard_Failure& e) {
        setError(error, std::string("OpenCASCADE error while loading: ") +
                        (e.GetMessageString() ? e.GetMessageString() : "unknown"));
        return {};
    }
    catch (const std::exception& e) {
        setError(error, std::string("error while loading: ") + e.what());
        return {};
    }
}

bool writeStep(const std::string& path, const TopoDS_Shape& shape, std::string* error)
{
    if (shape.IsNull()) {
        setError(error, "cannot write a null shape to STEP");
        return false;
    }
    try {
        // loadShape reads geometry into OCCT's native millimetres regardless of
        // the source file's unit, so pin the writer to mm to round-trip 1:1.
        Interface_Static::SetCVal("write.step.unit", "MM");
        STEPControl_Writer writer;
        if (writer.Transfer(shape, STEPControl_AsIs) != IFSelect_RetDone) {
            setError(error, "failed to transfer shape for STEP write");
            return false;
        }
        if (writer.Write(path.c_str()) != IFSelect_RetDone) {
            setError(error, "failed to write STEP file: " + path);
            return false;
        }
        return true;
    }
    catch (const Standard_Failure& e) {
        setError(error, std::string("OpenCASCADE error while writing STEP: ") +
                        (e.GetMessageString() ? e.GetMessageString() : "unknown"));
        return false;
    }
    catch (const std::exception& e) {
        setError(error, std::string("error while writing STEP: ") + e.what());
        return false;
    }
}

int countSubShapes(const TopoDS_Shape& shape, TopAbs_ShapeEnum type)
{
    if (shape.IsNull())
        return 0;

    TopTools_IndexedMapOfShape map;
    TopExp::MapShapes(shape, type, map);
    return map.Extent();
}

TopoDS_Shape subShape(const TopoDS_Shape& shape, TopAbs_ShapeEnum type, int index1Based)
{
    if (shape.IsNull())
        return {};

    TopTools_IndexedMapOfShape map;
    TopExp::MapShapes(shape, type, map);
    if (index1Based < 1 || index1Based > map.Extent())
        return {};

    return map(index1Based);
}

} // namespace argos
