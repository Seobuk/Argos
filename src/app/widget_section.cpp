/****************************************************************************
** Argos - SolidWorks-style Section (cutting-plane) panel
** SPDX-License-Identifier: BSD-2-Clause
****************************************************************************/

#include "widget_section.h"

#include "../base/bnd_utils.h"
#include "../base/math_utils.h"
#include "../graphics/graphics_scene.h"
#include "../graphics/graphics_utils.h"
#include "../gui/gui_document.h"
#include "../argos_core/section.h"

#include <AIS_ConnectedInteractive.hxx>
#include <AIS_Shape.hxx>
#include <BRepAlgoAPI_Section.hxx>
#include <BRep_Builder.hxx>
#include <Graphic3d_SequenceOfHClipPlane.hxx>
#include <Graphic3d_ZLayerId.hxx>
#include <Quantity_Color.hxx>
#include <Standard_Version.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shape.hxx>
#include <gp.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Trsf.hxx>

#include <QtCore/QSignalBlocker>
#include <QtGui/QColor>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QVBoxLayout>

#include <cmath>

namespace Mayo {

namespace {

// Resolve a displayed graphics object to its BRep shape placed in world space.
//
// A part shown at the assembly root is a plain AIS_Shape; an instanced part is
// an AIS_ConnectedInteractive that references a shared product shape and applies
// its own placement transform. Either way the section CAP is produced by
// clipping the TRANSFORMED presentation, so the outline must slice the shape at
// that same world placement. Slicing the raw (untransformed) product -- and
// skipping the connected instances entirely, as the old AIS_Shape-only path did
// -- put the outline at the wrong spot and dropped most assembly parts.
TopoDS_Shape worldPlacedShape(const GraphicsObjectPtr& obj)
{
    TopoDS_Shape shape;
    if (auto aisShape = OccHandle<AIS_Shape>::DownCast(obj)) {
        shape = aisShape->Shape();
    }
    else if (auto conn = OccHandle<AIS_ConnectedInteractive>::DownCast(obj)) {
        if (auto ref = OccHandle<AIS_Shape>::DownCast(conn->ConnectedTo()))
            shape = ref->Shape();
    }

    if (shape.IsNull())
        return {};

    TopLoc_Location loc(obj->Transformation());
#if OCC_VERSION_HEX >= 0x070600
    // TopoDS_Shape::Move() throws on a scale factor != 1 since OCCT 7.6; the cut
    // only needs the rigid placement, so normalize the scale away.
    const double absScale = std::abs(loc.Transformation().ScaleFactor());
    const double scalePrec = TopLoc_Location::ScalePrec();
    if (absScale < (1. - scalePrec) || absScale > (1. + scalePrec)) {
        gp_Trsf trsf = loc.Transformation();
        trsf.SetScaleFactor(1.);
        loc = trsf;
    }
#endif
    return loc.IsIdentity() ? shape : shape.Moved(loc);
}

} // namespace

WidgetSection::WidgetSection(GuiDocument* guiDoc, QWidget* parent)
    : QWidget(parent),
      m_guiDoc(guiDoc),
      m_view(guiDoc->graphicsView())
{
    m_plane = makeOccHandle<Graphic3d_ClipPlane>(gp_Pln{ gp::Origin(), gp::DZ() });
    m_plane->SetOn(false);
    m_plane->SetCapping(true);
    m_plane->SetUseObjectMaterial(true);

    const QString dimQss = "color: rgba(140,140,140,230); font-weight:600;";
    auto caption = [this, &dimQss](const QString& t) {
        auto l = new QLabel(t, this);
        l->setStyleSheet(dimQss);
        return l;
    };
    auto planeBtn = [this](const QString& t) {
        auto b = new QPushButton(t, this);
        b->setCheckable(true);
        b->setMinimumWidth(46);
        return b;
    };

    auto root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    // 절단면
    root->addWidget(caption(tr("절단면")));
    auto planeRow = new QHBoxLayout;
    planeRow->setSpacing(6);
    m_btnXY = planeBtn("XY");
    m_btnYZ = planeBtn("YZ");
    m_btnZX = planeBtn("ZX");
    planeRow->addWidget(m_btnXY);
    planeRow->addWidget(m_btnYZ);
    planeRow->addWidget(m_btnZX);
    planeRow->addStretch(1);
    root->addLayout(planeRow);

    // 옵셋
    root->addWidget(caption(tr("옵셋")));
    auto offRow = new QHBoxLayout;
    offRow->setSpacing(6);
    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setRange(0, 1000);
    m_spin = new QDoubleSpinBox(this);
    m_spin->setDecimals(2);
    m_spin->setRange(-1.0e6, 1.0e6);
    m_spin->setSuffix(" mm");
    m_spin->setMaximumWidth(110);
    offRow->addWidget(m_slider, 1);
    offRow->addWidget(m_spin);
    root->addLayout(offRow);

    // 방향 반전
    auto btnFlip = new QPushButton(tr("방향 반전 (Flip)"), this);
    root->addWidget(btnFlip);

    // 캡 (Capping)
    root->addWidget(caption(tr("캡 (Capping)")));
    m_checkCapping = new QCheckBox(tr("단면 채우기"), this);
    m_checkCapping->setChecked(true);
    root->addWidget(m_checkCapping);

    m_checkOutline = new QCheckBox(tr("검정 테두리 (외곽선)"), this);
    m_checkOutline->setChecked(true);
    m_checkOutline->setToolTip(tr("절단면 경계를 검정색 선으로 강조합니다"));
    root->addWidget(m_checkOutline);

    auto colorRow = new QHBoxLayout;
    colorRow->setSpacing(6);
    colorRow->addWidget(new QLabel(tr("캡 색상"), this));
    struct Sw { int r, g, b; };
    const Sw swatches[] = { {95,163,179}, {217,138,79}, {140,152,163}, {126,107,208} };
    for (const Sw& s : swatches) {
        const QColor col(s.r, s.g, s.b);
        auto b = new QPushButton(this);
        b->setFixedSize(20, 20);
        b->setToolTip(tr("캡 색상 적용"));
        b->setStyleSheet(QString("background:%1; border:1px solid palette(mid); border-radius:3px;").arg(col.name()));
        QObject::connect(b, &QPushButton::clicked, this, [this, col]{ this->setCapColor(col, false); });
        colorRow->addWidget(b);
    }
    auto btnObjColor = new QPushButton(tr("객체색"), this);
    btnObjColor->setToolTip(tr("객체 재질 색상으로 캡을 채웁니다"));
    QObject::connect(btnObjColor, &QPushButton::clicked, this, [this]{ this->setCapColor(QColor(), true); });
    colorRow->addWidget(btnObjColor);
    colorRow->addStretch(1);
    root->addLayout(colorRow);

    // 단면 정보 (둘레/모서리 from argos_core)
    root->addWidget(caption(tr("단면 정보")));
    auto info = new QFormLayout;
    info->setContentsMargins(0, 0, 0, 0);
    info->setSpacing(6);
    m_labelPerimeter = new QLabel("-", this);
    m_labelPerimeter->setStyleSheet("font-weight:600;");
    m_labelEdges = new QLabel("-", this);
    m_labelEdges->setStyleSheet("font-weight:600;");
    info->addRow(new QLabel(tr("둘레"), this), m_labelPerimeter);
    info->addRow(new QLabel(tr("절단 모서리"), this), m_labelEdges);
    root->addLayout(info);
    root->addStretch(1);

    // Wiring
    QObject::connect(m_btnXY, &QPushButton::clicked, this, [this]{ this->setPlane(Plane::XY, false); });
    QObject::connect(m_btnYZ, &QPushButton::clicked, this, [this]{ this->setPlane(Plane::YZ, false); });
    QObject::connect(m_btnZX, &QPushButton::clicked, this, [this]{ this->setPlane(Plane::ZX, false); });

    QObject::connect(m_slider, &QSlider::valueChanged, this, [this](int v) {
        QSignalBlocker block(m_spin);
        const double pos = this->posFromSlider(v);
        m_spin->setValue(pos);
        this->applyOffset(pos);
    });
    QObject::connect(m_slider, &QSlider::sliderReleased, this, [this]{ this->recomputeReadout(); });

    QObject::connect(m_spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double p) {
        QSignalBlocker block(m_slider);
        m_slider->setValue(this->sliderFromPos(p));
        this->applyOffset(p);
    });
    QObject::connect(m_spin, &QDoubleSpinBox::editingFinished, this, [this]{ this->recomputeReadout(); });

