/****************************************************************************
** Argos - SolidWorks-style Section (cutting-plane) panel
** SPDX-License-Identifier: BSD-2-Clause
**
** Reuses OCCT's Graphic3d_ClipPlane for the live visual cut and argos_core's
** computeSection() for the section-outline readout.
****************************************************************************/

#pragma once

#include "../base/occ_handle.h"
#include "../graphics/graphics_view_ptr.h"

#include <QtWidgets/QWidget>
#include <Bnd_Box.hxx>
#include <Graphic3d_ClipPlane.hxx>
#include <gp_Dir.hxx>

class QCheckBox;
class QColor;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSlider;

namespace Mayo {

class GuiDocument;

class WidgetSection : public QWidget {
    Q_OBJECT
public:
    WidgetSection(GuiDocument* guiDoc, QWidget* parent = nullptr);

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
    void recomputeReadout();
    void redraw();

    GuiDocument* m_guiDoc = nullptr;
    GraphicsViewPtr m_view;
    OccHandle<Graphic3d_ClipPlane> m_plane;
    Bnd_Box m_bndBox;
    Plane m_curPlane = Plane::XY;
    bool m_flipped = false;

    QPushButton* m_btnXY = nullptr;
    QPushButton* m_btnYZ = nullptr;
    QPushButton* m_btnZX = nullptr;
    QSlider* m_slider = nullptr;
    QDoubleSpinBox* m_spin = nullptr;
    QCheckBox* m_checkCapping = nullptr;
    QLabel* m_labelPerimeter = nullptr;
    QLabel* m_labelEdges = nullptr;
};

} // namespace Mayo
