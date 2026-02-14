#include "MainWindow.h"
#include "ProjectManager.h"
#include "pages/ConvergencePlot.h"
#include <QAction>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QScrollArea>
#include <QShortcut>
#include <QVBoxLayout>
#include <cstdio>

// OCCT Includes
#include <AIS_ColoredShape.hxx>
#include <AIS_Shape.hxx>
#include <QDebug>
#include <STEPControl_Reader.hxx>
#include <TopAbs.hxx>
#include <TopExp.hxx>
#include <TopoDS_Shape.hxx>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
  // Initialize Map Pointers
  m_faceMap = new TopTools_IndexedMapOfShape();
  m_edgeMap = new TopTools_IndexedMapOfShape();
  m_vertexMap = new TopTools_IndexedMapOfShape();

  // Initialize Topology data model
  m_topology = new Topology();

  // 1. Setup UI Layout
  resize(1200, 800);

  QWidget *centralWidget = new QWidget(this);
  setCentralWidget(centralWidget);

  QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // Banner
  m_banner = new BannerWidget(this);
  mainLayout->addWidget(m_banner);

  // Content Area (Overlay Container)
  // We need a container where OccView is the bottom layer, and Pages are
  // transparent overlays on top. A clean way is to use a QGridLayout where
  // everything occupies (0,0).
  QWidget *viewContainer = new QWidget(this);
  QGridLayout *viewLayout = new QGridLayout(viewContainer);
  viewLayout->setContentsMargins(0, 0, 0, 0);

  m_occView = new OccView(viewContainer);
  m_occView->setTopologyModel(m_topology);

  // Initialize Pages as independent tool windows
  m_geometryPage = new GeometryPage(this);
  m_geometryPage->setWindowFlags(Qt::Tool);
  m_geometryPage->setWindowTitle("Geometry Groups");
  m_geometryPage->hide();

  m_topologyPage = new TopologyPage(this);
  m_topologyPage->setWindowFlags(Qt::Tool);
  m_topologyPage->setWindowTitle("Topology Toolkit");
  m_topologyPage->hide();

  m_smootherPage = new SmootherPage(this);
  m_smootherPage->setWindowFlags(Qt::Tool);
  m_smootherPage->setWindowTitle("Smoother Controls");
  m_smootherPage->hide();

  // Add OccView to grid
  viewLayout->addWidget(m_occView, 0, 0);

  mainLayout->addWidget(viewContainer);

  // Connect Banner Signals
  connect(m_banner, &BannerWidget::modeChanged, this,
          &MainWindow::onPageChanged);
  connect(m_banner, &BannerWidget::importRequested, this,
          &MainWindow::onImportStp);
  connect(m_banner, &BannerWidget::saveRequested, this,
          &MainWindow::onSaveProject);
  connect(m_banner, &BannerWidget::consoleToggleRequested, [this]() {
    if (m_consoleDock)
      m_consoleDock->setVisible(!m_consoleDock->isVisible());
  });

  // Configure initial view (Geometry)
  // Note: we'll call onBannerModeChanged(0) at end of constructor

  // Connect geometry panel update signal
  connect(m_geometryPage, &GeometryPage::updateViewerRequested, this,
          &MainWindow::onUpdateGeometryGroups);

  // Connect topology panel signals
  connect(m_topologyPage, &TopologyPage::updateViewerRequested, this,
          &MainWindow::onUpdateTopologyGroups);
  connect(m_topologyPage, &TopologyPage::faceHighlightRequested, m_occView,
          &OccView::highlightTopologyFace);
  connect(m_topologyPage, &TopologyPage::edgeHighlightRequested, m_occView,
          &OccView::highlightTopologyEdge);
  connect(m_topologyPage, &TopologyPage::nodeHighlightRequested, m_occView,
          &OccView::highlightTopologyNode);
  connect(m_topologyPage, &TopologyPage::topologySelectionModeChanged,
          m_occView, &OccView::setTopologySelectionMode);

  // Workbench request from OccView HUD (Legacy, but keeping standard signals)
  connect(m_occView, &OccView::workbenchRequested, [this](int index) {
    m_banner->setMode(index); // Sync banner
    onPageChanged(index);     // Sync logic
  });

  connect(m_smootherPage, &SmootherPage::runSolverRequested, this,
          &MainWindow::onRunSolver);

  // Smoother Plotting Connections
  connect(
      m_occView, &OccView::smootherIterationReported, this,
      [this](int id, int iter, double error) {
        if (m_smootherPage) {
          if (m_smootherPage->plot())
            m_smootherPage->plot()->addPoint(id, iter, error);

          QString msg =
              (id < 0)
                  ? QString("Smoothing Edge %1 (it %2)").arg(-id).arg(iter + 1)
                  : QString("Smoothing Face %1 (it %2)").arg(id).arg(iter + 1);
          m_smootherPage->setStatusText(msg);
        }
      });

  connect(m_occView, &OccView::smootherFinished, this, [this]() {
    if (m_smootherPage) {
      if (m_smootherPage->runButton())
        m_smootherPage->runButton()->setEnabled(true);
      m_smootherPage->setStatusText("");
    }
    logMessage("Smoothing complete.");
  });

  // Bottom Console Dock
  m_consoleDock = new QDockWidget("Console", this);
  m_consoleDock->setAllowedAreas(Qt::BottomDockWidgetArea |
                                 Qt::TopDockWidgetArea);
  m_consoleDock->setFeatures(QDockWidget::DockWidgetClosable |
                             QDockWidget::DockWidgetMovable |
                             QDockWidget::DockWidgetFloatable);
  m_console = new QTextEdit();
  m_console->setReadOnly(true);
  m_consoleDock->setWidget(m_console);
  addDockWidget(Qt::BottomDockWidgetArea, m_consoleDock);

  // 3. Create Menu
  QMenu *fileMenu = menuBar()->addMenu("&File");
  QAction *importAction = new QAction("&Import STP...", this);
  importAction->setShortcut(QKeySequence("Ctrl+O"));
  fileMenu->addAction(importAction);

  // View Menu to toggle docks
  QMenu *viewMenu = menuBar()->addMenu("&View");

  viewMenu->addAction(m_consoleDock->toggleViewAction());

  // 4. Connect Signals
  connect(importAction, &QAction::triggered, this, &MainWindow::onImportStp);

  QAction *openProjAction = new QAction("&Open Project...", this);
  openProjAction->setShortcut(QKeySequence("Ctrl+L"));
  fileMenu->addAction(openProjAction);
  connect(openProjAction, &QAction::triggered, this,
          &MainWindow::onLoadProject);

  QAction *saveProjAction = new QAction("&Save Project...", this);
  saveProjAction->setShortcut(QKeySequence("Ctrl+S"));
  fileMenu->addAction(saveProjAction);
  connect(saveProjAction, &QAction::triggered, this,
          &MainWindow::onSaveProject);

  // Selection signal from OccView
  connect(m_occView, &OccView::shapeSelected, this,
          &MainWindow::onShapeSelected);

  // Topology signals
  connect(m_occView, &OccView::topologyNodeCreated,
          [this](int id, double, double, int) {
            if (m_topology) {
              gp_Pnt p = m_occView->getTopologyNodePosition(id);
              m_topology->createNodeWithID(id, p);
              // Also update UI Page
              m_topologyPage->onNodeCreated(id, 0, 0,
                                            0); // params unused in UI list
            }
          });

  connect(m_occView, &OccView::topologyNodeMoved,
          [this](int id, const gp_Pnt &p) {
            if (m_topology) {
              m_topology->updateNodePosition(id, p);
              m_topologyPage->onNodeMoved(id, p);
            }
          });

  connect(m_occView, &OccView::topologyNodeDeleted, [this](int id) {
    if (m_topology) {
      m_topology->deleteNode(id);
      m_topologyPage->onNodeDeleted(id);
    }
  });

  connect(m_occView, &OccView::topologyEdgeDeleted, [this](int n1, int n2) {
    if (m_topology) {
      TopoEdge *e = m_topology->getEdge(n1, n2);
      if (e) {
        m_topology->deleteEdge(e->getID());
      }
    }
    m_topologyPage->onEdgeDeleted(n1, n2);
  });

  connect(m_occView, &OccView::topologyFaceDeleted, [this](int id) {
    if (m_topology) {
      m_topology->deleteFace(id);
    }
    m_topologyPage->onFaceDeleted(id);
  });

  connect(m_occView, &OccView::topologySelectionChanged, [this]() {
    m_topologyPage->onTopologySelectionChanged(m_occView->getSelectedNodeIds(),
                                               m_occView->getSelectedEdgeIds(),
                                               m_occView->getSelectedFaceIds());
  });

  connect(m_occView, &OccView::topologyEdgeCreated,
          [this](int n1, int n2, int id) {
            if (m_topology) {
              TopoNode *node1 = m_topology->getNode(n1);
              TopoNode *node2 = m_topology->getNode(n2);
              if (node1 && node2) {
                m_topology->createEdgeWithID(id, node1, node2);
                m_topologyPage->onEdgeCreated(n1, n2, id);
              }
            }
          });

  // connect(m_occView, &OccView::topologyNodeCreated, m_topologyPage,
  //        &TopologyPage::onNodeCreated); // Replaced by lambda above

  connect(m_occView, &OccView::topologyFaceCreated,
          [this](int id, const QList<int> &nodeIds) {
            if (m_topology) {
              std::vector<TopoEdge *> edges;
              for (int i = 0; i < nodeIds.size(); ++i) {
                int n1 = nodeIds[i];
                int n2 = nodeIds[(i + 1) % nodeIds.size()];
                TopoEdge *edge = m_topology->getEdge(n1, n2);
                if (edge)
                  edges.push_back(edge);
              }
              if (edges.size() == (size_t)nodeIds.size()) {
                m_topology->createFaceWithID(id, edges);
              } else {
                qDebug() << "MainWindow: Error - partial edges for face" << id;
              }
            }
            m_topologyPage->onFaceCreated(id, nodeIds);
          });

  connect(m_occView, &OccView::topologyNodeSelected, m_topologyPage,
          &TopologyPage::onNodeSelected);

  // Connect merge signal to update data model
  connect(
      m_occView, &OccView::topologyNodesMerged,
      [this](int keepId, int removeId) {
        qDebug() << "MainWindow: Received topologyNodesMerged(" << keepId << ","
                 << removeId << ")";
        if (m_topology) {
          // Record existing entity IDs
          QSet<int> oldFaceIds;
          for (auto const &[fid, face] : m_topology->getFaces()) {
            oldFaceIds.insert(fid);
          }
          QSet<QPair<int, int>> oldEdgePairs;
          for (auto const &[eid, edge] : m_topology->getEdges()) {
            int n1 = edge->getStartNode()->getID();
            int n2 = edge->getEndNode()->getID();
            oldEdgePairs.insert(qMakePair(qMin(n1, n2), qMax(n1, n2)));
          }

          qDebug() << "MainWindow: Calling m_topology->mergeNodes(" << keepId
                   << "," << removeId << ")";
          m_topology->mergeNodes(keepId, removeId);
          qDebug() << "MainWindow: mergeNodes returned. Detecting deletions...";

          // Visual orchestration
          m_occView->mergeTopologyNodeEdges(keepId, removeId);
          m_occView->removeTopologyNode(removeId);

          // Detect deleted or modified faces
          for (int fid : oldFaceIds) {
            auto *face = m_topology->getFace(fid);
            if (face == nullptr) {
              qDebug() << "MainWindow: Detected face deletion in core for fid:"
                       << fid;
              m_occView->removeTopologyFace(fid);
              m_topologyPage->onFaceDeleted(fid);
            } else {
              // The face survived. Refresh it with its updated node list.
              QList<int> newNodeIds;
              auto *he = face->getBoundary();
              if (he) {
                auto *startHe = he;
                int safety = 0;
                do {
                  if (he->origin) {
                    newNodeIds.append(he->origin->getID());
                  }
                  he = he->next;
                  if (++safety > Topology::kHalfEdgeLoopLimit) {
                    qDebug() << "MainWindow: Safety break in "
                                "topologyNodesMerged for face"
                             << fid;
                    break;
                  }
                } while (he && he != startHe);
              }
              if (!newNodeIds.isEmpty()) {
                m_occView->refreshFaceVisual(fid, newNodeIds);
              }
            }
          }

          // Detect deleted edges (that might have been removed by core cascade)
          // mergeTopologyNodeEdges handles some but core might delete more due
          // to self-loops
          for (auto const &pair : oldEdgePairs) {
            if (m_topology->getEdge(pair.first, pair.second) == nullptr) {
              m_occView->removeTopologyEdge(pair.first, pair.second);
              m_topologyPage->onEdgeDeleted(pair.first, pair.second);
            }
          }

          logMessage(
              QString("Merged node %1 into node %2").arg(removeId).arg(keepId));
          m_topologyPage->onNodesMerged(keepId, removeId);
          qDebug() << "MainWindow: Finished merge orchestration.";
        }
      });

  // 5. Setup Keyboard Shortcuts
  // Workbench Selector
  QShortcut *rShortcut = new QShortcut(QKeySequence("R"), this);
  connect(rShortcut, &QShortcut::activated, [this]() {
    if (m_banner)
      m_banner->setMode(0);
    onPageChanged(0);
  });

  QShortcut *tShortcut = new QShortcut(QKeySequence("T"), this);
  connect(tShortcut, &QShortcut::activated, [this]() {
    if (m_banner)
      m_banner->setMode(1);
    onPageChanged(1);
  });

  QShortcut *yShortcut = new QShortcut(QKeySequence("Y"), this);
  connect(yShortcut, &QShortcut::activated, [this]() {
    if (m_banner)
      m_banner->setMode(2);
    onPageChanged(2);
  });

  // Note: Ctrl+F, Z, C shortcuts are still handled in
  // OccView::keyPressEvent for now, but Q, W, E and R, T, Y are managed
  // here or via HUD buttons for better reliability.

  QShortcut *qShortcut = new QShortcut(QKeySequence("Q"), this);
  connect(qShortcut, &QShortcut::activated, [this]() {
    qDebug() << "MainWindow: Q Shortcut (Vertex/Nodes)";
    m_occView->setFocus();
    if (m_occView->getInteractionMode() == OccView::Mode_Geometry) {
      m_occView->setSelectionMode(1);
    } else {
      m_occView->setTopologySelectionMode(OccView::SelNodes);
    }
  });

  QShortcut *wShortcut = new QShortcut(QKeySequence("W"), this);
  connect(wShortcut, &QShortcut::activated, [this]() {
    qDebug() << "MainWindow: W Shortcut (Edge/Edges)";
    m_occView->setFocus();
    if (m_occView->getInteractionMode() == OccView::Mode_Geometry) {
      m_occView->setSelectionMode(2);
    } else {
      m_occView->setTopologySelectionMode(OccView::SelEdges);
    }
  });

  QShortcut *eShortcut = new QShortcut(QKeySequence("E"), this);
  connect(eShortcut, &QShortcut::activated, [this]() {
    qDebug() << "MainWindow: E Shortcut (Face/Faces)";
    m_occView->setFocus();
    if (m_occView->getInteractionMode() == OccView::Mode_Geometry) {
      m_occView->setSelectionMode(4);
    } else {
      m_occView->setTopologySelectionMode(OccView::SelFaces);
    }
  });

  // Console Toggle Shortcut (Ctrl+`)
  QShortcut *consoleShortcut = new QShortcut(QKeySequence("Ctrl+`"), this);
  connect(consoleShortcut, &QShortcut::activated, [this]() {
    if (m_consoleDock)
      m_consoleDock->setVisible(!m_consoleDock->isVisible());
  });

  // Initial Update
  onPageChanged(0);

  logMessage("Application started. Use Ctrl+O to import, F to fit view.");
}

