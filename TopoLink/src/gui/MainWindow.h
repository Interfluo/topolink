#pragma once
#include "../core/Topology.h"
#include "BannerWidget.h" // [NEW]
#include "OccView.h"
#include "pages/GeometryPage.h"
#include "pages/SmootherPage.h"
#include "pages/TopologyPage.h"
#include <QGridLayout> // [NEW]
#include <QMainWindow>

#include <QDockWidget> // [NEW]
#include <QLabel>
#include <QPushButton>
#include <QShortcut>
#include <QTextEdit>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS_Shape.hxx>

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

  bool importStep(const QString &fileName);
  void restoreTopologyToView();

  // Log message to console
  void logMessage(const QString &msg);

private slots:
  void onImportStp(); // Slot for the menu action
  void onSelectionModeChanged(int id);
  void onShapeSelected(const TopoDS_Shape &shape);
  void onSaveProject();
  void onLoadProject();
  void onUpdateGeometryGroups();
  void onUpdateTopologyGroups();
  void onRunSolver();

  // Navigation
  void onPageChanged(int index);

private:
  // UI Components
  BannerWidget *m_banner;
  OccView *m_occView;
  QDockWidget *m_consoleDock;
  QTextEdit *m_console;

  // Pages (Overlays)
  GeometryPage *m_geometryPage;
  TopologyPage *m_topologyPage;
  SmootherPage *m_smootherPage;

  TopTools_IndexedMapOfShape *m_faceMap;
  TopTools_IndexedMapOfShape *m_edgeMap;
  TopTools_IndexedMapOfShape *m_vertexMap;

  // Core data model
  Topology *m_topology = nullptr;
  QString m_lastImportedStepPath;

  QTimer *m_syncTimer;

  friend class ProjectManager;
};