    QObject::connect(btnFlip, &QPushButton::clicked, this, [this]{ this->applyFlip(); });
    QObject::connect(m_checkCapping, &QCheckBox::toggled, this, [this](bool on) { this->applyCapping(on); });
    QObject::connect(m_checkOutline, &QCheckBox::toggled, this, [this](bool on) {
        m_showOutline = on;
        this->recomputeReadout();
    });

    this->setPlane(Plane::XY, false);
}

WidgetSection::~WidgetSection()
{
    if (!m_outline.IsNull())
        m_guiDoc->graphicsScene()->eraseObject(m_outline);
}

gp_Dir WidgetSection::baseNormal() const
{
    switch (m_curPlane) {
    case Plane::YZ: return gp::DX();
    case Plane::ZX: return gp::DY();
    case Plane::XY:
    default:        return gp::DZ();
    }
}

double WidgetSection::posFromSlider(int v) const
{
    return MathUtils::mappedValue(double(v), 0.0, 1000.0, m_spin->minimum(), m_spin->maximum());
}

int WidgetSection::sliderFromPos(double pos) const
{
    const double v = MathUtils::mappedValue(pos, m_spin->minimum(), m_spin->maximum(), 0.0, 1000.0);
    return static_cast<int>(std::lround(v));
}

void WidgetSection::setPlane(Plane p, bool keepOffset)
{
    m_curPlane = p;
    const gp_Dir base = this->baseNormal();

    double rmin = -50.0, rmax = 50.0;
    if (!m_bndBox.IsVoid()) {
        const auto bbc = BndBoxCoords::get(m_bndBox);
        const auto range = MathUtils::planeRange(bbc, base);
        rmin = range.first;
        rmax = range.second;
        if (rmax - rmin < 1.0e-6) { rmin -= 50.0; rmax += 50.0; }
    }
    const double mid = rmin + (rmax - rmin) * 0.5;
    double pos = mid;
    if (keepOffset) {
        const double cur = m_spin->value();
        if (cur >= rmin && cur <= rmax)
            pos = cur;
    }

    {
        QSignalBlocker bSpin(m_spin);
        QSignalBlocker bSlider(m_slider);
        m_spin->setRange(rmin, rmax);
        m_spin->setSingleStep((rmax - rmin) / 100.0);
        m_spin->setValue(pos);
        m_slider->setValue(this->sliderFromPos(pos));
    }

    const gp_Dir n = m_flipped ? base.Reversed() : base;
    GraphicsUtils::Gfx3dClipPlane_setNormal(m_plane, n);
    GraphicsUtils::Gfx3dClipPlane_setPosition(m_plane, pos);

    m_btnXY->setChecked(p == Plane::XY);
    m_btnYZ->setChecked(p == Plane::YZ);
    m_btnZX->setChecked(p == Plane::ZX);

    this->redraw();
    this->recomputeReadout();
}