MainWindow::~MainWindow() {
  delete m_faceMap;
  delete m_edgeMap;
  delete m_vertexMap;
}

void MainWindow::logMessage(const QString &msg) { m_console->append(msg); }

void MainWindow::onSelectionModeChanged(int id) {
  if (m_occView) {
    m_occView->setSelectionMode(id);
    QString modeName;
    switch (id) {
    case TopAbs_VERTEX:
      modeName = "Vertex";
      break;
    case TopAbs_EDGE:
      modeName = "Edge";
      break;
    case TopAbs_FACE:
      modeName = "Face";
      break;
    default:
      modeName = QString::number(id);
    }
    logMessage(QString("Selection mode set to: %1").arg(modeName));
  }
}

void MainWindow::onShapeSelected(const TopoDS_Shape &shape) {
  if (shape.IsNull())
    return;

  if (shape.ShapeType() == TopAbs_FACE) {
    if (m_faceMap->Contains(shape)) {
      int id = m_faceMap->FindIndex(shape);
      logMessage(QString("Selected Face ID: %1").arg(id));
    } else {
      logMessage("Selected Face (Unknown ID)");
    }
  } else if (shape.ShapeType() == TopAbs_EDGE) {
    if (m_edgeMap->Contains(shape)) {
      int id = m_edgeMap->FindIndex(shape);
      logMessage(QString("Selected Edge ID: %1").arg(id));
    } else {
      logMessage("Selected Edge (Unknown ID)");
    }
  } else if (shape.ShapeType() == TopAbs_VERTEX) {
    if (m_vertexMap->Contains(shape)) {
      int id = m_vertexMap->FindIndex(shape);
      logMessage(QString("Selected Vertex ID: %1").arg(id));
    } else {
      logMessage("Selected Vertex (Unknown ID)");
    }
  }
}

