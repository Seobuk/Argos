/****************************************************************************
** Copyright (c) 2016, Fougue SAS <https://www.fougue.pro>
** SPDX-License-Identifier: BSD-2-Clause
****************************************************************************/

#include "widget_model_tree.h"

#include "../base/application.h"
#include "../base/application_item_selection_model.h"
#include "../base/bnd_utils.h"
#include "../base/document.h"
#include "../base/xcaf.h"
#include "../gui/gui_application.h"
#include "../graphics/graphics_utils.h"
#include "../argos_core/measure.h"
#include "../qtcommon/qtcore_utils.h"
#include "item_view_buttons.h"
#include "qtgui_utils.h"
#include "theme.h"
#include "widget_model_tree_builder.h"

#include <QtCore/QtDebug>
#include <QtCore/QMetaType>
#include <QtGui/QAction>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QColorDialog>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QTreeWidgetItemIterator>

#include <gsl/util>
#include <cassert>
#include <memory>
#include <unordered_map>

Q_DECLARE_METATYPE(Mayo::DocumentPtr)
Q_DECLARE_METATYPE(Mayo::DocumentTreeNode)

namespace Mayo::Internal {

static std::vector<WidgetModelTree::BuilderPtr>& arrayPrototypeBuilder()
{
    static std::vector<WidgetModelTree::BuilderPtr> vecPtrBuilder;
    if (vecPtrBuilder.empty())
        vecPtrBuilder.emplace_back(std::make_unique<WidgetModelTreeBuilder>()); // Fallback

    return vecPtrBuilder;
}

class TreeWidget : public QTreeWidget {
public:
    using QTreeWidget::QTreeWidget; // Inherit QTreeWidget constructors

    QModelIndex indexFromItem(QTreeWidgetItem* item, int column = 0) const {
        return QTreeWidget::indexFromItem(item, column);
    }

    QTreeWidgetItem* itemFromIndex(const QModelIndex& index) const {
        return QTreeWidget::itemFromIndex(index);
    }

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->buttons().testFlag(Qt::RightButton)) {
        }
        else {
            QTreeWidget::mousePressEvent(event);
        }
    }
};

} // namespace Mayo::Internal

#include "ui_widget_model_tree.h"

namespace Mayo::Internal {

enum TreeItemRole {
    TreeItemTypeRole = Qt::UserRole + 1,
    TreeItemDocumentRole,
    TreeItemDocumentTreeNodeRole
};

enum TreeItemType {
    TreeItemType_Unknown = 0,
    TreeItemType_Document = 0x01,
    TreeItemType_DocumentTreeNode = 0x02,
    TreeItemType_DocumentEntity = 0x10 | TreeItemType_DocumentTreeNode
};

static TreeItemType treeItemType(const QTreeWidgetItem* treeItem)
{
    const QVariant varType = treeItem->data(0, TreeItemTypeRole);
    return varType.isValid() ?
               static_cast<Internal::TreeItemType>(varType.toInt()) :
               Internal::TreeItemType_Unknown
        ;
}

static DocumentPtr treeItemDocument(const QTreeWidgetItem* treeItem)
{
    //Expects(treeItemType(treeItem) == TreeItemType_Document);
    const QVariant var = treeItem->data(0, TreeItemDocumentRole);
    return var.isValid() ? var.value<DocumentPtr>() : DocumentPtr();
}

static DocumentTreeNode treeItemDocumentTreeNode(const QTreeWidgetItem* treeItem)
{
    //Expects(treeItemType(treeItem) & TreeItemType_DocumentTreeNode);
    const QVariant var = treeItem->data(0, TreeItemDocumentTreeNodeRole);
    return var.isValid() ? var.value<DocumentTreeNode>() : DocumentTreeNode::null();
}

static void setTreeItemDocument(QTreeWidgetItem* treeItem, const DocumentPtr& doc)
{
    treeItem->setData(0, TreeItemTypeRole, Internal::TreeItemType_Document);
    treeItem->setData(0, TreeItemDocumentRole, QVariant::fromValue(doc));
}

static void setTreeItemDocumentTreeNode(QTreeWidgetItem* treeItem, const DocumentTreeNode& node)
{
    const TreeItemType treeItemType =
        node.isEntity() ? TreeItemType_DocumentEntity : TreeItemType_DocumentTreeNode;
    treeItem->setData(0, TreeItemTypeRole, treeItemType);
    treeItem->setData(0, TreeItemDocumentTreeNodeRole, QVariant::fromValue(node));
}

static ApplicationItem toApplicationItem(const QTreeWidgetItem* treeItem)
{
    const TreeItemType type = Internal::treeItemType(treeItem);
    if (type == TreeItemType_Document)
        return ApplicationItem(Internal::treeItemDocument(treeItem));
    else if (type & TreeItemType_DocumentTreeNode)
        return ApplicationItem(Internal::treeItemDocumentTreeNode(treeItem));

    return ApplicationItem();
}

} // namespace Mayo::Internal


