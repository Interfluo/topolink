#include "GeometryPage.h"

GeometryPage::GeometryPage(QWidget *parent) : QWidget(parent) {
  m_edgeModel = new GroupTableModel(this);
  m_faceModel = new GroupTableModel(this);
  setupUI();
}

void GeometryPage::initializeDefaultGroups(int numFaces, int numEdges) {
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
    if (existing) {
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
      }
    } else {
      model->addGroup();
      int row = model->rowCount() - 1;
      model->setData(model->index(row, 0), "Unused");
      QStringList sl;
      for (int id : unusedIds)
        sl << QString::number(id);
      model->setData(model->index(row, 1), sl.join(","));
      model->setData(model->index(row, 2), color);
    }
  };

  updateModelUnused(m_faceModel, numFaces, usedFaces, QColor(140, 140, 140));
  updateModelUnused(m_edgeModel, numEdges, usedEdges, QColor(100, 100, 100));
}

QSet<int> GeometryPage::getUsedEdgeIds() const {
  QSet<int> used;
  for (const GeometryGroup &group : m_edgeModel->groups()) {
    for (int id : group.ids)
      used.insert(id);
  }
  return used;
}

QSet<int> GeometryPage::getUsedFaceIds() const {
  QSet<int> used;
  for (const GeometryGroup &group : m_faceModel->groups()) {
    for (int id : group.ids)
      used.insert(id);
  }
  return used;
}

void GeometryPage::onAddEdgeGroup() { m_edgeModel->addGroup(); }
void GeometryPage::onAddFaceGroup() { m_faceModel->addGroup(); }

void GeometryPage::onDeleteEdgeGroup() {
  QModelIndex index = m_edgeTable->currentIndex();
  if (index.isValid())
    m_edgeModel->removeGroup(index.row());
}

void GeometryPage::onDeleteFaceGroup() {
  QModelIndex index = m_faceTable->currentIndex();
  if (index.isValid())
    m_faceModel->removeGroup(index.row());
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

  // Assign delete button internal tracking if needed, or connect signal here.
  // In original code, we stored m_delEdgeBtn etc.
  if (title.contains("Edge"))
    m_delEdgeBtn = delButton;
  else
    m_delFaceBtn = delButton;

  QTableView *table = new QTableView();
  table->setModel(model);
  table->setItemDelegateForColumn(2, new ColorDelegate(table));
  table->setItemDelegateForColumn(3, new RenderModeDelegate(table));
  table->horizontalHeader()->setStretchLastSection(true);
  table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
  table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
  table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
  table->setColumnWidth(2, 70);
  table->setColumnWidth(3, 80);
  table->setMinimumHeight(120);
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  layout->addWidget(table);

  tableRef = table;
  return box;
}

void GeometryPage::setupUI() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  QGroupBox *geometryBox = new QGroupBox("Define Geometry Groups");
  QVBoxLayout *geoLayout = new QVBoxLayout(geometryBox);

  geoLayout->addWidget(createGroupSection("Edge Groups", m_edgeModel,
                                          m_addEdgeBtn, m_edgeTable));
  geoLayout->addWidget(createGroupSection("Face Groups", m_faceModel,
                                          m_addFaceBtn, m_faceTable));

  m_updateBtn = new QPushButton("Update Viewer");
  m_updateBtn->setStyleSheet(
      "QPushButton { background-color: #4a90d9; color: white; padding: 8px; "
      "border-radius: 4px; font-weight: bold; } QPushButton:hover { "
      "background-color: #5aa0e9; } QPushButton:pressed { background-color: "
      "#3a80c9; }");
  geoLayout->addWidget(m_updateBtn);

  QHBoxLayout *csvLayout = new QHBoxLayout();
  m_exportBtn = new QPushButton("Export CSV");
  m_importBtn = new QPushButton("Import CSV");
  // m_exportBtn->setMaximumWidth(100);
  // m_importBtn->setMaximumWidth(100);
  csvLayout->addWidget(m_exportBtn);
  csvLayout->addWidget(m_importBtn);
  csvLayout->addStretch();
  geoLayout->addLayout(csvLayout);

  mainLayout->addWidget(geometryBox);
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
