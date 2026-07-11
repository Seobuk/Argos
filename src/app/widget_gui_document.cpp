/****************************************************************************
** Copyright (c) 2016, Fougue SAS <https://www.fougue.pro>
** SPDX-License-Identifier: BSD-2-Clause
****************************************************************************/

#include "widget_gui_document.h"

#include "../graphics/graphics_object_driver.h"
#include "../graphics/graphics_utils.h"
#include "../gui/gui_application.h"
#include "../gui/gui_document.h"
#include "../gui/v3d_view_camera_animation.h"
#include "../qtbackend/qt_animation_backend.h"
#include "button_flat.h"
#include "theme.h"
#include "widget_explode_assembly.h"
#include "widget_grid.h"
#include "widget_measure.h"
#include "widget_section.h"
#include "widget_occ_view.h"
#include "widget_occ_view_controller.h"
#include "qtwidgets_utils.h"
#include "qtgui_utils.h"
#include "../qtcommon/qstring_conv.h"

#include <QtGui/QCursor>
#include <QtGui/QPainter>
#include <QtGui/QGuiApplication>
#include <QtGui/QShortcut>
#include <QActionGroup> // WARNING Qt5 <QtWidgets/...> / Qt6 <QtGui/...>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QColorDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QProxyStyle>
#include <QtWidgets/QWidgetAction>
#include <V3d_View.hxx>
#include <Standard_Version.hxx>

namespace Mayo {

namespace {

// Provides an overlay widget to be used within 3D view
class PanelView3d : public QWidget {
public:
    using QWidget::QWidget; // Inherit QWidget constructors

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        const QRect frame = this->frameGeometry();
        const QRect surface(0, 0, frame.width(), frame.height());
        auto widgetGuiDocument = static_cast<const WidgetGuiDocument*>(this->parentWidget());
        painter.fillRect(surface, widgetGuiDocument->panelBackgroundColor());
    }    
};

// Provides style to redefine icon size of menu items
class MenuIconSizeStyle : public QProxyStyle {
public:
    void setMenuIconSize(int size) {
        m_menuIconSize = size;
    }

    int pixelMetric(QStyle::PixelMetric metric, const QStyleOption* option, const QWidget* widget) const override
    {
        if (metric == QStyle::PM_SmallIconSize && m_menuIconSize > 0)
            return m_menuIconSize;

        return QProxyStyle::pixelMetric(metric, option, widget);
    }

private:
    int m_menuIconSize = -1;
};

// Default margin to be used in widgets
const int Internal_widgetMargin = 4;

} // namespace

