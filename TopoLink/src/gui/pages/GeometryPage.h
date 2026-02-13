#pragma once

#include "../GroupDelegates.h"

#include <QAbstractTableModel>
#include <QComboBox>
#include <QDebug>
#include <QFileDialog>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSet>
#include <QTableView>
#include <QTextStream>
#include <QVBoxLayout>
#include <QWidget>

// ============================================================================
// Data Structures & Models
// ============================================================================

struct GeometryGroup {
  QString name;
  QList<int> ids;
  QColor color;
  RenderMode renderMode;

  GeometryGroup()
      : name("New Group"), color(Qt::red), renderMode(RenderMode::Shaded) {}
};

class GroupTableModel : public QAbstractTableModel {
  Q_OBJECT
public:
  explicit GroupTableModel(QObject *parent = nullptr)
      : QAbstractTableModel(parent) {}

  int rowCount(const QModelIndex &parent = QModelIndex()) const override {
    if (parent.isValid())
      return 0;
    return m_groups.size();
  }
  int columnCount(const QModelIndex &parent = QModelIndex()) const override {
    if (parent.isValid())
      return 0;
    return 4; // Name, IDs, Color, RenderMode
  }
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override {
    if (!index.isValid() || index.row() >= m_groups.size())
      return QVariant();
    const GeometryGroup &group = m_groups.at(index.row());

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
      switch (index.column()) {
      case 0:
        return group.name;
      case 1: {
        QStringList idStrings;
        for (int id : group.ids)
          idStrings << QString::number(id);
        return idStrings.join(",");
      }
      case 2:
        return QString();
      case 3:
        switch (group.renderMode) {
        case RenderMode::Shaded:
          return "Shaded";
        case RenderMode::Translucent:
          return "Translucent";
        case RenderMode::Hidden:
          return "Hidden";
        }
      }
    } else if (role == Qt::BackgroundRole && index.column() == 2) {
      return group.color;
    } else if (role == Qt::UserRole) {
      if (index.column() == 2)
        return group.color;
      if (index.column() == 3)
        return static_cast<int>(group.renderMode);
    }
    return QVariant();
  }
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
      switch (section) {
      case 0:
        return "Name";
      case 1:
        return "IDs";
      case 2:
        return "Color";
      case 3:
        return "Mode";
      }
    }
    return QVariant();
  }
  Qt::ItemFlags flags(const QModelIndex &index) const override {
    if (!index.isValid())
      return Qt::NoItemFlags;

    // For "Unused" group, the IDs column should be read-only
    if (index.column() == 1 && m_groups[index.row()].name == "Unused") {
      return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    }

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
  }
  bool setData(const QModelIndex &index, const QVariant &value,
               int role = Qt::EditRole) override {
    if (!index.isValid() || role != Qt::EditRole)
      return false;
    GeometryGroup &group = m_groups[index.row()];
    switch (index.column()) {
    case 0:
      group.name = value.toString();
      break;
    case 1: { // IDs
      QString str = value.toString();
      QStringList parts = str.split(",", Qt::SkipEmptyParts);
      QList<int> newIds;
      for (const QString &part : parts) {
        bool ok;
        int id = part.trimmed().toInt(&ok);
        if (ok) {
          removeIdFromAllGroups(id);
          newIds.append(id);
        }
      }
      group.ids = newIds;
      break;
    }
    case 2:
      if (value.canConvert<QColor>())
        group.color = value.value<QColor>();
      break;
    case 3:
      group.renderMode = static_cast<RenderMode>(value.toInt());
      break;
    default:
      return false;
    }
    emit dataChanged(index, index, {role});
    return true;
  }

  void addGroup() {
    beginInsertRows(QModelIndex(), m_groups.size(), m_groups.size());
    m_groups.append(GeometryGroup());
    endInsertRows();
  }
  void removeGroup(int row) {
    if (row < 0 || row >= m_groups.size())
      return;
    beginRemoveRows(QModelIndex(), row, row);
    m_groups.removeAt(row);
    endRemoveRows();
  }
  void setDefaultGroup(const GeometryGroup &group) {
    beginResetModel();
    m_groups.clear();
    m_groups.append(group);
    endResetModel();
  }

  void clearGroups() {
    beginResetModel();
    m_groups.clear();
    endResetModel();
  }

  void removeIdFromAllGroups(int id) {
    for (int i = 0; i < m_groups.size(); ++i) {
      if (m_groups[i].ids.removeOne(id)) {
        emit dataChanged(index(i, 1), index(i, 1)); // Column 1 is IDs
      }
    }
  }

  void appendIdToGroup(int id, const QString &groupName) {
    removeIdFromAllGroups(id);
    for (int i = 0; i < m_groups.size(); ++i) {
      if (m_groups[i].name == groupName) {
        if (!m_groups[i].ids.contains(id)) {
          m_groups[i].ids.append(id);
          emit dataChanged(index(i, 1), index(i, 1));
        }
        return;
      }
    }
  }

  const QList<GeometryGroup> &groups() const { return m_groups; }

  // Get list of group names for use in dropdowns
  QStringList groupNames() const {
    QStringList names;
    for (const GeometryGroup &g : m_groups)
      names << g.name;
    return names;
  }

  const GeometryGroup *getGroupByName(const QString &name) const {
    for (const auto &g : m_groups) {
      if (g.name == name)
        return &g;
    }
    return nullptr;
  }

