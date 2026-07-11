/****************************************************************************
** Argos - SolidWorks-style Section (cutting-plane) panel
** SPDX-License-Identifier: BSD-2-Clause
**
** Reuses OCCT's Graphic3d_ClipPlane for the live visual cut and argos_core's
** computeSection() for the section-outline readout.
****************************************************************************/

#pragma once

#include "../base/occ_handle.h"
#include "../base/signal.h"
#include "../graphics/graphics_view_ptr.h"

#include <QtWidgets/QWidget>
#include <AIS_Shape.hxx>
#include <Bnd_Box.hxx>
#include <Graphic3d_ClipPlane.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Dir.hxx>

class QCheckBox;
class QColor;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSlider;
class QTimer;

namespace argos { struct SectionResult; }

namespace Mayo {

class GuiDocument;

class WidgetSection : public QWidget {
    Q_OBJECT
public:
    WidgetSection(GuiDocument* guiDoc, QWidget* parent = nullptr);
    ~WidgetSection() override;

    void setSectionOn(bool on);
    void setRanges(const Bnd_Box& box);   // slot for signalGraphicsBoundingBoxChanged

signals:
    void sizeAdjustmentRequested();

private:
    enum class Plane { XY, YZ, ZX };

    gp_Dir baseNormal() const;
    double posFromSlider(int v) const;
    int sliderFromPos(double pos) const;

    void setPlane(Plane p, bool keepOffset);
    void applyOffset(double pos);
    void applyFlip();
    void applyCapping(bool on);
    void setCapColor(const QColor& color, bool useObjectMaterial);

    // Slice the displayed model with the current plane -- ONE BRepAlgoAPI_Section
    // pass, run off the UI thread -- and feed both the perimeter/edge readout and
    // the black outline from that single result. Requests arriving while a slice
    // is in flight are coalesced via m_sliceGen.
    void recomputeReadout();
    // Recompute only if the settled state differs from what is displayed
    // (m_lastSliceGen != m_sliceGen) and no slice is in flight. Shared by the
    // slider-release / editing-finished / settle-timer / flip triggers so a
    // no-op release or focus-out never blanks the readout with a useless
    // re-slice, nor discards an in-flight slice of the identical state.
    void settleRecompute();
    // UI-thread continuation of recomputeReadout(): fills the readout labels and
    // shows/hides the outline from the finished slice.
    void applySliceResult(const argos::SectionResult& result);

    // Collect every displayed BRep shape (minus view-cube and the outline itself)
    // into a compound; returns the number of shapes added.
    int collectDisplayedShapes(TopoDS_Compound& comp) const;
    // Display 'sectionCurves' (edges of the cut boundary, in world space) as a
    // crisp black outline on top of the capping.
    void showOutline(const TopoDS_Shape& sectionCurves);
    void hideOutline();
    void redraw();

    GuiDocument* m_guiDoc = nullptr;
    GraphicsViewPtr m_view;
    OccHandle<Graphic3d_ClipPlane> m_plane;
    OccHandle<AIS_Shape> m_outline;   // black border traced on the cut section
    Bnd_Box m_bndBox;
    Plane m_curPlane = Plane::XY;
    bool m_flipped = false;
    bool m_showOutline = true;

    // Async slicing state. m_sliceGen identifies the plane/scene state a slice
    // request was made for; anything that invalidates the cut bumps it, so a
    // finished worker knows whether its result is still current. m_sliceBusy
    // caps the in-flight slices at one (stale completions re-enter
    // recomputeReadout()). m_lastSectionShape/m_lastSliceGen cache the latest
    // applied slice so re-enabling the outline checkbox needs no re-slice.
    quint64 m_sliceGen = 0;
    bool m_sliceBusy = false;
    TopoDS_Shape m_lastSectionShape;
    quint64 m_lastSliceGen = 0;
    // Fires a settle-recompute for offset paths that emit neither
    // sliderReleased nor editingFinished (keyboard steps, groove clicks,
    // spinbox arrows).
    QTimer* m_settleTimer = nullptr;
    // On document close the GuiDocument (and the GraphicsScene inside it) dies
    // BEFORE this widget, which is merely deleteLater()'d -- this connection
    // nulls m_guiDoc at that moment so late slice completions and the
    // destructor never touch the freed scene.
    SignalConnectionHandle m_connGuiDocErased;
    SignalConnectionHandle m_connNodesVisibility;

    QPushButton* m_btnXY = nullptr;
    QPushButton* m_btnYZ = nullptr;
    QPushButton* m_btnZX = nullptr;
    QSlider* m_slider = nullptr;
    QDoubleSpinBox* m_spin = nullptr;
    QCheckBox* m_checkCapping = nullptr;
    QCheckBox* m_checkOutline = nullptr;
    QLabel* m_labelPerimeter = nullptr;
    QLabel* m_labelEdges = nullptr;
};

} // namespace Mayo