WidgetGuiDocument::WidgetGuiDocument(GuiDocument* guiDoc, QWidget* parent)
    : QWidget(parent),
      m_guiDoc(guiDoc),
      m_qtOccView(IWidgetOccView::create(guiDoc->v3dView(), this)),
      m_controller(new WidgetOccViewController(m_qtOccView))
{
    {
        auto layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(m_qtOccView->widget());
    }

    auto widgetBtnsContents = new QWidget;
    auto layoutBtns = new QHBoxLayout(widgetBtnsContents);
    layoutBtns->setSpacing(0);
    layoutBtns->setContentsMargins(QMargins{0, 0, 0, 0});

    m_btnFitAll = this->createViewBtn(widgetBtnsContents, Theme::Icon::Expand, tr("화면 맞춤 (전체 보기)"));

    m_btnGrid = this->createViewBtn(widgetBtnsContents, Theme::Icon::Grid, tr("그리드 편집"));
    m_btnGrid->setCheckable(true);

    m_btnExplode = this->createViewBtn(widgetBtnsContents, Theme::Icon::Multiple, tr("조립체 분해"));
    m_btnExplode->setCheckable(true);

    // Argos: Inventor-style "구성요소 개별 색상" — paints every part of the assembly
    // in its own distinct color, display-only (the document/XCAF materials and
    // colors are untouched, so exports and part data stay exactly as loaded).
    m_btnPartColors = this->createViewBtn(
        widgetBtnsContents, Theme::Icon::Palette,
        tr("구성요소 개별 색상 — 부품마다 고유한 색으로 구분 (화면 표시 전용, 재질/데이터 불변)")
    );
    m_btnPartColors->setCheckable(true);

    m_btnMeasure = this->createViewBtn(widgetBtnsContents, Theme::Icon::Measure, tr("형상 측정 — 점·모서리·면 클릭 (단축키 M)"));
    m_btnMeasure->setCheckable(true);

    m_btnSection = this->createViewBtn(widgetBtnsContents, Theme::Icon::ClipPlane, tr("단면 — XY/YZ/ZX 절단 + 캡"));
    m_btnSection->setCheckable(true);

    layoutBtns->addWidget(m_btnFitAll);
    this->createMenuViewProjections(widgetBtnsContents);
    this->createMenuItemVisibility(widgetBtnsContents);
    layoutBtns->addWidget(m_btnGrid);
    layoutBtns->addWidget(m_btnExplode);
    layoutBtns->addWidget(m_btnPartColors);
    layoutBtns->addWidget(m_btnMeasure);
    layoutBtns->addWidget(m_btnSection);
    m_widgetBtns = this->createWidgetPanelContainer(widgetBtnsContents);

    // Argos: always-on 3D mouse-coordinate readout, anchored at the bottom-right
    // corner of the view (previously shown in the top document bar, hover-only).
    {
        auto coordsContents = new QWidget;
        auto layoutCoords = new QHBoxLayout(coordsContents);
        layoutCoords->setSpacing(0);
        layoutCoords->setContentsMargins(QMargins{4, 1, 4, 1});

        m_labelHoverMeasure = new QLabel(coordsContents);
        m_labelHoverMeasure->setStyleSheet(QStringLiteral("color: palette(highlight); font-weight: 600;"));
        layoutCoords->addWidget(m_labelHoverMeasure);
        layoutCoords->addSpacing(18);

        m_labelMouseCoords = new QLabel(coordsContents);
        m_labelMouseCoords->setText(QStringLiteral("X=?  Y=?  Z=?"));
        layoutCoords->addWidget(m_labelMouseCoords);

        m_widgetMouseCoords = this->createWidgetPanelContainer(coordsContents);
    }

    auto gfxScene = m_guiDoc->graphicsScene();
    gfxScene->signalRedrawRequested.connectSlot([=](const OccHandle<V3d_View>& view) {
        if (view == m_qtOccView->v3dView())
            m_qtOccView->redraw();
    });
    QObject::connect(m_btnFitAll, &ButtonFlat::clicked, this, [this]{ this->viewFitAll(); });
    QObject::connect(m_btnGrid, &ButtonFlat::checked, this, &WidgetGuiDocument::toggleWidgetGrid);
    QObject::connect(m_btnExplode, &ButtonFlat::checked, this, &WidgetGuiDocument::toggleWidgetExplode);
    QObject::connect(m_btnMeasure, &ButtonFlat::checked, this, &WidgetGuiDocument::toggleWidgetMeasure);
    QObject::connect(m_btnSection, &ButtonFlat::checked, this, &WidgetGuiDocument::toggleWidgetSection);
    // Display-only toggle: independent of the exclusive tool-panel buttons, so it
    // combines freely with measure/section/explode.
    QObject::connect(m_btnPartColors, &ButtonFlat::checked, this, [=](bool on) {
        m_guiDoc->setUniquePartColorsOn(on);
    });

    // Argos: keyboard shortcut "M" toggles the Measure tool. Scoped to the 3D
    // view (WidgetWithChildren) so it never hijacks typing elsewhere in the app.
    {
        auto scMeasure = new QShortcut(QKeySequence(Qt::Key_M), this);
        scMeasure->setContext(Qt::WidgetWithChildrenShortcut);
        QObject::connect(scMeasure, &QShortcut::activated, this, [this]{
            if (m_btnMeasure)
                m_btnMeasure->setChecked(!m_btnMeasure->isChecked());
        });
    }

    m_controller->signalDynamicActionStarted.connectSlot([=]{ m_guiDoc->stopViewCameraAnimation(); });
    m_controller->signalViewScaled.connectSlot([=]{ m_guiDoc->stopViewCameraAnimation(); });
    m_controller->signalMouseButtonClicked.connectSlot([=](Aspect_VKeyMouse btn) {
        if (btn == Aspect_VKeyMouse_LeftButton && !m_guiDoc->processAction(gfxScene->currentHighlightedOwner())) {
            gfxScene->select();
            m_qtOccView->redraw();
        }
        // Argos: right-click (without dragging) pops up the standard-view menu so
        // the user can snap the current view to 정면/윗면/… .
        else if (btn == Aspect_VKeyMouse_RightButton) {
            this->popupViewContextMenu(QCursor::pos());
        }
    });
    m_controller->signalMultiSelectionToggled.connectSlot([=](bool on) {
        auto mode = on ? GraphicsScene::SelectionMode::Multi : GraphicsScene::SelectionMode::Single;
        gfxScene->setSelectionMode(mode);
    });

    m_guiDoc->viewCameraAnimation()->setBackend(std::make_unique<QtAnimationBackend>(QEasingCurve::OutExpo));
    m_guiDoc->viewCameraAnimation()->setRenderFunction([=](const OccHandle<V3d_View>& view){
        if (view == m_qtOccView->v3dView())
            m_qtOccView->redraw();
    });
    m_guiDoc->signalViewTrihedronCornerChanged.connectSlot([=](Aspect_TypeOfTriedronPosition) {
        this->layoutViewControls();
        this->layoutWidgetPanels();
        m_guiDoc->graphicsView().redraw();
    });
}

