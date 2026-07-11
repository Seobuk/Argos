/****************************************************************************
** Copyright (c) 2016, Fougue SAS <https://www.fougue.pro>
** SPDX-License-Identifier: BSD-2-Clause
****************************************************************************/

#pragma once

#include "../base/document.h"
#include "theme.h"
#include "widget_occ_view_controller.h"

#include <QtWidgets/QWidget>
#include <V3d_TypeOfOrientation.hxx>

class QLabel;
class QMenu;

namespace Mayo {

class ButtonFlat;
class GuiDocument;
class IWidgetOccView;
class WidgetExplodeAssembly;
class WidgetGrid;
class WidgetMeasure;
class WidgetSection;

// QWidget providing user-interaction with a GuiDocument object
class WidgetGuiDocument : public QWidget {
    Q_OBJECT
public:
    WidgetGuiDocument(GuiDocument* guiDoc, QWidget* parent = nullptr);
    ~WidgetGuiDocument();

    GuiDocument* guiDocument() const { return m_guiDoc; }
    WidgetOccViewController* controller() const { return m_controller; }
    IWidgetOccView* view() const { return m_qtOccView; }

    Document::Identifier documentIdentifier() const;

    QColor panelBackgroundColor() const;

    // Argos: update the always-on 3D mouse-coordinate readout anchored at the
    // bottom-right corner of the view.
    void updateMouseCoords(double x, double y, double z, const QString& hoverText);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    QWidget* createWidgetPanelContainer(QWidget* widgetContents);
    void updageWidgetPanelControls(QWidget* panelWidget, ButtonFlat* btnPanel);

    void toggleWidgetGrid(bool on);
    void toggleWidgetExplode(bool on);
    void toggleWidgetMeasure(bool on);
    void toggleWidgetSection(bool on);
    void exclusiveButtonCheck(const ButtonFlat* btn);

    void createMenuViewProjections(QWidget* container);
    void createMenuItemVisibility(QWidget* container);
    // Argos: right-click context menu — standard-view snaps plus fit-all,
    // display mode and background-color entries.
    void popupViewContextMenu(const QPoint& globalPos);
    // Argos: fit the whole model into the view (shared by the toolbar button and
    // the right-click context menu).
    void viewFitAll();
    QRect viewControlsRect() const;
    void layoutViewControls();
    void layoutWidgetPanels();
    // Argos: re-anchor the mouse-coordinate readout to the view bottom-right.
    void layoutMouseCoords();

    ButtonFlat* createViewBtn(QWidget* parent, Theme::Icon icon, const QString& tooltip) const;
    QMenu* createViewMenu(QWidget* parent) const;
    static void popupViewMenu(QMenu* menu, const ButtonFlat* btnMenu, const QWidget* container);

    GuiDocument* m_guiDoc = nullptr;
    IWidgetOccView* m_qtOccView = nullptr;
    QWidget* m_widgetBtns = nullptr;
    QWidget* m_widgetMouseCoords = nullptr; // Argos: bottom-right coordinate readout
    QLabel* m_labelHoverMeasure = nullptr;
    QLabel* m_labelMouseCoords = nullptr;
    WidgetOccViewController* m_controller = nullptr;
    WidgetExplodeAssembly* m_widgetExplodeAsm = nullptr;
    WidgetGrid* m_widgetGrid = nullptr;
    WidgetMeasure* m_widgetMeasure = nullptr;
    WidgetSection* m_widgetSection = nullptr;
    QRect m_rectControls;

    ButtonFlat* m_btnFitAll = nullptr;
    ButtonFlat* m_btnGrid = nullptr;
    ButtonFlat* m_btnExplode = nullptr;
    ButtonFlat* m_btnPartColors = nullptr; // Argos: per-part unique display colors
    ButtonFlat* m_btnMeasure = nullptr;
    ButtonFlat* m_btnSection = nullptr;
};

} // namespace Mayo