void MainWindow::onImportStp() {
  QString fileName = QFileDialog::getOpenFileName(
      this, "Import STP", QDir::homePath(), "STEP Files (*.stp *.step)",
      nullptr, QFileDialog::DontUseNativeDialog);
  if (fileName.isEmpty())
    return;

  importStep(fileName);
}

bool MainWindow::importStep(const QString &fileName) {
  logMessage("Importing: " + fileName);
  m_lastImportedStepPath = fileName;

  // Ensure View is initialized
  if (m_occView->getContext().IsNull()) {
    m_occView->init();
  }

  // Reset the view and model to clear previous state
  m_occView->reset();
  if (m_topology) {
    // Note: m_topology is a raw pointer owned by MainWindow
    delete m_topology;
    m_topology = new Topology();
    m_occView->setTopologyModel(
        m_topology); // IMPORTANT: Update viewer pointer!
  }
  if (m_geometryPage)
    m_geometryPage->clearGroups();
  if (m_topologyPage)
    m_topologyPage->clear();

  // Convert QString to C-string for OCCT
  // Keep QByteArray alive while using the pointer
  QByteArray fileNameData = fileName.toLocal8Bit();
  const char *cFileName = fileNameData.constData();

  // 1. Initialize Reader
  STEPControl_Reader reader;
  IFSelect_ReturnStatus status = reader.ReadFile(cFileName);

  if (status == IFSelect_RetDone) {
    // 2. Transfer Roots (Load into memory)
    reader.TransferRoots();

    // 3. Get the Resulting Shape
    TopoDS_Shape shape = reader.OneShape();

    // Map Shapes for ID lookup
    m_faceMap->Clear();
    m_edgeMap->Clear();
    m_vertexMap->Clear();
    TopExp::MapShapes(shape, TopAbs_FACE, *m_faceMap);
    TopExp::MapShapes(shape, TopAbs_EDGE, *m_edgeMap);
    TopExp::MapShapes(shape, TopAbs_VERTEX, *m_vertexMap);

    logMessage(QString("Loaded %1 faces, %2 edges, %3 vertices.")
                   .arg(m_faceMap->Extent())
                   .arg(m_edgeMap->Extent())
                   .arg(m_vertexMap->Extent()));

    // 4. Create an Interactive Object
    Handle(AIS_ColoredShape) aisShape = new AIS_ColoredShape(shape);
    aisShape->SetColor(Quantity_Color(0.85, 0.85, 0.85, Quantity_TOC_RGB));
    aisShape->SetMaterial(Graphic3d_NOM_PLASTIC);

    Handle(Prs3d_Drawer) drawer = aisShape->Attributes();
    drawer->SetFaceBoundaryDraw(true);
    drawer->SetFaceBoundaryAspect(
        new Prs3d_LineAspect(Quantity_NOC_BLACK, Aspect_TOL_SOLID, 1.0));

    // 5. Display it in the Context
    m_occView->getContext()->Display(aisShape, Standard_True);
    m_occView->setShapeData(shape, *m_faceMap, *m_edgeMap);
    m_occView->setAisShape(aisShape);

    if (m_geometryPage) {
      m_geometryPage->initializeDefaultGroups(m_faceMap->Extent(),
                                              m_edgeMap->Extent());
    }

    m_occView->setSelectionMode(TopAbs_FACE);
    onUpdateGeometryGroups();
    m_occView->fitAll();

    logMessage("Import successful.");
    return true;
  } else {
    QMessageBox::critical(this, "Error", "Could not read STEP file.");
    logMessage("Error: Could not read STEP file.");
    return false;
  }
}