namespace Mayo {

WidgetModelTree::WidgetModelTree(QWidget* widget)
    : QWidget(widget),
      m_ui(new Ui_WidgetModelTree)
{
    m_ui->setupUi(this);
    m_ui->treeWidget_Model->setUniformRowHeights(true);
    for (const BuilderPtr& ptrBuilder : Internal::arrayPrototypeBuilder()) {
        m_vecBuilder.push_back(ptrBuilder->clone());
        m_vecBuilder.back()->setTreeWidget(m_ui->treeWidget_Model);
    }

    // Add action "Remove item from document"
    auto modelTreeBtns = new ItemViewButtons(m_ui->treeWidget_Model, this);
    constexpr int idBtnRemove = 1;
    modelTreeBtns->addButton(
        idBtnRemove, mayoTheme()->icon(Theme::Icon::Cross), tr("Remove from document")
    );
    modelTreeBtns->setButtonDetection(
        idBtnRemove, Internal::TreeItemTypeRole, QVariant{Internal::TreeItemType_DocumentEntity}
    );
    modelTreeBtns->setButtonDisplayColumn(idBtnRemove, 0);
    modelTreeBtns->setButtonDisplayModes(idBtnRemove, ItemViewButtons::DisplayOnDetection);
    modelTreeBtns->setButtonItemSide(idBtnRemove, ItemViewButtons::ItemRightSide);
    const int iconSize = this->style()->pixelMetric(QStyle::PM_ListViewIconSize);
    modelTreeBtns->setButtonIconSize(idBtnRemove, QSize(iconSize * 0.66, iconSize * 0.66));
    modelTreeBtns->installDefaultItemDelegate();
    QObject::connect(
        modelTreeBtns, &ItemViewButtons::buttonClicked,
        this, [=](int btnId, const QModelIndex& index) {
            if (btnId == idBtnRemove && index.isValid()) {
                const QTreeWidgetItem* treeItem = m_ui->treeWidget_Model->itemFromIndex(index);
                const DocumentTreeNode entityNode = Internal::treeItemDocumentTreeNode(treeItem);
                entityNode.document()->destroyEntity(entityNode.id());
            }
    });

    // Right-click context menu: change / reset the color of the selected part(s).
    m_ui->treeWidget_Model->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(
        m_ui->treeWidget_Model, &QWidget::customContextMenuRequested,
        this, [=](const QPoint& pos) { this->showContextMenu(pos); }
    );

    this->connectTreeModelDataChanged(true);
}

WidgetModelTree::~WidgetModelTree()
{
    delete m_ui;
}

void WidgetModelTree::showContextMenu(const QPoint& pos)
{
    if (!m_guiApp)
        return;

    // Target nodes: the current selection, or the item under the cursor when the
    // right-click happened outside the selection.
    QList<QTreeWidgetItem*> items = m_ui->treeWidget_Model->selectedItems();
    QTreeWidgetItem* itemAtPos = m_ui->treeWidget_Model->itemAt(pos);
    if (itemAtPos && !items.contains(itemAtPos))
        items = { itemAtPos };

    // Keep only items that map to a document tree node (i.e. an actual part).
    std::vector<DocumentTreeNode> nodes;
    for (QTreeWidgetItem* item : items) {
        if (Internal::treeItemType(item) & Internal::TreeItemType_DocumentTreeNode) {
            const DocumentTreeNode node = Internal::treeItemDocumentTreeNode(item);
            if (node.isValid())
                nodes.push_back(node);
        }
    }

    if (nodes.empty())
        return;

    QMenu menu(this);
    QAction* actChangeColor = menu.addAction(tr("색상 변경..."));
    QAction* actResetColor = menu.addAction(tr("색상 초기화"));
    menu.addSeparator();
    QAction* actToggleVisible = menu.addAction(tr("표시/숨김"));
    QAction* actIsolate = menu.addAction(tr("선택 파트만 표시"));
    QAction* actFitCamera = menu.addAction(tr("이 파트로 화면 맞춤"));
    QAction* actSelectInstances = menu.addAction(tr("동일 파트 인스턴스 모두 선택"));
    QAction* actPartInfo = menu.addAction(tr("파트 정보"));

    // Helper: run 'fn(guiDoc, nodeId)' for every targeted node.
    auto fnForEachNode = [&](const std::function<void(GuiDocument*, TreeNodeId)>& fn) {
        for (const DocumentTreeNode& node : nodes) {
            GuiDocument* guiDoc = m_guiApp->findGuiDocument(node.document());
            if (guiDoc)
                fn(guiDoc, node.id());
        }
    };

    QAction* chosen = menu.exec(m_ui->treeWidget_Model->viewport()->mapToGlobal(pos));
    if (chosen == actChangeColor) {
        const QColor qcolor = QColorDialog::getColor(
            Qt::lightGray, this, tr("파트 색상 선택")
        );
        if (qcolor.isValid()) {
            // sRGB-aware conversion so the rendered part matches the picked color.
            const Quantity_Color color = QtGuiUtils::toPreferredColorSpace(qcolor);
            fnForEachNode([&](GuiDocument* guiDoc, TreeNodeId nodeId) {
                guiDoc->setNodeColor(nodeId, color);
            });
        }
    }
    else if (chosen == actResetColor) {
        fnForEachNode([&](GuiDocument* guiDoc, TreeNodeId nodeId) {
            guiDoc->resetNodeColor(nodeId);
        });
    }
    else if (chosen == actToggleVisible) {
        // Direction decided from the first node: if it's not fully shown, show all; else hide all.
        GuiDocument* g0 = m_guiApp->findGuiDocument(nodes.front().document());
        if (g0) {
            const bool makeVisible = g0->nodeVisibleState(nodes.front().id()) != CheckState::On;
            // Redraw every affected document's scene (not just the first) — a
            // cross-document multi-selection touches more than one GuiDocument.
            std::vector<GuiDocument*> touched;
            fnForEachNode([&](GuiDocument* g, TreeNodeId id) {
                g->setNodeVisible(id, makeVisible);
                if (std::find(touched.begin(), touched.end(), g) == touched.end())
                    touched.push_back(g);
            });
            for (GuiDocument* g : touched)
                g->graphicsScene()->redraw();
        }
    }
    else if (chosen == actIsolate) {
        // Group targets by document; per document hide every root, then re-show the targets.
        // ponytail: only full hide/show exists (no per-node translucency) — dimming the rest is deferred.
        std::vector<DocumentPtr> doneDocs;
        for (const DocumentTreeNode& node : nodes) {
            const DocumentPtr doc = node.document();
            if (std::find(doneDocs.begin(), doneDocs.end(), doc) != doneDocs.end())
                continue;

            doneDocs.push_back(doc);
            GuiDocument* g = m_guiApp->findGuiDocument(doc);
            if (!g)
                continue;

            for (TreeNodeId rootId : doc->modelTree().roots())
                g->setNodeVisible(rootId, false);

            for (const DocumentTreeNode& target : nodes) {
                if (target.document() == doc)
                    g->setNodeVisible(target.id(), true);
            }

            g->graphicsScene()->redraw();
        }
    }
    else if (chosen == actFitCamera) {
        GuiDocument* g0 = m_guiApp->findGuiDocument(nodes.front().document());
        if (g0) {
            Bnd_Box box;
            fnForEachNode([&](GuiDocument* g, TreeNodeId id) {
                g->foreachGraphicsObject(id, [&](GraphicsObjectPtr o) {
                    BndUtils::add(&box, GraphicsUtils::AisObject_boundingBox(o));
                });
            });
            g0->runViewCameraAnimation([box](OccHandle<V3d_View> view) {
                GraphicsUtils::V3dView_fitAll(view, box);
            });
        }
    }
    else if (chosen == actSelectInstances) {
        // Select every tree node that points at the same product label as the primary node.
        const DocumentPtr doc = nodes.front().document();
        const TDF_Label frontLabel = nodes.front().label();
        const TDF_Label ref = XCaf::isShapeReference(frontLabel)
                ? XCaf::shapeReferred(frontLabel) : frontLabel;
        if (!ref.IsNull()) {
            std::vector<ApplicationItem> hits;
            const Tree<TDF_Label>& tree = doc->modelTree();
            traverseTree(tree, [&](TreeNodeId id) {
                const TDF_Label lbl = tree.nodeData(id);
                const TDF_Label lblRef = XCaf::isShapeReference(lbl)
                        ? XCaf::shapeReferred(lbl) : lbl;
                if (!lblRef.IsNull() && lblRef.IsEqual(ref))
                    hits.push_back(ApplicationItem(DocumentTreeNode(doc, id)));
            });
            m_guiApp->selectionModel()->clear();
            m_guiApp->selectionModel()->add(hits);
        }
    }
    else if (chosen == actPartInfo) {
        const TopoDS_Shape shape = XCaf::shape(nodes.front().label());
        if (shape.IsNull()) {
            QMessageBox::information(this, tr("파트 정보"), tr("이 파트에는 형상이 없습니다."));
            return;
        }

        const argos::MassProperties mp = argos::massProperties(shape);

        // ponytail: single part on the UI thread — fine; move off-thread only if it ever stalls.
        Bnd_Box box;
        GuiDocument* g0 = m_guiApp->findGuiDocument(nodes.front().document());
        if (g0) {
            g0->foreachGraphicsObject(nodes.front().id(), [&](GraphicsObjectPtr o) {
                BndUtils::add(&box, GraphicsUtils::AisObject_boundingBox(o));
            });
        }

        QString text;
        if (mp.ok) {
            text += tr("부피: %1 mm^3\n").arg(mp.volume_mm3, 0, 'f', 2);
            text += tr("표면적: %1 mm^2\n").arg(mp.area_mm2, 0, 'f', 2);
        }

        if (!box.IsVoid()) {
            const BndBoxCoords c = BndBoxCoords::get(box);
            text += tr("크기: %1 x %2 x %3 mm")
                        .arg(c.xmax - c.xmin, 0, 'f', 2)
                        .arg(c.ymax - c.ymin, 0, 'f', 2)
                        .arg(c.zmax - c.zmin, 0, 'f', 2);
        }

        if (text.isEmpty())
            text = tr("측정 가능한 정보가 없습니다.");

        QMessageBox::information(this, tr("파트 정보"), text);
    }
}

void WidgetModelTree::refreshItemText(const ApplicationItem& appItem)
{
    if (appItem.isDocument()) {
        const DocumentPtr doc = appItem.document();
        QTreeWidgetItem* treeItem = this->findTreeItem(doc);
        if (treeItem)
            this->findSupportBuilder(doc)->refreshTextTreeItem(doc, treeItem);
    }
    else if (appItem.isDocumentTreeNode()) {
        const DocumentTreeNode& node = appItem.documentTreeNode();
        QTreeWidgetItem* treeItemDocItem = this->findTreeItem(node);
        if (treeItemDocItem)
            this->findSupportBuilder(node)->refreshTextTreeItem(node, treeItemDocItem);
    }
}

void WidgetModelTree::registerGuiApplication(GuiApplication* guiApp)
{
    if (m_guiApp == guiApp)
        return;

    m_guiApp = guiApp;
    auto app = guiApp->application();
    app->signalDocumentAdded.connectSlot(&WidgetModelTree::onDocumentAdded, this);
    app->signalDocumentAboutToClose.connectSlot(&WidgetModelTree::onDocumentAboutToClose, this);
    app->signalDocumentNameChanged.connectSlot(&WidgetModelTree::onDocumentNameChanged, this);
    app->signalDocumentEntityAdded.connectSlot(&WidgetModelTree::onDocumentEntityAdded, this);
    app->signalDocumentEntityAboutToBeDestroyed.connectSlot(&WidgetModelTree::onDocumentEntityAboutToBeDestroyed, this);

    m_guiApp->selectionModel()->signalChanged.connectSlot(&WidgetModelTree::onApplicationItemSelectionModelChanged, this);
    m_guiApp->signalGuiDocumentAdded.connectSlot([=](GuiDocument* guiDoc) {
        guiDoc->signalNodesVisibilityChanged.connectSlot([=](const GuiDocument::MapVisibilityByTreeNodeId& mapNodeId) {
            this->onNodesVisibilityChanged(guiDoc, mapNodeId);
        });
    });

    this->connectTreeWidgetDocumentSelectionChanged(true);
}

WidgetModelTree_UserActions WidgetModelTree::createUserActions(QObject* parent)
{
    WidgetModelTree_UserActions userActions;
    std::vector<WidgetModelTree_UserActions::FunctionSyncItems> vecFnSyncItems;
    for (const BuilderPtr& builder : m_vecBuilder) {
        const WidgetModelTree_UserActions subUserActions = builder->createUserActions(parent);
        for (QAction* action : subUserActions.items)
            userActions.items.push_back(action);

        if (subUserActions.fnSyncItems)
            vecFnSyncItems.push_back(std::move(subUserActions.fnSyncItems));
    }

    userActions.fnSyncItems = [=]{
        for (const WidgetModelTree_UserActions::FunctionSyncItems& fn : vecFnSyncItems)
            fn();
    };
    return userActions;
}

void WidgetModelTree::addPrototypeBuilder(BuilderPtr builder)
{
    Internal::arrayPrototypeBuilder().push_back(std::move(builder));
}

DocumentTreeNode WidgetModelTree::documentTreeNode(const QTreeWidgetItem* treeItem)
{
    return Internal::treeItemDocumentTreeNode(treeItem);
}

void WidgetModelTree::setDocumentTreeNode(QTreeWidgetItem* treeItem, const DocumentTreeNode& node)
{
    Internal::setTreeItemDocumentTreeNode(treeItem, node);
}

void WidgetModelTree::setDocument(QTreeWidgetItem* treeItem, const DocumentPtr& doc)
{
    Internal::setTreeItemDocument(treeItem, doc);
}

bool WidgetModelTree::holdsDocument(const QTreeWidgetItem* treeItem)
{
    return Internal::treeItemType(treeItem) == Internal::TreeItemType_Document;
}

bool WidgetModelTree::holdsDocumentTreeNode(const QTreeWidgetItem* treeItem)
{
    return Internal::treeItemType(treeItem) & Internal::TreeItemType_DocumentTreeNode;
}

void WidgetModelTree::onDocumentAdded(const DocumentPtr& doc)
{
    auto treeItem = this->findSupportBuilder(doc)->createTreeItem(doc);
    Internal::setTreeItemDocument(treeItem, doc);
    Q_ASSERT(Internal::treeItemDocument(treeItem) == doc);
    m_ui->treeWidget_Model->addTopLevelItem(treeItem);
}

void WidgetModelTree::onDocumentAboutToClose(const DocumentPtr& doc)
{
    delete this->findTreeItem(doc);
}

void WidgetModelTree::onDocumentNameChanged(const DocumentPtr& doc, const std::string& /*name*/)
{
    QTreeWidgetItem* treeItem = this->findTreeItem(doc);
    if (treeItem)
        this->findSupportBuilder(doc)->refreshTextTreeItem(doc, treeItem);
}

QTreeWidgetItem* WidgetModelTree::loadDocumentEntity(const DocumentTreeNode& node)
{
    Expects(node.isEntity());
    auto treeItem = this->findSupportBuilder(node)->createTreeItem(node);
    Internal::setTreeItemDocumentTreeNode(treeItem, node);
    return treeItem;
}

QTreeWidgetItem* WidgetModelTree::findTreeItem(const DocumentPtr& doc) const
{
    for (int i = 0; i < m_ui->treeWidget_Model->topLevelItemCount(); ++i) {
        QTreeWidgetItem* treeItem = m_ui->treeWidget_Model->topLevelItem(i);
        if (Internal::treeItemDocument(treeItem) == doc)
            return treeItem;
    }

    return nullptr;
}

QTreeWidgetItem* WidgetModelTree::findTreeItem(const DocumentTreeNode& node) const
{
    QTreeWidgetItem* treeItemDoc = this->findTreeItem(node.document());
    if (treeItemDoc) {
        for (QTreeWidgetItemIterator it(treeItemDoc); *it; ++it) {
            if (Internal::treeItemDocumentTreeNode(*it) == node)
                return *it;
        }
    }

    return nullptr;
}

WidgetModelTreeBuilder* WidgetModelTree::findSupportBuilder(const DocumentPtr& doc) const
{
    auto it = std::find_if(
        std::next(m_vecBuilder.cbegin()),
        m_vecBuilder.cend(),
        [=](const BuilderPtr& builder) { return builder->supportsDocument(doc); }
    );
    return it != m_vecBuilder.cend() ? it->get() : m_vecBuilder.front().get();
}

WidgetModelTreeBuilder* WidgetModelTree::findSupportBuilder(const DocumentTreeNode& node) const
{
    Expects(node.isValid());
    auto it = std::find_if(
        std::next(m_vecBuilder.cbegin()),
        m_vecBuilder.cend(),
        [=](const BuilderPtr& builder) { return builder->supportsDocumentTreeNode(node); }
    );
    return it != m_vecBuilder.cend() ? it->get() : m_vecBuilder.front().get();
}

void WidgetModelTree::onDocumentEntityAdded(const DocumentPtr& doc, TreeNodeId entityId)
{
    QTreeWidgetItem* treeDocEntity = this->loadDocumentEntity({ doc, entityId });
    QTreeWidgetItem* treeDoc = this->findTreeItem(doc);
    if (treeDoc) {
        treeDoc->addChild(treeDocEntity);
        treeDoc->setExpanded(true);
    }
}

void WidgetModelTree::onDocumentEntityAboutToBeDestroyed(const DocumentPtr& doc, TreeNodeId entityId)
{
    QTreeWidgetItem* treeItem = this->findTreeItem({ doc, entityId });
    delete treeItem;
}

void WidgetModelTree::onTreeWidgetDocumentSelectionChanged(
        const QItemSelection& selected, const QItemSelection& deselected
    )
{
    const QModelIndexList listSelectedIndex = selected.indexes();
    const QModelIndexList listDeselectedIndex = deselected.indexes();
    std::vector<ApplicationItem> vecSelected;
    std::vector<ApplicationItem> vecDeselected;
    vecSelected.reserve(listSelectedIndex.size());
    vecDeselected.reserve(listDeselectedIndex.size());
    for (const QModelIndex& index : listSelectedIndex) {
        const QTreeWidgetItem* treeItem = m_ui->treeWidget_Model->itemFromIndex(index);
        vecSelected.push_back(Internal::toApplicationItem(treeItem));
    }

    for (const QModelIndex& index : listDeselectedIndex) {
        const QTreeWidgetItem* treeItem = m_ui->treeWidget_Model->itemFromIndex(index);
        vecDeselected.push_back(Internal::toApplicationItem(treeItem));
    }

    m_guiApp->selectionModel()->add(vecSelected);
    m_guiApp->selectionModel()->remove(vecDeselected);
}

void WidgetModelTree::onApplicationItemSelectionModelChanged(
        gsl::span<const ApplicationItem> selected, gsl::span<const ApplicationItem> deselected
    )
{
    this->connectTreeWidgetDocumentSelectionChanged(false);
    auto _ = gsl::finally([=] { this->connectTreeWidgetDocumentSelectionChanged(true); });

    auto fnSetSelected = [=](gsl::span<const ApplicationItem> spanAppItem, bool on) {
        for (const ApplicationItem& appItem : spanAppItem) {
            if (!appItem.isDocumentTreeNode())
                continue;

            QTreeWidgetItem* treeItem = this->findTreeItem(appItem.documentTreeNode());
            if (!treeItem)
                continue;

            treeItem->setSelected(on);
            if (on)
                m_ui->treeWidget_Model->scrollToItem(treeItem);
        }
    };

    fnSetSelected(selected, true);
    fnSetSelected(deselected, false);
}

void WidgetModelTree::connectTreeModelDataChanged(bool on)
{
    if (on) {
        m_connTreeModelDataChanged = QObject::connect(
            m_ui->treeWidget_Model->model(), &QAbstractItemModel::dataChanged,
            this, &WidgetModelTree::onTreeModelDataChanged, Qt::UniqueConnection
        );
    }
    else {
        QObject::disconnect(m_connTreeModelDataChanged);
    }
}

void WidgetModelTree::connectTreeWidgetDocumentSelectionChanged(bool on)
{
    if (on) {
        m_connTreeWidgetDocumentSelectionChanged = QObject::connect(
            m_ui->treeWidget_Model->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &WidgetModelTree::onTreeWidgetDocumentSelectionChanged,
            Qt::UniqueConnection
        );
    }
    else {
        QObject::disconnect(m_connTreeWidgetDocumentSelectionChanged);
    }
}

void WidgetModelTree::onTreeModelDataChanged(
        const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles
    )
{
    if (roles.contains(Qt::CheckStateRole) && topLeft == bottomRight) {
        const QModelIndex& indexItem = topLeft;
        const auto qtCheckState = Qt::CheckState(indexItem.data(Qt::CheckStateRole).toInt());
        const auto checkState = QtCoreUtils::toCheckState(qtCheckState);
        if (checkState == CheckState::Partially)
            return;

        const QVariant var = indexItem.data(Internal::TreeItemDocumentTreeNodeRole);
        auto treeNode = var.isValid() ? var.value<DocumentTreeNode>() : DocumentTreeNode::null();
        if (!treeNode.isValid())
            return;

        GuiDocument* guiDoc = m_guiApp->findGuiDocument(treeNode.document());
        if (!guiDoc)
            return;

        guiDoc->setNodeVisible(treeNode.id(), checkState == CheckState::On ? true : false);
        guiDoc->graphicsScene()->redraw();
    }
}

void WidgetModelTree::onNodesVisibilityChanged(
        const GuiDocument* guiDoc, const std::unordered_map<TreeNodeId, CheckState>& mapNodeId
    )
{
    QTreeWidgetItem* treeItemDoc = this->findTreeItem(guiDoc->document());
    if (!treeItemDoc)
        return;

    this->connectTreeModelDataChanged(false);
    auto _ = gsl::finally([=]{ this->connectTreeModelDataChanged(true); });

    auto localMapNodeId = mapNodeId;
    for (QTreeWidgetItemIterator it(treeItemDoc); *it; ++it) {
        QTreeWidgetItem* treeItem = *it;
        const DocumentTreeNode docTreeNode = Internal::treeItemDocumentTreeNode(treeItem);
        auto itNodeVisibleState = localMapNodeId.find(docTreeNode.id());
        if (itNodeVisibleState != localMapNodeId.cend()) {
            treeItem->setCheckState(0, QtCoreUtils::toQtCheckState(itNodeVisibleState->second));
            localMapNodeId.erase(itNodeVisibleState);
        }

        if (localMapNodeId.empty())
            return;
    }
}

} // namespace Mayo
