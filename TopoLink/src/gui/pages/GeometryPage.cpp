#include "GeometryPage.h"

GeometryPage::GeometryPage(QWidget *parent) : QWidget(parent) {
  m_edgeModel = new GroupTableModel(this);
  m_faceModel = new GroupTableModel(this);

  // Setup automatic repopulation of "Unused" when other groups change
  auto onGroupChanged = [this]() {
    if (m_isUpdatingUnused)
      return;
    repopulateUnused(m_numFaces, m_numEdges);
  };

  connect(m_edgeModel, &QAbstractTableModel::dataChanged, onGroupChanged);
  connect(m_edgeModel, &QAbstractTableModel::rowsRemoved, onGroupChanged);
  connect(m_faceModel, &QAbstractTableModel::dataChanged, onGroupChanged);
  connect(m_faceModel, &QAbstractTableModel::rowsRemoved, onGroupChanged);

  setupUI();
}

void GeometryPage::initializeDefaultGroups(int numFaces, int numEdges) {
  m_numFaces = numFaces;
  m_numEdges = numEdges;

  GeometryGroup unusedFaces;
  unusedFaces.name = "Unused";
  unusedFaces.color = QColor(140, 140, 140);
  unusedFaces.renderMode = RenderMode::Shaded;
  for (int i = 1; i <= numFaces; ++i)
    unusedFaces.ids.append(i);
  m_faceModel->setDefaultGroup(unusedFaces);

  GeometryGroup unusedEdges;
  unusedEdges.name = "Unused";
  unusedEdges.color = QColor(100, 100, 100);
  unusedEdges.renderMode = RenderMode::Shaded;
  for (int i = 1; i <= numEdges; ++i)
    unusedEdges.ids.append(i);
  m_edgeModel->setDefaultGroup(unusedEdges);
}

void GeometryPage::repopulateUnused(int numFaces, int numEdges) {
  if (m_isUpdatingUnused)
    return;
  m_isUpdatingUnused = true;

  QSet<int> usedFaces = getUsedFaceIds();
  QSet<int> usedEdges = getUsedEdgeIds();

  auto updateModelUnused = [](GroupTableModel *model, int count,
                              const QSet<int> &used, const QColor &color) {
    QList<int> unusedIds;
    for (int i = 1; i <= count; ++i) {
      if (!used.contains(i))
        unusedIds.append(i);
    }

    const GeometryGroup *existing = model->getGroupByName("Unused");
    int row = -1;
    for (int i = 0; i < model->rowCount(); ++i) {
      if (model->data(model->index(i, 0)).toString() == "Unused") {
        row = i;
        break;
      }
    }

    if (row != -1) {
      QStringList sl;
      for (int id : unusedIds)
        sl << QString::number(id);
      model->setData(model->index(row, 1), sl.join(","));
    } else {
      model->addGroup();
      int newRow = model->rowCount() - 1;
      model->setData(model->index(newRow, 0), "Unused");
      QStringList sl;
      for (int id : unusedIds)
        sl << QString::number(id);
      model->setData(model->index(newRow, 1), sl.join(","));
      model->setData(model->index(newRow, 2), color);
    }
  };

  updateModelUnused(m_faceModel, numFaces, usedFaces, QColor(140, 140, 140));
  updateModelUnused(m_edgeModel, numEdges, usedEdges, QColor(100, 100, 100));

  m_isUpdatingUnused = false;
}

QSet<int> GeometryPage::getUsedEdgeIds() const {
  QSet<int> used;
  for (const GeometryGroup &group : m_edgeModel->groups()) {
    if (group.name == "Unused")
      continue;
    for (int id : group.ids)
      used.insert(id);
  }
  return used;
}

QSet<int> GeometryPage::getUsedFaceIds() const {
  QSet<int> used;
  for (const GeometryGroup &group : m_faceModel->groups()) {
    if (group.name == "Unused")
      continue;
    for (int id : group.ids)
      used.insert(id);
  }
  return used;
}

void GeometryPage::onAddEdgeGroup() { m_edgeModel->addGroup(); }
void GeometryPage::onAddFaceGroup() { m_faceModel->addGroup(); }

void GeometryPage::onDeleteEdgeGroup() {
  QModelIndex index = m_edgeTable->currentIndex();
  if (index.isValid()) {
    if (m_edgeModel->data(m_edgeModel->index(index.row(), 0)).toString() ==
        "Unused") {
      QMessageBox::warning(this, "Protected Group",
                           "The 'Unused' group cannot be deleted.");
      return;
    }
    m_edgeModel->removeGroup(index.row());
  }
}

void GeometryPage::onDeleteFaceGroup() {
  QModelIndex index = m_faceTable->currentIndex();
  if (index.isValid()) {
    if (m_faceModel->data(m_faceModel->index(index.row(), 0)).toString() ==
        "Unused") {
      QMessageBox::warning(this, "Protected Group",
                           "The 'Unused' group cannot be deleted.");
      return;
    }
    m_faceModel->removeGroup(index.row());
  }
}