void MainWindow::onUpdateGeometryGroups() {
  if (!m_geometryPage)
    return;

  logMessage("Updating geometry groups...");

  // Apply edge groups
  for (const GeometryGroup &group : m_geometryPage->edgeGroups()) {
    if (group.ids.isEmpty())
      continue;

    m_occView->setEdgeGroupAppearance(group.ids, group.color,
                                      static_cast<int>(group.renderMode));
    logMessage(QString("  Edge Group '%1': %2 edges, %3")
                   .arg(group.name)
                   .arg(group.ids.size())
                   .arg(group.renderMode == RenderMode::Hidden
                            ? "Hidden"
                            : (group.renderMode == RenderMode::Translucent
                                   ? "Translucent"
                                   : "Shaded")));
  }

  // Apply face groups
  for (const GeometryGroup &group : m_geometryPage->faceGroups()) {
    if (group.ids.isEmpty())
      continue;

    m_occView->setFaceGroupAppearance(group.ids, group.color,
                                      static_cast<int>(group.renderMode));
    logMessage(QString("  Face Group '%1': %2 faces, %3")
                   .arg(group.name)
                   .arg(group.ids.size())
                   .arg(group.renderMode == RenderMode::Hidden
                            ? "Hidden"
                            : (group.renderMode == RenderMode::Translucent
                                   ? "Translucent"
                                   : "Shaded")));
  }

  m_occView->update();
  logMessage("Geometry groups updated.");
}

