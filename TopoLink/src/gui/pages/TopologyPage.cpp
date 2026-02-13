#include "TopologyPage.h"
#include <QLabel>
#include <QListWidget>
#include <QRegExp>
#include <QString>
#include <QVBoxLayout>

// ============================================================================
// GeometryGroupDelegate Implementation
// ============================================================================

QWidget *GeometryGroupDelegate::createEditor(QWidget *parent,
                                             const QStyleOptionViewItem &option,
                                             const QModelIndex &index) const {
  Q_UNUSED(option);
  Q_UNUSED(index);
  QComboBox *combo = new QComboBox(parent);
  combo->addItem("(none)");
  for (const QString &name : m_groupNames) {
    combo->addItem(name);
  }
  return combo;
}

void GeometryGroupDelegate::setEditorData(QWidget *editor,
                                          const QModelIndex &index) const {
  QComboBox *combo = qobject_cast<QComboBox *>(editor);
  if (!combo)
    return;
  QString current = index.data(Qt::UserRole).toString();
  int idx = combo->findText(current);
  combo->setCurrentIndex(idx >= 0 ? idx : 0);
}

void GeometryGroupDelegate::setModelData(QWidget *editor,
                                         QAbstractItemModel *model,
                                         const QModelIndex &index) const {
  QComboBox *combo = qobject_cast<QComboBox *>(editor);
  if (!combo)
    return;
  QString text = combo->currentText();
  if (text == "(none)")
    text = "";
  model->setData(index, text, Qt::EditRole);
}

// ============================================================================
// TopologyPage Implementation
// ============================================================================

TopologyPage::TopologyPage(QWidget *parent) : QWidget(parent) {
  m_edgeGroupModel = new TopologyGroupTableModel(this);
  m_faceGroupModel = new TopologyGroupTableModel(this);
  m_edgeGeoDelegate = new GeometryGroupDelegate(this);
  m_faceGeoDelegate = new GeometryGroupDelegate(this);
  setupUI();
}

