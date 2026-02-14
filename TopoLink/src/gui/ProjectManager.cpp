#include "ProjectManager.h"
#include "MainWindow.h"
#include "OccView.h"
#include "pages/GeometryPage.h"
#include "pages/TopologyPage.h"

#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>

ProjectManager::ProjectManager(MainWindow *mainWindow, QObject *parent)
    : QObject(parent), m_mainWindow(mainWindow) {}

bool ProjectManager::saveProject(const QString &filePath) {
  QJsonObject root;

  // 1. Geometry File Path
  root["geometry_file"] = m_mainWindow->m_lastImportedStepPath;

  // 2. Geometry Groups
  QJsonObject geomFaceGroups;
  for (const auto &group : m_mainWindow->m_geometryPage->faceGroups()) {
    if (group.name == "Unused")
      continue; // Skip default
    QJsonObject g;
    QJsonArray ids;
    for (int id : group.ids)
      ids.append(id);
    g["face_ids"] = ids;
    g["color"] = QJsonArray(
        {group.color.red(), group.color.green(), group.color.blue()});
    g["rendering"] = (group.renderMode == RenderMode::Shaded) ? "shaded"
                     : (group.renderMode == RenderMode::Translucent)
                         ? "translucent"
                         : "hidden";
    geomFaceGroups[group.name] = g;
  }
  root["geom_face_groups"] = geomFaceGroups;

  QJsonObject geomEdgeGroups;
  for (const auto &group : m_mainWindow->m_geometryPage->edgeGroups()) {
    if (group.name == "Unused")
      continue;
    QJsonObject g;
    QJsonArray ids;
    for (int id : group.ids)
      ids.append(id);
    g["edge_ids"] = ids;
    g["color"] = QJsonArray(
        {group.color.red(), group.color.green(), group.color.blue()});
    geomEdgeGroups[group.name] = g;
  }
  root["geom_edge_groups"] = geomEdgeGroups;

  // 3. Topology Data (Core)
  QJsonObject topoJson = m_mainWindow->m_topology->toJson();
  root["topo_nodes"] = topoJson["topo_nodes"];
  root["topo_edges"] = topoJson["topo_edges"];
  root["topo_faces"] = topoJson["topo_faces"];

  // 4. Topology Groups (from TopologyPage)
  QJsonObject topoFaceGroups;
  for (const auto &group :
       m_mainWindow->m_topologyPage->faceGroupModel()->groups()) {
    QJsonObject g;
    QJsonArray ids;
    for (int id : group.ids)
      ids.append(id);
    g["face_ids"] = ids;
    g["name"] = group.name;
    g["color"] = QJsonArray(
        {group.color.red(), group.color.green(), group.color.blue()});
    g["geometry_id"] = group.linkedGeometryGroup;
    topoFaceGroups[group.name] = g;
  }
  root["topo_face_groups"] = topoFaceGroups;

  QJsonObject topoEdgeGroups;
  for (const auto &group :
       m_mainWindow->m_topologyPage->edgeGroupModel()->groups()) {
    QJsonObject g;
    QJsonArray ids;
    for (int id : group.ids)
      ids.append(id);
    g["edge_ids"] = ids;
    g["name"] = group.name;
    g["color"] = QJsonArray(
        {group.color.red(), group.color.green(), group.color.blue()});
    g["geometry_id"] = group.linkedGeometryGroup;
    topoEdgeGroups[group.name] = g;
  }
  root["topo_edge_groups"] = topoEdgeGroups;

  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }

  QJsonDocument doc(root);
  file.write(doc.toJson());
  m_currentProjectPath = filePath;
  return true;
}

