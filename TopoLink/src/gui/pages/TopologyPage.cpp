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

  // Setup automatic repopulation of "Unused"
  auto onGroupChanged = [this]() { repopulateUnused(); };
  connect(m_edgeGroupModel, &QAbstractTableModel::dataChanged, onGroupChanged);
  connect(m_edgeGroupModel, &QAbstractTableModel::rowsRemoved, onGroupChanged);
  connect(m_faceGroupModel, &QAbstractTableModel::dataChanged, onGroupChanged);
  connect(m_faceGroupModel, &QAbstractTableModel::rowsRemoved, onGroupChanged);

  setupUI();
}

void TopologyPage::setupUI() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);

  // Create Tab Widget
  m_tabWidget = new QTabWidget();
  m_tabWidget->setStyleSheet(
      "QTabWidget::pane { border: none; }"
      "QLabel { color: #333333; background-color: transparent; }"
      "QGroupBox { color: #333333; font-weight: bold; }"
      "QCheckBox { color: #333333; }");

  // --- Tab 1: Entities ---
  QWidget *entitiesTab = new QWidget();
  QVBoxLayout *entitiesLayout = new QVBoxLayout(entitiesTab);
  entitiesLayout->setContentsMargins(5, 5, 5, 5);

  // Selection Mode (keep in entities tab)
  QGroupBox *selModeGroupBox = new QGroupBox("Interaction Control");
  QHBoxLayout *selModeLayout = new QHBoxLayout(selModeGroupBox);
  selModeLayout->addWidget(new QLabel("Selection Mode:"));
  m_selModeCombo = new QComboBox();
  m_selModeCombo->addItem("Nodes", 0);
  m_selModeCombo->addItem("Edges", 1);
  m_selModeCombo->addItem("Faces", 2);
  selModeLayout->addWidget(m_selModeCombo);
  selModeLayout->addStretch();
  entitiesLayout->addWidget(selModeGroupBox);

  // Entity Lists
  QGroupBox *entityBox = new QGroupBox("Topology Entities");
  QVBoxLayout *entityLayout = new QVBoxLayout(entityBox);

  QLabel *nodeLabel = new QLabel("Nodes", this);
  nodeLabel->setStyleSheet("font-weight: bold; background-color: transparent;");
  entityLayout->addWidget(nodeLabel);
  m_nodeList = new QListWidget(this);
  m_nodeList->setMaximumHeight(80);
  m_nodeList->setSelectionMode(QAbstractItemView::ExtendedSelection);
  entityLayout->addWidget(m_nodeList);

  QLabel *edgeLabel = new QLabel("Edges", this);
  edgeLabel->setStyleSheet("font-weight: bold; background-color: transparent;");
  entityLayout->addWidget(edgeLabel);
  m_edgeList = new QListWidget(this);
  m_edgeList->setMaximumHeight(80);
  m_edgeList->setSelectionMode(QAbstractItemView::ExtendedSelection);
  entityLayout->addWidget(m_edgeList);

  QLabel *faceLabel = new QLabel("Faces", this);
  faceLabel->setStyleSheet("font-weight: bold; background-color: transparent;");
  entityLayout->addWidget(faceLabel);
  m_faceList = new QListWidget(this);
  m_faceList->setMaximumHeight(80);
  m_faceList->setSelectionMode(QAbstractItemView::ExtendedSelection);
  entityLayout->addWidget(m_faceList);

  entityLayout->addStretch();
  entitiesLayout->addWidget(entityBox);
  m_tabWidget->addTab(entitiesTab, "Entities");

  // --- Tab 2: Groups ---
  QWidget *groupsTab = new QWidget();
  QVBoxLayout *groupsLayout = new QVBoxLayout(groupsTab);
  groupsLayout->setContentsMargins(5, 5, 5, 5);

  QGroupBox *groupBox = new QGroupBox("Topology Groups");
  QVBoxLayout *groupLayout = new QVBoxLayout(groupBox);

  groupLayout->addWidget(createGroupSection("Edge Groups", m_edgeGroupModel,
                                            m_addEdgeGroupBtn, m_edgeGroupTable,
                                            m_edgeGeoDelegate));

  groupLayout->addWidget(createGroupSection("Face Groups", m_faceGroupModel,
                                            m_addFaceGroupBtn, m_faceGroupTable,
                                            m_faceGeoDelegate));

  // Update Viewer Button inside Groups tab (or both? or outside?)
  // Let's put it outside
  groupsLayout->addWidget(groupBox);
  groupsLayout->addStretch();
  m_tabWidget->addTab(groupsTab, "Groups");

  mainLayout->addWidget(m_tabWidget);

  // Update Viewer Button (Global)
  m_updateBtn = new QPushButton("Update Viewer");
  m_updateBtn->setStyleSheet(
      "QPushButton { background-color: #4a90d9; color: white; padding: 8px; "
      "border-radius: 4px; font-weight: bold; } QPushButton:hover { "
      "background-color: #5aa0e9; } QPushButton:pressed { background-color: "
      "#3a80c9; }");
  mainLayout->addWidget(m_updateBtn);

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

  // Re-connect list signals (they were in the deleted block)
  connect(m_faceList, &QListWidget::itemSelectionChanged, this,
          [this]() { onFaceSelectionChanged(nullptr, nullptr); });
  connect(m_edgeList, &QListWidget::itemSelectionChanged, this,
          [this]() { onEdgeSelectionChanged(nullptr, nullptr); });
  connect(m_nodeList, &QListWidget::itemSelectionChanged, this,
          [this]() { onNodeSelectionChanged(nullptr, nullptr); });
}

