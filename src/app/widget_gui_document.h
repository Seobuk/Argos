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

class QMenu;

namespace Mayo {

class ButtonFlat;
class GuiDocument;
class IWidgetOccView;
class WidgetClipPlanes;
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

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    QWidget* createWidgetPanelContainer(QWidget* widgetContents);
    void updageWidgetPanelControls(QWidget* panelWidget, ButtonFlat* btnPanel);

    void toggleWidgetGrid(bool on);
    void toggleWidgetClipPlanes(bool on);
    void toggleWidgetExplode(bool on);
    void toggleWidgetMeasure(bool on);
    void toggleWidgetSection(bool on);
    void exclusiveButtonCheck(const ButtonFlat* btn);

    void createMenuViewProjections(QWidget* container);
    void createMenuItemVisibility(QWidget* container);
    // Argos: right-click context menu to snap the current view to a standard
    // orientation (정면/후면/좌·우/윗·아랫면/등각).
    void popupViewOrientationMenu(const QPoint& globalPos);
    QRect viewControlsRect() const;
    void layoutViewControls();
    void layoutWidgetPanel(QWidget* panel);
    void layoutWidgetPanels();

    ButtonFlat* createViewBtn(QWidget* parent, Theme::Icon icon, const QString& tooltip) const;
    QMenu* createViewMenu(QWidget* parent) const;
    static void popupViewMenu(QMenu* menu, const ButtonFlat* btnMenu, const QWidget* container);

    GuiDocument* m_guiDoc = nullptr;
    IWidgetOccView* m_qtOccView = nullptr;
    QWidget* m_widgetBtns = nullptr;
    WidgetOccViewController* m_controller = nullptr;
    WidgetClipPlanes* m_widgetClipPlanes = nullptr;
    WidgetExplodeAssembly* m_widgetExplodeAsm = nullptr;
    WidgetGrid* m_widgetGrid = nullptr;
    WidgetMeasure* m_widgetMeasure = nullptr;
    WidgetSection* m_widgetSection = nullptr;
    QRect m_rectControls;

    ButtonFlat* m_btnFitAll = nullptr;
    ButtonFlat* m_btnGrid = nullptr;
    ButtonFlat* m_btnEditClipping = nullptr;
    ButtonFlat* m_btnExplode = nullptr;
    ButtonFlat* m_btnMeasure = nullptr;
    ButtonFlat* m_btnSection = nullptr;
};

} // namespace Mayo