private:
  QList<GeometryGroup> m_groups;
};

// ============================================================================
// GeometryPage Class
// ============================================================================

class GeometryPage : public QWidget {
  Q_OBJECT
public:
  explicit GeometryPage(QWidget *parent = nullptr);

  const QList<GeometryGroup> &edgeGroups() const {
    return m_edgeModel->groups();
  }
  const QList<GeometryGroup> &faceGroups() const {
    return m_faceModel->groups();
  }

  void clearGroups() {
    m_edgeModel->clearGroups();
    m_faceModel->clearGroups();
  }

  const GeometryGroup *getEdgeGroupByName(const QString &name) const {
    return m_edgeModel->getGroupByName(name);
  }

  const GeometryGroup *getFaceGroupByName(const QString &name) const {
    return m_faceModel->getGroupByName(name);
  }

  void setEdgeGroups(const QList<GeometryGroup> &groups) {
    m_edgeModel->clearGroups();
    for (const auto &g : groups) {
      m_edgeModel->addGroup();
      int last = m_edgeModel->rowCount() - 1;
      m_edgeModel->setData(m_edgeModel->index(last, 0), g.name);
      m_edgeModel->setData(m_edgeModel->index(last, 1), [g]() {
        QStringList sl;
        for (int id : g.ids)
          sl << QString::number(id);
        return sl.join(",");
      }());
      m_edgeModel->setData(m_edgeModel->index(last, 2), g.color);
      m_edgeModel->setData(m_edgeModel->index(last, 3),
                           static_cast<int>(g.renderMode));
    }
  }

  void setFaceGroups(const QList<GeometryGroup> &groups) {
    m_faceModel->clearGroups();
    for (const auto &g : groups) {
      m_faceModel->addGroup();
      int last = m_faceModel->rowCount() - 1;
      m_faceModel->setData(m_faceModel->index(last, 0), g.name);
      m_faceModel->setData(m_faceModel->index(last, 1), [g]() {
        QStringList sl;
        for (int id : g.ids)
          sl << QString::number(id);
        return sl.join(",");
      }());
      m_faceModel->setData(m_faceModel->index(last, 2), g.color);
      m_faceModel->setData(m_faceModel->index(last, 3),
                           static_cast<int>(g.renderMode));
    }
  }

  void initializeDefaultGroups(int numFaces, int numEdges);
  void repopulateUnused(int numFaces, int numEdges);
  QSet<int> getUsedEdgeIds() const;
  QSet<int> getUsedFaceIds() const;

signals:
  void updateViewerRequested();

private slots:
  void onAddEdgeGroup();
  void onAddFaceGroup();
  void onDeleteEdgeGroup();
  void onDeleteFaceGroup();
  void onUpdateViewer();
  void onExportCsv();
  void onImportCsv();

private:
  void setupUI();
  QGroupBox *createGroupSection(const QString &title, GroupTableModel *model,
                                QPushButton *&addButton, QTableView *&tableRef);

  GroupTableModel *m_edgeModel;
  GroupTableModel *m_faceModel;
  QTableView *m_edgeTable;
  QTableView *m_faceTable;
  QPushButton *m_addEdgeBtn;
  QPushButton *m_addFaceBtn;
  QPushButton *m_delEdgeBtn;
  QPushButton *m_delFaceBtn;
  QPushButton *m_updateBtn;
  QPushButton *m_exportBtn;
  QPushButton *m_importBtn;
  int m_numFaces = 0;
  int m_numEdges = 0;
  bool m_isUpdatingUnused = false;
};