void WidgetSection::applyOffset(double pos)
{
    GraphicsUtils::Gfx3dClipPlane_setPosition(m_plane, pos);
    // The outline is expensive to re-slice; hide it during the live drag and let
    // recomputeReadout() (fired on release) redraw it at the settled position.
    this->hideOutline();
    this->redraw();
}

void WidgetSection::applyFlip()
{
    m_flipped = !m_flipped;
    const gp_Dir n = m_flipped ? this->baseNormal().Reversed() : this->baseNormal();
    GraphicsUtils::Gfx3dClipPlane_setNormal(m_plane, n);
    GraphicsUtils::Gfx3dClipPlane_setPosition(m_plane, m_spin->value());
    this->recomputeReadout();
    this->redraw();
}

void WidgetSection::applyCapping(bool on)
{
    m_plane->SetCapping(on);
    this->redraw();
}

void WidgetSection::setCapColor(const QColor& color, bool useObjectMaterial)
{
    if (useObjectMaterial) {
        m_plane->SetUseObjectMaterial(true);
    }
    else {
        m_plane->SetUseObjectMaterial(false);
        m_plane->SetCappingColor(
            Quantity_Color(color.redF(), color.greenF(), color.blueF(), Quantity_TOC_RGB));
    }
    this->redraw();
}

void WidgetSection::setSectionOn(bool on)
{
    if (on) {
        if (!GraphicsUtils::V3dView_hasClipPlane(m_view.v3dView(), m_plane))
            m_view->AddClipPlane(m_plane);

        m_plane->SetOn(true);
        this->recomputeReadout();
    }
    else {
        m_plane->SetOn(false);
        this->hideOutline();
    }

    this->redraw();
}

void WidgetSection::setRanges(const Bnd_Box& box)
{
    // On the first real bounding box, snap the plane to the model's mid (a nice
    // SolidWorks-style default cut); afterwards keep the user's chosen offset.
    const bool wasVoid = m_bndBox.IsVoid();
    m_bndBox = box;
    this->setPlane(m_curPlane, !wasVoid);
}