WidgetGuiDocument::~WidgetGuiDocument() = default;

Document::Identifier WidgetGuiDocument::documentIdentifier() const
{
    return m_guiDoc->document()->identifier();
}

QColor WidgetGuiDocument::panelBackgroundColor() const
{
    QColor color = mayoTheme()->color(Theme::Color::Palette_Window);
    if (m_qtOccView->supportsWidgetOpacity())
        color.setAlpha(175);

    return color;
}

void WidgetGuiDocument::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    this->layoutViewControls();
    this->layoutWidgetPanels();
    this->layoutMouseCoords();
}

QWidget* WidgetGuiDocument::createWidgetPanelContainer(QWidget* widgetContents)
{
    auto panel = new PanelView3d(this);
    QtWidgetsUtils::addContentsWidget(panel, widgetContents);
    panel->show();
    panel->adjustSize();
    return panel;
}

void WidgetGuiDocument::updageWidgetPanelControls(QWidget* panelWidget, ButtonFlat* btnPanel)
{
    this->exclusiveButtonCheck(btnPanel);
    if (panelWidget)
        panelWidget->parentWidget()->setVisible(btnPanel->isChecked());

    // Re-layout all panels: showing/hiding one shifts its side-by-side neighbors.
    this->layoutWidgetPanels();
}

void adjustWidgetSize(QWidget* widget)
{
    widget->updateGeometry();
    if (static_cast<const PanelView3d*>(widget->parentWidget()))
        widget->parentWidget()->adjustSize();
}

void WidgetGuiDocument::toggleWidgetGrid(bool on)
{
    if (!m_widgetGrid && on) {
        m_widgetGrid = new WidgetGrid(m_guiDoc->graphicsView());
        auto container = this->createWidgetPanelContainer(m_widgetGrid);
        QObject::connect(
            m_widgetGrid, &WidgetGrid::sizeAdjustmentRequested,
            container, [=]{ adjustWidgetSize(m_widgetGrid); this->layoutWidgetPanels(); },
            Qt::QueuedConnection
        );
    }

    this->updageWidgetPanelControls(m_widgetGrid, m_btnGrid);
}

