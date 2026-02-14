#pragma once

#include "../GroupDelegates.h"

#include <QAbstractTableModel>
#include <QComboBox>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTabWidget>
#include <QTableView>
#include <QVBoxLayout>
#include <QWidget>
#include <gp_Pnt.hxx>

// ============================================================================
// Topology Group Data & Model
// ============================================================================

struct TopologyGroup {
  QString name;
  QList<int> ids;
  QColor color;
  RenderMode renderMode;
  QString linkedGeometryGroup;

  TopologyGroup()
      : name("New Group"), color(Qt::green), renderMode(RenderMode::Shaded) {}
};

class TopologyGroupTableModel : public QAbstractTableModel {
  Q_OBJECT
public:
  // Columns: 0=Name, 1=IDs, 2=Linked Group, 3=Color, 4=Mode
  enum Column {
    ColName = 0,
    ColIDs,
    ColLinkedGroup,
    ColColor,
    ColMode,
    ColCount
  };

  explicit TopologyGroupTableModel(QObject *parent = nullptr)
      : QAbstractTableModel(parent) {}

  int rowCount(const QModelIndex &parent = QModelIndex()) const override {
    if (parent.isValid())
      return 0;
    return m_groups.size();
  }
  int columnCount(const QModelIndex &parent = QModelIndex()) const override {
    if (parent.isValid())
      return 0;
    return ColCount;
  }
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override {
    if (!index.isValid() || index.row() >= m_groups.size())
      return QVariant();
    const TopologyGroup &group = m_groups.at(index.row());

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
      switch (index.column()) {
      case ColName:
        return group.name;
      case ColIDs: {
        QStringList idStrings;
        for (int id : group.ids)
          idStrings << QString::number(id);
        return idStrings.join(",");
      }
      case ColLinkedGroup:
        return group.linkedGeometryGroup;
      case ColColor:
        return QString();
      case ColMode:
        switch (group.renderMode) {
        case RenderMode::Shaded:
          return "Shaded";
        case RenderMode::Translucent:
          return "Translucent";
        case RenderMode::Hidden:
          return "Hidden";
        }
      }
    } else if (role == Qt::BackgroundRole && index.column() == ColColor) {
      return group.color;
    } else if (role == Qt::UserRole) {
      if (index.column() == ColColor)
        return group.color;
      if (index.column() == ColMode)
        return static_cast<int>(group.renderMode);
      if (index.column() == ColLinkedGroup)
        return group.linkedGeometryGroup;
    }
    return QVariant();
  }

  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
      switch (section) {
      case ColName:
        return "Name";
      case ColIDs:
        return "IDs";
      case ColLinkedGroup:
        return "Linked Group";
      case ColColor:
        return "Color";
      case ColMode:
        return "Mode";
      }
    }
    return QVariant();
  }

  Qt::ItemFlags flags(const QModelIndex &index) const override {
    if (!index.isValid())
      return Qt::NoItemFlags;

    // For "Unused" group, the IDs column should be read-only
    if (index.column() == ColIDs && m_groups[index.row()].name == "Unused") {
      return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    }

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
  }

  bool setData(const QModelIndex &index, const QVariant &value,
               int role = Qt::EditRole) override {
    if (!index.isValid() || role != Qt::EditRole)
      return false;
    TopologyGroup &group = m_groups[index.row()];
    switch (index.column()) {
    case ColName:
      group.name = value.toString();
      break;
    case ColIDs: {
      QStringList parts = value.toString().split(",", Qt::SkipEmptyParts);
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
    case ColLinkedGroup:
      group.linkedGeometryGroup = value.toString();
      break;
    case ColColor:
      if (value.canConvert<QColor>())
        group.color = value.value<QColor>();
      break;
    case ColMode:
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
    m_groups.append(TopologyGroup());
    endInsertRows();
  }
  void removeGroup(int row) {
    if (row < 0 || row >= m_groups.size())
      return;
    beginRemoveRows(QModelIndex(), row, row);
    m_groups.removeAt(row);
    endRemoveRows();
  }

  void setDefaultGroup(const TopologyGroup &group) {
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
        emit dataChanged(index(i, ColIDs), index(i, ColIDs));
      }
    }
  }

  void appendIdToGroup(int id, const QString &groupName) {
    removeIdFromAllGroups(id);
    for (int i = 0; i < m_groups.size(); ++i) {
      if (m_groups[i].name == groupName) {
        if (!m_groups[i].ids.contains(id)) {
          m_groups[i].ids.append(id);
          emit dataChanged(index(i, ColIDs), index(i, ColIDs));
        }
        return;
      }
    }
  }

  bool hasGroup(const QString &name) const {
    for (const auto &g : m_groups) {
      if (g.name == name)
        return true;
    }
    return false;
  }

  const QList<TopologyGroup> &groups() const { return m_groups; }