void TopologyPage::setupUI() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);

  // --- Selection Mode ---
  QGroupBox *selModeGroupBox = new QGroupBox("Interaction Control");
  QHBoxLayout *selModeLayout = new QHBoxLayout(selModeGroupBox);
  selModeLayout->addWidget(new QLabel("Selection Mode:"));
  m_selModeCombo = new QComboBox();
  m_selModeCombo->addItem("Nodes", 0);
  m_selModeCombo->addItem("Edges", 1);
  m_selModeCombo->addItem("Faces", 2);
  selModeLayout->addWidget(m_selModeCombo);
  selModeLayout->addStretch();
  mainLayout->addWidget(selModeGroupBox);

  // --- Entity Lists (read-only topology info) ---
  QGroupBox *entityBox = new QGroupBox("Topology Entities");
  QVBoxLayout *entityLayout = new QVBoxLayout(entityBox);

  QLabel *nodeLabel = new QLabel("Nodes", this);
  nodeLabel->setStyleSheet("font-weight: bold;");
  entityLayout->addWidget(nodeLabel);
  m_nodeList = new QListWidget(this);
  m_nodeList->setMaximumHeight(80);
  m_nodeList->setSelectionMode(QAbstractItemView::ExtendedSelection);
  entityLayout->addWidget(m_nodeList);

  QLabel *edgeLabel = new QLabel("Edges", this);
  edgeLabel->setStyleSheet("font-weight: bold;");
  entityLayout->addWidget(edgeLabel);
  m_edgeList = new QListWidget(this);
  m_edgeList->setMaximumHeight(80);
  m_edgeList->setSelectionMode(QAbstractItemView::ExtendedSelection);
  entityLayout->addWidget(m_edgeList);

  QLabel *faceLabel = new QLabel("Faces", this);
  faceLabel->setStyleSheet("font-weight: bold;");
  entityLayout->addWidget(faceLabel);
  m_faceList = new QListWidget(this);
  m_faceList->setMaximumHeight(80);
  m_faceList->setSelectionMode(QAbstractItemView::ExtendedSelection);
  entityLayout->addWidget(m_faceList);

  mainLayout->addWidget(entityBox);

  // Connect entity list selection for highlighting
  connect(m_faceList, &QListWidget::itemSelectionChanged, this,
          [this]() { onFaceSelectionChanged(nullptr, nullptr); });
  connect(m_edgeList, &QListWidget::itemSelectionChanged, this,
          [this]() { onEdgeSelectionChanged(nullptr, nullptr); });
  connect(m_nodeList, &QListWidget::itemSelectionChanged, this,
          [this]() { onNodeSelectionChanged(nullptr, nullptr); });

  // --- Group Tables ---
  QGroupBox *groupBox = new QGroupBox("Topology Groups");
  QVBoxLayout *groupLayout = new QVBoxLayout(groupBox);

  groupLayout->addWidget(createGroupSection("Edge Groups", m_edgeGroupModel,
                                            m_addEdgeGroupBtn, m_edgeGroupTable,
                                            m_edgeGeoDelegate));

  groupLayout->addWidget(createGroupSection("Face Groups", m_faceGroupModel,
                                            m_addFaceGroupBtn, m_faceGroupTable,
                                            m_faceGeoDelegate));

  // Update Viewer Button
  m_updateBtn = new QPushButton("Update Viewer");
  m_updateBtn->setStyleSheet(
      "QPushButton { background-color: #4a90d9; color: white; padding: 8px; "
      "border-radius: 4px; font-weight: bold; } QPushButton:hover { "
      "background-color: #5aa0e9; } QPushButton:pressed { background-color: "
      "#3a80c9; }");
  groupLayout->addWidget(m_updateBtn);

  mainLayout->addWidget(groupBox);
  mainLayout->addStretch();

  // Connections
  connect(m_addEdgeGroupBtn, &QPushButton::clicked, this,
          &TopologyPage::onAddEdgeGroup);
  connect(m_addFaceGroupBtn, &QPushButton::clicked, this,
          &TopologyPage::onAddFaceGroup);
  connect(m_delEdgeGroupBtn, &QPushButton::clicked, this,
          &TopologyPage::onDeleteEdgeGroup);
  connect(m_delFaceGroupBtn, &QPushButton::clicked, this,
          &TopologyPage::onDeleteFaceGroup);
  connect(m_updateBtn, &QPushButton::clicked, this,
          &TopologyPage::onUpdateViewer);

  connect(m_selModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          [this](int index) {
            emit topologySelectionModeChanged(
                m_selModeCombo->itemData(index).toInt());
          });
}

QGroupBox *
TopologyPage::createGroupSection(const QString &title,
                                 TopologyGroupTableModel *model,
                                 QPushButton *&addButton, QTableView *&tableRef,
                                 GeometryGroupDelegate *geoDelegate) {

  QGroupBox *box = new QGroupBox(title);
  QVBoxLayout *layout = new QVBoxLayout(box);

  QHBoxLayout *buttonRow = new QHBoxLayout();
  addButton = new QPushButton("+ Add");
  addButton->setMaximumWidth(60);
  QPushButton *delButton = new QPushButton("- Del");
  delButton->setMaximumWidth(60);
  delButton->setStyleSheet("color: #cc4444;");
  buttonRow->addWidget(addButton);
  buttonRow->addWidget(delButton);
  buttonRow->addStretch();
  layout->addLayout(buttonRow);

  // Store delete button reference
  if (title.contains("Edge"))
    m_delEdgeGroupBtn = delButton;
  else
    m_delFaceGroupBtn = delButton;

  QTableView *table = new QTableView();
  table->setModel(model);
  table->setItemDelegateForColumn(TopologyGroupTableModel::ColLinkedGroup,
                                  geoDelegate);
  table->setItemDelegateForColumn(TopologyGroupTableModel::ColColor,
                                  new ColorDelegate(table));
  table->setItemDelegateForColumn(TopologyGroupTableModel::ColMode,
                                  new RenderModeDelegate(table));
  table->horizontalHeader()->setStretchLastSection(true);
  table->horizontalHeader()->setSectionResizeMode(
      TopologyGroupTableModel::ColName, QHeaderView::Stretch);
  table->horizontalHeader()->setSectionResizeMode(
      TopologyGroupTableModel::ColIDs, QHeaderView::Stretch);
  table->horizontalHeader()->setSectionResizeMode(
      TopologyGroupTableModel::ColLinkedGroup, QHeaderView::Stretch);
  table->horizontalHeader()->setSectionResizeMode(
      TopologyGroupTableModel::ColColor, QHeaderView::Fixed);
  table->horizontalHeader()->setSectionResizeMode(
      TopologyGroupTableModel::ColMode, QHeaderView::Fixed);
  table->setColumnWidth(TopologyGroupTableModel::ColColor, 70);
  table->setColumnWidth(TopologyGroupTableModel::ColMode, 80);
  table->setMinimumHeight(100);
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  layout->addWidget(table);

  tableRef = table;
  return box;
}

