#pragma once

#include <QObject>
#include <QString>

class QJsonObject;
class MainWindow;

class ProjectManager : public QObject {
  Q_OBJECT
public:
  explicit ProjectManager(MainWindow *mainWindow, QObject *parent = nullptr);

  bool saveProject(const QString &filePath);
  bool loadProject(const QString &filePath);

  QString currentProjectPath() const { return m_currentProjectPath; }

private:
  MainWindow *m_mainWindow;
  QString m_currentProjectPath;

  QJsonObject serializeGeometryGroups();
  QJsonObject serializeTopologyGroups();

  void deserializeGeometryGroups(const QJsonObject &json);
  void deserializeTopologyGroups(const QJsonObject &json);
};