int WidgetSection::collectDisplayedShapes(TopoDS_Compound& comp) const
{
    BRep_Builder builder;
    builder.MakeCompound(comp);
    int count = 0;
    m_guiDoc->graphicsScene()->foreachDisplayedObject([&](const GraphicsObjectPtr& obj) {
        if (GuiDocument::isAisViewCubeObject(obj))
            return;

        // Never feed our own outline back into the section input.
        if (!m_outline.IsNull()
            && static_cast<const void*>(obj.get()) == static_cast<const void*>(m_outline.get())) {
            return;
        }

        const TopoDS_Shape s = worldPlacedShape(obj);
        if (!s.IsNull()) {
            builder.Add(comp, s);
            ++count;
        }
    });

    return count;
}

void WidgetSection::recomputeReadout()
{
    // Build a compound of all displayed BRep shapes and section it via argos_core.
    TopoDS_Compound comp;
    const int count = this->collectDisplayedShapes(comp);

    if (count == 0) {
        m_labelPerimeter->setText("-");
        m_labelEdges->setText("-");
        this->updateOutline(TopoDS_Shape{});
        return;
    }

    argos::SectionState st;
    st.plane = (m_curPlane == Plane::XY) ? argos::StandardPlane::XY
             : (m_curPlane == Plane::YZ) ? argos::StandardPlane::YZ
                                         : argos::StandardPlane::ZX;
    st.offset = m_spin->value();
    st.flipped = m_flipped;

    try {
        const argos::SectionResult r = argos::computeSection(comp, st);
        if (r.ok) {
            m_labelPerimeter->setText(QString::number(r.outlineLength, 'f', 2) + " mm");
            m_labelEdges->setText(QString::number(r.edgeCount));
        }
        else {
            m_labelPerimeter->setText("-");
            m_labelEdges->setText("-");
        }
    }
    catch (...) {
        m_labelPerimeter->setText("-");
        m_labelEdges->setText("-");
    }

    // Trace the section boundary as a crisp black outline over the capping fill.
    this->updateOutline(comp);
}

void WidgetSection::hideOutline()
{
    if (!m_outline.IsNull())
        m_guiDoc->graphicsScene()->setObjectVisible(m_outline, false);
}

void WidgetSection::updateOutline(const TopoDS_Shape& modelComp)
{
    GraphicsScene* scene = m_guiDoc->graphicsScene();

    const bool want = m_showOutline && m_plane->IsOn() && !modelComp.IsNull();
    TopoDS_Shape secShape;
    if (want) {
        try {
            // Slice with the very plane driving the visual cut so the outline and
            // the capping always coincide, whatever the offset/flip.
            const gp_Pln pln = m_plane->ToPlane();
            BRepAlgoAPI_Section sec(modelComp, pln, Standard_False);
            sec.ComputePCurveOn1(Standard_False);
            sec.Build();
            if (sec.IsDone()) {
                const TopoDS_Shape sh = sec.Shape();
                if (TopExp_Explorer(sh, TopAbs_EDGE).More())
                    secShape = sh;
            }
        }
        catch (...) {
            secShape.Nullify();
        }
    }

    if (secShape.IsNull()) {
        this->hideOutline();
        this->redraw();
        return;
    }

    if (m_outline.IsNull()) {
        m_outline = new AIS_Shape(secShape);
        m_outline->SetDisplayMode(0);   // wireframe: the section is edges only
        m_outline->SetColor(Quantity_Color(0.0, 0.0, 0.0, Quantity_TOC_RGB));
        m_outline->SetWidth(2.5);
        // Draw on top of the capping and exempt it from the section clip plane so
        // the whole boundary stays visible (the curve lies exactly on the plane).
        m_outline->SetZLayer(Graphic3d_ZLayerId_Topmost);
        auto exemptPlanes = makeOccHandle<Graphic3d_SequenceOfHClipPlane>();
        exemptPlanes->SetOverrideGlobal(Standard_True);
        m_outline->SetClipPlanes(exemptPlanes);
        scene->addObject(m_outline, GraphicsScene::AddObjectDisableSelectionMode);
    }
    else {
        m_outline->SetShape(secShape);
        scene->recomputeObjectPresentation(m_outline);
    }

    scene->setObjectVisible(m_outline, true);
    this->redraw();
}

void WidgetSection::redraw()
{
    m_view.redraw();
}

} // namespace Mayo