void WidgetGuiDocument::toggleWidgetSection(bool on)
{
    if (!m_widgetSection && on) {
        m_widgetSection = new WidgetSection(m_guiDoc);
        auto container = this->createWidgetPanelContainer(m_widgetSection);
        QObject::connect(
            m_widgetSection, &WidgetSection::sizeAdjustmentRequested,
            container, [=]{ adjustWidgetSize(m_widgetSection); this->layoutWidgetPanels(); },
            Qt::QueuedConnection
        );
        m_guiDoc->signalGraphicsBoundingBoxChanged.connectSlot(&WidgetSection::setRanges, m_widgetSection);
        m_widgetSection->setRanges(m_guiDoc->graphicsBoundingBox());
    }

    if (m_widgetSection)
        m_widgetSection->setSectionOn(on);

    this->updageWidgetPanelControls(m_widgetSection, m_btnSection);
}

void WidgetGuiDocument::toggleWidgetExplode(bool on)
{
    if (!m_widgetExplodeAsm && on) {
        m_widgetExplodeAsm = new WidgetExplodeAssembly(m_guiDoc);
        this->createWidgetPanelContainer(m_widgetExplodeAsm);
    }

    this->updageWidgetPanelControls(m_widgetExplodeAsm, m_btnExplode);
}

void WidgetGuiDocument::toggleWidgetMeasure(bool on)
{
    if (!m_widgetMeasure && on) {
        m_widgetMeasure = new WidgetMeasure(m_guiDoc);
        auto container = this->createWidgetPanelContainer(m_widgetMeasure);
        QObject::connect(
            m_widgetMeasure, &WidgetMeasure::sizeAdjustmentRequested,
            container, [=]{ adjustWidgetSize(m_widgetMeasure); this->layoutWidgetPanels(); },
            Qt::QueuedConnection
        );
    }

    if (m_widgetMeasure)
        m_widgetMeasure->setMeasureOn(on);

    this->updageWidgetPanelControls(m_widgetMeasure, m_btnMeasure);
}

void WidgetGuiDocument::exclusiveButtonCheck(const ButtonFlat* btnCheck)
{
    if (!btnCheck || !btnCheck->isChecked())
        return;

    // Argos: Measure and Section are allowed to be active at the same time, so
    // measurements can be taken directly on the sectioned model (섹션뷰에서 측정).
    // Unchecking a ButtonFlat fires its `checked` signal, which would otherwise
    // turn the section clip plane off the moment the measure tool opens.
    auto fnCompatible = [=](const ButtonFlat* a, const ButtonFlat* b) {
        return (a == m_btnMeasure && b == m_btnSection)
            || (a == m_btnSection && b == m_btnMeasure);
    };

    ButtonFlat* arrayToggleBtn[] = { m_btnGrid, m_btnExplode, m_btnMeasure, m_btnSection };
    for (ButtonFlat* btn : arrayToggleBtn) {
        assert(btn->isCheckable());
        if (btn != btnCheck && !fnCompatible(btnCheck, btn))
            btn->setChecked(false);
    }
}

void WidgetGuiDocument::layoutWidgetPanels()
{
    // Argos: Measure and Section can be open simultaneously, so the visible
    // panels are laid out side by side (left to right, in toolbar-button order)
    // below the view controls instead of all stacking on the same spot.
    const QRect ctrlRect = this->viewControlsRect();
    int xPos = ctrlRect.left();
    QWidget* widgetPanels[] = {
        m_widgetGrid, m_widgetExplodeAsm, m_widgetMeasure, m_widgetSection
    };
    for (QWidget* panel : widgetPanels) {
        QWidget* container = panel ? panel->parentWidget() : nullptr;
        if (!container)
            continue;

        const int margin = panel->devicePixelRatio() * Internal_widgetMargin;
        container->move(xPos, ctrlRect.bottom() + margin);
        if (container->isVisible())
            xPos += container->frameGeometry().width() + margin;
    }
}