// ============================================================================
// Group Slots
// ============================================================================

void TopologyPage::onAddEdgeGroup() { m_edgeGroupModel->addGroup(); }
void TopologyPage::onAddFaceGroup() { m_faceGroupModel->addGroup(); }

void TopologyPage::onDeleteEdgeGroup() {
  QModelIndex index = m_edgeGroupTable->currentIndex();
  if (index.isValid())
    m_edgeGroupModel->removeGroup(index.row());
}

void TopologyPage::onDeleteFaceGroup() {
  QModelIndex index = m_faceGroupTable->currentIndex();
  if (index.isValid())
    m_faceGroupModel->removeGroup(index.row());
}

void TopologyPage::onUpdateViewer() { emit updateViewerRequested(); }

void TopologyPage::initializeDefaultGroups() {
  if (!m_edgeGroupModel->hasGroup("Unused")) {
    TopologyGroup unused;
    unused.name = "Unused";
    unused.color = QColor(255, 0, 0); // Red
    unused.renderMode = RenderMode::Shaded;

    m_edgeGroupModel->setDefaultGroup(unused);
  }
  if (!m_faceGroupModel->hasGroup("Unused")) {
    TopologyGroup unused;
    unused.name = "Unused";
    unused.color = QColor(255, 0, 0, 100); // Red & Translucent
    unused.renderMode = RenderMode::Translucent;
    m_faceGroupModel->setDefaultGroup(unused);
  }
}

// ============================================================================
// Geometry Group Name Sync
// ============================================================================

void TopologyPage::setGeometryGroupNames(const QStringList &edgeNames,
                                         const QStringList &faceNames) {
  m_edgeGeoDelegate->setGroupNames(edgeNames);
  m_faceGeoDelegate->setGroupNames(faceNames);
}

// ============================================================================
// Face Highlight on Selection
// ============================================================================

void TopologyPage::onFaceSelectionChanged(QListWidgetItem *current,
                                          QListWidgetItem *previous) {
  if (m_isUpdatingSelection)
    return;
  Q_UNUSED(current);
  Q_UNUSED(previous);

  // Clear previous manual highlights - although we are moving to native
  // selection we still need to tell the viewer which items are selected based
  // on the list but wait, if we are doing two-way sync, it's better to just
  // emit what's selected. Actually, let's just use the viewer's selection as
  // source of truth. When user clicks list, we update viewer. When user clicks
  // viewer, we update list.

  for (int i = 0; i < m_faceList->count(); ++i) {
    QListWidgetItem *item = m_faceList->item(i);
    QRegExp rx("Face (\\d+):");
    if (rx.indexIn(item->text()) >= 0) {
      int id = rx.cap(1).toInt();
      emit faceHighlightRequested(id, item->isSelected());
    }
  }
}