void MainWindow::onSaveProject() {
  QString fileName = QFileDialog::getSaveFileName(
      this, "Save Project", QDir::homePath(), "TopoLink Project (*.topolink)",
      nullptr, QFileDialog::DontUseNativeDialog);
  if (fileName.isEmpty())
    return;

  if (!fileName.endsWith(".topolink"))
    fileName += ".topolink";

  ProjectManager manager(this);
  if (manager.saveProject(fileName)) {
    logMessage("Project saved to: " + fileName);
  } else {
    QMessageBox::critical(this, "Error", "Failed to save project.");
  }
}

void MainWindow::onLoadProject() {
  QString fileName = QFileDialog::getOpenFileName(
      this, "Open Project", QDir::homePath(), "TopoLink Project (*.topolink)",
      nullptr, QFileDialog::DontUseNativeDialog);
  if (fileName.isEmpty())
    return;

  ProjectManager manager(this);
  if (manager.loadProject(fileName)) {
    logMessage("Project loaded from: " + fileName);
    onUpdateGeometryGroups();
    onUpdateTopologyGroups();
  } else {
    QMessageBox::critical(this, "Error", "Failed to load project.");
  }
}

void MainWindow::onUpdateTopologyGroups() {
  if (!m_topologyPage || !m_topology)
    return;

  logMessage("Updating topology groups and derivation constraints...");
  qDebug() << "MainWindow: onUpdateTopologyGroups starting...";

  // Derive node constraints from group links
  QMap<int, OccView::NodeConstraint> newNodeConstraints;

  auto addOrMergeConstraint = [&](int nodeId,
                                  const OccView::NodeConstraint &c) {
    if (!newNodeConstraints.contains(nodeId)) {
      newNodeConstraints[nodeId] = c;
    } else {
      auto &existing = newNodeConstraints[nodeId];
      if (c.isEdgeGroup && !existing.isEdgeGroup) {
        // Edge priority: Overwrite Face with Edge
        existing = c;
      } else if (c.isEdgeGroup == existing.isEdgeGroup) {
        // Same priority: Merge geometry IDs
        for (int geoId : c.geometryIds) {
          if (!existing.geometryIds.contains(geoId)) {
            existing.geometryIds.append(geoId);
          }
        }
      }
    }
  };

  // Apply topology edge groups
  for (const TopologyGroup &group : m_topologyPage->edgeGroups()) {
    if (group.ids.isEmpty())
      continue;

    m_occView->setTopologyEdgeGroupAppearance(
        group.ids, group.color, static_cast<int>(group.renderMode));
    logMessage(QString("  Topo Edge Group '%1': %2 edges")
                   .arg(group.name)
                   .arg(group.ids.size()));

    if (!group.linkedGeometryGroup.isEmpty()) {
      const GeometryGroup *geoGroup =
          m_geometryPage->getEdgeGroupByName(group.linkedGeometryGroup);
      if (geoGroup && !geoGroup->ids.isEmpty()) {
        logMessage(
            QString("    Linking Topo Group '%1' to Geo Group '%2' (%3 edges)")
                .arg(group.name)
                .arg(geoGroup->name)
                .arg(geoGroup->ids.size()));
        for (int edgeId : group.ids) {
          TopoEdge *edge = m_topology->getEdge(edgeId);
          if (edge) {
            OccView::NodeConstraint c;
            c.type = OccView::ConstraintGeometry;
            c.geometryIds = geoGroup->ids;
            c.isEdgeGroup = true;

            addOrMergeConstraint(edge->getStartNode()->getID(), c);
            addOrMergeConstraint(edge->getEndNode()->getID(), c);
          }
        }
      } else {
        logMessage(
            QString("    WARNING: Linked Geo Group '%1' not found or empty!")
                .arg(group.linkedGeometryGroup));
      }
    }
  }

  // Apply topology face groups
  for (const TopologyGroup &group : m_topologyPage->faceGroups()) {
    if (group.ids.isEmpty())
      continue;

    m_occView->setTopologyFaceGroupAppearance(
        group.ids, group.color, static_cast<int>(group.renderMode));
    logMessage(QString("  Topo Face Group '%1': %2 faces")
                   .arg(group.name)
                   .arg(group.ids.size()));

    if (!group.linkedGeometryGroup.isEmpty()) {
      const GeometryGroup *geoGroup =
          m_geometryPage->getFaceGroupByName(group.linkedGeometryGroup);
      if (geoGroup && !geoGroup->ids.isEmpty()) {
        for (int faceId : group.ids) {
          TopoFace *face = m_topology->getFace(faceId);
          if (face) {
            OccView::NodeConstraint c;
            c.type = OccView::ConstraintGeometry;
            c.geometryIds = geoGroup->ids;
            c.isEdgeGroup = false;

            for (TopoEdge *edge : face->getEdges()) {
              addOrMergeConstraint(edge->getStartNode()->getID(), c);
              addOrMergeConstraint(edge->getEndNode()->getID(), c);
            }
          }
        }
      }
    }
  }

  m_occView->setNodeConstraints(newNodeConstraints);
  logMessage(QString("Applied constraints to %1 nodes.")
                 .arg(newNodeConstraints.size()));

  // Debug log specific nodes
  for (int nodeId : newNodeConstraints.keys()) {
    const auto &c = newNodeConstraints[nodeId];
    logMessage(QString("  Node %1: %2 constraint on %3 geos")
                   .arg(nodeId)
                   .arg(c.isEdgeGroup ? "Edge" : "Face")
                   .arg(c.geometryIds.size()));
  }

  m_occView->update();
  logMessage("Topology groups updated and constraints applied.");
}

