/****************************************************************************
** Copyright (c) 2016, Fougue SAS <https://www.fougue.pro>
** SPDX-License-Identifier: BSD-2-Clause
****************************************************************************/

#include "commands_tools.h"

#include "../argos_core/drawing.h"
#include "../base/application.h"
#include "../base/application_item_selection_model.h"
#include "../base/document.h"
#include "../gui/gui_application.h"
#include "../gui/gui_document.h"
#include "app_module.h"
#include "dialog_export_drawing.h"
#include "dialog_inspect_xde.h"
#include "dialog_options.h"
#include "dialog_save_image_view.h"
#include "qtwidgets_utils.h"
#include "theme.h"
#include "../qtcommon/filepath_conv.h"

#include <BRep_Builder.hxx>
#include <TDF_Label.hxx>
#include <TDF_LabelSequence.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shape.hxx>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QStringList>
#include <QtGui/QCursor>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QWidget>

#include <string>
#include <utility>
#include <vector>

namespace Mayo {

CommandSaveViewImage::CommandSaveViewImage(IAppContext* context)
    : Command(context)
{
    auto action = this->createAction();
    action->setText(Command::tr("Save View to Image"));
    action->setToolTip(Command::tr("Save View to Image"));
    action->setIcon(mayoTheme()->icon(Theme::Icon::Camera));
}

void CommandSaveViewImage::execute()
{
    auto guiDoc = this->currentGuiDocument();
    auto dlg = new DialogSaveImageView(guiDoc->v3dView(), this->widgetMain());
    QtWidgetsUtils::asyncDialogExec(dlg);
}

bool CommandSaveViewImage::getEnabledStatus() const
{
    return this->app()->documentCount() != 0
           && this->context()->currentPage() == IAppContext::Page::Documents;
}

CommandInspectXde::CommandInspectXde(IAppContext* context)
    : Command(context)
{
    auto action = this->createAction();
    action->setText(Command::tr("Inspect XDE"));
    action->setToolTip(Command::tr("Inspect XDE"));
}

void CommandInspectXde::execute()
{
    const gsl::span<const ApplicationItem> spanAppItem = this->guiApp()->selectionModel()->selectedItems();
    DocumentPtr doc;
    for (const ApplicationItem& appItem : spanAppItem) {
        if (appItem.document()->isXCafDocument()) {
            doc = appItem.document();
            break;
        }
    }

    if (doc) {
        auto dlg = new DialogInspectXde(this->widgetMain());
        dlg->load(doc);
        QtWidgetsUtils::asyncDialogExec(dlg);
    }
}

bool CommandInspectXde::getEnabledStatus() const
{
    gsl::span<const ApplicationItem> spanSelectedAppItem = this->guiApp()->selectionModel()->selectedItems();
    const ApplicationItem firstAppItem =
            !spanSelectedAppItem.empty() ? spanSelectedAppItem.front() : ApplicationItem();
    return spanSelectedAppItem.size() == 1
            && firstAppItem.isValid()
            && firstAppItem.document()->isXCafDocument()
            && this->context()->currentPage() == IAppContext::Page::Documents
        ;
}

CommandExportDrawing::CommandExportDrawing(IAppContext* context)
    : Command(context)
{
    auto action = this->createAction();
    action->setText(Command::tr("2D 도면 내보내기…"));
    action->setToolTip(Command::tr("현재 모델을 정투상 2D 도면(SVG/DXF)으로 내보냅니다"));
}

void CommandExportDrawing::execute()
{
    auto guiDoc = this->currentGuiDocument();
    const DocumentPtr doc = guiDoc ? guiDoc->document() : DocumentPtr();
    if (!doc) {
        QMessageBox::warning(this->widgetMain(), Command::tr("도면 내보내기"),
                             Command::tr("열려 있는 문서가 없습니다."));
        return;
    }

    // Assemble the document's top-level B-Rep shapes into one compound.
    TopoDS_Compound comp;
    BRep_Builder builder;
    builder.MakeCompound(comp);
    int shapeCount = 0;
    for (const TDF_Label& label : doc->xcaf().topLevelFreeShapes()) {
        const TopoDS_Shape shape = XCaf::shape(label);
        if (!shape.IsNull()) {
            builder.Add(comp, shape);
            ++shapeCount;
        }
    }
    if (shapeCount == 0) {
        QMessageBox::warning(this->widgetMain(), Command::tr("도면 내보내기"),
            Command::tr("이 문서에는 도면으로 만들 B-Rep 형상이 없습니다 "
                        "(STL 등 메시 전용 모델은 지원되지 않습니다)."));
        return;
    }

    // Options dialog, seeded with a path next to the source file.
    DialogExportDrawing dlg(this->widgetMain());
    const QString srcPath = filepathTo<QString>(doc->filePath());
    QString title = QString::fromStdString(doc->name());
    if (!srcPath.isEmpty()) {
        const QFileInfo fi(srcPath);
        dlg.setSuggestedPath(QDir(fi.absolutePath()).filePath(fi.completeBaseName() + ".svg"));
        if (title.isEmpty())
            title = fi.completeBaseName();
    }
    if (dlg.exec() != QDialog::Accepted)
        return;

    const argos::DrawingOptions opt = dlg.drawingOptions();
    const QString path = dlg.outputPath();
    const QString format = dlg.formatKey();

    // Compute the drawing (synchronous, with a wait cursor).
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const argos::Drawing dr = argos::computeDrawing(comp, opt, title.toStdString());
    QApplication::restoreOverrideCursor();
    if (!dr.ok) {
        QMessageBox::critical(this->widgetMain(), Command::tr("도면 내보내기 실패"),
            Command::tr("도면 생성에 실패했습니다: %1")
                .arg(QString::fromStdString(dr.error)));
        return;
    }

    // Resolve output file(s): honour the chosen format, deriving names from the
    // path stem when the format needs an extension the path does not carry.
    const QFileInfo fi(path);
    const QString suffix = fi.suffix().toLower();
    const QString dir = fi.path();
    const QString base = (suffix == "svg" || suffix == "dxf") ? fi.completeBaseName()
                                                              : fi.fileName();
    auto withExt = [&](const QString& ext) { return QDir(dir).filePath(base + ext); };

    std::vector<std::pair<QString, QString>> targets;  // (format, path)
    if (format == "both") {
        targets.push_back({ "svg", withExt(".svg") });
        targets.push_back({ "dxf", withExt(".dxf") });
    }
    else if (format == "dxf")
        targets.push_back({ "dxf", suffix == "dxf" ? path : withExt(".dxf") });
    else
        targets.push_back({ "svg", suffix == "svg" ? path : withExt(".svg") });

    QStringList written;
    for (const auto& t : targets) {
        const std::string content = (t.first == "svg") ? argos::to_svg(dr) : argos::to_dxf(dr);
        QFile f(t.second);
        if (!f.open(QIODevice::WriteOnly)
                || f.write(content.data(), qint64(content.size())) < 0) {
            QMessageBox::critical(this->widgetMain(), Command::tr("도면 내보내기 실패"),
                Command::tr("파일을 저장할 수 없습니다: %1").arg(t.second));
            return;
        }
        f.close();
        written << t.second;
    }

    QMessageBox::information(this->widgetMain(), Command::tr("도면 저장 완료"),
        Command::tr("도면을 저장했습니다:\n%1").arg(written.join("\n")));
}

bool CommandExportDrawing::getEnabledStatus() const
{
    return this->app()->documentCount() != 0
           && this->context()->currentPage() == IAppContext::Page::Documents;
}

CommandEditOptions::CommandEditOptions(IAppContext* context)
    : Command(context)
{
    auto action = this->createAction();
    action->setMenuRole(QAction::PreferencesRole);
    action->setText(Command::tr("Options"));
    action->setToolTip(Command::tr("Options"));
    action->setShortcut(QKeySequence::StandardKey::Preferences);
}

void CommandEditOptions::execute()
{
    auto dlg = new DialogOptions(AppModule::get()->settings(), this->widgetMain());
    QtWidgetsUtils::asyncDialogExec(dlg);
}

} // namespace Mayo