private:
  QList<TopologyGroup> m_groups;
};

// ============================================================================
// Geometry Group Delegate (Dropdown populated from GeometryPage groups)
// ============================================================================

class GeometryGroupDelegate : public QStyledItemDelegate {
  Q_OBJECT
public:
  explicit GeometryGroupDelegate(QObject *parent = nullptr)
      : QStyledItemDelegate(parent) {}

  void setGroupNames(const QStringList &names) { m_groupNames = names; }

  QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                        const QModelIndex &index) const override;
  void setEditorData(QWidget *editor, const QModelIndex &index) const override;
  void setModelData(QWidget *editor, QAbstractItemModel *model,
                    const QModelIndex &index) const override;

private:
  QStringList m_groupNames;
};

// ============================================================================
// TopologyPage Class
// ============================================================================

class TopologyPage : public QWidget {
  Q_OBJECT
public:
  explicit TopologyPage(QWidget *parent = nullptr);

  // Existing topology entity list management
  void addNodeToList(int id);
  void updateNodePosition(int id, double x, double y, double z);

  TopologyGroupTableModel *edgeGroupModel() const { return m_edgeGroupModel; }
  TopologyGroupTableModel *faceGroupModel() const { return m_faceGroupModel; }

  void initializeDefaultGroups();
  void repopulateUnused();

  // Update geometry group names for linked group dropdowns
  void setGeometryGroupNames(const QStringList &edgeNames,
                             const QStringList &faceNames);

  void setAutoGroupUnused(bool autoGroup) { m_autoGroupUnused = autoGroup; }

  void appendEdgeIdToGroup(int id, const QString &groupName) {
    m_edgeGroupModel->appendIdToGroup(id, groupName);
  }

  void appendFaceIdToGroup(int id, const QString &groupName) {
    m_faceGroupModel->appendIdToGroup(id, groupName);
  }

  void setEdgeGroups(const QList<TopologyGroup> &groups) {
    m_edgeGroupModel->clearGroups();
    for (const auto &g : groups) {
      m_edgeGroupModel->addGroup();
      int last = m_edgeGroupModel->rowCount() - 1;
      m_edgeGroupModel->setData(
          m_edgeGroupModel->index(last, TopologyGroupTableModel::ColName),
          g.name);
      m_edgeGroupModel->setData(
          m_edgeGroupModel->index(last, TopologyGroupTableModel::ColIDs),
          [g]() {
            QStringList sl;
            for (int id : g.ids)
              sl << QString::number(id);
            return sl.join(",");
          }());
      m_edgeGroupModel->setData(
          m_edgeGroupModel->index(last,
                                  TopologyGroupTableModel::ColLinkedGroup),
          g.linkedGeometryGroup);
      m_edgeGroupModel->setData(
          m_edgeGroupModel->index(last, TopologyGroupTableModel::ColColor),
          g.color);
      m_edgeGroupModel->setData(
          m_edgeGroupModel->index(last, TopologyGroupTableModel::ColMode),
          static_cast<int>(g.renderMode));
    }
  }