void MainWindow::restoreTopologyToView() {
  if (!m_topology || !m_occView || !m_topologyPage)
    return;

  logMessage("Restoring topology visuals and groups...");

  // Disable automatic grouping of "Unused" while we recreate the entities
  m_topologyPage->setAutoGroupUnused(false);

  // 1. Restore Nodes
  for (const auto &[id, node] : m_topology->getNodes()) {
    m_occView->restoreTopologyNode(id, node->getPosition());
  }

  // 2. Restore Edges
  for (const auto &[id, edge] : m_topology->getEdges()) {
    m_occView->restoreTopologyEdge(id, edge->getStartNode()->getID(),
                                   edge->getEndNode()->getID());
  }

  // 3. Restore Faces
  for (const auto &[id, face] : m_topology->getFaces()) {
    QList<int> nodeIds;
    auto *he = face->getBoundary();
    if (he) {
      auto *start = he;
      int safety = 0;
      do {
        if (he->origin)
          nodeIds.append(he->origin->getID());
        he = he->next;
        if (++safety > Topology::kHalfEdgeLoopLimit) {
          qDebug() << "MainWindow: Safety break in restoreTopologyToView "
                      "(Step 3) for face"
                   << id;
          break;
        }
      } while (he && he != start);
    }
    if (!nodeIds.isEmpty()) {
      m_occView->restoreTopologyFace(id, nodeIds);
    }
  }

  // 4. Update UI Lists
  m_topologyPage->clear(false); // Do not clear groups!
  for (const auto &[id, node] : m_topology->getNodes()) {
    m_topologyPage->addNodeToList(id);
    m_topologyPage->onNodeMoved(id, node->getPosition());
  }
  for (const auto &[id, edge] : m_topology->getEdges()) {
    m_topologyPage->onEdgeCreated(edge->getStartNode()->getID(),
                                  edge->getEndNode()->getID(), id);
  }
  for (const auto &[id, face] : m_topology->getFaces()) {
    QList<int> nodeIds;
    auto *he = face->getBoundary();
    if (he) {
      auto *start = he;
      int safety = 0;
      do {
        nodeIds.append(he->origin->getID());
        he = he->next;
        if (++safety > Topology::kHalfEdgeLoopLimit) {
          qDebug() << "MainWindow: Safety break in restoreTopologyToView "
                      "(Step 4) for face"
                   << id;
          break;
        }
      } while (he && he != start);
    }
    m_topologyPage->onFaceCreated(id, nodeIds);
  }

  // Re-enable automatic grouping for future user actions
  m_topologyPage->setAutoGroupUnused(true);

  // 5. Repopulate Geometry "Unused" Groups
  if (m_geometryPage && m_faceMap && m_edgeMap) {
    m_geometryPage->repopulateUnused(m_faceMap->Extent(), m_edgeMap->Extent());
  }

  m_occView->finalizeRestoration();
  onUpdateTopologyGroups(); // Re-derive and apply constraints and appearances
  logMessage("Topology restoration complete.");
}