void TopologyPage::showEntities() {
  if (m_tabWidget)
    m_tabWidget->setCurrentIndex(0);
}

void TopologyPage::showGroups() {
  if (m_tabWidget)
    m_tabWidget->setCurrentIndex(1);
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
  repopulateUnused();
}

void TopologyPage::repopulateUnused() {
  if (m_isUpdatingUnused)
    return;
  m_isUpdatingUnused = true;

  auto repopulate = [this](TopologyGroupTableModel *model, QListWidget *list,
                           const QColor &color, RenderMode mode) {
    QSet<int> universe;
    for (int i = 0; i < list->count(); ++i) {
      bool ok;
      int id = list->item(i)->data(Qt::UserRole).toInt(&ok);
      if (ok)
        universe.insert(id);
      else {
        // Fallback to text parsing if needed
        QRegExp rx("(Edge|Face|Node) (\\d+)"); // Updated regex to include Node
        if (rx.indexIn(list->item(i)->text()) >= 0) {
          universe.insert(rx.cap(2).toInt());
        }
      }
    }

    QSet<int> used;
    int unusedRow = -1;
    for (int i = 0; i < model->rowCount(); ++i) {
      QString name = model->data(model->index(i, 0)).toString();
      if (name == "Unused") {
        unusedRow = i;
      } else {
        QStringList ids = model->data(model->index(i, 1))
                              .toString()
                              .split(",", Qt::SkipEmptyParts);
        for (const QString &s : ids)
          used.insert(s.toInt());
      }
    }

    QList<int> unusedList;
    for (int id : universe) {
      if (!used.contains(id))
        unusedList.append(id);
    }

    if (unusedRow == -1) {
      model->addGroup();
      unusedRow = model->rowCount() - 1;
      model->setData(model->index(unusedRow, 0), "Unused");
      model->setData(model->index(unusedRow, 3), color);
      model->setData(model->index(unusedRow, 4), static_cast<int>(mode));
    }

    QStringList sl;
    for (int id : unusedList)
      sl << QString::number(id);
    model->setData(model->index(unusedRow, 1), sl.join(","));
  };

  repopulate(m_edgeGroupModel, m_edgeList, QColor(255, 0, 0),
             RenderMode::Shaded);
  repopulate(m_faceGroupModel, m_faceList, QColor(255, 0, 0, 100),
             RenderMode::Translucent);

  m_isUpdatingUnused = false;
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

  for (int i = 0; i < m_faceList->count(); ++i) {
    QListWidgetItem *item = m_faceList->item(i);
    // Prefer ID from UserRole if available
    if (item->data(Qt::UserRole).isValid()) {
      int id = item->data(Qt::UserRole).toInt();
      emit faceHighlightRequested(id, item->isSelected());
    } else {
      QRegExp rx("Face (\\d+):");
      if (rx.indexIn(item->text()) >= 0) {
        int id = rx.cap(1).toInt();
        emit faceHighlightRequested(id, item->isSelected());
      }
    }
  }
}

// ============================================================================
// Topology Entity List Slots
// ============================================================================

void TopologyPage::onNodeCreated(int id, double u, double v, int faceId) {
  Q_UNUSED(u);
  Q_UNUSED(v);
  Q_UNUSED(faceId);
  addNodeToList(id);
}

void TopologyPage::addNodeToList(int id) {
  // Store ID in UserRole for robust selection
  QListWidgetItem *item = new QListWidgetItem(QString("Node %1").arg(id));
  item->setData(Qt::UserRole, id);
  m_nodeList->addItem(item);
  initializeDefaultGroups();
}