// ============================================================================
// Topology Entity List Slots (unchanged logic, reformatted)
// ============================================================================

void TopologyPage::onNodeCreated(int id, double u, double v, int faceId) {
  Q_UNUSED(u);
  Q_UNUSED(v);
  Q_UNUSED(faceId);
  addNodeToList(id);
}

void TopologyPage::addNodeToList(int id) {
  m_nodeList->addItem(QString("Node %1").arg(id));
  initializeDefaultGroups(); // Ensure groups exist
}

void TopologyPage::onNodeMoved(int id, const gp_Pnt &p) {
  for (int i = 0; i < m_nodeList->count(); ++i) {
    QListWidgetItem *item = m_nodeList->item(i);
    if (item->text().startsWith(QString("Node %1").arg(id))) {
      item->setText(QString("Node %1: (%2, %3, %4)")
                        .arg(id)
                        .arg(p.X(), 0, 'f', 2)
                        .arg(p.Y(), 0, 'f', 2)
                        .arg(p.Z(), 0, 'f', 2));
      break;
    }
  }
}

void TopologyPage::updateNodePosition(int id, double x, double y, double z) {
  for (int i = 0; i < m_nodeList->count(); ++i) {
    QListWidgetItem *item = m_nodeList->item(i);
    if (item->text().startsWith(QString("Node %1").arg(id))) {
      item->setText(QString("Node %1: (%2, %3, %4)")
                        .arg(id)
                        .arg(x, 0, 'f', 2)
                        .arg(y, 0, 'f', 2)
                        .arg(z, 0, 'f', 2));
      break;
    }
  }
}

void TopologyPage::onNodeDeleted(int id) {
  for (int i = 0; i < m_nodeList->count(); ++i) {
    QListWidgetItem *item = m_nodeList->item(i);
    if (item->text().startsWith(QString("Node %1").arg(id))) {
      delete m_nodeList->takeItem(i);
      break;
    }
  }
}

void TopologyPage::onEdgeCreated(int n1, int n2, int id) {
  QString edgeStr =
      QString("Edge %1: Node %2 - Node %3").arg(id).arg(n1).arg(n2);
  QListWidgetItem *item = new QListWidgetItem(edgeStr);
  item->setData(Qt::UserRole, id);
  m_edgeList->addItem(item);

  if (m_autoGroupUnused) {
    m_edgeGroupModel->appendIdToGroup(id, "Unused");
  }
}

void TopologyPage::onNodesMerged(int keepId, int removeId) {
  for (int i = 0; i < m_nodeList->count(); ++i) {
    QListWidgetItem *item = m_nodeList->item(i);
    if (item->text().startsWith(QString("Node %1").arg(removeId))) {
      delete m_nodeList->takeItem(i);
      break;
    }
  }
}

void TopologyPage::onFaceCreated(int id, const QList<int> &nodeIds) {
  QString nodesStr;
  for (int nid : nodeIds) {
    if (!nodesStr.isEmpty())
      nodesStr += ", ";
    nodesStr += QString::number(nid);
  }
  if (m_faceList) {
    m_faceList->addItem(QString("Face %1: [%2]").arg(id).arg(nodesStr));
    if (m_autoGroupUnused) {
      m_faceGroupModel->appendIdToGroup(id, "Unused");
    }
  }
}

void TopologyPage::onNodeSelected(int id) {
  for (int i = 0; i < m_nodeList->count(); ++i) {
    QListWidgetItem *item = m_nodeList->item(i);
    if (item->text().startsWith(QString("Node %1").arg(id))) {
      m_nodeList->setCurrentItem(item);
      break;
    }
  }
}