void MainWindow::onRunSolver() {
  if (m_smootherPage && m_topology && m_topologyPage && m_geometryPage) {
    // SYNC GROUPS TO CORE
    m_topology->clearGroups();

    // 1. Sync Face Groups
    for (const auto &g : m_topologyPage->faceGroups()) {
      QString geoIdsStr;
      if (!g.linkedGeometryGroup.isEmpty()) {
        const GeometryGroup *gg =
            m_geometryPage->getFaceGroupByName(g.linkedGeometryGroup);
        if (gg) {
          QStringList sl;
          for (int id : gg->ids)
            sl << QString::number(id);
          geoIdsStr = sl.join(",");
        }
      }
      TopoFaceGroup *fg = m_topology->createFaceGroup(geoIdsStr.toStdString());
      for (int fid : g.ids) {
        TopoFace *f = m_topology->getFace(fid);
        if (f)
          m_topology->addFaceToGroup(fg->id, f);
      }
    }

    // 2. Sync Edge Groups
    for (const auto &g : m_topologyPage->edgeGroups()) {
      QString geoIdsStr;
      if (!g.linkedGeometryGroup.isEmpty()) {
        const GeometryGroup *gg =
            m_geometryPage->getEdgeGroupByName(g.linkedGeometryGroup);
        if (gg) {
          QStringList sl;
          for (int id : gg->ids)
            sl << QString::number(id);
          geoIdsStr = sl.join(",");
        }
      }
      TopoEdgeGroup *eg = m_topology->createEdgeGroup(geoIdsStr.toStdString());
      for (int eid : g.ids) {
        TopoEdge *e = m_topology->getEdge(eid);
        if (e)
          m_topology->addEdgeToGroup(eg->id, e);
      }
    }

    if (m_smootherPage->plot())
      m_smootherPage->plot()->clear();
    if (m_smootherPage->runButton())
      m_smootherPage->runButton()->setEnabled(false);

    logMessage("Running elliptic grid smoother...");
    m_occView->runEllipticSolver(m_smootherPage->getConfig());
  }
}

