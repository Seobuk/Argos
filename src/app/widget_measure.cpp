/****************************************************************************
** Copyright (c) 2016, Fougue SAS <https://www.fougue.pro>
** SPDX-License-Identifier: BSD-2-Clause
****************************************************************************/

#include "widget_measure.h"
#include "app_module.h"
#include "theme.h"
#include "ui_widget_measure.h"

#include "../base/cpp_utils.h"
#include "../base/occ_handle.h"
#include "../gui/gui_document.h"
#include "../measure/measure_tool_brep.h"
#include "../qtcommon/qstring_conv.h"
#include "../argos_core/measure.h"

#include <AIS_Shape.hxx>
#include <Standard_Failure.hxx>
#include <Standard_Version.hxx>
#include <StdSelect_BRepOwner.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Trsf.hxx>

#include <QtCore/QString>
#include <QtCore/QtDebug>
#include <QtGui/QAction>
#include <QtGui/QClipboard>
#include <QtGui/QFontDatabase>
#include <QtGui/QGuiApplication>
#include <QtGui/QShortcut>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMenu>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include <cmath>

namespace Mayo {

namespace {

using IMeasureToolPtr = std::unique_ptr<IMeasureTool>;

// Convert a selected graphics owner into the underlying (located) TopoDS_Shape.
// Mirrors the private getShape() helper of MeasureToolBRep so argos_core can be
// fed raw shapes.
TopoDS_Shape ownerToShape(const GraphicsOwnerPtr& owner)
{
    auto brepOwner = OccHandle<StdSelect_BRepOwner>::DownCast(owner);
    if (!brepOwner)
        return {};

    TopLoc_Location loc = owner->Location();
#if OCC_VERSION_HEX >= 0x070600
    const double absScale = std::abs(loc.Transformation().ScaleFactor());
    const double scalePrec = TopLoc_Location::ScalePrec();
    if (absScale < (1. - scalePrec) || absScale > (1. + scalePrec)) {
        gp_Trsf trsf = loc.Transformation();
        trsf.SetScaleFactor(1.);
        loc = trsf;
    }
#endif
    return brepOwner->Shape().Moved(loc);
}

// Map the argos auto-dispatched kind onto a Mayo MeasureType, so the existing
// on-screen callout machinery (BaseMeasureDisplay) can be reused.
MeasureType mayoTypeFromKind(argos::MeasureKind k)
{
    switch (k) {
    case argos::MeasureKind::VertexPosition: return MeasureType::VertexPosition;
    case argos::MeasureKind::Length:         return MeasureType::Length;
    case argos::MeasureKind::Circle:         return MeasureType::CircleDiameter;
    case argos::MeasureKind::Area:           return MeasureType::Area;
    case argos::MeasureKind::MinDistance:    return MeasureType::MinDistance;
    case argos::MeasureKind::CenterDistance: return MeasureType::CenterDistance;
    case argos::MeasureKind::Angle:          return MeasureType::Angle;
    case argos::MeasureKind::BoundingBox:    return MeasureType::BoundingBox;
    default:                                 return MeasureType::None;
    }
}

// Decimal count for the panel/history, honoring the user's unit setting so the
// text readout matches the on-screen callout (which uses the same setting).
int uiDecimals()
{
    const AppModule* m = AppModule::get();
    const int d = m ? m->defaultTextOptions().unitDecimals : 3;
    return (d >= 0 && d <= 12) ? d : 3;
}

QString num(double v) { return QString::number(v, 'f', uiDecimals()); }
QString xyz(const argos::Vec3& p) { return QString("(%1, %2, %3)").arg(num(p.x), num(p.y), num(p.z)); }

// Human-readable, Korean-labelled readout of an argos measurement result.
QString formatArgosResult(const argos::MeasureResult& r)
{
    QString s;
    switch (r.kind) {
    case argos::MeasureKind::VertexPosition:
        if (r.point)
            s = QString("점 좌표\n  X: %1\n  Y: %2\n  Z: %3")
                    .arg(num(r.point->x), num(r.point->y), num(r.point->z));
        break;
    case argos::MeasureKind::Length:
        s = QString("길이: %1 mm").arg(num(r.value.value_or(0)));
        break;
    case argos::MeasureKind::Circle:
        s = QString("지름: %1 mm").arg(num(r.diameter.value_or(0)));
        if (r.radius)  s += QString("\n  반경: %1 mm").arg(num(*r.radius));
        if (r.point)   s += QString("\n  중심: %1").arg(xyz(*r.point));
        if (r.area)    s += QString("\n  면적: %1 mm²").arg(num(*r.area));
        break;
    case argos::MeasureKind::Area:
        s = QString("면적: %1 mm²").arg(num(r.value.value_or(0)));
        if (r.length) s += QString("\n  둘레: %1 mm").arg(num(*r.length));
        break;
    case argos::MeasureKind::MinDistance:
        s = QString("거리: %1 mm").arg(num(r.value.value_or(0)));
        if (r.delta)
            s += QString("\n  dX: %1  dY: %2  dZ: %3")
                    .arg(num(r.delta->x), num(r.delta->y), num(r.delta->z));
        break;
    case argos::MeasureKind::CenterDistance:
        s = QString("중심간 거리: %1 mm").arg(num(r.value.value_or(0)));
        if (r.delta)
            s += QString("\n  dX: %1  dY: %2  dZ: %3")
                    .arg(num(r.delta->x), num(r.delta->y), num(r.delta->z));
        break;
    case argos::MeasureKind::Angle:
        s = QString("각도: %1°").arg(num(r.value.value_or(0)));
        break;
    case argos::MeasureKind::BoundingBox:
        if (r.bboxSize)
            s = QString("경계 상자: %1 × %2 × %3 mm")
                    .arg(num(r.bboxSize->x), num(r.bboxSize->y), num(r.bboxSize->z));
        if (r.value) s += QString("\n  부피: %1 mm³").arg(num(*r.value));
        break;
    case argos::MeasureKind::SumLength:
        s = QString("총 길이 (%1개): %2 mm").arg(r.inputCount).arg(num(r.value.value_or(0)));
        break;
    case argos::MeasureKind::SumArea:
        s = QString("총 면적 (%1개): %2 mm²").arg(r.inputCount).arg(num(r.value.value_or(0)));
        break;
    default:
        break;
    }

    // Argos: the JSON payload is no longer dumped into the human-facing readout
    // (it cluttered the result). It is still available to the user via the
    // "JSON 복사" button and to the CLI/MCP via argos::to_json().
    return s;
}

// One-line summary for the measurement-history list.
QString formatArgosShort(const argos::MeasureResult& r)
{
    switch (r.kind) {
    case argos::MeasureKind::VertexPosition:
        return r.point ? QString("좌표 %1").arg(xyz(*r.point)) : QString("좌표");
    case argos::MeasureKind::Length:
        return QString("길이 %1 mm").arg(num(r.value.value_or(0)));
    case argos::MeasureKind::Circle:
        return QString("지름 %1 mm").arg(num(r.diameter.value_or(0)));
    case argos::MeasureKind::Area:
        return QString("면적 %1 mm²").arg(num(r.value.value_or(0)));
    case argos::MeasureKind::MinDistance:
        return QString("거리 %1 mm").arg(num(r.value.value_or(0)));
    case argos::MeasureKind::CenterDistance:
        return QString("중심거리 %1 mm").arg(num(r.value.value_or(0)));
    case argos::MeasureKind::Angle:
        return QString("각도 %1°").arg(num(r.value.value_or(0)));
    case argos::MeasureKind::BoundingBox:
        return r.bboxSize ? QString("경계상자 %1×%2×%3")
                                .arg(num(r.bboxSize->x), num(r.bboxSize->y), num(r.bboxSize->z))
                          : QString("경계상자");
    case argos::MeasureKind::SumLength:
        return QString("총 길이 %1 mm").arg(num(r.value.value_or(0)));
    case argos::MeasureKind::SumArea:
        return QString("총 면적 %1 mm²").arg(num(r.value.value_or(0)));
    default:
        return QString();
    }
}

// Get global array of available measurement tool objects
std::vector<IMeasureToolPtr>& getMeasureTools()
{
    static std::vector<IMeasureToolPtr> vecTool;
    return vecTool;
}

// Returns the tool object adapted for the graphics object 'gfxObject' and measure type
// Note: the search is performed in the array returned by getMeasureTools() function
IMeasureTool* findSupportingMeasureTool(const GraphicsObjectPtr& gfxObject, MeasureType measureType)
{
    for (const IMeasureToolPtr& ptr : getMeasureTools()) {
        if (ptr->supports(measureType) && ptr->supports(gfxObject))
            return ptr.get();
    }

    return nullptr;
}

// Helper function to iterate and execute function 'fn' on all the graphics objects owned by a
// measure display
template<typename Function>
void foreachGraphicsObject(const IMeasureDisplay* ptr, Function fn)
{
    if (ptr) {
        const int count = ptr->graphicsObjectsCount();
        for (int i = 0; i < count; ++i)
            fn(ptr->graphicsObjectAt(i));
    }
}

// Overload of the helper function above
template<typename Function>
void foreachGraphicsObject(const std::unique_ptr<IMeasureDisplay>& ptr, Function fn) {
    foreachGraphicsObject(ptr.get(), fn);
}

} // namespace

WidgetMeasure::WidgetMeasure(GuiDocument* guiDoc, QWidget* parent)
    : QWidget(parent),
      m_ui(new Ui_WidgetMeasure),
      m_guiDoc(guiDoc)
{
    if (getMeasureTools().empty())
        getMeasureTools().push_back(std::make_unique<MeasureToolBRep>());

    m_ui->setupUi(this);
    QObject::connect(
        m_ui->combo_MeasureType, qOverload<int>(&QComboBox::currentIndexChanged),
        this, &WidgetMeasure::onMeasureTypeChanged
    );
    QObject::connect(
        m_ui->combo_LengthUnit, qOverload<int>(&QComboBox::currentIndexChanged),
        this, &WidgetMeasure::onMeasureUnitsChanged
    );
    QObject::connect(
        m_ui->combo_AngleUnit, qOverload<int>(&QComboBox::currentIndexChanged),
        this, &WidgetMeasure::onMeasureUnitsChanged
    );
    QObject::connect(
        m_ui->combo_AreaUnit, qOverload<int>(&QComboBox::currentIndexChanged),
        this, &WidgetMeasure::onMeasureUnitsChanged
    );
    QObject::connect(
        m_ui->combo_VolumeUnit, qOverload<int>(&QComboBox::currentIndexChanged),
        this, &WidgetMeasure::onMeasureUnitsChanged
    );

    this->onMeasureTypeChanged(m_ui->combo_MeasureType->currentIndex());

    // Argos: SolidWorks-style auto-dispatch -> the measure-type selector is no
    // longer needed (type is inferred from the selection set). Unit selectors are
    // deferred to P1; the panel readout currently uses canonical units (mm, mm^2,
    // deg, mm^3), so hide them to avoid advertising controls that do nothing yet.
    // (Done AFTER onMeasureTypeChanged, which toggles unit-widget visibility.)
    for (QWidget* w : {
            static_cast<QWidget*>(m_ui->label_MeasureType), static_cast<QWidget*>(m_ui->combo_MeasureType),
            static_cast<QWidget*>(m_ui->label_LengthUnit),  static_cast<QWidget*>(m_ui->combo_LengthUnit),
            static_cast<QWidget*>(m_ui->label_AngleUnit),   static_cast<QWidget*>(m_ui->combo_AngleUnit),
            static_cast<QWidget*>(m_ui->label_AreaUnit),    static_cast<QWidget*>(m_ui->combo_AreaUnit),
            static_cast<QWidget*>(m_ui->label_VolumeUnit),  static_cast<QWidget*>(m_ui->combo_VolumeUnit) }) {
        w->setVisible(false);
    }

    // Argos: SolidWorks-style options (Show XYZ, Point-to-Point), quick actions
    // (clear selection / copy value / copy JSON) and the measurement history,
    // built in code and inserted into the panel grid (rows 5, 7-8). This is an
    // ADDITIVE layer: the original (hidden) unit/type controls are left intact.
    m_checkShowXyz = new QCheckBox(tr("dX/dY/dZ 표시"), this);
    m_checkShowXyz->setChecked(true);
    m_checkPointToPoint = new QCheckBox(tr("점-점 거리"), this);

    auto btnClearSel = new QPushButton(tr("선택 해제"), this);
    btnClearSel->setToolTip(tr("현재 선택을 비웁니다 (Esc)"));
    auto btnCopyValue = new QPushButton(tr("값 복사"), this);
    btnCopyValue->setToolTip(tr("측정값을 클립보드로 복사합니다 (Ctrl+C)"));
    auto btnCopyJson = new QPushButton(tr("JSON 복사"), this);
    btnCopyJson->setToolTip(tr("측정 결과를 JSON으로 복사합니다 (CLI/자동화용)"));

    auto optionRow = new QWidget(this);
    auto optionLayout = new QVBoxLayout(optionRow);
    optionLayout->setContentsMargins(0, 0, 0, 0);
    optionLayout->setSpacing(4);
    {
        auto checkRow = new QHBoxLayout;
        checkRow->setContentsMargins(0, 0, 0, 0);
        checkRow->addWidget(m_checkShowXyz);
        checkRow->addWidget(m_checkPointToPoint);
        checkRow->addStretch(1);
        optionLayout->addLayout(checkRow);

        auto actionRow = new QHBoxLayout;
        actionRow->setContentsMargins(0, 0, 0, 0);
        actionRow->addWidget(btnClearSel);
        actionRow->addStretch(1);
        actionRow->addWidget(btnCopyValue);
        actionRow->addWidget(btnCopyJson);
        optionLayout->addLayout(actionRow);
    }
    m_ui->layout_Main->addWidget(optionRow, 5, 0, 1, 2);

    // History header: "측정 이력" + a "모두 지우기" button on the right (design).
    auto historyHeader = new QWidget(this);
    auto historyHeaderLayout = new QHBoxLayout(historyHeader);
    historyHeaderLayout->setContentsMargins(0, 0, 0, 0);
    historyHeaderLayout->addWidget(new QLabel(tr("측정 이력"), historyHeader));
    historyHeaderLayout->addStretch(1);
    auto btnClearHistory = new QPushButton(tr("모두 지우기"), historyHeader);
    btnClearHistory->setToolTip(tr("측정 이력을 모두 비웁니다"));
    historyHeaderLayout->addWidget(btnClearHistory);

    m_historyList = new QListWidget(this);
    m_historyList->setMaximumHeight(140);
    m_historyList->setToolTip(tr("항목을 더블클릭하거나 우클릭하여 값을 복사합니다"));
    m_historyList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_ui->layout_Main->addWidget(historyHeader, 7, 0, 1, 2);
    m_ui->layout_Main->addWidget(m_historyList, 8, 0, 1, 2);

    QObject::connect(m_checkShowXyz, &QCheckBox::toggled, this, [this]{ this->recompute(false); });
    QObject::connect(m_checkPointToPoint, &QCheckBox::toggled, this, [this]{ this->recompute(false); });

    // Quick actions — all reuse existing engine/scene state; nothing is removed.
    auto copyText = [](const QString& text) {
        if (!text.isEmpty())
            QGuiApplication::clipboard()->setText(text);
    };
    QObject::connect(btnCopyValue, &QPushButton::clicked, this, [=]{ copyText(m_lastShort); });
    QObject::connect(btnCopyJson, &QPushButton::clicked, this, [=]{ copyText(m_lastJson); });
    QObject::connect(btnClearSel, &QPushButton::clicked, this, [this]{
        m_guiDoc->graphicsScene()->clearSelection();
    });
    QObject::connect(btnClearHistory, &QPushButton::clicked, this, [this]{
        if (m_historyList)
            m_historyList->clear();
    });
    QObject::connect(m_historyList, &QListWidget::itemDoubleClicked, this,
                     [=](QListWidgetItem* item) { if (item) copyText(item->text()); });
    QObject::connect(m_historyList, &QListWidget::customContextMenuRequested, this,
                     [=](const QPoint& pos) {
        QListWidgetItem* item = m_historyList->itemAt(pos);
        if (!item)
            return;

        QMenu menu(this);
        QAction* actCopy = menu.addAction(tr("복사"));
        if (menu.exec(m_historyList->viewport()->mapToGlobal(pos)) == actCopy)
            copyText(item->text());
    });

    // Keyboard: Esc clears the current selection, Ctrl+C copies the value.
    // Scoped to this panel so no app-wide shortcut is ever hijacked.
    auto scEsc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    scEsc->setContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(scEsc, &QShortcut::activated, this, [this]{
        m_guiDoc->graphicsScene()->clearSelection();
    });
    auto scCopy = new QShortcut(QKeySequence::Copy, this);
    scCopy->setContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(scCopy, &QShortcut::activated, this, [=]{ copyText(m_lastShort); });

    this->updateMessagePanel();
}

WidgetMeasure::~WidgetMeasure()
{
    m_connGraphicsSelectionChanged.disconnect();
    m_connDocumentEntityAdded.disconnect();
    delete m_ui;
}

void WidgetMeasure::setMeasureOn(bool on)
{
    m_errorMessage.clear();
    m_resultText.clear();
    auto gfxScene = m_guiDoc->graphicsScene();
    if (on) {
        // Argos: history is preserved across tool re-activation; it is cleared
        // only via the explicit "모두 지우기" button (non-destructive).
        if (getMeasureTools().empty())
            getMeasureTools().push_back(std::make_unique<MeasureToolBRep>());

        m_tool = getMeasureTools().front().get();

        // SolidWorks-style: activate vertex + edge + face selection modes at once
        // so the user can mix-click sub-shapes without switching tool modes.
        gfxScene->clearSelection();
        gfxScene->foreachDisplayedObject([&](const GraphicsObjectPtr& gfxObject) {
            if (GuiDocument::isAisViewCubeObject(gfxObject))
                return;

            gfxScene->deactivateObjectSelection(gfxObject);
            for (TopAbs_ShapeEnum t : { TopAbs_VERTEX, TopAbs_EDGE, TopAbs_FACE })
                gfxScene->activateObjectSelection(gfxObject, AIS_Shape::SelectionMode(t));
        });
        gfxScene->redraw();

        m_connGraphicsSelectionChanged = gfxScene->signalSelectionChanged.connectSlot(
            &WidgetMeasure::onGraphicsSelectionChanged, this
        );
        m_connDocumentEntityAdded = m_guiDoc->document()->signalEntityAdded.connectSlot(
            &WidgetMeasure::onDocumentEntityAdded, this
        );
    }
    else {
        this->clearAllMeasureDisplays();
        gfxScene->foreachDisplayedObject([=](const GraphicsObjectPtr& gfxObject) {
            gfxScene->deactivateObjectSelection(gfxObject);
            gfxScene->activateObjectSelection(gfxObject, 0);
        });
        gfxScene->clearSelection();
        m_vecSelectedOwner.clear();
        m_connGraphicsSelectionChanged.disconnect();
        m_connDocumentEntityAdded.disconnect();
        gfxScene->redraw();
    }

    this->updateMessagePanel();
}

void WidgetMeasure::addTool(std::unique_ptr<IMeasureTool> tool)
{
    if (tool)
        getMeasureTools().push_back(std::move(tool));
}

MeasureType WidgetMeasure::toMeasureType(int comboBoxId)
{
    switch (comboBoxId) {
    case 0: return MeasureType::VertexPosition;
    case 1: return MeasureType::CircleCenter;
    case 2: return MeasureType::CircleDiameter;
    case 3: return MeasureType::MinDistance;
    case 4: return MeasureType::CenterDistance;
    case 5: return MeasureType::Angle;
    case 6: return MeasureType::Length;
    case 7: return MeasureType::Area;
    case 8: return MeasureType::BoundingBox;
    }
    return MeasureType::None;
}

LengthUnit WidgetMeasure::toMeasureLengthUnit(int comboBoxId)
{
    switch (comboBoxId) {
    case 0: return LengthUnit::Millimeter;
    case 1: return LengthUnit::Centimeter;
    case 2: return LengthUnit::Meter;
    case 3: return LengthUnit::Inch;
    case 4: return LengthUnit::Foot;
    case 5: return LengthUnit::Yard;
    }
    return {};
}

AngleUnit WidgetMeasure::toMeasureAngleUnit(int comboBoxId)
{
    switch (comboBoxId) {
    case 0: return AngleUnit::Degree;
    case 1: return AngleUnit::Radian;
    }
    return {};
}

AreaUnit WidgetMeasure::toMeasureAreaUnit(int comboBoxId)
{
    switch (comboBoxId) {
    case 0: return AreaUnit::SquareMillimeter;
    case 1: return AreaUnit::SquareCentimeter;
    case 2: return AreaUnit::SquareMeter;
    case 3: return AreaUnit::SquareInch;
    case 4: return AreaUnit::SquareFoot;
    case 5: return AreaUnit::SquareYard;
    }
    return {};
}

VolumeUnit WidgetMeasure::toMeasureVolumeUnit(int comboBoxId)
{
    switch (comboBoxId) {
    case 0: return VolumeUnit::CubicMillimeter;
    case 1: return VolumeUnit::CubicCentimeter;
    case 2: return VolumeUnit::CubicMeter;
    case 3: return VolumeUnit::CubicInch;
    case 4: return VolumeUnit::CubicFoot;
    case 5: return VolumeUnit::Liter;
    case 6: return VolumeUnit::ImperialGallon;
    case 7: return VolumeUnit::USGallon;
    }
    return {};
}

void WidgetMeasure::onMeasureTypeChanged(int id)
{
    // Update widgets visibility
    const MeasureType measureType = WidgetMeasure::toMeasureType(id);
    const bool measureIsLengthBased = measureType != MeasureType::Angle;
    const bool measureIsAngle = measureType == MeasureType::Angle;
    const bool measureIsArea = measureType == MeasureType::Area;
    const bool measureIsVolume = measureType == MeasureType::BoundingBox;
    // Note: don't call "ui->comboUnit->setVisible(labelUnit->isVisible())" because at this point
    //       QWidget::isVisible() might not be effective(probably needs to process eventloop)
    m_ui->label_LengthUnit->setVisible(measureIsLengthBased && !measureIsArea);
    m_ui->combo_LengthUnit->setVisible(measureIsLengthBased && !measureIsArea);
    m_ui->label_AngleUnit->setVisible(measureIsAngle);
    m_ui->combo_AngleUnit->setVisible(measureIsAngle);
    m_ui->label_AreaUnit->setVisible(measureIsArea);
    m_ui->combo_AreaUnit->setVisible(measureIsArea);
    m_ui->label_VolumeUnit->setVisible(measureIsVolume);
    m_ui->combo_VolumeUnit->setVisible(measureIsVolume);

    auto gfxScene = m_guiDoc->graphicsScene();

    // Find measure tool
    m_tool = nullptr;
    gfxScene->foreachDisplayedObject([=](const GraphicsObjectPtr& gfxObject) {
        if (!m_tool)
            m_tool = findSupportingMeasureTool(gfxObject, measureType);
    });

    // Apply 3D selection modes required by the measure tool
    gfxScene->clearSelection();
    gfxScene->foreachDisplayedObject([=](const GraphicsObjectPtr& gfxObject) {
        if (GuiDocument::isAisViewCubeObject(gfxObject))
            return; // Skip

        gfxScene->deactivateObjectSelection(gfxObject);
        if (m_tool) {
            for (GraphicsObjectSelectionMode mode : m_tool->selectionModes(measureType))
                gfxScene->activateObjectSelection(gfxObject, mode);
        }
    });
    gfxScene->redraw();
}

void WidgetMeasure::onMeasureUnitsChanged()
{
    const MeasureDisplayConfig config = this->currentMeasureDisplayConfig();
    for (IMeasureDisplayPtr& measure : m_vecMeasureDisplay) {
        measure->update(config);
        foreachGraphicsObject(measure, [](const GraphicsObjectPtr& gfxObject) {
            gfxObject->Redisplay();
        });
    }

    m_guiDoc->graphicsScene()->redraw();
    this->updateMessagePanel();
}

MeasureType WidgetMeasure::currentMeasureType() const
{
    return WidgetMeasure::toMeasureType(m_ui->combo_MeasureType->currentIndex());
}

MeasureDisplayConfig WidgetMeasure::currentMeasureDisplayConfig() const
{
    MeasureDisplayConfig cfg;
    cfg.lengthUnit = WidgetMeasure::toMeasureLengthUnit(m_ui->combo_LengthUnit->currentIndex());
    cfg.angleUnit = WidgetMeasure::toMeasureAngleUnit(m_ui->combo_AngleUnit->currentIndex());
    cfg.areaUnit = WidgetMeasure::toMeasureAreaUnit(m_ui->combo_AreaUnit->currentIndex());
    cfg.volumeUnit = WidgetMeasure::toMeasureVolumeUnit(m_ui->combo_VolumeUnit->currentIndex());
    cfg.doubleToStringOptions.locale = AppModule::get()->stdLocale();
    cfg.doubleToStringOptions.decimalCount = AppModule::get()->defaultTextOptions().unitDecimals;
    cfg.devicePixelRatio = this->devicePixelRatioF();
    return cfg;
}

void WidgetMeasure::onGraphicsSelectionChanged()
{
    // Snapshot the full current selection set, then (re)compute. SolidWorks-style:
    // we re-evaluate the whole set on every change rather than accumulating pairs.
    std::vector<GraphicsOwnerPtr> owners;
    m_guiDoc->graphicsScene()->foreachSelectedOwner([&](const GraphicsOwnerPtr& owner) {
        owners.push_back(owner);
    });
    m_vecSelectedOwner = owners;
    this->recompute(true);
}

void WidgetMeasure::recompute(bool addToHistory)
{
    auto gfxScene = m_guiDoc->graphicsScene();

    // Drop the previous on-screen callouts; they are rebuilt for the current set.
    this->clearAllMeasureDisplays();
    m_errorMessage.clear();
    m_resultText.clear();
    m_lastShort.clear();
    m_lastJson.clear();

    const std::vector<GraphicsOwnerPtr>& owners = m_vecSelectedOwner;
    if (owners.empty()) {
        gfxScene->redraw();
        this->updateMessagePanel();
        return;
    }

    // Owners -> raw shapes -> argos_core (the authoritative, Qt-free computation)
    std::vector<TopoDS_Shape> shapes;
    shapes.reserve(owners.size());
    for (const GraphicsOwnerPtr& owner : owners) {
        const TopoDS_Shape s = ownerToShape(owner);
        if (!s.IsNull())
            shapes.push_back(s);
    }

    argos::MeasureOptions opt;
    opt.showXyz = !m_checkShowXyz || m_checkShowXyz->isChecked();
    opt.pointToPoint = m_checkPointToPoint && m_checkPointToPoint->isChecked();

    const argos::MeasureResult res = argos::dispatch(shapes, opt);
    if (!res.ok) {
        m_errorMessage = QString::fromStdString(res.error);
    }
    else {
        m_resultText = formatArgosResult(res);
        m_lastShort = formatArgosShort(res);
        m_lastJson = QString::fromStdString(argos::to_json(res));
        if (addToHistory && m_historyList) {
            const QString line = formatArgosShort(res);
            const int n = m_historyList->count();
            if (!line.isEmpty() && (n == 0 || m_historyList->item(n - 1)->text() != line)) {
                m_historyList->addItem(line);
                constexpr int kMaxHistory = 200;
                while (m_historyList->count() > kMaxHistory)
                    delete m_historyList->takeItem(0);
            }
        }
    }

    // Reuse Mayo's mature on-screen callout for the simple 1- or 2-entity cases.
    // The numbers shown in the panel always come from argos_core; the callout is
    // only a graphical aid (best-effort).
    if (m_tool && res.ok) {
        const MeasureType mtype = mayoTypeFromKind(res.kind);
        if (mtype != MeasureType::None && (owners.size() == 1 || owners.size() == 2)) {
            try {
                const MeasureValue value =
                    owners.size() == 1 ?
                        IMeasureTool_computeValue(*m_tool, mtype, owners[0]) :
                        IMeasureTool_computeValue(*m_tool, mtype, owners[0], owners[1]);
                if (MeasureValue_isValid(value)) {
                    IMeasureDisplayPtr disp = BaseMeasureDisplay::createFrom(mtype, value);
                    if (disp) {
                        disp->update(this->currentMeasureDisplayConfig());
                        disp->adaptGraphics(gfxScene->v3dViewer()->Driver());
                        foreachGraphicsObject(disp, [=](const GraphicsObjectPtr& gfxObject) {
                            gfxScene->addObject(gfxObject, GraphicsScene::AddObjectDisableSelectionMode);
                        });
                        m_vecMeasureDisplay.push_back(std::move(disp));
                    }
                }
            }
            catch (const IMeasureError&) {
                // callout not available for this selection (e.g. cylindrical face)
            }
            catch (const Standard_Failure&) {
                // OCCT rejected the callout (Mayo's edge-only angle callout on faces
                // does TopoDS::Edge(face) -> throws); panel readout is authoritative
            }
            catch (...) {
                // never let a best-effort graphical callout crash the measure tool
            }
        }
    }

    gfxScene->redraw();
    this->updateMessagePanel();
}

void WidgetMeasure::onDocumentEntityAdded(TreeNodeId entityNodeId)
{
    // Keep newly added entities measurable: activate vertex/edge/face modes.
    auto gfxScene = m_guiDoc->graphicsScene();
    m_guiDoc->foreachGraphicsObject(entityNodeId, [=](const GraphicsObjectPtr& gfxObject) {
        if (GuiDocument::isAisViewCubeObject(gfxObject))
            return;

        for (TopAbs_ShapeEnum t : { TopAbs_VERTEX, TopAbs_EDGE, TopAbs_FACE })
            gfxScene->activateObjectSelection(gfxObject, AIS_Shape::SelectionMode(t));
    });
}

void WidgetMeasure::updateMessagePanel()
{
    // Clear message panel
    while (!m_ui->layout_Message->isEmpty()) {
        QLayoutItem* item = m_ui->layout_Message->takeAt(m_ui->layout_Message->count() - 1);
        delete item->widget();
        delete item;
    }

    // Error message takes precedence, then the argos result, then a hint.
    if (!m_errorMessage.isEmpty() || m_resultText.isEmpty()) {
        auto labelMessage = new QLabel(m_ui->widget_Message);
        labelMessage->setContentsMargins(m_ui->layout_Main->contentsMargins());
        m_ui->layout_Message->addWidget(labelMessage);
        const auto msgTextColorRole =
                m_errorMessage.isEmpty() ?
                    Theme::Color::MessageIndicator_InfoText :
                    Theme::Color::MessageIndicator_ErrorText;
        const auto msgBackgroundColorRole =
                m_errorMessage.isEmpty() ?
                    Theme::Color::MessageIndicator_InfoBackground :
                    Theme::Color::MessageIndicator_ErrorBackground;
        labelMessage->setStyleSheet(
                    QString("QLabel { color: %1; background-color: %2 }")
                    .arg(mayoTheme()->color(msgTextColorRole).name(),
                         mayoTheme()->color(msgBackgroundColorRole).name())
        );
        const QString msg = m_errorMessage.isEmpty()
                ? tr("측정할 형상을 선택하세요 (점 · 모서리 · 면)")
                : m_errorMessage;
        labelMessage->setText(msg);
    }
    else {
        // SolidWorks-style single readout for the whole selection set, computed
        // by argos_core.
        const QFont fontResult = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        auto label = new QLabel(m_resultText, m_ui->widget_Message);
        label->setFont(fontResult);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        label->setWordWrap(true);
        m_ui->layout_Message->addWidget(label);
    }

    emit this->sizeAdjustmentRequested();
}

void WidgetMeasure::eraseMeasureDisplay(const IMeasureDisplay* measure)
{
    if (!measure)
        return;

    auto it = std::find_if(
                m_vecMeasureDisplay.begin(),
                m_vecMeasureDisplay.end(),
                [=](const IMeasureDisplayPtr& ptr) { return ptr.get() == measure; }
    );
    if (it != m_vecMeasureDisplay.end()) {
        foreachGraphicsObject(measure, [=](const GraphicsObjectPtr& gfxObject) {
            m_guiDoc->graphicsScene()->eraseObject(gfxObject);
        });

        m_vecMeasureDisplay.erase(it);
    }
}

void WidgetMeasure::clearAllMeasureDisplays()
{
    auto gfxScene = m_guiDoc->graphicsScene();
    for (const IMeasureDisplayPtr& measure : m_vecMeasureDisplay) {
        foreachGraphicsObject(measure, [&](const GraphicsObjectPtr& gfxObject) {
            gfxScene->eraseObject(gfxObject);
        });
    }

    m_vecMeasureDisplay.clear();
    m_vecLinkGfxOwnerMeasure.clear();
}

void WidgetMeasure::addLink(const GraphicsOwnerPtr& owner, const IMeasureDisplayPtr& measure)
{
    if (owner && measure) {
        m_vecLinkGfxOwnerMeasure.push_back({ owner, measure.get() });
    }
}

void WidgetMeasure::eraseLink(const GraphicsOwner_MeasureDisplay* link)
{
    if (!link)
        return;

    m_vecLinkGfxOwnerMeasure.erase(m_vecLinkGfxOwnerMeasure.begin() + (link - &m_vecLinkGfxOwnerMeasure.front()));
}

const WidgetMeasure::GraphicsOwner_MeasureDisplay* WidgetMeasure::findLink(const GraphicsOwnerPtr& owner) const
{
    auto itFound = std::find_if(
        m_vecLinkGfxOwnerMeasure.begin(),
        m_vecLinkGfxOwnerMeasure.end(),
        [=](const GraphicsOwner_MeasureDisplay& link) { return link.gfxOwner == owner; }
    );
    return itFound != m_vecLinkGfxOwnerMeasure.end() ? &(*itFound) : nullptr;
}

} // namespace Mayo