bool ProjectManager::loadProject(const QString &filePath) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }

  QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  if (doc.isNull() || !doc.isObject()) {
    return false;
  }

  QJsonObject root = doc.object();

  // 1. Load Geometry
  if (root.contains("geometry_file")) {
    QString stepPath = root["geometry_file"].toString();
    if (QFile::exists(stepPath)) {
      // Note: We need a way to import without prompt if possible, or just use
      // the existing onImportStp but it has a dialog. For now, let's assume we
      // can add a method to MainWindow or just let the user know. Actually, we
      // can just call reader logic here or add importStep(path) to MainWindow.
      m_mainWindow->importStep(stepPath);
    }
  }

  // 2. Load Topology Data
  m_mainWindow->m_topology->fromJson(root);

  // 3. Load Geometry Groups
  if (root.contains("geom_face_groups")) {
    QList<GeometryGroup> groups;
    QJsonObject obj = root["geom_face_groups"].toObject();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
      GeometryGroup g;
      g.name = it.key();
      QJsonObject data = it.value().toObject();
      QJsonArray ids = data["face_ids"].toArray();
      for (const auto &id : ids)
        g.ids.append(id.toInt());
      QJsonArray color = data["color"].toArray();
      g.color = QColor(color[0].toInt(), color[1].toInt(), color[2].toInt());
      QString rend = data["rendering"].toString();
      g.renderMode = (rend == "shaded")        ? RenderMode::Shaded
                     : (rend == "translucent") ? RenderMode::Translucent
                                               : RenderMode::Hidden;
      groups.append(g);
    }
    m_mainWindow->m_geometryPage->setFaceGroups(groups);
  }

  if (root.contains("geom_edge_groups")) {
    QList<GeometryGroup> groups;
    QJsonObject obj = root["geom_edge_groups"].toObject();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
      GeometryGroup g;
      g.name = it.key();
      QJsonObject data = it.value().toObject();
      QJsonArray ids = data["edge_ids"].toArray();
      for (const auto &id : ids)
        g.ids.append(id.toInt());
      QJsonArray color = data["color"].toArray();
      g.color = QColor(color[0].toInt(), color[1].toInt(), color[2].toInt());
      groups.append(g);
    }
    m_mainWindow->m_geometryPage->setEdgeGroups(groups);
  }

  // 4. Load Topology Groups
  if (root.contains("topo_face_groups")) {
    QList<TopologyGroup> groups;
    QJsonObject obj = root["topo_face_groups"].toObject();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
      TopologyGroup g;
      g.name = it.key();
      QJsonObject data = it.value().toObject();
      QJsonArray ids = data["face_ids"].toArray();
      for (const auto &id : ids)
        g.ids.append(id.toInt());
      QJsonArray color = data["color"].toArray();
      if (color.size() >= 3)
        g.color = QColor(color[0].toInt(), color[1].toInt(), color[2].toInt());
      g.linkedGeometryGroup = data["geometry_id"].toString();
      groups.append(g);
    }
    m_mainWindow->m_topologyPage->setFaceGroups(groups);
  }

  if (root.contains("topo_edge_groups")) {
    QList<TopologyGroup> groups;
    QJsonObject obj = root["topo_edge_groups"].toObject();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
      TopologyGroup g;
      g.name = it.key();
      QJsonObject data = it.value().toObject();
      QJsonArray ids = data["edge_ids"].toArray();
      for (const auto &id : ids)
        g.ids.append(id.toInt());
      QJsonArray color = data["color"].toArray();
      if (color.size() >= 3)
        g.color = QColor(color[0].toInt(), color[1].toInt(), color[2].toInt());
      g.linkedGeometryGroup = data["geometry_id"].toString();
      groups.append(g);
    }
    m_mainWindow->m_topologyPage->setEdgeGroups(groups);
  }

  // 5. Restore Visuals and UI
  m_mainWindow->restoreTopologyToView();

  // 6. Apply Appearances (Groups)
  m_mainWindow->onUpdateGeometryGroups();
  m_mainWindow->onUpdateTopologyGroups();

  m_currentProjectPath = filePath;
  return true;
}

QJsonObject ProjectManager::serializeGeometryGroups() { return QJsonObject(); }
QJsonObject ProjectManager::serializeTopologyGroups() { return QJsonObject(); }
void ProjectManager::deserializeGeometryGroups(const QJsonObject &json) {}
void ProjectManager::deserializeTopologyGroups(const QJsonObject &json) {}
