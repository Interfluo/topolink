#pragma once
#include "../core/Topology.h"
#include "OccView.h"
#include "pages/GeometryPage.h"
#include "pages/SmootherPage.h"
#include "pages/TopologyPage.h"
#include <QMainWindow>

#include <QDockWidget>
#include <QLabel>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>
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

  // Wizard Navigation
  void onNextPage();
  void onBackPage();
  void onPageChanged(int index);

private:
  OccView *m_occView;
  QDockWidget *m_controlsDock;
  QDockWidget *m_consoleDock;
  QTextEdit *m_console;

  // Wizard Components
  QStackedWidget *m_pageStack;
  GeometryPage *m_geometryPage;
  TopologyPage *m_topologyPage;
  SmootherPage *m_smootherPage;

  QPushButton *m_nextBtn;
  QPushButton *m_backBtn;
  QLabel *m_pageTitle;

  TopTools_IndexedMapOfShape *m_faceMap;
  TopTools_IndexedMapOfShape *m_edgeMap;
  TopTools_IndexedMapOfShape *m_vertexMap;

  // Core data model
  Topology *m_topology = nullptr;
  QString m_lastImportedStepPath;

  friend class ProjectManager;
};