  void setFaceGroups(const QList<TopologyGroup> &groups) {
    m_faceGroupModel->clearGroups();
    for (const auto &g : groups) {
      m_faceGroupModel->addGroup();
      int last = m_faceGroupModel->rowCount() - 1;
      m_faceGroupModel->setData(
          m_faceGroupModel->index(last, TopologyGroupTableModel::ColName),
          g.name);
      m_faceGroupModel->setData(
          m_faceGroupModel->index(last, TopologyGroupTableModel::ColIDs),
          [g]() {
            QStringList sl;
            for (int id : g.ids)
              sl << QString::number(id);
            return sl.join(",");
          }());
      m_faceGroupModel->setData(
          m_faceGroupModel->index(last,
                                  TopologyGroupTableModel::ColLinkedGroup),
          g.linkedGeometryGroup);
      m_faceGroupModel->setData(
          m_faceGroupModel->index(last, TopologyGroupTableModel::ColColor),
          g.color);
      m_faceGroupModel->setData(
          m_faceGroupModel->index(last, TopologyGroupTableModel::ColMode),
          static_cast<int>(g.renderMode));
    }
  }

public slots:
  void onNodeCreated(int id, double u, double v, int faceId);
  void onNodeMoved(int id, const gp_Pnt &p);
  void onNodeDeleted(int id);
  void onNodeSelected(int id);
  void onEdgeCreated(int n1, int n2, int id);
  void onEdgeDeleted(int n1, int n2);

  void onNodesMerged(int keepId, int removeId);
  void onFaceCreated(int id, const QList<int> &nodeIds);
  void onFaceDeleted(int id);
  void onTopologySelectionChanged(const QList<int> &nodeIds,
                                  const QList<QPair<int, int>> &edgeIds,
                                  const QList<int> &faceIds);

  void clear(bool clearGroups = true) {
    m_nodeList->clear();
    m_edgeList->clear();
    m_faceList->clear();
    if (clearGroups) {
      m_edgeGroupModel->clearGroups();
      m_faceGroupModel->clearGroups();
    }
  }

signals:
  void updateViewerRequested();
  void faceHighlightRequested(int faceId, bool highlight);
  void edgeHighlightRequested(int n1, int n2, bool highlight);
  void nodeHighlightRequested(int id, bool highlight);
  void topologySelectionModeChanged(int mode);

public slots:
  void showEntities();
  void showGroups();

private slots:
  void onAddEdgeGroup();
  void onAddFaceGroup();
  void onDeleteEdgeGroup();
  void onDeleteFaceGroup();
  void onUpdateViewer();
  void onFaceSelectionChanged(QListWidgetItem *current,
                              QListWidgetItem *previous);
  void onEdgeSelectionChanged(QListWidgetItem *current,
                              QListWidgetItem *previous);
  void onNodeSelectionChanged(QListWidgetItem *current,
                              QListWidgetItem *previous);

private:
  void setupUI();
  QGroupBox *createGroupSection(const QString &title,
                                TopologyGroupTableModel *model,
                                QPushButton *&addButton, QTableView *&tableRef,
                                GeometryGroupDelegate *geoDelegate);

  QTabWidget *m_tabWidget;

  // Entity lists (read-only info)
  QListWidget *m_nodeList;
  QListWidget *m_edgeList;
  QListWidget *m_faceList;

  // Group tables
  TopologyGroupTableModel *m_edgeGroupModel;
  TopologyGroupTableModel *m_faceGroupModel;
  QTableView *m_edgeGroupTable;
  QTableView *m_faceGroupTable;
  QPushButton *m_addEdgeGroupBtn;
  QPushButton *m_addFaceGroupBtn;
  QPushButton *m_delEdgeGroupBtn;
  QPushButton *m_delFaceGroupBtn;
  QPushButton *m_updateBtn;

  // Geometry group delegates (hold group name lists)
  GeometryGroupDelegate *m_edgeGeoDelegate;
  GeometryGroupDelegate *m_faceGeoDelegate;

  QComboBox *m_selModeCombo;

  int m_lastHighlightedFaceId = -1;
  QPair<int, int> m_lastHighlightedEdge = {-1, -1};
  int m_lastHighlightedNodeId = -1;
  bool m_isUpdatingSelection = false;
  bool m_isUpdatingUnused = false;
  bool m_autoGroupUnused = true;
};