void GeometryPage::onHighlightGroup(const QModelIndex &index) {
  if (!index.isValid() || index.column() != GroupTableModel::ColHighlight)
    return;

  QTableView *table = qobject_cast<QTableView *>(sender());
  GroupTableModel *model = (table == m_edgeTable) ? m_edgeModel : m_faceModel;

  const GeometryGroup &group = model->groups().at(index.row());
  if (table == m_edgeTable)
    emit edgeGroupHighlightRequested(group.ids);
  else
    emit faceGroupHighlightRequested(group.ids);
}

void GeometryPage::onUpdateViewer() { emit updateViewerRequested(); }

void GeometryPage::onExportCsv() {
  QString fileName = QFileDialog::getSaveFileName(
      this, "Export Groups", "", "CSV files (*.csv)", nullptr,
      QFileDialog::DontUseNativeDialog);
  if (fileName.isEmpty())
    return;

  QFile file(fileName);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QMessageBox::warning(this, "Error", "Could not open file for writing.");
    return;
  }

  QTextStream out(&file);
  out << "Type,Name,IDs,Color,RenderMode\n";

  auto writeGroup = [&](const GeometryGroup &group, const QString &type) {
    QStringList ids;
    for (int id : group.ids)
      ids << QString::number(id);
    QString modeStr;
    switch (group.renderMode) {
    case RenderMode::Shaded:
      modeStr = "Shaded";
      break;
    case RenderMode::Translucent:
      modeStr = "Translucent";
      break;
    case RenderMode::Hidden:
      modeStr = "Hidden";
      break;
    }
    out << type << "," << group.name << "," << ids.join(";") << ","
        << group.color.name() << "," << modeStr << "\n";
  };

  for (const GeometryGroup &group : m_edgeModel->groups())
    writeGroup(group, "Edge");
  for (const GeometryGroup &group : m_faceModel->groups())
    writeGroup(group, "Face");

  file.close();
  QMessageBox::information(this, "Export Complete",
                           "Groups exported to: " + fileName);
}

void GeometryPage::onImportCsv() {
  QString fileName = QFileDialog::getOpenFileName(
      this, "Import Groups", "", "CSV files (*.csv);;All files (*)", nullptr,
      QFileDialog::DontUseNativeDialog);
  if (fileName.isEmpty())
    return;

  QFile file(fileName);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QMessageBox::warning(this, "Error", "Could not open file for reading.");
    return;
  }

  m_edgeModel->clearGroups();
  m_faceModel->clearGroups();

  QTextStream in(&file);
  bool firstLine = true;
  while (!in.atEnd()) {
    QString line = in.readLine();
    if (firstLine) {
      firstLine = false;
      continue;
    }
    QStringList parts = line.split(",");
    if (parts.size() < 5)
      continue;

    GeometryGroup group;
    QString type = parts[0].trimmed();
    group.name = parts[1].trimmed();
    for (const QString &idStr : parts[2].split(";")) {
      bool ok;
      int id = idStr.trimmed().toInt(&ok);
      if (ok)
        group.ids.append(id);
    }
    group.color = QColor(parts[3].trimmed());
    QString modeStr = parts[4].trimmed();
    if (modeStr == "Translucent")
      group.renderMode = RenderMode::Translucent;
    else if (modeStr == "Hidden")
      group.renderMode = RenderMode::Hidden;
    else
      group.renderMode = RenderMode::Shaded;

    GroupTableModel *model = (type == "Edge")
                                 ? m_edgeModel
                                 : ((type == "Face") ? m_faceModel : nullptr);
    if (model) {
      model->addGroup();
      int row = model->rowCount() - 1;
      model->setData(model->index(row, 0), group.name);
      QStringList idStrs;
      for (int id : group.ids)
        idStrs << QString::number(id);
      model->setData(model->index(row, 1), idStrs.join(","));
      model->setData(model->index(row, 2), group.color);
      model->setData(model->index(row, 3), static_cast<int>(group.renderMode));
    }
  }
  file.close();
  QMessageBox::information(this, "Import Complete",
                           "Groups imported. Click 'Update Viewer' to apply.");
}