void TopologyPage::onNodeMoved(int id, const gp_Pnt &p) {
  for (int i = 0; i < m_nodeList->count(); ++i) {
    QListWidgetItem *item = m_nodeList->item(i);
    // Check ID matches (robust check)
    bool match = false;
    if (item->data(Qt::UserRole).isValid()) {
      if (item->data(Qt::UserRole).toInt() == id)
        match = true;
    } else if (item->text().startsWith(QString("Node %1").arg(id))) {
      match = true;
    }

    if (match) {
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
  // Wrapper for consistency
  onNodeMoved(id, gp_Pnt(x, y, z));
}

void TopologyPage::onNodeDeleted(int id) {
  for (int i = 0; i < m_nodeList->count(); ++i) {
    QListWidgetItem *item = m_nodeList->item(i);
    bool match = false;
    if (item->data(Qt::UserRole).isValid()) {
      if (item->data(Qt::UserRole).toInt() == id)
        match = true;
    } else if (item->text().startsWith(QString("Node %1").arg(id))) {
      match = true;
    }

    if (match) {
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
  } else {
    repopulateUnused();
  }
}

void TopologyPage::onNodesMerged(int keepId, int removeId) {
  onNodeDeleted(removeId);
  repopulateUnused();
}

void TopologyPage::onFaceCreated(int id, const QList<int> &nodeIds) {
  QString nodesStr;
  for (int nid : nodeIds) {
    if (!nodesStr.isEmpty())
      nodesStr += ", ";
    nodesStr += QString::number(nid);
  }
  if (m_faceList) {
    QListWidgetItem *item =
        new QListWidgetItem(QString("Face %1: [%2]").arg(id).arg(nodesStr));
    item->setData(Qt::UserRole, id);
    m_faceList->addItem(item);
    if (m_autoGroupUnused) {
      m_faceGroupModel->appendIdToGroup(id, "Unused");
    } else {
      repopulateUnused();
    }
  }
}

void TopologyPage::onNodeSelected(int id) {
  for (int i = 0; i < m_nodeList->count(); ++i) {
    QListWidgetItem *item = m_nodeList->item(i);
    if (item->data(Qt::UserRole).toInt() == id) {
      m_nodeList->setCurrentItem(item);
      break;
    }
  }
}

void TopologyPage::onEdgeDeleted(int n1, int n2) {
  if (!m_edgeList)
    return;

  // We don't easily know the ID here unless we search by nodes
  // Loop through items and check text pattern "Node n1 - Node n2"
  QString pattern1 = QString("Node %1 - Node %2").arg(n1).arg(n2);
  QString pattern2 = QString("Node %2 - Node %1").arg(n1).arg(n2);

  for (int i = 0; i < m_edgeList->count(); ++i) {
    QListWidgetItem *item = m_edgeList->item(i);
    QString t = item->text();
    if (t.contains(pattern1) || t.contains(pattern2)) {
      int id = item->data(Qt::UserRole).toInt();
      m_edgeGroupModel->removeIdFromAllGroups(id);
      delete m_edgeList->takeItem(i);
      repopulateUnused();
      break;
    }
  }
}

void TopologyPage::onFaceDeleted(int id) {
  if (!m_faceList)
    return;
  for (int i = 0; i < m_faceList->count(); ++i) {
    QListWidgetItem *item = m_faceList->item(i);
    if (item->data(Qt::UserRole).toInt() == id) {
      m_faceGroupModel->removeIdFromAllGroups(id);
      delete m_faceList->takeItem(i);
      repopulateUnused();
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

    // Updated Regex to match "Edge X: Node A - Node B"
    QRegExp rx("Node (\\d+) - Node (\\d+)");

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

    // Prefer ID from UserRole (set in addNodeToList)
    if (item->data(Qt::UserRole).isValid()) {
      int nodeId = item->data(Qt::UserRole).toInt();
      emit nodeHighlightRequested(nodeId, item->isSelected());
    } else {
      // Fallback for any legacy items (shouldn't happen with new addNodeToList)
      QRegExp rx("Node (\\d+)");
      if (rx.indexIn(item->text()) >= 0) {
        int nodeId = rx.cap(1).toInt();
        emit nodeHighlightRequested(nodeId, item->isSelected());
      }
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
    int id = -1;
    if (item->data(Qt::UserRole).isValid()) {
      id = item->data(Qt::UserRole).toInt();
    } else {
      QRegExp rx("Node (\\d+)");
      if (rx.indexIn(item->text()) >= 0)
        id = rx.cap(1).toInt();
    }

    if (id != -1 && nodeIds.contains(id)) {
      item->setSelected(true);
    }
  }

  // Sync Edges
  m_edgeList->clearSelection();
  for (int i = 0; i < m_edgeList->count(); ++i) {
    QListWidgetItem *item = m_edgeList->item(i);
    // Parse nodes from text to match against edgeIds pair
    QRegExp rx("Node (\\d+) - Node (\\d+)");
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
    int id = -1;
    if (item->data(Qt::UserRole).isValid()) {
      id = item->data(Qt::UserRole).toInt();
    } else {
      QRegExp rx("Face (\\d+):");
      if (rx.indexIn(item->text()) >= 0)
        id = rx.cap(1).toInt();
    }

    if (id != -1 && faceIds.contains(id)) {
      item->setSelected(true);
    }
  }

  m_isUpdatingSelection = false;
}