/****************************************************************************
** Argos - export-drawing options dialog implementation
** SPDX-License-Identifier: BSD-2-Clause
****************************************************************************/

#include "dialog_export_drawing.h"

#include <QtCore/QFileInfo>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

namespace Mayo {

DialogExportDrawing::DialogExportDrawing(QWidget* parent)
    : QDialog(parent)
{
    this->setWindowTitle(tr("2D 도면 내보내기"));

    auto form = new QFormLayout;

    // Output path + browse.
    m_editPath = new QLineEdit(this);
    auto btnBrowse = new QPushButton(tr("찾아보기…"), this);
    auto pathRow = new QHBoxLayout;
    pathRow->addWidget(m_editPath, 1);
    pathRow->addWidget(btnBrowse);
    form->addRow(tr("저장 위치:"), pathRow);

    // Format.
    m_comboFormat = new QComboBox(this);
    m_comboFormat->addItem(tr("SVG (벡터 열람용)"), "svg");
    m_comboFormat->addItem(tr("DXF (CAD 편집용)"), "dxf");
    m_comboFormat->addItem(tr("SVG + DXF 둘 다"), "both");
    form->addRow(tr("형식:"), m_comboFormat);

    // Projection convention.
    m_comboProjection = new QComboBox(this);
    m_comboProjection->addItem(tr("제1각법 (ISO / 한국)"), int(argos::ProjectionAngle::First));
    m_comboProjection->addItem(tr("제3각법 (ASME / 미국)"), int(argos::ProjectionAngle::Third));
    form->addRow(tr("투상법:"), m_comboProjection);

    // Views.
    auto viewsBox = new QGroupBox(tr("포함할 뷰"), this);
    m_checkFront = new QCheckBox(tr("정면도"), viewsBox);
    m_checkTop   = new QCheckBox(tr("평면도"), viewsBox);
    m_checkRight = new QCheckBox(tr("우측면도"), viewsBox);
    m_checkIso   = new QCheckBox(tr("등각도"), viewsBox);
    for (QCheckBox* c : { m_checkFront, m_checkTop, m_checkRight, m_checkIso })
        c->setChecked(true);
    auto viewsLayout = new QHBoxLayout(viewsBox);
    viewsLayout->addWidget(m_checkFront);
    viewsLayout->addWidget(m_checkTop);
    viewsLayout->addWidget(m_checkRight);
    viewsLayout->addWidget(m_checkIso);
    form->addRow(viewsBox);

    // Options.
    m_checkHidden = new QCheckBox(tr("은선(점선) 표시"), this);
    m_checkHidden->setChecked(true);
    m_checkDims = new QCheckBox(tr("전체 치수 표시"), this);
    m_checkDims->setChecked(true);
    auto optRow = new QHBoxLayout;
    optRow->addWidget(m_checkHidden);
    optRow->addWidget(m_checkDims);
    optRow->addStretch(1);
    form->addRow(tr("옵션:"), optRow);

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("내보내기"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("취소"));

    auto root = new QVBoxLayout(this);
    root->addLayout(form);
    root->addWidget(buttons);

    QObject::connect(btnBrowse, &QPushButton::clicked, this, &DialogExportDrawing::browse);
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, &DialogExportDrawing::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void DialogExportDrawing::setSuggestedPath(const QString& path)
{
    m_editPath->setText(path);
}

void DialogExportDrawing::browse()
{
    const QString key = this->formatKey();
    QString filter = tr("SVG 도면 (*.svg)");
    QString suffix = ".svg";
    if (key == "dxf") { filter = tr("DXF 도면 (*.dxf)"); suffix = ".dxf"; }
    else if (key == "both") { filter = tr("도면 (*.svg *.dxf)"); }

    QString start = m_editPath->text();
    const QString chosen = QFileDialog::getSaveFileName(
        this, tr("도면 저장"), start, filter);
    if (!chosen.isEmpty())
        m_editPath->setText(chosen);
}

QString DialogExportDrawing::outputPath() const
{
    return m_editPath->text().trimmed();
}

QString DialogExportDrawing::formatKey() const
{
    return m_comboFormat->currentData().toString();
}

argos::DrawingOptions DialogExportDrawing::drawingOptions() const
{
    argos::DrawingOptions opt;
    opt.projection = argos::ProjectionAngle(m_comboProjection->currentData().toInt());
    opt.views.clear();
    if (m_checkFront->isChecked()) opt.views.push_back(argos::ViewKind::Front);
    if (m_checkTop->isChecked())   opt.views.push_back(argos::ViewKind::Top);
    if (m_checkRight->isChecked()) opt.views.push_back(argos::ViewKind::Right);
    if (m_checkIso->isChecked())   opt.views.push_back(argos::ViewKind::Iso);
    opt.hiddenLines = m_checkHidden->isChecked();
    opt.dimensions = m_checkDims->isChecked();
    return opt;
}

void DialogExportDrawing::accept()
{
    if (this->outputPath().isEmpty()) {
        QMessageBox::warning(this, tr("저장 위치 필요"),
                             tr("도면을 저장할 파일 경로를 지정하세요."));
        return;
    }
    if (!m_checkFront->isChecked() && !m_checkTop->isChecked()
            && !m_checkRight->isChecked() && !m_checkIso->isChecked()) {
        QMessageBox::warning(this, tr("뷰 선택 필요"),
                             tr("최소한 하나의 뷰를 선택하세요."));
        return;
    }
    QDialog::accept();
}

} // namespace Mayo