QGroupBox *GeometryPage::createGroupSection(const QString &title,
                                            GroupTableModel *model,
                                            QPushButton *&addButton,
                                            QTableView *&tableRef) {
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

  // Assign buttons internal tracking
  if (title.contains("Edge")) {
    m_delEdgeBtn = delButton;
  } else {
    m_delFaceBtn = delButton;
  }

  QTableView *table = new QTableView();
  table->setModel(model);
  table->setItemDelegateForColumn(GroupTableModel::ColColor,
                                  new ColorDelegate(table));
  table->setItemDelegateForColumn(GroupTableModel::ColRenderMode,
                                  new RenderModeDelegate(table));
  table->setItemDelegateForColumn(GroupTableModel::ColHighlight,
                                  new HighlightButtonDelegate(table));

  table->horizontalHeader()->setStretchLastSection(false);
  table->horizontalHeader()->setSectionResizeMode(GroupTableModel::ColName,
                                                  QHeaderView::Stretch);
  table->horizontalHeader()->setSectionResizeMode(GroupTableModel::ColIDs,
                                                  QHeaderView::Stretch);
  table->horizontalHeader()->setSectionResizeMode(GroupTableModel::ColColor,
                                                  QHeaderView::Fixed);
  table->horizontalHeader()->setSectionResizeMode(
      GroupTableModel::ColRenderMode, QHeaderView::Fixed);
  table->horizontalHeader()->setSectionResizeMode(GroupTableModel::ColHighlight,
                                                  QHeaderView::Fixed);

  table->setColumnWidth(GroupTableModel::ColColor, 60);
  table->setColumnWidth(GroupTableModel::ColRenderMode, 80);
  table->setColumnWidth(GroupTableModel::ColHighlight, 85);

  table->setMinimumHeight(120);
  table->setSelectionBehavior(QAbstractItemView::SelectRows);

  connect(table, &QTableView::clicked, this, &GeometryPage::onHighlightGroup);
  layout->addWidget(table);

  tableRef = table;
  return box;
}

void GeometryPage::setupUI() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  // Create Tab Widget
  m_tabWidget = new QTabWidget();
  m_tabWidget->setStyleSheet(
      "QTabWidget::pane { border: none; }"
      "QLabel { color: #333333; background-color: transparent; }"
      "QGroupBox { color: #333333; font-weight: bold; }"
      "QCheckBox { color: #333333; }"); // Clean look with dark text

  // Tab 1: Edge Groups
  QWidget *edgeTab = new QWidget();
  QVBoxLayout *edgeLayout = new QVBoxLayout(edgeTab);
  edgeLayout->setContentsMargins(5, 5, 5, 5);
  edgeLayout->addWidget(createGroupSection("Edge Groups", m_edgeModel,
                                           m_addEdgeBtn, m_edgeTable));
  m_tabWidget->addTab(edgeTab, "Edge Groups");

  // Tab 2: Face Groups
  QWidget *faceTab = new QWidget();
  QVBoxLayout *faceLayout = new QVBoxLayout(faceTab);
  faceLayout->setContentsMargins(5, 5, 5, 5);
  faceLayout->addWidget(createGroupSection("Face Groups", m_faceModel,
                                           m_addFaceBtn, m_faceTable));
  m_tabWidget->addTab(faceTab, "Face Groups");

  mainLayout->addWidget(m_tabWidget);

  // Controls below tabs
  m_updateBtn = new QPushButton("Update Viewer");
  m_updateBtn->setStyleSheet(
      "QPushButton { background-color: #4a90d9; color: white; padding: 8px; "
      "border-radius: 4px; font-weight: bold; } QPushButton:hover { "
      "background-color: #5aa0e9; } QPushButton:pressed { background-color: "
      "#3a80c9; }");
  mainLayout->addWidget(m_updateBtn);

  QHBoxLayout *csvLayout = new QHBoxLayout();
  m_exportBtn = new QPushButton("Export CSV");
  m_importBtn = new QPushButton("Import CSV");

  // Simplify buttons for clean look
  QString smallBtnStyle =
      "QPushButton { background-color: transparent; border: 1px solid #666; "
      "color: #ccc; padding: 4px; border-radius: 3px; } QPushButton:hover { "
      "background-color: #444; }";
  m_exportBtn->setStyleSheet(smallBtnStyle);
  m_importBtn->setStyleSheet(smallBtnStyle);

  csvLayout->addWidget(m_exportBtn);
  csvLayout->addWidget(m_importBtn);
  csvLayout->addStretch();
  mainLayout->addLayout(csvLayout);

  mainLayout->addStretch();

  connect(m_addEdgeBtn, &QPushButton::clicked, this,
          &GeometryPage::onAddEdgeGroup);
  connect(m_addFaceBtn, &QPushButton::clicked, this,
          &GeometryPage::onAddFaceGroup);
  connect(m_delEdgeBtn, &QPushButton::clicked, this,
          &GeometryPage::onDeleteEdgeGroup);
  connect(m_delFaceBtn, &QPushButton::clicked, this,
          &GeometryPage::onDeleteFaceGroup);
  connect(m_updateBtn, &QPushButton::clicked, this,
          &GeometryPage::onUpdateViewer);
  connect(m_exportBtn, &QPushButton::clicked, this, &GeometryPage::onExportCsv);
  connect(m_importBtn, &QPushButton::clicked, this, &GeometryPage::onImportCsv);
}

void GeometryPage::showEdgeGroups() {
  if (m_tabWidget)
    m_tabWidget->setCurrentIndex(0);
}

void GeometryPage::showFaceGroups() {
  if (m_tabWidget)
    m_tabWidget->setCurrentIndex(1);
}
