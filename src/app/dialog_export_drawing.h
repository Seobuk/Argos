/****************************************************************************
** Argos - export-drawing options dialog
** SPDX-License-Identifier: BSD-2-Clause
****************************************************************************/

#pragma once

#include "../argos_core/drawing.h"

#include <QtWidgets/QDialog>
#include <QtCore/QString>

class QCheckBox;
class QComboBox;
class QLineEdit;

namespace Mayo {

// Modal dialog collecting the options for a 2D drawing export: output path,
// format (SVG / DXF / both), projection convention, which views to draw, and
// whether to include hidden lines and overall dimensions. The values are read
// back by the command after the dialog is accepted.
class DialogExportDrawing : public QDialog {
    Q_OBJECT
public:
    explicit DialogExportDrawing(QWidget* parent = nullptr);

    // Suggests an initial output path (its stem seeds the file name).
    void setSuggestedPath(const QString& path);

    argos::DrawingOptions drawingOptions() const;
    QString outputPath() const;    // raw path from the line edit
    QString formatKey() const;     // "svg" | "dxf" | "both"

    void accept() override;         // validates before closing

private:
    void browse();

    QLineEdit* m_editPath = nullptr;
    QComboBox* m_comboFormat = nullptr;
    QComboBox* m_comboProjection = nullptr;
    QCheckBox* m_checkFront = nullptr;
    QCheckBox* m_checkTop = nullptr;
    QCheckBox* m_checkRight = nullptr;
    QCheckBox* m_checkIso = nullptr;
    QCheckBox* m_checkHidden = nullptr;
    QCheckBox* m_checkDims = nullptr;
};

} // namespace Mayo