ButtonFlat* WidgetGuiDocument::createViewBtn(QWidget* parent, Theme::Icon icon, const QString& tooltip) const
{
    const QColor bkgndColor =
        m_qtOccView->supportsWidgetOpacity() ?
            Qt::transparent :
            mayoTheme()->color(Theme::Color::ButtonView3d_Background)
    ;

    auto btn = new ButtonFlat(parent);
    const double pxRatio = btn->devicePixelRatio();
    btn->setBackgroundBrush(bkgndColor);
    btn->setCheckedBrush(mayoTheme()->color(Theme::Color::ButtonView3d_Checked));
    btn->setHoverBrush(mayoTheme()->color(Theme::Color::ButtonView3d_Hover));
    btn->setIcon(mayoTheme()->icon(icon));
    btn->setIconSize(pxRatio * QSize{24, 24});
    btn->setFixedSize(pxRatio * QSize{40, 40});
    btn->setToolTip(tooltip);
    return btn;
}

QMenu* WidgetGuiDocument::createViewMenu(QWidget* parent) const
{
    static MenuIconSizeStyle* menuStyle = nullptr;
    if (!menuStyle) {
        menuStyle = new MenuIconSizeStyle;
        menuStyle->setMenuIconSize(m_btnFitAll->iconSize().width());
    }

    auto menu = new QMenu(parent);
    menu->setStyle(menuStyle);
    const QString strPanelBkgndColor = this->panelBackgroundColor().name(QColor::HexArgb);
    menu->setStyleSheet(QString("QMenu { background:%1; border: 0px }").arg(strPanelBkgndColor));
    menu->setWindowFlags(menu->windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    if (m_qtOccView->supportsWidgetOpacity())
        menu->setAttribute(Qt::WA_TranslucentBackground);

    return menu;
}

void WidgetGuiDocument::popupViewMenu(QMenu* menu, const ButtonFlat* menuBtn, const QWidget* container)
{
    menu->popup(menuBtn->mapToGlobal(QPoint{0, container->height()}));
}

void WidgetGuiDocument::createMenuViewProjections(QWidget* container)
{
    struct ButtonCreationData {
        V3d_TypeOfOrientation proj;
        Theme::Icon icon;
        QString text;
    };
    const ButtonCreationData btnCreationData[] = {
        { V3d_XposYnegZpos, Theme::Icon::View3dIso, tr("Isometric") },
        { V3d_Ypos, Theme::Icon::View3dBack,   tr("Back")},
        { V3d_Yneg, Theme::Icon::View3dFront,  tr("Front") },
        { V3d_Xneg, Theme::Icon::View3dLeft,   tr("Left") },
        { V3d_Xpos, Theme::Icon::View3dRight,  tr("Right") },
        { V3d_Zpos, Theme::Icon::View3dTop,    tr("Top") },
        { V3d_Zneg, Theme::Icon::View3dBottom, tr("Bottom") }
    };

    const QString strTemplateTooltip =
        tr("<b>Left-click</b>: popup menu of pre-defined views\n"
           "<b>CTRL+Left-click</b>: apply '%1' view");
    auto menuBtn = this->createViewBtn(container, Theme::Icon::View3dIso, QString{});
    container->layout()->addWidget(menuBtn);
    menuBtn->setToolTip(strTemplateTooltip.arg(btnCreationData[0].text));
    menuBtn->setData(static_cast<int>(btnCreationData[0].proj));
    auto menu = this->createViewMenu(menuBtn);

    for (const ButtonCreationData& btnData : btnCreationData) {
        auto action = menu->addAction(mayoTheme()->icon(btnData.icon), btnData.text);
        QObject::connect(action, &QAction::triggered, this, [=]{
            m_guiDoc->setViewCameraOrientation(btnData.proj, GuiDocument::ViewOrientationFlag_FitAll);
            menuBtn->setIcon(action->icon());
            menuBtn->setToolTip(strTemplateTooltip.arg(btnData.text));
            menuBtn->setData(int(btnData.proj));
            menuBtn->update();
        });
    }

    //QStyle::PE_IndicatorArrowDown
    QObject::connect(menuBtn, &ButtonFlat::clicked, this, [=]{
        if (!QGuiApplication::queryKeyboardModifiers().testFlag(Qt::ControlModifier)) {
            WidgetGuiDocument::popupViewMenu(menu, menuBtn, container);
        }
        else {
            auto orientation = V3d_TypeOfOrientation(menuBtn->data().toInt());
            m_guiDoc->setViewCameraOrientation(orientation, GuiDocument::ViewOrientationFlag_FitAll);
        }
    });
}

void WidgetGuiDocument::viewFitAll()
{
    m_guiDoc->runViewCameraAnimation([=](OccHandle<V3d_View> view) {
        auto bndBoxFlags = GuiDocument::OnlySelectedGraphics | GuiDocument::OnlyVisibleGraphics;
        GraphicsUtils::V3dView_fitAll(view, this->guiDocument()->graphicsBoundingBox(bndBoxFlags));
    });
}

void WidgetGuiDocument::popupViewContextMenu(const QPoint& globalPos)
{
    struct OrientationData {
        V3d_TypeOfOrientation proj;
        Theme::Icon icon;
        QString text;
    };
    const OrientationData items[] = {
        { V3d_Yneg, Theme::Icon::View3dFront,  tr("정면 (Front)") },
        { V3d_Ypos, Theme::Icon::View3dBack,   tr("후면 (Back)") },
        { V3d_Xneg, Theme::Icon::View3dLeft,   tr("좌측 (Left)") },
        { V3d_Xpos, Theme::Icon::View3dRight,  tr("우측 (Right)") },
        { V3d_Zpos, Theme::Icon::View3dTop,    tr("윗면 (Top)") },
        { V3d_Zneg, Theme::Icon::View3dBottom, tr("아랫면 (Bottom)") },
        { V3d_XposYnegZpos, Theme::Icon::View3dIso, tr("등각 (Isometric)") }
    };

    auto menu = this->createViewMenu(this);
    menu->setTitle(tr("보는 면 바꾸기"));
    auto header = menu->addAction(tr("보는 면 바꾸기"));
    header->setEnabled(false);
    menu->addSeparator();
    for (const OrientationData& it : items) {
        auto action = menu->addAction(mayoTheme()->icon(it.icon), it.text);
        const V3d_TypeOfOrientation proj = it.proj;
        QObject::connect(action, &QAction::triggered, this, [=]{
            m_guiDoc->setViewCameraOrientation(proj, GuiDocument::ViewOrientationFlag_FitAll);
        });
    }

    // Argos: fit-all
    menu->addSeparator();
    auto actionFitAll = menu->addAction(mayoTheme()->icon(Theme::Icon::Expand), tr("화면 맞춤"));
    QObject::connect(actionFitAll, &QAction::triggered, this, [this]{ this->viewFitAll(); });

    // Argos: display-mode submenu (음영/와이어프레임/HLR)
    const auto spanDrivers = m_guiDoc->guiApplication()->graphicsObjectDrivers();
    QMenu* menuDisplayMode = nullptr;
    for (const GraphicsObjectDriverPtr& driver : spanDrivers) {
        if (driver->displayModes().empty())
            continue;

        if (!menuDisplayMode)
            menuDisplayMode = menu->addMenu(tr("표시 모드"));

        auto group = new QActionGroup(menuDisplayMode);
        group->setExclusive(true);
        for (const Enumeration::Item& displayMode : driver->displayModes().items()) {
            auto action = new QAction(to_QString(displayMode.name.tr()), menuDisplayMode);
            action->setCheckable(true);
            action->setData(displayMode.value);
            menuDisplayMode->addAction(action);
            group->addAction(action);
            if (displayMode.value == m_guiDoc->activeDisplayMode(driver))
                action->setChecked(true);
        }

        QObject::connect(group, &QActionGroup::triggered, this, [=](const QAction* action) {
            m_guiDoc->setActiveDisplayMode(driver, action->data().toInt());
            m_qtOccView->redraw();
        });
    }

    // Argos: background color
    auto actionBkgnd = menu->addAction(tr("배경색..."));
    QObject::connect(actionBkgnd, &QAction::triggered, this, [this]{
        auto dlg = new QColorDialog(this);
        QObject::connect(dlg, &QColorDialog::colorSelected, this, [this](const QColor& c) {
            m_guiDoc->v3dView()->SetBackgroundColor(QtGuiUtils::toPreferredColorSpace(c));
            m_qtOccView->redraw();
        });
        QtWidgetsUtils::asyncDialogExec(dlg);
    });

    // Argos: quick toggle to leave section mode when it's active
    if (m_btnSection && m_btnSection->isChecked()) {
        menu->addSeparator();
        auto actionSectionOff = menu->addAction(tr("Section 끄기"));
        QObject::connect(actionSectionOff, &QAction::triggered, this, [this]{
            m_btnSection->setChecked(false);
        });
    }

    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->popup(globalPos);
}

void WidgetGuiDocument::createMenuItemVisibility(QWidget* container)
{
    auto menuBtn = this->createViewBtn(container, Theme::Icon::VisibilityMenu, QString{});
    container->layout()->addWidget(menuBtn);
    menuBtn->setToolTip(tr("Show/hide items"));
    auto menu = this->createViewMenu(menuBtn);

    // Helper function to make addition of QMenu actions more straightforward
    auto fnAddAction = [=](Theme::Icon icon, const QString& text) {
        return menu->addAction(mayoTheme()->icon(icon), text);
    };
    // Create menu actions
    auto actionShowAll = fnAddAction(Theme::Icon::VisibilityShowAll, tr("Show all"));
    auto actionShowSel = fnAddAction(Theme::Icon::VisibilityShowSelection, tr("Show selection"));
    auto actionHideSel = fnAddAction(Theme::Icon::VisibilityHideSelection, tr("Hide selection"));
    auto actionShowSelOnly = fnAddAction(Theme::Icon::VisibilityShowSelectionOnly, tr("Show only selection"));

    const DocumentPtr doc = m_guiDoc->document();
    // Helper function to get the selected tree nodes(ids) in the document
    auto fnGetDocSelectedNodeIds = [=]() -> std::vector<TreeNodeId> {
        std::vector<TreeNodeId> nodeIds;
        const GuiApplication* guiApp = m_guiDoc->guiApplication();
        for (const ApplicationItem& appItem : guiApp->selectionModel()->selectedItems()) {
            if (appItem.isDocumentTreeNode() && appItem.document() == m_guiDoc->document())
                nodeIds.push_back(appItem.documentTreeNode().id());
        }
        return nodeIds;
    };

    // "Show all" action
    QObject::connect(actionShowAll, &QAction::triggered, this, [=]{
        for (TreeNodeId nodeId : doc->allEntityNodeIds())
            m_guiDoc->setNodeVisible(nodeId, true);
        m_guiDoc->graphicsScene()->redraw();
    });

    // "Show selection" action
    QObject::connect(actionShowSel, &QAction::triggered, this, [=]{
        for (TreeNodeId nodeId : fnGetDocSelectedNodeIds())
            m_guiDoc->setNodeVisible(nodeId, true);
        m_guiDoc->graphicsScene()->redraw();
    });

    // "Hide selection" action
    QObject::connect(actionHideSel, &QAction::triggered, this, [=]{
        for (TreeNodeId nodeId : fnGetDocSelectedNodeIds())
            m_guiDoc->setNodeVisible(nodeId, false);
        m_guiDoc->graphicsScene()->redraw();
    });

    // "Show selection only" action
    QObject::connect(actionShowSelOnly, &QAction::triggered, this, [=]{
        // Hide all entities(root nodes)
        for (TreeNodeId nodeId : doc->allEntityNodeIds())
            m_guiDoc->setNodeVisible(nodeId, false);

        // Show selected document nodes
        for (TreeNodeId nodeId : fnGetDocSelectedNodeIds())
            m_guiDoc->setNodeVisible(nodeId, true);

        m_guiDoc->graphicsScene()->redraw();
    });

    // Popup menu
    QObject::connect(menuBtn, &ButtonFlat::clicked, this, [=]{
        const bool allEntityVisible = std::all_of(
            doc->allEntityNodeIds().begin(), doc->allEntityNodeIds().end(),
            [=](TreeNodeId nodeId) { return m_guiDoc->nodeVisibleState(nodeId) == CheckState::On; }
        );
        const auto vecSelectedNodeId = fnGetDocSelectedNodeIds();
        const bool allSelectedItemVisible = std::all_of(
            vecSelectedNodeId.cbegin(), vecSelectedNodeId.cend(),
            [=](TreeNodeId nodeId) { return m_guiDoc->nodeVisibleState(nodeId) == CheckState::On; }
        );
        const bool allSelectedItemHidden = std::all_of(
            vecSelectedNodeId.cbegin(), vecSelectedNodeId.cend(),
            [=](TreeNodeId nodeId) { return m_guiDoc->nodeVisibleState(nodeId) == CheckState::Off; }
        );

        actionShowAll->setEnabled(!allEntityVisible);
        actionShowSel->setEnabled(!allSelectedItemVisible);
        actionHideSel->setEnabled(!allSelectedItemHidden);
        actionShowSelOnly->setEnabled(!vecSelectedNodeId.empty());
        WidgetGuiDocument::popupViewMenu(menu, menuBtn, container);
    });
}

QRect WidgetGuiDocument::viewControlsRect() const
{
    return m_widgetBtns->frameGeometry();
}

void WidgetGuiDocument::layoutViewControls()
{
    const int margin = this->devicePixelRatio() * 2 * Internal_widgetMargin;
    auto fnGetViewControlsPos = [=]() -> QPoint {
        if (m_guiDoc->viewTrihedronMode() == GuiDocument::ViewTrihedronMode::AisViewCube) {
            const int viewCubeBndSize = m_guiDoc->aisViewCubeBoundingSize() / m_guiDoc->devicePixelRatio();
            if (m_guiDoc->viewTrihedronCorner() == Aspect_TOTP_LEFT_UPPER)
                return { margin, viewCubeBndSize + margin };
        }

        return { margin, margin };
    };

    m_widgetBtns->move(fnGetViewControlsPos());
}

void WidgetGuiDocument::layoutMouseCoords()
{
    if (!m_widgetMouseCoords)
        return;

    m_widgetMouseCoords->adjustSize();
    const int margin = this->devicePixelRatio() * 2 * Internal_widgetMargin;
    const QSize sz = m_widgetMouseCoords->frameSize();
    m_widgetMouseCoords->move(this->width() - sz.width() - margin, this->height() - sz.height() - margin);
    m_widgetMouseCoords->raise();
}

void WidgetGuiDocument::updateMouseCoords(double x, double y, double z, const QString& hoverText)
{
    if (!m_widgetMouseCoords)
        return;

    m_labelHoverMeasure->setText(hoverText);
    m_labelMouseCoords->setText(
        QStringLiteral("X=%1  Y=%2  Z=%3").arg(
            QString::number(x, 'f', 3), QString::number(y, 'f', 3), QString::number(z, 'f', 3)
        )
    );
    // Content width changes with the values/hover text, so re-anchor to the corner.
    this->layoutMouseCoords();
}

} // namespace Mayo