void TopologyPage::onEdgeDeleted(int n1, int n2) {
  if (!m_edgeList)
    return;
  QString s1 = QString("Edge [%1-%2]").arg(n1).arg(n2);
  QString s2 = QString("Edge [%1-%2]").arg(n2).arg(n1);
  // Also try old format for backwards compat
  QString s3 = QString("Edge: %1 - %2").arg(n1).arg(n2);
  QString s4 = QString("Edge: %1 - %2").arg(n2).arg(n1);
  for (int i = 0; i < m_edgeList->count(); ++i) {
    QListWidgetItem *item = m_edgeList->item(i);
    QString t = item->text();
    if (t == s1 || t == s2 || t == s3 || t == s4) {
      delete m_edgeList->takeItem(i);
      break;
    }
  }
}

void TopologyPage::onFaceDeleted(int id) {
  if (!m_faceList)
    return;
  for (int i = 0; i < m_faceList->count(); ++i) {
    QListWidgetItem *item = m_faceList->item(i);
    if (item->text().startsWith(QString("Face %1:").arg(id))) {
      delete m_faceList->takeItem(i);
      break;
    }
  }
}

// ============================================================================
// Edge and Node Highlight on Selection
// ============================================================================

void TopologyPage::onEdgeSelectionChanged(QListWidgetItem *current,
                                          QListWidgetItem *previous) {
  if (m_isUpdatingSelection)
    return;
  Q_UNUSED(current);
  Q_UNUSED(previous);

  for (int i = 0; i < m_edgeList->count(); ++i) {
    QListWidgetItem *item = m_edgeList->item(i);
    QRegExp rx("Edge \\[(\\d+)-(\\d+)\\]");
    if (rx.indexIn(item->text()) >= 0) {
      int n1 = rx.cap(1).toInt();
      int n2 = rx.cap(2).toInt();
      emit edgeHighlightRequested(n1, n2, item->isSelected());
    }
  }
}

void TopologyPage::onNodeSelectionChanged(QListWidgetItem *current,
                                          QListWidgetItem *previous) {
  if (m_isUpdatingSelection)
    return;
  Q_UNUSED(current);
  Q_UNUSED(previous);

  for (int i = 0; i < m_nodeList->count(); ++i) {
    QListWidgetItem *item = m_nodeList->item(i);
    QRegExp rx("Node (\\d+)");
    if (rx.indexIn(item->text()) >= 0) {
      int nodeId = rx.cap(1).toInt();
      emit nodeHighlightRequested(nodeId, item->isSelected());
    }
  }
}

void TopologyPage::onTopologySelectionChanged(
    const QList<int> &nodeIds, const QList<QPair<int, int>> &edgeIds,
    const QList<int> &faceIds) {
  m_isUpdatingSelection = true;

  // Sync Nodes
  m_nodeList->clearSelection();
  for (int i = 0; i < m_nodeList->count(); ++i) {
    QListWidgetItem *item = m_nodeList->item(i);
    QRegExp rx("Node (\\d+)");
    if (rx.indexIn(item->text()) >= 0) {
      int id = rx.cap(1).toInt();
      if (nodeIds.contains(id)) {
        item->setSelected(true);
      }
    }
  }

  // Sync Edges
  m_edgeList->clearSelection();
  for (int i = 0; i < m_edgeList->count(); ++i) {
    QListWidgetItem *item = m_edgeList->item(i);
    QRegExp rx("Edge \\[(\\d+)-(\\d+)\\]");
    if (rx.indexIn(item->text()) >= 0) {
      int n1 = rx.cap(1).toInt();
      int n2 = rx.cap(2).toInt();
      if (edgeIds.contains({qMin(n1, n2), qMax(n1, n2)})) {
        item->setSelected(true);
      }
    }
  }

  // Sync Faces
  m_faceList->clearSelection();
  for (int i = 0; i < m_faceList->count(); ++i) {
    QListWidgetItem *item = m_faceList->item(i);
    QRegExp rx("Face (\\d+):");
    if (rx.indexIn(item->text()) >= 0) {
      int id = rx.cap(1).toInt();
      if (faceIds.contains(id)) {
        item->setSelected(true);
      }
    }
  }

  m_isUpdatingSelection = false;
}