void MainWindow::onPageChanged(int index) {
  // 1. Update Banner (prevent signal loop if needed)
  if (m_banner) {
    m_banner->setMode(index);
    m_banner->clearContextButtons();
  }

  // 2. Manage Tool Window Visibility
  // Hide all first (Cleaner experience)
  if (m_geometryPage)
    m_geometryPage->hide();
  if (m_topologyPage)
    m_topologyPage->hide();
  if (m_smootherPage)
    m_smootherPage->hide();

  // Add Context Buttons based on mode
  if (index == 0) { // Geometry
    if (m_banner) {
      m_banner->addContextButton("Edge Groups", ":/resources/hud/edge.png",
                                 [this]() {
                                   m_geometryPage->show();
                                   m_geometryPage->raise();
                                   m_geometryPage->showEdgeGroups();
                                 });
      m_banner->addContextButton("Face Groups", ":/resources/hud/face.png",
                                 [this]() {
                                   m_geometryPage->show();
                                   m_geometryPage->raise();
                                   m_geometryPage->showFaceGroups();
                                 });
    }
  } else if (index == 1) { // Topology
    if (m_banner) {
      m_banner->addContextButton("Entities", ":/resources/hud/topology.png",
                                 [this]() {
                                   m_topologyPage->show();
                                   m_topologyPage->raise();
                                   m_topologyPage->showEntities();
                                 });
      m_banner->addContextButton("Groups", ":/resources/hud/geometry.png",
                                 [this]() {
                                   m_topologyPage->show();
                                   m_topologyPage->raise();
                                   m_topologyPage->showGroups();
                                 });
    }
  } else if (index == 2) { // Smoother
    if (m_banner) {
      m_banner->addContextButton("Options", ":/resources/hud/smoother.png",
                                 [this]() {
                                   m_smootherPage->show();
                                   m_smootherPage->raise();
                                   m_smootherPage->showOptions();
                                 });
      m_banner->addContextButton("Plot", ":/resources/hud/face.png", [this]() {
        m_smootherPage->show();
        m_smootherPage->raise();
        m_smootherPage->showPlot();
      });
      m_banner->addContextButton("Run", ":/resources/MeshingApp.png",
                                 [this]() { onRunSolver(); });
    }
  }

  // 3. Update 3D View Interaction Mode
  if (m_occView) {
    // Mode 0: Geometry
    if (index == 0) {
      if (m_occView->getInteractionMode() != OccView::Mode_Geometry)
        m_occView->setInteractionMode(OccView::Mode_Geometry);
      m_occView->setWorkbench(0);
    }
    // Mode 1: Topology
    else if (index == 1) {
      // Ensure we have valid geometry loaded
      if (m_faceMap->IsEmpty()) {
        logMessage(
            "Warning: No geometry loaded. Please import a STEP file first.");
      }
      if (m_occView->getInteractionMode() != OccView::Mode_Topology)
        m_occView->setInteractionMode(OccView::Mode_Topology);

      // We need to ensure topology visuals are present
      restoreTopologyToView();

      // Sync groups from Geometry Page to Topology Page
      QStringList edgeNames;
      for (const auto &g : m_geometryPage->edgeGroups())
        edgeNames << g.name;
      QStringList faceNames;
      for (const auto &g : m_geometryPage->faceGroups())
        faceNames << g.name;
      m_topologyPage->setGeometryGroupNames(edgeNames, faceNames);

      m_occView->setWorkbench(1);
    }
    // Mode 2: Smoother
    else if (index == 2) {
      if (m_occView->getInteractionMode() != OccView::Mode_Geometry)
        m_occView->setInteractionMode(OccView::Mode_Geometry);

      m_occView->setWorkbench(2);
    }
  }
}
