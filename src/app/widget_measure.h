/****************************************************************************
** Copyright (c) 2016, Fougue SAS <https://www.fougue.pro>
** SPDX-License-Identifier: BSD-2-Clause
****************************************************************************/

#pragma once

#include "../base/signal.h"
#include "../base/libtree.h"
#include "../measure/measure_display.h"
#include "../measure/measure_tool.h"
#include "../argos_core/measure.h"

#include <Bnd_Box.hxx>

#include <QtWidgets/QWidget>
#include <memory>
#include <optional>
#include <vector>

class QCheckBox;
class QComboBox;
class QListWidget;
class QPushButton;

namespace Mayo {

class GuiDocument;

// Widget panel dedicated to measurements in 3D view
class WidgetMeasure : public QWidget {
    Q_OBJECT
public:
    WidgetMeasure(GuiDocument* guiDoc, QWidget* parent = nullptr);
    ~WidgetMeasure();

    void setMeasureOn(bool on);

    static void addTool(std::unique_ptr<IMeasureTool> tool);

    // Argos: one-line measure of a single hovered sub-shape (vertex/edge/face),
    // for the live hover readout. Returns empty for non-measurable owners.
    static QString quickMeasureText(const GraphicsOwnerPtr& owner);

signals:
    void sizeAdjustmentRequested();

private:
    void onMeasureTypeChanged(int id);
    void onMeasureUnitsChanged();

    static MeasureType toMeasureType(int comboBoxId);
    static LengthUnit toMeasureLengthUnit(int comboBoxId);
    static AngleUnit toMeasureAngleUnit(int comboBoxId);
    static AreaUnit toMeasureAreaUnit(int comboBoxId);
    static VolumeUnit toMeasureVolumeUnit(int comboBoxId);

    MeasureType currentMeasureType() const;
    MeasureDisplayConfig currentMeasureDisplayConfig() const;

    void onGraphicsSelectionChanged();
    void onDocumentEntityAdded(TreeNodeId entityNodeId);

    // Argos: sub-shape selection filter. Returns the OCCT selection modes enabled
    // by the 점/선/면 toggles, so the user can restrict picking to just faces (or
    // just edges) when overlapping vertices/edges make faces hard to click.
    std::vector<int> activeSelectionModes() const;
    // Re-apply activeSelectionModes() to every displayed object of the document.
    void applySelectionModes();

    // Argos: (re)compute the measurement for the current selection set, applying
    // the Show-XYZ / Point-to-Point options. addToHistory appends the result to
    // the measurement-history list (only on a genuine selection change).
    void recompute(bool addToHistory);

    // Argos: one-click measurement of the whole visible model's overall size —
    // the axis-aligned bounding box giving width/depth/height plus the highest
    // (max Z) and lowest (min Z) points, without needing any selection.
    void measureOverallSize();

    // Argos: UI-thread continuation of measureOverallSize() — takes the computed
    // exact B-Rep box (void for mesh-only docs), caches it, applies the
    // graphics-box fallback, then renders the result card and on-screen callout.
    void applyOverallSizeResult(Bnd_Box box);

    void updateMessagePanel();

    using IMeasureDisplayPtr = std::unique_ptr<IMeasureDisplay>;
    void eraseMeasureDisplay(const IMeasureDisplay* measure);
    // Argos: SolidWorks-style set-based measure -> erase all current displays
    void clearAllMeasureDisplays();

    // Provides link between GraphicsOwner and IMeasureDisplay object
    struct GraphicsOwner_MeasureDisplay {
        GraphicsOwnerPtr gfxOwner;
        const IMeasureDisplay* measureDisplay = nullptr;
    };
    void addLink(const GraphicsOwnerPtr& owner, const IMeasureDisplayPtr& measure);
    void eraseLink(const GraphicsOwner_MeasureDisplay* link);
    const GraphicsOwner_MeasureDisplay* findLink(const GraphicsOwnerPtr& owner) const;

    // -- Attributes
    class Ui_WidgetMeasure* m_ui = nullptr;
    GuiDocument* m_guiDoc = nullptr;
    std::vector<GraphicsOwnerPtr> m_vecSelectedOwner;
    std::vector<IMeasureDisplayPtr> m_vecMeasureDisplay;
    std::vector<GraphicsOwner_MeasureDisplay> m_vecLinkGfxOwnerMeasure;
    IMeasureTool* m_tool = nullptr;
    QString m_errorMessage;
    QString m_resultText;   // Argos: formatted readout of the current selection set
    QString m_lastShort;    // Argos: one-line summary of the last result (for "값 복사")
    QString m_lastJson;     // Argos: JSON of the last result (for "JSON 복사")
    argos::MeasureResult m_lastResult;  // Argos: structured result driving the card panel
    bool m_hasResult = false;           // Argos: true when m_lastResult is valid

    // Argos: exact overall bounding box, cached so the CPU-heavy exact-surface
    // computation runs at most once per document (and off the UI thread). Reset
    // in onDocumentEntityAdded() whenever the document's shapes change.
    std::optional<Bnd_Box> m_overallBox;
    bool m_overallBusy = false;         // an overall-size computation is in flight

    // Argos SolidWorks-style controls (created in code, see constructor)
    QCheckBox* m_checkShowXyz = nullptr;
    QCheckBox* m_checkPointToPoint = nullptr;
    // Argos: SolidWorks-style distance mode for a two-circle selection
    // (center-to-center / minimum / maximum). Enabled only when the current
    // selection resolves to a circle pair.
    QComboBox* m_comboCircleMode = nullptr;
    QWidget* m_circleModeRow = nullptr;
    QCheckBox* m_checkSelVertex = nullptr;   // Argos: filter — allow picking vertices
    QCheckBox* m_checkSelEdge = nullptr;     // Argos: filter — allow picking edges
    QCheckBox* m_checkSelFace = nullptr;     // Argos: filter — allow picking faces
    QPushButton* m_btnOverallSize = nullptr;  // Argos: "전체 크기 측정" button (disabled while measuring)
    QListWidget* m_historyList = nullptr;
    SignalConnectionHandle m_connGraphicsSelectionChanged;
    SignalConnectionHandle m_connDocumentEntityAdded;
};

} // namespace Mayo
