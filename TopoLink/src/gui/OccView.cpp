#include "OccView.h"
#include "../core/EllipticSolver.h"
#include "../core/TopoEdge.h"
#include "../core/TopoFace.h"
#include "../core/TopoHalfEdge.h"
#include "../core/TopoNode.h"
#include "../core/Topology.h"
#include "EntityOwner.h"
#include "pages/SmootherPage.h"

#include <AIS_ColoredShape.hxx>
#include <AIS_Line.hxx>
#include <AIS_Point.hxx>
#include <AIS_Shape.hxx>
#include <AIS_Triangulation.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <GeomAPI_ProjectPointOnCurve.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <Geom_CartesianPoint.hxx>
#include <Geom_Line.hxx>
#include <Graphic3d_Camera.hxx>
#include <IntCurvesFace_ShapeIntersector.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <Poly_Array1OfTriangle.hxx>
#include <Poly_Triangulation.hxx>
#include <Precision.hxx>
#include <Prs3d_PointAspect.hxx>
#include <Quantity_Color.hxx>
#include <Quantity_NameOfColor.hxx>
#include <StdSelect_BRepOwner.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Lin.hxx>
#include <gp_Pnt.hxx>
#include <gp_XYZ.hxx>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPair>
#include <QSet>
#include <QShowEvent>

#include <cmath>

// Platform specific window handles
#if defined(_WIN32)
#include <WNT_Window.hxx>
#elif defined(__APPLE__)
#include <Cocoa_Window.hxx>
#else
#include <Xw_Window.hxx>
#endif

// Includes for AIS_Point customization
#include <AIS_Point.hxx>
#include <Geom_CartesianPoint.hxx>
#include <Prs3d_PointAspect.hxx>

#include <AIS_Line.hxx>
#include <AIS_Triangulation.hxx>
#include <Geom_Line.hxx>
#include <Poly_Array1OfTriangle.hxx>
#include <Poly_Triangulation.hxx>
#include <QPair>
#include <QSet>
#include <TColgp_Array1OfPnt.hxx>

#include <BRepAdaptor_Curve.hxx>
#include <BRep_Builder.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <TopoDS_Compound.hxx>

OccView::OccView(QWidget *parent)
    : QWidget(parent), myXmin(0), myYmin(0), myXmax(0), myYmax(0), myCurX(0),
      myCurY(0), m_interactionMode(Mode_Geometry), m_nextNodeId(1),
      m_nextFaceId(1) {
  // Initialize map pointers
  m_faceMap = new TopTools_IndexedMapOfShape();
  m_edgeMap = new TopTools_IndexedMapOfShape();

  setAttribute(Qt::WA_PaintOnScreen);
  setAttribute(Qt::WA_NoSystemBackground);
  setMouseTracking(true);
}

// ... (Rest of constructor/destructor/init)

void OccView::removeTopologyNode(int id) {
  if (!m_topologyNodes.contains(id))
    return;

  Handle(AIS_InteractiveObject) obj = m_topologyNodes[id];
  m_context->Remove(obj, Standard_True);
  m_topologyNodes.remove(id);

  // Remove connected edges
  QList<QPair<int, int>> edgesToRemove;
  for (auto it = m_topologyEdges.begin(); it != m_topologyEdges.end(); ++it) {
    if (it.key().first == id || it.key().second == id) {
      edgesToRemove.append(it.key());
    }
  }

  for (const auto &key : edgesToRemove) {
    Handle(AIS_InteractiveObject) edgeObj = m_topologyEdges[key];
    m_context->Remove(edgeObj, Standard_True);
    m_topologyEdges.remove(key);
    emit topologyEdgeDeleted(key.first, key.second);
  }

  // Remove connected faces
  QList<int> facesToRemove;
  for (auto it = m_faceNodeMap.begin(); it != m_faceNodeMap.end(); ++it) {
    if (it.value().contains(id)) {
      facesToRemove.append(it.key());
    }
  }

  for (int faceId : facesToRemove) {
    removeTopologyFace(faceId);
  }

  // Remove constraints
  m_nodeConstraints.remove(id);

  emit topologyNodeDeleted(id);

  if (!m_view.IsNull()) {
    m_view->Redraw();
  }
}

void OccView::highlightTopologyNode(int id, bool highlight) {
  if (!m_topologyNodes.contains(id) || m_context.IsNull())
    return;

  Handle(AIS_InteractiveObject) aisObj = m_topologyNodes[id];
  if (highlight != m_context->IsSelected(aisObj)) {
    m_context->AddOrRemoveSelected(aisObj, Standard_True);
  }
}

void OccView::highlightTopologyEdge(int n1, int n2, bool highlight) {
  QPair<int, int> key(qMin(n1, n2), qMax(n1, n2));
  if (!m_topologyEdges.contains(key) || m_context.IsNull())
    return;

  Handle(AIS_InteractiveObject) aisObj = m_topologyEdges[key];
  if (highlight != m_context->IsSelected(aisObj)) {
    m_context->AddOrRemoveSelected(aisObj, Standard_True);
  }
}

void OccView::highlightTopologyFace(int id, bool highlight) {
  if (!m_topologyFaces.contains(id) || m_context.IsNull())
    return;

  Handle(AIS_InteractiveObject) aisObj = m_topologyFaces[id];
  if (highlight != m_context->IsSelected(aisObj)) {
    m_context->AddOrRemoveSelected(aisObj, Standard_True);
  }
}

QList<int> OccView::getSelectedNodeIds() const {
  QList<int> ids;
  if (m_context.IsNull())
    return ids;

  for (m_context->InitSelected(); m_context->MoreSelected();
       m_context->NextSelected()) {
    Handle(AIS_InteractiveObject) selObj = m_context->SelectedInteractive();

    // Find which topology node this is using the Owner
    if (selObj->HasOwner()) {
      Handle(EntityOwner) owner =
          Handle(EntityOwner)::DownCast(selObj->GetOwner());
      if (!owner.IsNull()) {
        // Check if this ID corresponds to a node
        if (m_topologyNodes.contains(owner->id())) {
          ids.append(owner->id());
        }
      }
    }
  }
  return ids;
}

QList<QPair<int, int>> OccView::getSelectedEdgeIds() const {
  QList<QPair<int, int>> ids;
  if (m_context.IsNull())
    return ids;

  for (m_context->InitSelected(); m_context->MoreSelected();
       m_context->NextSelected()) {
    Handle(AIS_InteractiveObject) selObj = m_context->SelectedInteractive();
    for (auto it = m_topologyEdges.begin(); it != m_topologyEdges.end(); ++it) {
      if (it.value() == selObj) {
        ids.append(it.key());
        break;
      }
    }
  }
  return ids;
}

QList<int> OccView::getSelectedFaceIds() const {
  QList<int> ids;
  if (m_context.IsNull())
    return ids;

  for (m_context->InitSelected(); m_context->MoreSelected();
       m_context->NextSelected()) {
    Handle(AIS_InteractiveObject) selObj = m_context->SelectedInteractive();

    // Find which topology face this is using the Owner
    if (selObj->HasOwner()) {
      Handle(EntityOwner) owner =
          Handle(EntityOwner)::DownCast(selObj->GetOwner());
      if (!owner.IsNull()) {
        // Check if this ID corresponds to a face
        if (m_topologyFaces.contains(owner->id())) {
          ids.append(owner->id());
        }
      }
    }
  }
  return ids;
}

void OccView::removeTopologyEdge(int n1, int n2) {
  QPair<int, int> key1 = qMakePair(n1, n2);
  QPair<int, int> key2 = qMakePair(n2, n1);
  QPair<int, int> key = m_topologyEdges.contains(key1) ? key1 : key2;

  if (m_topologyEdges.contains(key)) {
    m_context->Remove(m_topologyEdges[key], Standard_True);
    m_topologyEdges.remove(key);

    if (m_nodePairToEdgeIdMap.contains(key)) {
      int id = m_nodePairToEdgeIdMap[key];
      m_edgeIdMap.remove(id);
      m_nodePairToEdgeIdMap.remove(key);
    }

    // Cascading deletion: remove faces that contain this edge
    QList<int> facesToRemove;
    for (auto it = m_faceNodeMap.begin(); it != m_faceNodeMap.end(); ++it) {
      const QList<int> &nodes = it.value();
      bool containsEdge = false;
      for (int i = 0; i < nodes.size(); ++i) {
        int nA = nodes[i];
        int nB = nodes[(i + 1) % nodes.size()];
        if ((nA == n1 && nB == n2) || (nA == n2 && nB == n1)) {
          containsEdge = true;
          break;
        }
      }
      if (containsEdge) {
        facesToRemove.append(it.key());
      }
    }
    for (int fid : facesToRemove) {
      removeTopologyFace(fid);
    }

    emit topologyEdgeDeleted(n1, n2);
  }
}

void OccView::removeTopologyFace(int id) {
  qDebug() << "OccView::removeTopologyFace called for id:" << id;
  if (m_topologyFaces.contains(id)) {
    qDebug() << "  Removing AIS object for face" << id;
    m_context->Remove(m_topologyFaces[id], Standard_True);
    m_topologyFaces.remove(id);
    m_faceNodeMap.remove(id);
    emit topologyFaceDeleted(id);
  } else {
    qDebug() << "  Face" << id << "not found in m_topologyFaces";
  }
}

void OccView::refreshFaceVisual(int faceId, const QList<int> &nodeIds) {
  if (m_topologyFaces.contains(faceId)) {
    m_context->Remove(m_topologyFaces[faceId], Standard_False);
    m_topologyFaces.remove(faceId);
  }
  m_faceNodeMap.insert(faceId, nodeIds);
  createFaceVisual(faceId, nodeIds);
}

gp_Pnt OccView::getTopologyNodePosition(int id) const {
  if (!m_topologyNodes.contains(id)) {
    return gp_Pnt();
  }

  Handle(AIS_InteractiveObject) obj = m_topologyNodes[id];

  // Handle AIS_Point (new primary representation for nodes)
  Handle(AIS_Point) aisPoint = Handle(AIS_Point)::DownCast(obj);
  if (!aisPoint.IsNull()) {
    Handle(Geom_Point) geomPoint = aisPoint->Component();
    Handle(Geom_CartesianPoint) p =
        Handle(Geom_CartesianPoint)::DownCast(geomPoint);
    if (!p.IsNull()) {
      return p->Pnt();
    }
  }

  // Fallback for AIS_ColoredShape (old representation)
  Handle(AIS_ColoredShape) aisShape = Handle(AIS_ColoredShape)::DownCast(obj);
  if (!aisShape.IsNull()) {
    const TopoDS_Shape &shape = aisShape->Shape();
    if (shape.ShapeType() == TopAbs_VERTEX) {
      return BRep_Tool::Pnt(TopoDS::Vertex(shape));
    }
  }

  return gp_Pnt();
}

void OccView::setNodeConstraint(int nodeId, ConstraintType type, int targetId) {
  NodeConstraint c;
  c.type = type;
  if (targetId != -1) {
    c.geometryIds.append(targetId);
  }
  m_nodeConstraints[nodeId] = c;
}

void OccView::setNodeConstraints(const QMap<int, NodeConstraint> &constraints) {
  qDebug() << "OccView: setNodeConstraints called with" << constraints.size()
           << "constraints";
  m_nodeConstraints = constraints;

  // Apply constraints immediately
  for (auto it = m_nodeConstraints.begin(); it != m_nodeConstraints.end();
       ++it) {
    int nodeId = it.key();
    if (m_topologyNodes.contains(nodeId)) {
      gp_Pnt currentPos = getTopologyNodePosition(nodeId);
      gp_Pnt snappedPos = applyConstraint(nodeId, currentPos);

      // If the node moved significantly, update it and notify the model
      if (currentPos.SquareDistance(snappedPos) >
          Precision::Confusion() * Precision::Confusion()) {
        qDebug() << "OccView: Snapping node" << nodeId << "from"
                 << currentPos.X() << currentPos.Y() << currentPos.Z() << "to"
                 << snappedPos.X() << snappedPos.Y() << snappedPos.Z();
        moveTopologyNode(nodeId, snappedPos);
        emit topologyNodeMoved(nodeId, snappedPos);
      }
    } else {
      qDebug() << "OccView: Constraint for node" << nodeId
               << "but node not found in m_topologyNodes!";
    }
  }
}

OccView::NodeConstraint OccView::getNodeConstraint(int nodeId) const {
  if (m_nodeConstraints.contains(nodeId)) {
    return m_nodeConstraints[nodeId];
  }
  return {ConstraintNone, QList<int>(), false, gp_Pnt(), gp_Dir()};
}

void OccView::addEdge(int node1, int node2) {
  qDebug() << "OccView::addEdge(" << node1 << "," << node2 << ")";

  // Check if edge already exists (bidirectional check)
  auto pair1 = qMakePair(node1, node2);
  auto pair2 = qMakePair(node2, node1);
  if (m_topologyEdges.contains(pair1) || m_topologyEdges.contains(pair2)) {
    qDebug() << "  Edge already exists in GUI map. Skipping.";
    return;
  }

  if (node1 == node2)
    return;

  gp_Pnt p1 = getTopologyNodePosition(node1);
  gp_Pnt p2 = getTopologyNodePosition(node2);

  if (p1.SquareDistance(p2) < 1e-4)
    return;

  try {
    Handle(Geom_Line) line = new Geom_Line(p1, gp_Dir(gp_Vec(p1, p2)));
    Handle(AIS_Line) aisLine = new AIS_Line(line);

    // Set points to limit the infinite line
    Handle(Geom_CartesianPoint) pt1 = new Geom_CartesianPoint(p1);
    Handle(Geom_CartesianPoint) pt2 = new Geom_CartesianPoint(p2);
    aisLine->SetPoints(pt1, pt2);

    aisLine->SetWidth(m_edgeWidth);
    aisLine->SetColor(Quantity_NOC_RED);

    // Apply persistent style if it exists (for re-created or grouped edges)
    // Note: edgeId is not known until below, so we check using the key if
    // needed Actually, edgeId is the key in m_edgeStyles. If addEdge is called
    // for a brand new edge, it won't have a style yet. But if we ever re-create
    // edgeId, it will.

    m_context->Display(aisLine, Standard_True);
    auto key = qMakePair(qMin(node1, node2), qMax(node1, node2));
    m_topologyEdges.insert(key, aisLine);

    int edgeId = m_nextEdgeId++;
    m_edgeIdMap.insert(edgeId, key);
    m_nodePairToEdgeIdMap.insert(key, edgeId);
    qDebug() << "  Created GUI Edge ID:" << edgeId << "for nodes" << node1
             << "-" << node2;

    if (m_edgeStyles.contains(edgeId)) {
      const auto &style = m_edgeStyles[edgeId];
      Quantity_Color occColor(style.color.redF(), style.color.greenF(),
                              style.color.blueF(), Quantity_TOC_RGB);
      aisLine->SetColor(occColor);
    }

    emit topologyEdgeCreated(node1, node2, edgeId);
  } catch (Standard_ConstructionError &) {
    qDebug() << "addEdge: Construction error for edge" << node1 << "-" << node2;
  }
}

// ... existing methods ...

OccView::~OccView() {
  delete m_faceMap;
  delete m_edgeMap;
}

void OccView::init() {
  loadConfig();

  if (!m_view.IsNull())
    return;

  // 1. Create Graphic Driver
  Handle(Aspect_DisplayConnection) displayConnection =
      new Aspect_DisplayConnection();
  Handle(OpenGl_GraphicDriver) graphicDriver =
      new OpenGl_GraphicDriver(displayConnection);

  // 2. Create Viewer
  m_viewer = new V3d_Viewer(graphicDriver);
  m_viewer->SetDefaultLights();
  m_viewer->SetLightOn();

  // 3. Create View
  m_view = m_viewer->CreateView();
  m_view->SetImmediateUpdate(false);

  // 4. Create Window Handle
  WId windowHandle = this->winId();
  Handle(Aspect_Window) wnd;

#if defined(_WIN32)
  wnd = new WNT_Window((Aspect_Handle)windowHandle);
#elif defined(__APPLE__)
  NSView *viewHandle = (NSView *)windowHandle;
  wnd = new Cocoa_Window(viewHandle);
#else
  wnd = new Xw_Window(displayConnection, (Window)windowHandle);
#endif

  m_view->SetWindow(wnd);
  if (!wnd->IsMapped())
    wnd->Map();

  // 5. Create Interactive Context
  m_context = new AIS_InteractiveContext(m_viewer);

  // Increase picker tolerance for easier edge/vertex selection
  m_context->SetPixelTolerance(6);

  // Configure highlight styles for better visibility
  // Dynamic highlight (hover)
  Handle(Prs3d_Drawer) hiStyle =
      m_context->HighlightStyle(Prs3d_TypeOfHighlight_Dynamic);
  Quantity_Color hiColor(m_highlightColor.redF(), m_highlightColor.greenF(),
                         m_highlightColor.blueF(), Quantity_TOC_RGB);
  hiStyle->SetColor(hiColor);
  hiStyle->SetTransparency(0.0f);
  hiStyle->SetDisplayMode(1); // Shaded

  // Selected highlight
  Handle(Prs3d_Drawer) selStyle =
      m_context->HighlightStyle(Prs3d_TypeOfHighlight_Selected);
  Quantity_Color selColor(m_selectionColor.redF(), m_selectionColor.greenF(),
                          m_selectionColor.blueF(), Quantity_TOC_RGB);
  selStyle->SetColor(selColor);
  selStyle->SetTransparency(0.0f);
  selStyle->SetDisplayMode(1); // Shaded

  // Optional: Set display mode to Shaded (1) instead of Wireframe (0)
  m_context->SetDisplayMode(AIS_Shaded, Standard_True);

  // Apply Background Gradient
  Quantity_Color bgTop(m_bgGradientTop.redF(), m_bgGradientTop.greenF(),
                       m_bgGradientTop.blueF(), Quantity_TOC_RGB);
  Quantity_Color bgBott(m_bgGradientBottom.redF(), m_bgGradientBottom.greenF(),
                        m_bgGradientBottom.blueF(), Quantity_TOC_RGB);
  m_view->SetBgGradientColors(bgTop, bgBott, Aspect_GFM_VER);

  m_view->MustBeResized();
  m_view->TriedronDisplay(Aspect_TOTP_LEFT_LOWER, Quantity_NOC_WHITE, 0.1,
                          V3d_ZBUFFER);

  // Enable keyboard focus
  setFocusPolicy(Qt::StrongFocus);

  // Create HUD overlay
  createHUD();
}

void OccView::paintEvent(QPaintEvent *) {
  if (m_view.IsNull())
    init();
  m_view->Redraw();
}

void OccView::resizeEvent(QResizeEvent *event) {
  if (!m_view.IsNull()) {
    m_view->MustBeResized();
  }

  if (m_hudWidget) {
    // Positioning the HUD at the top-left of the view
    QPoint globalPos = mapToGlobal(QPoint(10, 10));
    m_hudWidget->move(globalPos);
    m_hudWidget->raise();
  }
}

void OccView::fitAll() {
  if (!m_view.IsNull())
    m_view->FitAll();
}

void OccView::alignToClosestAxis() {
  if (m_view.IsNull())
    return;

  // Get current view direction
  Standard_Real Vx, Vy, Vz;
  m_view->Proj(Vx, Vy, Vz);

  // Find which axis the view is closest to
  Standard_Real absX = std::abs(Vx);
  Standard_Real absY = std::abs(Vy);
  Standard_Real absZ = std::abs(Vz);

  // Set view based on dominant axis
  if (absZ >= absX && absZ >= absY) {
    // Closest to Z axis
    if (Vz > 0) {
      m_view->SetProj(V3d_Zpos); // Top view
    } else {
      m_view->SetProj(V3d_Zneg); // Bottom view
    }
  } else if (absY >= absX && absY >= absZ) {
    // Closest to Y axis
    if (Vy > 0) {
      m_view->SetProj(V3d_Ypos); // Front view
    } else {
      m_view->SetProj(V3d_Yneg); // Back view
    }
  } else {
    // Closest to X axis
    if (Vx > 0) {
      m_view->SetProj(V3d_Xpos); // Right view
    } else {
      m_view->SetProj(V3d_Xneg); // Left view
    }
  }

  m_view->FitAll();
  m_view->Redraw();
}

void OccView::keyPressEvent(QKeyEvent *event) {
  if (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_F) {
    fitAll();
    event->accept();
    return;
  }

  // Workbench Switching
  if (event->key() == Qt::Key_R) {
    emit workbenchRequested(0);
    event->accept();
    return;
  }
  if (event->key() == Qt::Key_T) {
    emit workbenchRequested(1);
    event->accept();
    return;
  }
  if (event->key() == Qt::Key_Y) {
    emit workbenchRequested(2);
    event->accept();
    return;
  }

  // Selection Mode Toggles
  if (event->key() == Qt::Key_Q) {
    qDebug() << "OccView: Q key handler - mode:" << m_interactionMode
             << "workbench:" << m_workbenchIndex;
    if (m_interactionMode == Mode_Geometry) {
      setSelectionMode(1); // Vertex
    } else if (m_workbenchIndex != 2) {
      setTopologySelectionMode(SelNodes);
    }
    event->accept();
    return;
  }
  if (event->key() == Qt::Key_W) {
    qDebug() << "OccView: W key handler - mode:" << m_interactionMode
             << "workbench:" << m_workbenchIndex;
    if (m_interactionMode == Mode_Geometry) {
      setSelectionMode(2); // Edge
    } else if (m_workbenchIndex != 2) {
      setTopologySelectionMode(SelEdges);
    }
    event->accept();
    return;
  }
  if (event->key() == Qt::Key_E) {
    qDebug() << "OccView: E key handler - mode:" << m_interactionMode
             << "workbench:" << m_workbenchIndex;
    if (m_interactionMode == Mode_Geometry) {
      setSelectionMode(4); // Face
    } else if (m_workbenchIndex != 2) {
      setTopologySelectionMode(SelFaces);
    }
    event->accept();
    return;
  }

  if (event->key() == Qt::Key_Z) {
    cycleSelection();
    event->accept();
    return;
  }
  if (event->key() == Qt::Key_C) {
    selectAdjacents();
    event->accept();
    return;
  }

  if (event->key() == Qt::Key_A) {
    alignToClosestAxis();
    event->accept();
  } else if (event->key() == Qt::Key_Delete ||
             event->key() == Qt::Key_Backspace) {
    if (m_interactionMode == Mode_Topology) {
      // 1. Gather all selected IDs from the AIS context
      QList<int> nodesToDelete = getSelectedNodeIds();
      QList<QPair<int, int>> edgesToDelete = getSelectedEdgeIds();
      QList<int> facesToDelete = getSelectedFaceIds();

      // 2. Perform deletions
      for (int nid : nodesToDelete) {
        removeTopologyNode(nid);
      }
      for (const auto &epair : edgesToDelete) {
        removeTopologyEdge(epair.first, epair.second);
      }
      for (int fid : facesToDelete) {
        removeTopologyFace(fid);
      }

      // 3. Clear transient selection state
      m_selectedNode.Nullify();
      m_selectedNodeId = -1;
      m_draggedNodeId = -1;
      m_isDraggingNode = false;
      m_selectedEdge = qMakePair(-1, -1);
    }
    event->accept();
  } else if (event->key() == Qt::Key_M) {
    // Merge nodes: keep selected node, remove hovered node
    qDebug() << "M key pressed - selectedNodeId:" << m_selectedNodeId
             << "hoveredNodeId:" << m_hoveredNodeId << "mode:"
             << (m_interactionMode == Mode_Topology ? "Topology" : "Other");

    if (m_interactionMode == Mode_Topology && m_selectedNodeId != -1 &&
        m_hoveredNodeId != -1 && m_selectedNodeId != m_hoveredNodeId) {
      qDebug() << "OccView: Emitting topologyNodesMerged(" << m_hoveredNodeId
               << "," << m_selectedNodeId << ")";
      emit topologyNodesMerged(m_hoveredNodeId, m_selectedNodeId);

      // Reset selection state as MainWindow will have removed the node
      m_hoveredNodeId = -1;
      m_selectedNodeId = -1;
      m_selectedNode.Nullify();
    } else {
      qDebug() << "Merge conditions NOT met:";
      if (m_interactionMode != Mode_Topology)
        qDebug() << "  - Interaction mode is NOT Topology";
      if (m_selectedNodeId == -1)
        qDebug() << "  - No node is selected (Click a node first)";
      if (m_hoveredNodeId == -1)
        qDebug() << "  - No node is hovered (Drag or move mouse over another "
                    "node)";
      if (m_selectedNodeId == m_hoveredNodeId && m_selectedNodeId != -1)
        qDebug() << "  - Selected and Hovered nodes are the same";
    }
    event->accept();
  } else {
    QWidget::keyPressEvent(event);
  }
}

void OccView::setInteractionMode(InteractionMode mode) {
  qDebug() << "OccView::setInteractionMode called with"
           << (mode == Mode_Geometry ? "Mode_Geometry" : "Mode_Topology");
  m_interactionMode = mode;

  if (m_context.IsNull()) {
    qDebug() << "OccView::setInteractionMode: Context is null, returning.";
    return;
  }

  if (mode == Mode_Topology) {
    qDebug() << "OccView::setInteractionMode: Switching to Mode_Topology.";
    if (!m_aisShape.IsNull()) {
      m_context->Deactivate(m_aisShape);
      qDebug() << "OccView::setInteractionMode: Deactivated m_aisShape.";
    }
    // Force refresh of topology activation
    TopologySelectionMode current = m_topologySelectionMode;
    m_topologySelectionMode = static_cast<TopologySelectionMode>(
        -1); // Invalidate to force re-activation
    setTopologySelectionMode(current);
  } else if (mode == Mode_Geometry) {
    qDebug() << "OccView::setInteractionMode: Switching to Mode_Geometry.";
    // Ensure all topology objects are deactivated
    for (auto node : m_topologyNodes) {
      m_context->Deactivate(node);
    }
    for (auto edge : m_topologyEdges) {
      m_context->Deactivate(edge);
    }
    for (auto face : m_topologyFaces) {
      m_context->Deactivate(face);
    }
    qDebug()
        << "OccView::setInteractionMode: Deactivated all topology objects.";

    // Explicitly activate geometry
    setSelectionMode(m_geometrySelectionMode);
  }

  updateHUDStates();
}

int OccView::addTopologyNode(const gp_Pnt &p) {
  int id = m_nextNodeId++;

  Handle(Geom_CartesianPoint) geomPoint = new Geom_CartesianPoint(p);
  Handle(AIS_Point) aisNode = new AIS_Point(geomPoint);

  // Set owner for robust selection
  aisNode->SetOwner(new EntityOwner(id));

  // Set appearance using a PointAspect in the node's own attributes
  Handle(Prs3d_PointAspect) pa =
      new Prs3d_PointAspect(Aspect_TOM_BALL, Quantity_NOC_RED, m_nodeSize);
  aisNode->Attributes()->SetPointAspect(pa);

  // FIX: Force highlight to use Mode 0 (the only mode points support)
  // This overrides the global Selection Style which defaults to Mode 1 (Shaded)
  aisNode->SetHilightMode(0);

  m_context->Display(aisNode, Standard_True);
  m_context->SetZLayer(aisNode, Graphic3d_ZLayerId_Topmost);
  m_context->Activate(aisNode, 1, true);

  m_topologyNodes.insert(id, aisNode);

  emit topologyNodeCreated(id, 0, 0, 0);

  return id;
}

void OccView::restoreTopologyNode(int id, const gp_Pnt &p) {
  Handle(Geom_CartesianPoint) geomPoint = new Geom_CartesianPoint(p);
  Handle(AIS_Point) aisNode = new AIS_Point(geomPoint);
  aisNode->SetOwner(new EntityOwner(id));
  Handle(Prs3d_PointAspect) pa =
      new Prs3d_PointAspect(Aspect_TOM_BALL, Quantity_NOC_RED, m_nodeSize);
  aisNode->Attributes()->SetPointAspect(pa);

  // FIX: Force highlight to use Mode 0
  aisNode->SetHilightMode(0);

  if (m_workbenchIndex == 1) {
    m_context->Display(aisNode, Standard_False); // Don't redraw yet
    m_context->SetZLayer(aisNode, Graphic3d_ZLayerId_Topmost);
    m_context->Activate(aisNode, 1, true);
  } else {
    m_context->Erase(aisNode, Standard_False);
  }

  m_topologyNodes.insert(id, aisNode);
  if (id >= m_nextNodeId)
    m_nextNodeId = id + 1;
}

void OccView::restoreTopologyEdge(int id, int n1, int n2) {
  if (n1 == n2)
    return;
  gp_Pnt p1 = getTopologyNodePosition(n1);
  gp_Pnt p2 = getTopologyNodePosition(n2);

  try {
    Handle(Geom_Line) line = new Geom_Line(p1, gp_Dir(gp_Vec(p1, p2)));
    Handle(AIS_Line) aisLine = new AIS_Line(line);
    Handle(Geom_CartesianPoint) pt1 = new Geom_CartesianPoint(p1);
    Handle(Geom_CartesianPoint) pt2 = new Geom_CartesianPoint(p2);
    aisLine->SetPoints(pt1, pt2);
    aisLine->SetWidth(m_edgeWidth);
    aisLine->SetColor(Quantity_NOC_RED);

    if (m_workbenchIndex == 1) {
      m_context->Display(aisLine, Standard_False);
    } else {
      m_context->Erase(aisLine, Standard_False);
    }
    auto key = qMakePair(qMin(n1, n2), qMax(n1, n2));
    m_topologyEdges.insert(key, aisLine);

    m_edgeIdMap.insert(id, key);
    m_nodePairToEdgeIdMap.insert(key, id);

    if (id >= m_nextEdgeId)
      m_nextEdgeId = id + 1;

  } catch (Standard_ConstructionError &) {
    qDebug() << "restoreTopologyEdge: Construction error for" << n1 << "-"
             << n2;
  }
}

void OccView::restoreTopologyFace(int id, const QList<int> &nodeIds) {
  m_faceNodeMap.insert(id, nodeIds);
  createFaceVisual(id, nodeIds);
  if (id >= m_nextFaceId)
    m_nextFaceId = id + 1;
}

void OccView::finalizeRestoration() {
  if (!m_context.IsNull()) {
    setWorkbench(m_workbenchIndex);
    m_context->UpdateCurrentViewer();
  }
}

#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>

void OccView::moveTopologyNode(int id, const gp_Pnt &p) {
  if (!m_topologyNodes.contains(id))
    return;

  Handle(AIS_InteractiveObject) obj = m_topologyNodes[id];

  // Handle AIS_Point (new primary representation)
  Handle(AIS_Point) aisPoint = Handle(AIS_Point)::DownCast(obj);
  if (!aisPoint.IsNull()) {
    Handle(Geom_CartesianPoint) geomPoint = new Geom_CartesianPoint(p);
    aisPoint->SetComponent(geomPoint);
    m_context->Redisplay(aisPoint, Standard_True);
    updateConnectedEdges(id);
    updateConnectedFaces(id);
    return;
  }

  // Fallback for original AIS_ColoredShape logic
  Handle(AIS_ColoredShape) aisShape = Handle(AIS_ColoredShape)::DownCast(obj);
  if (!aisShape.IsNull()) {
    // To "move" a vertex, we replace its geometry with a new one at the new
    // location.
    TopoDS_Vertex newVertex = BRepBuilderAPI_MakeVertex(p);
    aisShape->Set(newVertex); // Update the geometry within the AIS object

    // Redisplay the node itself to show the change.
    m_context->Redisplay(aisShape, Standard_True);

    // Update connected edges and faces, which rely on the now-corrected
    // getTopologyNodePosition
    updateConnectedEdges(id);
    updateConnectedFaces(id);
    return; // Exit after handling the AIS_ColoredShape case
  }
}

void OccView::updateConnectedEdges(int nodeId) {
  gp_Pnt pNode = getTopologyNodePosition(nodeId);

  for (auto it = m_topologyEdges.begin(); it != m_topologyEdges.end(); ++it) {
    int n1 = it.key().first;
    int n2 = it.key().second;

    if (n1 == nodeId || n2 == nodeId) {
      int otherNodeId = (n1 == nodeId) ? n2 : n1;
      gp_Pnt pOther = getTopologyNodePosition(otherNodeId);

      Handle(AIS_Line) aisLine = Handle(AIS_Line)::DownCast(it.value());
      if (!aisLine.IsNull()) {
        // We need to update the points. AIS_Line holds a Geom_Line, but
        // SetPoints modifies the visual extent. However, AIS_Line stores points
        // as handle to Geom_Point. Creating new points is the safest way to
        // ensure update.
        Handle(Geom_CartesianPoint) pt1 = new Geom_CartesianPoint(pNode);
        Handle(Geom_CartesianPoint) pt2 = new Geom_CartesianPoint(pOther);

        // AIS_Line::SetPoints expects points that define the segment.
        // Does it auto-update the underlying line? Not necessarily.
        // We might need to replace the line aspect or the object itself if
        // AIS_Line is rigid. Actually AIS_Line computes the line from points if
        // provided? Let's check constructor: AIS_Line(Geom_Line). It seems best
        // to update the underlying Geom_Line as well if we want correctness,
        // but strictly for visualization, updating the points *might* be enough
        // if AIS_Line uses them for presentation. Better: Create new line
        // geometry and update the AIS_Line

        // Correction: AIS_Line is usually infinite unless SetPoints is called.
        // Let's try just updating the points first.
        // Note: The order matters if we want to match internal start/end.
        if (n1 == nodeId) {
          aisLine->SetPoints(pt1, pt2);
        } else {
          aisLine->SetPoints(pt2, pt1);
        }

        // Also update the underlying geometric line to stay correct
        Handle(Geom_Line) geomLine = aisLine->Line();
        if (!geomLine.IsNull() && pNode.SquareDistance(pOther) > 1e-4) {
          try {
            geomLine->SetLin(gp_Lin(pNode, gp_Dir(gp_Vec(pNode, pOther))));
          } catch (Standard_ConstructionError &) {
            // Skip update if direction is degenerate
          }
        }

        m_context->Redisplay(aisLine, Standard_True);
      }
    }
  }
}

bool OccView::castRayToMesh(int x, int y, gp_Pnt &intersection,
                            TopoDS_Face &hitFace) {
  if (m_shape.IsNull()) {
    return false;
  }

  // 1. Manually construct High DPI / Aspect-Correct Ray
  // Bypasses V3d_View::Convert which has inconsistent window logic on Mac
  // Retina

  if (m_view.IsNull())
    return false;
  Handle(Graphic3d_Camera) cam = m_view->Camera();

  // Widget Dimensions (Logical)
  Standard_Real w = (Standard_Real)width();
  Standard_Real h = (Standard_Real)height();

  // Normalized Device Coordinates [-1, 1]
  Standard_Real ndcX = 2.0 * x / w - 1.0;
  Standard_Real ndcY = 1.0 - 2.0 * y / h; // Qt (0) -> +1. Qt (h) -> -1.

  // Camera Basis
  gp_Pnt eye = cam->Eye();
  gp_Dir dir = cam->Direction();
  gp_Dir up = cam->Up();

  gp_Vec Fwd(dir);
  gp_Vec Up(up);
  gp_Vec Side = Fwd.Crossed(Up);

  // Re-orthogonalize Up
  Up = Side.Crossed(Fwd);

  // Normalize Basis
  Fwd.Normalize();
  Up.Normalize();
  Side.Normalize();

  // Frustum Dimensions at Distance 1.0
  Standard_Real aspect =
      cam->Aspect(); // Use underlying camera aspect for consistency
  Standard_Real fovy = cam->FOVy();

  Standard_Real rads = fovy * M_PI / 180.0;
  Standard_Real tanHalf = std::tan(rads / 2.0);

  Standard_Real hSize = tanHalf;        // Half-height at dist 1
  Standard_Real wSize = hSize * aspect; // Half-width at dist 1

  // Ray Direction
  // Ray = Fwd + Side * (ndcX * wSize) + Up * (ndcY * hSize)
  gp_Vec rayVec = Fwd + Side * (ndcX * wSize) + Up * (ndcY * hSize);

  gp_Dir rayDir(rayVec);
  gp_Lin ray(eye, rayDir);

  // 2. Intersect ray with shape
  IntCurvesFace_ShapeIntersector intersector;
  intersector.Load(m_shape, 1e-1);

  intersector.Perform(ray, 0.0, 1e100);

  if (intersector.IsDone() && intersector.NbPnt() > 0) {
    // Find closest intersection
    double minDist = 1e100;
    int closestIndex = -1;

    for (int i = 1; i <= intersector.NbPnt(); ++i) {
      double dist = intersector.WParameter(i);
      if (dist < minDist && dist > 1e-9) {
        minDist = dist;
        closestIndex = i;
      }
    }

    if (closestIndex != -1) {
      intersection = intersector.Pnt(closestIndex);
      hitFace = intersector.Face(closestIndex);
      return true;
    }
  }

  return false;
}

void OccView::setSelectionMode(int mode) {
  if (m_context.IsNull())
    return;

  // Normalize mode immediately
  int aisMode = mode;
  if (mode == 7 || mode == 1)
    aisMode = 1; // Vertex
  else if (mode == 6 || mode == 2)
    aisMode = 2; // Edge
  else if (mode == 4)
    aisMode = 4; // Face

  qDebug() << "OccView::setSelectionMode: Normalized" << mode << "to"
           << aisMode;
  m_geometrySelectionMode = aisMode;

  if (!m_aisShape.IsNull() && m_interactionMode == Mode_Geometry) {
    qDebug() << "OccView::setSelectionMode: Activating mode" << aisMode
             << "on m_aisShape";
    m_context->Deactivate(m_aisShape);
    m_context->Activate(m_aisShape, aisMode);
  }

  updateHUDStates();
}

void OccView::setTopologySelectionMode(int mode) {
  TopologySelectionMode newMode = static_cast<TopologySelectionMode>(mode);
  qDebug() << "OccView::setTopologySelectionMode called with" << mode
           << "current:" << m_topologySelectionMode;

  if (newMode != m_topologySelectionMode) {
    clearTopologySelection();
    m_topologySelectionMode = newMode;
    qDebug() << "Topology selection mode set to:"
             << (mode == 0
                     ? "Nodes"
                     : (mode == 1 ? "Edges" : (mode == 2 ? "Faces" : "None")));

    // Activate/Deactivate objects based on mode
    if (m_interactionMode == Mode_Topology) {
      for (auto node : m_topologyNodes) {
        if (m_topologySelectionMode == SelNodes)
          m_context->Activate(node, 1, true);
        else
          m_context->Deactivate(node);
      }
      for (auto edge : m_topologyEdges) {
        // Activate edges for selection if in Edges mode,
        // but always keep them pickable for extrusion if in Topology mode?
        // Let's stick to the mode for now.
        if (m_topologySelectionMode == SelEdges)
          m_context->Activate(edge, 0, true);
        else
          m_context->Deactivate(edge);
      }
      for (auto face : m_topologyFaces) {
        if (m_topologySelectionMode == SelFaces)
          m_context->Activate(face, 0, true);
        else
          m_context->Deactivate(face);
      }
    }
    updateHUDStates();
  }
}

void OccView::clearTopologySelection() {
  if (m_context.IsNull())
    return;
  m_context->ClearSelected(Standard_True);
  m_selectedNode.Nullify();
  m_selectedNodeId = -1;
  m_selectedEdge = qMakePair(-1, -1);
  m_hoveredNodeId = -1;
}

void OccView::mousePressEvent(QMouseEvent *event) {
  if (m_view.IsNull() || m_context.IsNull()) {
    return;
  }

  qDebug() << "MousePress" << event->pos() << "Mode:"
           << (m_interactionMode == Mode_Topology ? "Topology" : "Geometry");

  // Ensure keyboard focus for key events (like M for merge)
  setFocus();

  if (event->button() == Qt::LeftButton) {
    if (m_interactionMode == Mode_Topology) {
      m_context->MoveTo(event->x(), event->y(), m_view, Standard_False);

      Handle(AIS_InteractiveObject) detectedObj;
      if (m_context->HasDetected()) {
        detectedObj = m_context->DetectedInteractive();
      }
      bool isShift = (event->modifiers() & Qt::ShiftModifier);
      bool isCtrl = (event->modifiers() & Qt::ControlModifier);

      // 1. Identification (Independent of selection)
      int discoveredNodeId = -1;
      int discoveredFaceId = -1; // Added for Face support
      QPair<int, int> discoveredEdge = qMakePair(-1, -1);

      if (!detectedObj.IsNull()) {
        // Use EntityOwner for O(1) identification
        if (detectedObj->HasOwner()) {
          Handle(EntityOwner) owner =
              Handle(EntityOwner)::DownCast(detectedObj->GetOwner());
          if (!owner.IsNull()) {
            int id = owner->id();

            // Resolve ambiguity: Check if it's really a node or a face
            // by comparing the detected object with our map entries.
            if (m_topologyNodes.contains(id) &&
                m_topologyNodes[id] == detectedObj) {
              discoveredNodeId = id;
              qDebug() << "Clicked Topology Node ID:" << id;
            } else if (m_topologyFaces.contains(id) &&
                       m_topologyFaces[id] == detectedObj) {
              discoveredFaceId = id;
              qDebug() << "Clicked Topology Face ID:" << id;
            }
          }
        }

        // Check Edges (if not identified as node/face)
        if (discoveredNodeId == -1 && discoveredFaceId == -1) {
          for (auto it = m_topologyEdges.begin(); it != m_topologyEdges.end();
               ++it) {
            if (it.value() == detectedObj) {
              discoveredEdge = it.key();
              // Print Edge ID
              int edgeId = m_nodePairToEdgeIdMap.value(discoveredEdge, -1);
              qDebug() << "Clicked Topology Edge ID:" << edgeId << "(Nodes"
                       << discoveredEdge.first << "-" << discoveredEdge.second
                       << ")";
              break;
            }
          }
        }
      }

      // 2. Handle Selection (Only if matching mode or shifted)
      bool belongsToCurrentMode = false;
      if (discoveredNodeId != -1 && m_topologySelectionMode == SelNodes)
        belongsToCurrentMode = true;
      if (discoveredFaceId != -1 &&
          m_topologySelectionMode == SelFaces) // Ensure faces can be selected
        belongsToCurrentMode = true;
      if (discoveredEdge != qMakePair(-1, -1) &&
          m_topologySelectionMode == SelEdges)
        belongsToCurrentMode = true;

      if (isShift) {
        m_context->SelectDetected(AIS_SelectionScheme_XOR);
      } else if (!isCtrl && belongsToCurrentMode) {
        m_context->SelectDetected(AIS_SelectionScheme_Replace);
      } else if (!isCtrl && !belongsToCurrentMode) {
        m_context->ClearSelected(Standard_True);
      }

      // 3. Update internal selection state and handle operations
      m_selectedNodeId = -1;
      m_selectedNode.Nullify();
      m_selectedEdge = qMakePair(-1, -1);

      if (discoveredNodeId != -1) {
        m_selectedNodeId = discoveredNodeId;
        m_selectedNode = m_topologyNodes[discoveredNodeId];
        emit topologyNodeSelected(discoveredNodeId);

        if (isCtrl && !isShift) {
          m_isCreatingEdge = true;
          m_edgeStartNodeId = discoveredNodeId;
          qDebug() << "Started creating edge from node" << m_edgeStartNodeId;
        } else if (!isCtrl && !isShift) {
          m_isDraggingNode = true;
          m_draggedNodeId = discoveredNodeId;
        }
      } else if (discoveredEdge != qMakePair(-1, -1)) {
        m_selectedEdge = discoveredEdge;
        if (isCtrl && !isShift) {
          m_isExtrudingFace = true;
          qDebug() << "Started extruding face from edge" << m_selectedEdge;
        }
      } else if (isCtrl && !isShift) {
        // Clicked on mesh or background with Ctrl -> Create Node
        gp_Pnt p;
        TopoDS_Face face;
        if (castRayToMesh(event->x(), event->y(), p, face)) {
          int id = addTopologyNode(p);
          // Auto-select the new node safely
          if (m_topologyNodes.contains(id)) {
            Handle(AIS_InteractiveObject) newNode = m_topologyNodes[id];
            if (!newNode.IsNull()) {
              m_context->ClearSelected(Standard_False);
              m_context->AddOrRemoveSelected(newNode, Standard_True);
              m_selectedNode = newNode;
              m_selectedNodeId = id;
            }
          }
        }
      }
      emit topologySelectionChanged();
    } else {
      // Geometry Selection
      if (!m_shape.IsNull() && event->modifiers() == Qt::NoModifier) {
        m_context->MoveTo(event->x(), event->y(), m_view, true);
        m_context->SelectDetected();
        m_context->InitSelected();
        if (m_context->MoreSelected()) {
          const TopoDS_Shape &shape = m_context->SelectedShape();
          emit shapeSelected(shape);
        }
      }
    }

    // Always allow rotation start if not interacting
    if (!m_isDraggingNode && !m_isCreatingEdge) {
      m_view->StartRotation(event->x(), event->y());
    }
  }
  myCurX = event->x();
  myCurY = event->y();
}

void OccView::mouseReleaseEvent(QMouseEvent *event) {
  if (m_view.IsNull()) {
    return;
  }

  if (m_isCreatingEdge) {
    // Always create a new node at the release location
    gp_Pnt pRelease;
    TopoDS_Face face;

    if (castRayToMesh(event->x(), event->y(), pRelease, face)) {
      int newNodeId = addTopologyNode(pRelease);

      addEdge(m_edgeStartNodeId, newNodeId);
      qDebug() << "Extruded edge to new node" << newNodeId;
    }

    // Cleanup temporary edge
    if (!m_draggedEdge.IsNull()) {
      m_context->Remove(m_draggedEdge, Standard_True);
      m_draggedEdge.Nullify();
    }
    m_isCreatingEdge = false;
    m_edgeStartNodeId = -1;
  }

  if (m_isExtrudingFace) {
    int n1 = m_selectedEdge.first;
    int n2 = m_selectedEdge.second;
    gp_Pnt p1 = getTopologyNodePosition(n1);
    gp_Pnt p2 = getTopologyNodePosition(n2);

    gp_Pnt pMouse;
    TopoDS_Face face;
    if (castRayToMesh(event->x(), event->y(), pMouse, face)) {
      gp_Vec vEdge(p1, p2);
      gp_Pnt pMid = p1.Translated(vEdge * 0.5);
      gp_Vec vMouse(pMid, pMouse);

      gp_Pnt p3Pos = p1.Translated(vMouse);
      gp_Pnt p4Pos = p2.Translated(vMouse);

      if (!m_shape.IsNull()) {
        BRepExtrema_DistShapeShape ext3(
            BRepBuilderAPI_MakeVertex(p3Pos).Vertex(), m_shape);
        if (ext3.IsDone() && ext3.NbSolution() > 0)
          p3Pos = ext3.PointOnShape2(1);
        BRepExtrema_DistShapeShape ext4(
            BRepBuilderAPI_MakeVertex(p4Pos).Vertex(), m_shape);
        if (ext4.IsDone() && ext4.NbSolution() > 0)
          p4Pos = ext4.PointOnShape2(1);

        qDebug() << "OccView: Snapped p3 to" << p3Pos.X() << p3Pos.Y()
                 << p3Pos.Z();
        qDebug() << "OccView: Snapped p4 to" << p4Pos.X() << p4Pos.Y()
                 << p4Pos.Z();
      }

      // 1. Create Nodes in GUI (Signals are emitted by addTopologyNode)
      int n3 = addTopologyNode(p3Pos);
      int n4 = addTopologyNode(p4Pos);

      // 2. Determine Winding based on Core availability
      // We want to use the half-edge that is NOT already part of a face.
      bool useReverseWinding = false;
      if (m_topologyModel) {
        TopoEdge *shared = m_topologyModel->getEdge(n1, n2);
        if (shared) {
          // Check if the forward half-edge (n1->n2) is already occupied
          TopoHalfEdge *fhe = shared->getForwardHalfEdge();
          if (fhe && fhe->face != nullptr) {
            // Forward is occupied, we must use n2->n1 (reverse) for the new
            // face
            useReverseWinding = true;
          }
        }
      }

      // 3. Create Edges (addEdge signals topologyEdgeCreated to MainWindow)
      addEdge(n1, n3);
      addEdge(n2, n4);
      addEdge(n3, n4);

      // 4. Construct Node List for Face
      QList<int> faceNodes;
      if (!useReverseWinding) {
        faceNodes << n1 << n2 << n4 << n3;
      } else {
        faceNodes << n2 << n1 << n3 << n4;
      }

      qDebug() << "OccView: Attempting face creation with nodes:" << faceNodes;
      addTopologyFace(faceNodes); // Signals topologyFaceCreated to MainWindow
    }

    for (const auto &obj : m_facePreviewShapes) {
      if (!obj.IsNull())
        m_context->Remove(obj, Standard_True);
    }
    m_facePreviewShapes.clear();
    m_isExtrudingFace = false;
  }

  if (m_isDraggingNode) {
    m_isDraggingNode = false;
    m_draggedNodeId = -1;
  }
}

void OccView::addTopologyFace(const QList<int> &nodeIds) {
  // Strict Quad Domains Only
  if (nodeIds.size() != 4) {
    qDebug() << "OccView: Rejecting face creation - only QUADS are supported.";
    return;
  }

  int id = m_nextFaceId++;
  qDebug() << "  Created GUI Face ID:" << id << "with nodes" << nodeIds;
  createFaceVisual(id, nodeIds);
  m_faceNodeMap.insert(id, nodeIds);

  emit topologyFaceCreated(id, nodeIds);
}

Handle(AIS_InteractiveObject) OccView::buildFaceShape(
    const QList<int> &nodeIds) {
  // 1. Clean up nodeIds: remove consecutive duplicates and ensure enough unique
  // nodes
  QList<int> cleanIds;
  for (int id : nodeIds) {
    if (cleanIds.isEmpty() || cleanIds.last() != id) {
      cleanIds.append(id);
    }
  }
  // Remove wrapping duplicate if any
  if (cleanIds.size() > 1 && cleanIds.first() == cleanIds.last()) {
    cleanIds.removeLast();
  }

  if (cleanIds.size() < 3)
    return nullptr;

  // 2. Build polygon wire
  BRepBuilderAPI_MakePolygon polygonMaker;
  for (int id : cleanIds) {
    gp_Pnt p = getTopologyNodePosition(id);
    polygonMaker.Add(p);
  }
  polygonMaker.Close();

  if (!polygonMaker.IsDone())
    return nullptr;

  TopoDS_Wire wire = polygonMaker.Wire();

  // 3. Try to build a single face (works for planar polygons)
  try {
    BRepBuilderAPI_MakeFace faceMaker(wire);
    if (faceMaker.IsDone()) {
      return new AIS_ColoredShape(faceMaker.Face());
    }
  } catch (Standard_ConstructionError &) {
    // Fallback if planar face creation fails
  }

  // 4. Generic fallback: Triangulate (Triangle Fan) for non-planar or many-node
  // polygons
  TopoDS_Compound compound;
  BRep_Builder builder;
  builder.MakeCompound(compound);
  bool addedAny = false;

  if (cleanIds.size() >= 3) {
    gp_Pnt p0 = getTopologyNodePosition(cleanIds[0]);
    for (int i = 1; i < cleanIds.size() - 1; ++i) {
      gp_Pnt p1 = getTopologyNodePosition(cleanIds[i]);
      gp_Pnt p2 = getTopologyNodePosition(cleanIds[i + 1]);

      try {
        BRepBuilderAPI_MakePolygon triPoly;
        triPoly.Add(p0);
        triPoly.Add(p1);
        triPoly.Add(p2);
        triPoly.Close();

        if (triPoly.IsDone()) {
          BRepBuilderAPI_MakeFace triFace(triPoly.Wire());
          if (triFace.IsDone()) {
            builder.Add(compound, triFace.Face());
            addedAny = true;
          }
        }
      } catch (...) {
        // Skip degenerate triangles
      }
    }
  }

  if (addedAny) {
    return new AIS_ColoredShape(compound);
  }

  // Generic fallback: failed to make robust face
  return nullptr;
}

void OccView::createFaceVisual(int faceId, const QList<int> &nodeIds) {
  Handle(AIS_InteractiveObject) aisFace = buildFaceShape(nodeIds);
  if (aisFace.IsNull())
    return;

  // Set owner for robust selection
  aisFace->SetOwner(new EntityOwner(faceId));

  // Check for persistent style
  if (m_faceStyles.contains(faceId)) {
    const auto &style = m_faceStyles[faceId];
    Quantity_Color occColor(style.color.redF(), style.color.greenF(),
                            style.color.blueF(), Quantity_TOC_RGB);
    aisFace->SetColor(occColor);
    if (style.renderMode == 1) { // Translucent
      aisFace->SetTransparency(0.5);
    } else {
      aisFace->SetTransparency(0.0);
    }
  } else {
    // Default style: Translucent Red
    aisFace->SetColor(Quantity_NOC_RED);
    aisFace->SetTransparency(0.4);
  }

  // Set Display Mode to Shaded (1)
  m_context->SetDisplayMode(aisFace, 1, Standard_False);

  if (m_workbenchIndex == 1) {
    m_context->Display(aisFace, Standard_False);
    m_context->SetZLayer(aisFace, Graphic3d_ZLayerId_Top);

    if (m_faceStyles.contains(faceId) && m_faceStyles[faceId].renderMode == 2) {
      m_context->Erase(aisFace, Standard_False);
    } else {
      m_context->Deactivate(aisFace);
    }
  } else {
    m_context->Erase(aisFace, Standard_False);
  }

  m_topologyFaces.insert(faceId, aisFace);
}

void OccView::updateConnectedFaces(int nodeId) {
  QList<int> facesToUpdate;
  for (auto it = m_faceNodeMap.begin(); it != m_faceNodeMap.end(); ++it) {
    if (it.value().contains(nodeId)) {
      facesToUpdate.append(it.key());
    }
  }

  for (int faceId : facesToUpdate) {
    if (m_topologyFaces.contains(faceId)) {
      m_context->Remove(m_topologyFaces[faceId], Standard_False);
      m_topologyFaces.remove(faceId);
    }
    createFaceVisual(faceId, m_faceNodeMap[faceId]);
  }

  if (!facesToUpdate.isEmpty()) {
    m_view->Redraw();
  }
}

void OccView::mouseMoveEvent(QMouseEvent *event) {
  if (m_view.IsNull() || m_context.IsNull()) {
    return;
  }

  // Debug: track which branch is executing (every 30th call)
  static int moveCounter = 0;
  bool shouldDebugMove = (++moveCounter % 30 == 0);

  // Edge Creation Drag
  if (m_isCreatingEdge && m_interactionMode == Mode_Topology) {
    if (!m_draggedEdge.IsNull()) {
      m_context->Remove(m_draggedEdge, Standard_False);
    }
    gp_Pnt pStart = getTopologyNodePosition(m_edgeStartNodeId);
    gp_Pnt pEnd;
    TopoDS_Face face;
    if (castRayToMesh(event->x(), event->y(), pEnd, face)) {
      if (pStart.SquareDistance(pEnd) > 1e-4) {
        try {
          Handle(Geom_Line) line =
              new Geom_Line(pStart, gp_Dir(gp_Vec(pStart, pEnd)));
          Handle(AIS_Line) aisLine = new AIS_Line(line);
          Handle(Geom_CartesianPoint) pt1 = new Geom_CartesianPoint(pStart);
          Handle(Geom_CartesianPoint) pt2 = new Geom_CartesianPoint(pEnd);
          aisLine->SetPoints(pt1, pt2);
          aisLine->SetColor(Quantity_NOC_RED);
          aisLine->SetWidth(1.0);
          aisLine->Attributes()->SetLineAspect(
              new Prs3d_LineAspect(Quantity_NOC_YELLOW, Aspect_TOL_DOT, 1.0));
          m_draggedEdge = aisLine;
          m_context->Display(m_draggedEdge, Standard_True);
        } catch (Standard_ConstructionError &) {
        }
      }
    }
    return;
  }

  // Face Extrusion Preview
  if (m_isExtrudingFace && m_interactionMode == Mode_Topology) {
    for (auto obj : m_facePreviewShapes) {
      m_context->Remove(obj, Standard_True);
    }
    m_facePreviewShapes.clear();

    int n1 = m_selectedEdge.first;
    int n2 = m_selectedEdge.second;
    gp_Pnt p1 = getTopologyNodePosition(n1);
    gp_Pnt p2 = getTopologyNodePosition(n2);

    gp_Pnt pMouse;
    TopoDS_Face face;
    if (castRayToMesh(event->x(), event->y(), pMouse, face)) {
      gp_Vec vEdge(p1, p2);
      gp_Pnt pMid = p1.Translated(vEdge * 0.5);
      gp_Vec vMouse(pMid, pMouse);

      // Use the full mouse vector for free dragging
      gp_Pnt p3 = p1.Translated(vMouse);
      gp_Pnt p4 = p2.Translated(vMouse);

      // Apply robust snapping to preview as well
      if (!m_shape.IsNull()) {
        BRepExtrema_DistShapeShape ext3(BRepBuilderAPI_MakeVertex(p3).Vertex(),
                                        m_shape);
        if (ext3.IsDone() && ext3.NbSolution() > 0)
          p3 = ext3.PointOnShape2(1);
        BRepExtrema_DistShapeShape ext4(BRepBuilderAPI_MakeVertex(p4).Vertex(),
                                        m_shape);
        if (ext4.IsDone() && ext4.NbSolution() > 0)
          p4 = ext4.PointOnShape2(1);
      }

      try {
        TopoDS_Edge e1 = BRepBuilderAPI_MakeEdge(p1, p3);
        TopoDS_Edge e2 = BRepBuilderAPI_MakeEdge(p2, p4);
        TopoDS_Edge e3 = BRepBuilderAPI_MakeEdge(p3, p4);

        Handle(AIS_Shape) aisE1 = new AIS_Shape(e1);
        Handle(AIS_Shape) aisE2 = new AIS_Shape(e2);
        Handle(AIS_Shape) aisE3 = new AIS_Shape(e3);
        aisE1->SetColor(Quantity_NOC_RED);
        aisE2->SetColor(Quantity_NOC_RED);
        aisE3->SetColor(Quantity_NOC_RED);

        // Add preview nodes
        Handle(Geom_CartesianPoint) geomP3 = new Geom_CartesianPoint(p3);
        Handle(Geom_CartesianPoint) geomP4 = new Geom_CartesianPoint(p4);
        Handle(AIS_Point) aisP3 = new AIS_Point(geomP3);
        Handle(AIS_Point) aisP4 = new AIS_Point(geomP4);

        // Style preview nodes
        Handle(Prs3d_PointAspect) pointAspect =
            new Prs3d_PointAspect(Aspect_TOM_O, Quantity_NOC_YELLOW, 2.0);
        aisP3->Attributes()->SetPointAspect(pointAspect);
        aisP4->Attributes()->SetPointAspect(pointAspect);

        m_context->Display(aisE1, Standard_False);
        m_context->Display(aisE2, Standard_False);
        m_context->Display(aisE3, Standard_False);
        m_context->Display(aisP3, Standard_False);
        m_context->Display(aisP4, Standard_False);

        m_facePreviewShapes.append(aisE1);
        m_facePreviewShapes.append(aisE2);
        m_facePreviewShapes.append(aisE3);
        m_facePreviewShapes.append(aisP3);
        m_facePreviewShapes.append(aisP4);

        m_context->UpdateCurrentViewer();
      } catch (Standard_ConstructionError &) {
      }
    }
    return;
  }

  // Node Dragging
  if (m_isDraggingNode && m_interactionMode == Mode_Topology) {
    gp_Pnt p;
    TopoDS_Face face;
    if (castRayToMesh(event->x(), event->y(), p, face)) {
      gp_Pnt constrainedPos = applyConstraint(m_draggedNodeId, p);
      moveTopologyNode(m_draggedNodeId, constrainedPos);
      emit topologyNodeMoved(m_draggedNodeId, constrainedPos);
      m_view->Redraw();
    }
    // Note: We don't return here because we want to allow hover detection while
    // dragging
  }

  // Handle View Navigation vs Highlight/Hover
  bool isNavigating = false;

  // Rotate view
  if (event->buttons() & Qt::LeftButton &&
      !(event->modifiers() & Qt::ShiftModifier) && !m_isDraggingNode &&
      !m_isCreatingEdge && !m_isExtrudingFace) {
    if (shouldDebugMove)
      qDebug() << "mouseMoveEvent: rotate view";
    m_view->Rotation(event->x(), event->y());
    m_view->Redraw();
    isNavigating = true;
  }
  // Pan view
  else if ((event->buttons() & Qt::MiddleButton) ||
           (event->buttons() & Qt::LeftButton &&
            event->modifiers() & Qt::ShiftModifier)) {
    if (shouldDebugMove)
      qDebug() << "mouseMoveEvent: pan view";
    m_view->Pan(event->x() - myCurX, myCurY - event->y());
    m_view->Redraw();
    isNavigating = true;
  }

  // Only run Highlight/Hover if NOT navigating (rotating/panning)
  // But we DO allow it during m_isDraggingNode (which isn't navigation)
  if (!isNavigating) {
    if (shouldDebugMove)
      qDebug() << "mouseMoveEvent: highlight/hover - nodes:"
               << m_topologyNodes.size();

    m_context->MoveTo(event->x(), event->y(), m_view, true);

    // Track which node the mouse is over (for merge feature)
    if (m_interactionMode == Mode_Topology && !m_topologyNodes.isEmpty()) {
      int prevHovered = m_hoveredNodeId;
      m_hoveredNodeId = -1; // Reset

      if (m_context->HasDetected()) {
        Handle(AIS_InteractiveObject) detected =
            m_context->DetectedInteractive();

        // Find if the detected object is one of our nodes
        for (auto it = m_topologyNodes.begin(); it != m_topologyNodes.end();
             ++it) {
          if (it.value() == detected) {
            // Rule: Cannot hover the node we are currently dragging
            if (m_isDraggingNode && it.key() == m_draggedNodeId) {
              continue;
            }
            m_hoveredNodeId = it.key();
            break;
          }
        }
      }

      if (shouldDebugMove) {
        qDebug() << "HoveredNode:" << m_hoveredNodeId;
      }

      // Debug: log when hovered node changes
      if (m_hoveredNodeId != prevHovered && m_hoveredNodeId != -1) {
        qDebug() << "Hovering over node" << m_hoveredNodeId;
      }
    }
  }

  myCurX = event->x();
  myCurY = event->y();
}

void OccView::wheelEvent(QWheelEvent *event) {
  if (m_view.IsNull()) {
    return;
  }
  m_view->StartZoomAtPoint(event->position().x(), event->position().y());
  m_view->ZoomAtPoint(0, 0, event->angleDelta().y(), 0); // Zoom with delta
  m_view->Redraw();
}

void OccView::contextMenuEvent(QContextMenuEvent *event) {
  if (m_context.IsNull() || m_interactionMode != Mode_Topology) {
    return;
  }

  // Detect object under cursor
  m_context->MoveTo(event->x(), event->y(), m_view, Standard_True);
  if (!m_context->HasDetected()) {
    return;
  }

  Handle(AIS_InteractiveObject) detectedObj = m_context->DetectedInteractive();
  QPair<int, int> edgeNodes = qMakePair(-1, -1);
  int edgeId = -1;

  for (auto it = m_topologyEdges.begin(); it != m_topologyEdges.end(); ++it) {
    if (it.value() == detectedObj) {
      edgeNodes = it.key();
      edgeId = m_nodePairToEdgeIdMap.value(edgeNodes, -1);
      break;
    }
  }

  if (edgeId != -1) {
    QMenu menu(this);
    QAction *setSubdivs = menu.addAction(tr("Set Subdivisions..."));

    QAction *selectedAction = menu.exec(event->globalPos());
    if (selectedAction == setSubdivs) {
      TopoEdge *edge = m_topologyModel->getEdge(edgeId);
      if (edge) {
        bool ok;
        int current = edge->getSubdivisions();
        int newVal = QInputDialog::getInt(this, tr("Set Subdivisions"),
                                          tr("Number of subdivisions:"),
                                          current, 2, 1000, 1, &ok);
        if (ok) {
          m_topologyModel->propagateSubdivisions(edgeId, newVal);
          m_view->Redraw();
          qDebug() << "Propagated subdivisions" << newVal << "from edge"
                   << edgeId;
        }
      }
    }
  }
}

void OccView::createHUD() {
  if (m_hudWidget)
    return;

  m_hudWidget = new QWidget(this, Qt::SubWindow | Qt::FramelessWindowHint);
  m_hudWidget->setAttribute(Qt::WA_TranslucentBackground);
  m_hudWidget->setStyleSheet("background: transparent;");

  QVBoxLayout *mainLayout = new QVBoxLayout(m_hudWidget);
  mainLayout->setContentsMargins(10, 10, 10, 10);
  mainLayout->setSpacing(5);

  auto createButton = [](const QString &text, const QString &tooltip) {
    QPushButton *btn = new QPushButton(text);
    btn->setFixedSize(40, 40);
    btn->setCheckable(true);
    btn->setToolTip(tooltip);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setStyleSheet(
        "QPushButton { background: rgba(50, 50, 50, 180); color: white; "
        "border: 1px solid rgba(100, 100, 100, 150); border-radius: 4px; "
        "font-weight: bold; font-size: 14px; }"
        "QPushButton:checked { background: rgba(0, 120, 215, 200); border: 2px "
        "solid white; }"
        "QPushButton:hover { background: rgba(80, 80, 80, 200); }"
        "QPushButton:disabled { background: rgba(30, 30, 30, 100); color: "
        "rgba(100, 100, 100, 100); }");
    return btn;
  };

  // Row 1: Workbench Selector
  QHBoxLayout *row1 = new QHBoxLayout();
  row1->setSpacing(5);
  m_btnF1 = createButton("R", "R: Geometry");
  m_btnF2 = createButton("T", "T: Topology");
  m_btnF3 = createButton("Y", "Y: Smoother");
  row1->addWidget(m_btnF1);
  row1->addWidget(m_btnF2);
  row1->addWidget(m_btnF3);
  row1->addStretch();
  mainLayout->addLayout(row1);

  // Row 2: Selection Mode
  QHBoxLayout *row2 = new QHBoxLayout();
  row2->setSpacing(5);
  m_btnQ = createButton("Q", "Q: Node Toggle");
  m_btnW = createButton("W", "W: Edge Toggle");
  m_btnE = createButton("E", "E: Face Toggle");
  row2->addWidget(m_btnQ);
  row2->addWidget(m_btnW);
  row2->addWidget(m_btnE);
  row2->addStretch();
  mainLayout->addLayout(row2);

  // Optional: Keep old Selection Combo for Geometry mode if needed,
  // but spec says Row 2 is for selection mode.
  // For now I'll hide the old combo or integrate it.

  connect(m_btnF1, &QPushButton::clicked,
          [this]() { emit workbenchRequested(0); });
  connect(m_btnF2, &QPushButton::clicked,
          [this]() { emit workbenchRequested(1); });
  connect(m_btnF3, &QPushButton::clicked,
          [this]() { emit workbenchRequested(2); });

  connect(m_btnQ, &QPushButton::clicked, [this]() {
    if (m_interactionMode == Mode_Geometry) {
      setSelectionMode(1);
      m_geometrySelectionMode = 1;
      updateHUDStates();
    } else {
      setTopologySelectionMode(SelNodes);
    }
  });
  connect(m_btnW, &QPushButton::clicked, [this]() {
    if (m_interactionMode == Mode_Geometry) {
      setSelectionMode(2);
      m_geometrySelectionMode = 2;
      updateHUDStates();
    } else {
      setTopologySelectionMode(SelEdges);
    }
  });
  connect(m_btnE, &QPushButton::clicked, [this]() {
    if (m_interactionMode == Mode_Geometry) {
      setSelectionMode(4);
      m_geometrySelectionMode = 4;
      updateHUDStates();
    } else {
      setTopologySelectionMode(SelFaces);
    }
  });

  updateHUDStates();

  m_hudWidget->setGeometry(10, 10, 200, 100);
  m_hudWidget->show();
  m_hudWidget->raise();
}

void OccView::setWorkbench(int index) {
  m_workbenchIndex = index;
  updateHUDStates();

  if (m_context.IsNull())
    return;

  if (index == 2) { // Smoother Workbench
    if (!m_aisShape.IsNull())
      m_context->Erase(m_aisShape, Standard_False);
    for (auto node : m_topologyNodes)
      m_context->Erase(node, Standard_False);
    for (auto edge : m_topologyEdges)
      m_context->Erase(edge, Standard_False);
    for (auto face : m_topologyFaces)
      m_context->Erase(face, Standard_False);
    updateSmootherVisualization();
  } else if (index == 0) { // Geometry Workbench
    // Show Geometry
    if (!m_aisShape.IsNull()) {
      m_context->Display(m_aisShape, Standard_False);
    }
    // Hide Topology
    for (auto node : m_topologyNodes)
      m_context->Erase(node, Standard_False);
    for (auto edge : m_topologyEdges)
      m_context->Erase(edge, Standard_False);
    for (auto face : m_topologyFaces)
      m_context->Erase(face, Standard_False);
  } else if (index == 1) { // Topology Workbench
    // Show Geometry
    if (!m_aisShape.IsNull()) {
      m_context->Display(m_aisShape, Standard_False);
    }
    // Show Topology
    for (auto node : m_topologyNodes)
      m_context->Display(node, Standard_False);
    for (auto edge : m_topologyEdges)
      m_context->Display(edge, Standard_False);
    for (auto face : m_topologyFaces)
      m_context->Display(face, Standard_False);
  }

  // ENSURE selection state is synchronized with interaction mode
  setInteractionMode(m_interactionMode);

  if (index != 2) {
    hideSmootherVisualization();
  }

  if (!m_context.IsNull()) {
    m_context->UpdateCurrentViewer();
  }
}

void OccView::cycleSelection() {
  if (m_context.IsNull())
    return;

  // OCCT fills the "detected" list when MoveTo is called (usually in
  // mouseMoveEvent) AIS_InteractiveContext::DetectedTest() can check if
  // something is under the mouse. However, for cycling, we often want to
  // iterate hits.

  // AIS_InteractiveContext::InitDetected() etc provides access to what's under
  // the cursor after the last MoveTo.

  if (!m_context->MoreDetected())
    return;

  Handle(AIS_InteractiveObject) currentSel =
      m_context->HasSelectedShape() ? m_context->SelectedInteractive()
                                    : nullptr;

  Handle(AIS_InteractiveObject) first = nullptr;
  Handle(AIS_InteractiveObject) nextToPick = nullptr;
  bool foundCurrent = false;

  for (m_context->InitDetected(); m_context->MoreDetected();
       m_context->NextDetected()) {
    Handle(AIS_InteractiveObject) obj = m_context->DetectedInteractive();
    if (first.IsNull())
      first = obj;

    if (foundCurrent && nextToPick.IsNull()) {
      nextToPick = obj;
      break;
    }

    if (obj == currentSel) {
      foundCurrent = true;
    }
  }

  // If we didn't find a "next", loop back to first
  if (nextToPick.IsNull())
    nextToPick = first;

  if (!nextToPick.IsNull()) {
    m_context->ClearSelected(Standard_False);
    m_context->AddOrRemoveSelected(nextToPick, Standard_True);
    emit topologySelectionChanged();
  }
}

void OccView::selectAdjacents() {
  if (m_context.IsNull())
    return;

  if (m_interactionMode == Mode_Geometry) {
    if (m_aisShape.IsNull())
      return;

    // Find selected sub-shapes
    TopTools_ListOfShape selectedShapes;
    for (m_context->InitSelected(); m_context->MoreSelected();
         m_context->NextSelected()) {
      if (m_context->HasSelectedShape()) {
        selectedShapes.Append(m_context->SelectedShape());
      }
    }

    if (selectedShapes.IsEmpty())
      return;

    TopTools_IndexedMapOfShape adjacents;

    // For each selected shape, find neighbors of the same type
    for (TopTools_ListIteratorOfListOfShape it(selectedShapes); it.More();
         it.Next()) {
      const TopoDS_Shape &s = it.Value();
      if (s.ShapeType() == TopAbs_FACE) {
        // Find faces sharing an edge
        TopTools_IndexedDataMapOfShapeListOfShape edgeToFaceMap;
        TopExp::MapShapesAndAncestors(m_aisShape->Shape(), TopAbs_EDGE,
                                      TopAbs_FACE, edgeToFaceMap);

        for (TopExp_Explorer exp(s, TopAbs_EDGE); exp.More(); exp.Next()) {
          const TopoDS_Shape &edge = exp.Current();
          if (edgeToFaceMap.Contains(edge)) {
            const TopTools_ListOfShape &neighbors =
                edgeToFaceMap.FindFromKey(edge);
            for (TopTools_ListIteratorOfListOfShape nit(neighbors); nit.More();
                 nit.Next()) {
              if (!nit.Value().IsSame(s))
                adjacents.Add(nit.Value());
            }
          }
        }
      } else if (s.ShapeType() == TopAbs_EDGE) {
        // Find edges sharing a vertex
        TopTools_IndexedDataMapOfShapeListOfShape vertToEdgeMap;
        TopExp::MapShapesAndAncestors(m_aisShape->Shape(), TopAbs_VERTEX,
                                      TopAbs_EDGE, vertToEdgeMap);

        for (TopExp_Explorer exp(s, TopAbs_VERTEX); exp.More(); exp.Next()) {
          const TopoDS_Shape &vert = exp.Current();
          if (vertToEdgeMap.Contains(vert)) {
            const TopTools_ListOfShape &neighbors =
                vertToEdgeMap.FindFromKey(vert);
            for (TopTools_ListIteratorOfListOfShape nit(neighbors); nit.More();
                 nit.Next()) {
              if (!nit.Value().IsSame(s))
                adjacents.Add(nit.Value());
            }
          }
        }
      }
    }

    // Add adjacents to selection
    for (int i = 1; i <= adjacents.Extent(); ++i) {
      Handle(StdSelect_BRepOwner) owner =
          new StdSelect_BRepOwner(adjacents(i), m_aisShape);
      m_context->AddOrRemoveSelected(owner, Standard_False);
    }
    m_context->UpdateCurrentViewer();

  } else if (m_interactionMode == Mode_Topology) {
    // Similar logic for topology entities
    // For brevity, I'll implement face-to-face via edges and edge-to-edge via
    // nodes Selection queries
    QList<int> selFaceIds = getSelectedFaceIds();
    QList<QPair<int, int>> selEdgeIds = getSelectedEdgeIds();

    if (!selFaceIds.isEmpty()) {
      for (int fid : selFaceIds) {
        if (m_faceNodeMap.contains(fid)) {
          const QList<int> &nodes = m_faceNodeMap[fid];
          for (int i = 0; i < nodes.size(); ++i) {
            int n1 = nodes[i];
            int n2 = nodes[(i + 1) % nodes.size()];
            // Find other faces sharing this edge (n1, n2)
            for (auto fit = m_faceNodeMap.begin(); fit != m_faceNodeMap.end();
                 ++fit) {
              if (fit.key() == fid)
                continue;
              const QList<int> &fNodes = fit.value();
              bool hasEdge = false;
              for (int j = 0; j < fNodes.size(); ++j) {
                int fn1 = fNodes[j];
                int fn2 = fNodes[(j + 1) % fNodes.size()];
                if ((fn1 == n1 && fn2 == n2) || (fn1 == n2 && fn2 == n1)) {
                  hasEdge = true;
                  break;
                }
              }
              if (hasEdge) {
                highlightTopologyFace(fit.key(), true);
              }
            }
          }
        }
      }
    } else if (!selEdgeIds.isEmpty()) {
      for (const auto &edgeKey : selEdgeIds) {
        int n1 = edgeKey.first;
        int n2 = edgeKey.second;
        // Find edges sharing n1 or n2
        for (auto eit = m_topologyEdges.begin(); eit != m_topologyEdges.end();
             ++eit) {
          if (eit.key() == edgeKey)
            continue;
          if (eit.key().first == n1 || eit.key().second == n1 ||
              eit.key().first == n2 || eit.key().second == n2) {
            m_context->AddOrRemoveSelected(eit.value(), Standard_False);
          }
        }
      }
    }
    m_context->UpdateCurrentViewer();
    emit topologySelectionChanged();
  }
}
void OccView::updateHUDStates() {
  if (!m_hudWidget)
    return;

  // Row 1: Workbench
  m_btnF1->setChecked(m_workbenchIndex == 0);
  m_btnF2->setChecked(m_workbenchIndex == 1);
  m_btnF3->setChecked(m_workbenchIndex == 2);

  // Row 2: Selection Mode
  // Selection mode is only relevant for F1 (Geometry) and F2 (Topology)
  // Constraint from functional spec: Row 2 disabled when F3 is active
  bool allowSelection = (m_workbenchIndex != 2);

  m_btnQ->setEnabled(allowSelection);
  m_btnW->setEnabled(allowSelection);
  m_btnE->setEnabled(allowSelection);

  if (allowSelection) {
    if (m_interactionMode == Mode_Geometry) {
      m_btnQ->setChecked(m_geometrySelectionMode == 1);
      m_btnW->setChecked(m_geometrySelectionMode == 2);
      m_btnE->setChecked(m_geometrySelectionMode == 4);
    } else {
      m_btnQ->setChecked(m_topologySelectionMode == SelNodes);
      m_btnW->setChecked(m_topologySelectionMode == SelEdges);
      m_btnE->setChecked(m_topologySelectionMode == SelFaces);
    }
  } else {
    m_btnQ->setChecked(false);
    m_btnW->setChecked(false);
    m_btnE->setChecked(false);
  }
}

void OccView::createOverlay() {
  // Legacy method, can be empty or call createHUD
  createHUD();
}

// Selection logic is now handled by HUD Row 2 buttons (Q, W, E)
// which call setTopologySelectionMode.

void OccView::setShapeData(const TopoDS_Shape &shape,
                           const TopTools_IndexedMapOfShape &faceMap,
                           const TopTools_IndexedMapOfShape &edgeMap) {
  m_shape = shape;
  *m_faceMap = faceMap;
  *m_edgeMap = edgeMap;

  // Apply deflection settings to the interactive shape if it exists
  // We assume m_aisShape availability or it will be set later via setAisShape
}

void OccView::refreshDisplay() {
  if (!m_view.IsNull()) {
    m_view->Redraw();
  }
}

void OccView::reset() {
  if (m_context.IsNull())
    return;

  // 1. Remove Scene Geometry
  if (!m_aisShape.IsNull()) {
    m_context->Remove(m_aisShape, Standard_False);
    m_aisShape.Nullify();
  }

  // 2. Remove Topology Nodes
  for (auto node : m_topologyNodes) {
    m_context->Remove(node, Standard_False);
  }
  m_topologyNodes.clear();
  m_nextNodeId = 1;

  // 3. Clear Interaction State
  m_selectedNode.Nullify();
  m_isDraggingNode = false;
  m_draggedNodeId = -1;

  if (m_selectionCombo) {
    m_selectionCombo->setCurrentIndex(0); // Reset to Face
  }

  // 4. Clear internal data maps
  m_shape.Nullify();
  m_faceMap->Clear();
  m_edgeMap->Clear();

  // 5. Update Viewer
  m_context->RemoveAll(Standard_True); // Ensure everything is gone
  m_nextEdgeId = 1;
  m_nextFaceId = 1;
  m_edgeIdMap.clear();
  m_nodePairToEdgeIdMap.clear();
  m_topologyEdges.clear();
  m_topologyFaces.clear();
  m_faceNodeMap.clear();
  m_smootherObjects.clear();
  m_edgeStyles.clear();
  m_faceStyles.clear();
  m_view->Redraw();
}

void OccView::setEdgeGroupAppearance(const QList<int> &ids, const QColor &color,
                                     int renderMode) {
  if (m_aisShape.IsNull() || m_edgeMap->IsEmpty())
    return;

  // Convert QColor to OCCT color
  Quantity_Color occColor(color.redF(), color.greenF(), color.blueF(),
                          Quantity_TOC_RGB);

  for (int id : ids) {
    if (id < 1 || id > m_edgeMap->Extent())
      continue;

    TopoDS_Shape edge = m_edgeMap->FindKey(id);

    // For edges, we need to set color AND line width
    // SetCustomColor affects the edge curve color
    // SetCustomWidth affects line thickness for visibility
    switch (renderMode) {
    case 0: // Shaded - visible with thicker line
      m_aisShape->SetCustomColor(edge, occColor);
      m_aisShape->SetCustomWidth(edge, m_edgeWidth); // Configured width
      break;
    case 1: // Translucent - medium line
      m_aisShape->SetCustomColor(edge, occColor);
      m_aisShape->SetCustomWidth(edge, m_edgeWidth); // Configured width
      break;
    case 2: // Hidden - minimal width
      m_aisShape->SetCustomWidth(edge, 0.1);
      break;
    }
  }

  // Update the display
  m_context->Redisplay(m_aisShape, Standard_True);
}

void OccView::setFaceGroupAppearance(const QList<int> &ids, const QColor &color,
                                     int renderMode) {
  if (m_aisShape.IsNull() || m_faceMap->IsEmpty())
    return;

  // Convert QColor to OCCT color
  Quantity_Color occColor(color.redF(), color.greenF(), color.blueF(),
                          Quantity_TOC_RGB);

  for (int id : ids) {
    if (id < 1 || id > m_faceMap->Extent())
      continue;

    TopoDS_Shape face = m_faceMap->FindKey(id);

    // Apply color based on render mode
    switch (renderMode) {
    case 0: // Shaded
      m_aisShape->SetCustomColor(face, occColor);
      m_aisShape->SetCustomTransparency(face, 0.0);
      break;
    case 1: // Translucent
      m_aisShape->SetCustomColor(face, occColor);
      m_aisShape->SetCustomTransparency(face, 0.5);
      break;
    case 2: // Hidden
      m_aisShape->SetCustomTransparency(face, 1.0);
      break;
    }
  }

  // Update the display
  m_context->Redisplay(m_aisShape, Standard_True);
}

void OccView::loadConfig() {
  QString configPath = QCoreApplication::applicationDirPath() +
                       "/../Resources/render_config.json";
  // Fallback to working directory for dev
  if (!QFile::exists(configPath)) {
    configPath = "render_config.json";
  }

  QFile file(configPath);
  if (file.open(QIODevice::ReadOnly)) {
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isNull() && doc.isObject()) {
      QJsonObject obj = doc.object();
      auto parseColor = [](const QJsonValue &val,
                           const QColor &defaultColor) -> QColor {
        if (val.isArray()) {
          QJsonArray arr = val.toArray();
          if (arr.size() >= 3) {
            return QColor(arr[0].toInt(), arr[1].toInt(), arr[2].toInt());
          }
        }
        return defaultColor;
      };

      if (obj.contains("linear_deflection")) {
        m_linearDeflection = obj["linear_deflection"].toDouble();
      }
      if (obj.contains("angular_deflection")) {
        m_angularDeflection = obj["angular_deflection"].toDouble();
      }
      if (obj.contains("node_size")) {
        m_nodeSize = obj["node_size"].toDouble();
      }

      m_bgGradientTop = parseColor(obj["background_top"], m_bgGradientTop);
      m_bgGradientBottom =
          parseColor(obj["background_bottom"], m_bgGradientBottom);
      m_highlightColor = parseColor(obj["highlight_color"], m_highlightColor);
      m_selectionColor = parseColor(obj["selection_color"], m_selectionColor);

      if (obj.contains("edge_width"))
        m_edgeWidth = obj["edge_width"].toDouble();
      m_edgeColor = parseColor(obj["edge_color"], m_edgeColor);

      qDebug() << "Loaded render configuration from" << configPath;
      qDebug() << "Deflection:" << m_linearDeflection << m_angularDeflection;
      qDebug() << "Node Size:" << m_nodeSize;
    }
  } else {
    qDebug() << "Could not open render configuration:" << configPath
             << "- using defaults";
  }
}

void OccView::showEvent(QShowEvent *event) {
  if (m_view.IsNull()) {
    init();
  }
  QWidget::showEvent(event);
}

void OccView::mergeTopologyNodeEdges(int keepId, int removeId) {
  // Update visual edges: rewire edges from removeId to keepId
  // and remove self-loop edges

  QList<QPair<int, int>> edgesToRemove;
  QList<QPair<QPair<int, int>, QPair<int, int>>> edgesToRewire;

  for (auto it = m_topologyEdges.begin(); it != m_topologyEdges.end(); ++it) {
    int n1 = it.key().first;
    int n2 = it.key().second;

    bool modified = false;
    int newN1 = n1;
    int newN2 = n2;

    if (n1 == removeId) {
      newN1 = keepId;
      modified = true;
    }
    if (n2 == removeId) {
      newN2 = keepId;
      modified = true;
    }

    if (modified) {
      if (newN1 == newN2) {
        // Self-loop: mark for removal
        edgesToRemove.append(it.key());
      } else {
        // Rewire
        edgesToRewire.append(qMakePair(it.key(), qMakePair(newN1, newN2)));
      }
    }
  }

  // Remove self-loop edges
  for (const auto &key : edgesToRemove) {
    if (m_topologyEdges.contains(key)) {
      Handle(AIS_InteractiveObject) obj = m_topologyEdges[key];
      m_context->Remove(obj, Standard_False);
      m_topologyEdges.remove(key);
      emit topologyEdgeDeleted(key.first, key.second);
    }
  }

  // Check for duplicate edges and rewire
  QSet<QPair<int, int>> seenEdges;
  for (auto it = m_topologyEdges.begin(); it != m_topologyEdges.end(); ++it) {
    // Skip edges that will be rewired
    bool willRewire = false;
    for (const auto &remap : edgesToRewire) {
      if (remap.first == it.key()) {
        willRewire = true;
        break;
      }
    }
    if (!willRewire) {
      int n1 = it.key().first;
      int n2 = it.key().second;
      auto normalized = qMakePair(qMin(n1, n2), qMax(n1, n2));
      seenEdges.insert(normalized);
    }
  }

  // Rewire edges
  for (const auto &remap : edgesToRewire) {
    QPair<int, int> oldKey = remap.first;
    QPair<int, int> newKey = remap.second;

    // Normalize new key
    auto normalizedNew = qMakePair(qMin(newKey.first, newKey.second),
                                   qMax(newKey.first, newKey.second));

    // Check for duplicate
    if (seenEdges.contains(normalizedNew)) {
      // Duplicate: remove the old edge
      if (m_topologyEdges.contains(oldKey)) {
        Handle(AIS_InteractiveObject) obj = m_topologyEdges[oldKey];
        m_context->Remove(obj, Standard_False);
        m_topologyEdges.remove(oldKey);
        emit topologyEdgeDeleted(oldKey.first, oldKey.second);
      }
    } else {
      // Rewire: remove old, create new
      if (m_topologyEdges.contains(oldKey)) {
        Handle(AIS_InteractiveObject) obj = m_topologyEdges[oldKey];
        m_context->Remove(obj, Standard_False);
        m_topologyEdges.remove(oldKey);
        emit topologyEdgeDeleted(oldKey.first, oldKey.second);

        // Create new edge visualization
        gp_Pnt p1 = getTopologyNodePosition(newKey.first);
        gp_Pnt p2 = getTopologyNodePosition(newKey.second);

        if (p1.SquareDistance(p2) > 1e-4) {
          try {
            Handle(Geom_Line) line = new Geom_Line(p1, gp_Dir(gp_Vec(p1, p2)));
            Handle(AIS_Line) aisLine = new AIS_Line(line);

            Handle(Geom_CartesianPoint) pt1 = new Geom_CartesianPoint(p1);
            Handle(Geom_CartesianPoint) pt2 = new Geom_CartesianPoint(p2);
            aisLine->SetPoints(pt1, pt2);

            aisLine->SetWidth(m_edgeWidth);
            aisLine->SetColor(Quantity_NOC_RED);

            m_context->Display(aisLine, Standard_False);
            m_topologyEdges.insert(newKey, aisLine);

            int edgeId = m_nextEdgeId++;
            m_edgeIdMap.insert(edgeId, newKey);
            m_nodePairToEdgeIdMap.insert(newKey, edgeId);

            emit topologyEdgeCreated(newKey.first, newKey.second, edgeId);
          } catch (Standard_ConstructionError &) {
            qDebug() << "mergeTopologyNodeEdges: Construction error for edge"
                     << newKey.first << "-" << newKey.second;
          }
        }

        seenEdges.insert(normalizedNew);
      }
    }
  }

  // Update Face Node Map
  // Since we merged nodes, any face using removeId now uses keepId
  // And we need to remove duplicate nodes in face definitions if any
  auto faceIt = m_faceNodeMap.begin();
  while (faceIt != m_faceNodeMap.end()) {
    QList<int> &nodes = faceIt.value();
    for (int i = 0; i < nodes.size(); ++i) {
      if (nodes[i] == removeId) {
        nodes[i] = keepId;
      }
    }

    // Clean up duplicates (collapse edges sequentially)
    QList<int> cleanNodes;
    for (int id : nodes) {
      if (cleanNodes.isEmpty() || cleanNodes.last() != id) {
        cleanNodes.append(id);
      }
    }
    if (cleanNodes.size() > 1 && cleanNodes.first() == cleanNodes.last()) {
      cleanNodes.removeLast();
    }
    nodes = cleanNodes;
    ++faceIt;
  }

  // Face and Edge updates are now strictly orchestrated by MainWindow
  // to ensure they are only refreshed if they still exist in the core model.

  // Refresh viewer if not handled by MainWindow calls later
  if (!m_view.IsNull()) {
    m_view->Redraw();
  }
}

gp_Pnt OccView::applyConstraint(int nodeId, const gp_Pnt &newPos) {
  if (!m_nodeConstraints.contains(nodeId)) {
    return newPos;
  }

  const NodeConstraint &c = m_nodeConstraints[nodeId];
  qDebug() << "OccView: applyConstraint for node" << nodeId << "type:" << c.type
           << "geoIds:" << c.geometryIds.size() << "isEdge:" << c.isEdgeGroup;
  if (c.type == ConstraintFixed) {
    return getTopologyNodePosition(nodeId); // Return original position
  } else if (c.type == ConstraintEdge) {
    // Project newPos onto line defined by origin and dir
    gp_Vec v(c.origin, newPos);
    double proj = v.Dot(c.dir);
    return c.origin.Translated(gp_Vec(c.dir) * proj);
  } else if (c.type == ConstraintFace) {
    // Project onto plane defined by origin and normal (dir)
    gp_Vec v(c.origin, newPos);
    double dist = v.Dot(c.dir);
    return newPos.Translated(-dist * gp_Vec(c.dir));
  } else if (c.type == ConstraintGeometry) {
    if (c.geometryIds.isEmpty())
      return newPos;

    gp_Pnt bestPnt = newPos;
    double minSqDist = RealLast();

    for (int geoId : c.geometryIds) {
      TopoDS_Shape shape;
      if (c.isEdgeGroup) {
        if (geoId > 0 && geoId <= m_edgeMap->Extent()) {
          shape = m_edgeMap->FindKey(geoId);
        }
      } else {
        if (geoId > 0 && geoId <= m_faceMap->Extent()) {
          shape = m_faceMap->FindKey(geoId);
        }
      }

      if (shape.IsNull())
        continue;

      BRepExtrema_DistShapeShape extrema(
          BRepBuilderAPI_MakeVertex(newPos).Vertex(), shape);
      if (extrema.IsDone() && extrema.NbSolution() > 0) {
        double d2 = extrema.Value();
        if (d2 < minSqDist) {
          minSqDist = d2;
          bestPnt = extrema.PointOnShape2(1);
        }
      }
    }
    return bestPnt;
  }

  return newPos;
}

// ============================================================================
// Topology Group Appearance
// ============================================================================

void OccView::setTopologyFaceGroupAppearance(const QList<int> &ids,
                                             const QColor &color,
                                             int renderMode) {
  if (m_context.IsNull())
    return;

  Quantity_Color occColor(color.redF(), color.greenF(), color.blueF(),
                          Quantity_TOC_RGB);

  for (int id : ids) {
    m_faceStyles[id] = {color, renderMode};
    if (!m_topologyFaces.contains(id))
      continue;

    Handle(AIS_InteractiveObject) aisObj = m_topologyFaces[id];
    Handle(AIS_ColoredShape) aisFace =
        Handle(AIS_ColoredShape)::DownCast(aisObj);
    if (aisFace.IsNull())
      continue;

    switch (renderMode) {
    case 0: // Shaded
      aisFace->SetColor(occColor);
      aisFace->SetTransparency(0.0);
      if (m_workbenchIndex == 1)
        m_context->Display(aisFace, Standard_False);
      else
        m_context->Erase(aisFace, Standard_False);
      break;
    case 1: // Translucent
      aisFace->SetColor(occColor);
      aisFace->SetTransparency(0.5);
      if (m_workbenchIndex == 1)
        m_context->Display(aisFace, Standard_False);
      else
        m_context->Erase(aisFace, Standard_False);
      break;
    case 2: // Hidden
      m_context->Erase(aisFace, Standard_False);
      break;
    }
  }

  m_context->UpdateCurrentViewer();
}

void OccView::setTopologyEdgeGroupAppearance(const QList<int> &ids,
                                             const QColor &color,
                                             int renderMode) {
  if (m_context.IsNull())
    return;

  Quantity_Color occColor(color.redF(), color.greenF(), color.blueF(),
                          Quantity_TOC_RGB);

  for (int id : ids) {
    m_edgeStyles[id] = {color, renderMode};
    if (!m_edgeIdMap.contains(id))
      continue;

    QPair<int, int> key = m_edgeIdMap[id];
    if (!m_topologyEdges.contains(key))
      continue;

    Handle(AIS_InteractiveObject) aisObj = m_topologyEdges[key];

    switch (renderMode) {
    case 0: // Shaded
      aisObj->SetColor(occColor);
      aisObj->SetWidth(m_edgeWidth);
      if (m_workbenchIndex == 1)
        m_context->Display(aisObj, Standard_False);
      else
        m_context->Erase(aisObj, Standard_False);
      break;
    case 1: // Translucent
      aisObj->SetColor(occColor);
      aisObj->SetWidth(m_edgeWidth);
      if (m_workbenchIndex == 1)
        m_context->Display(aisObj, Standard_False);
      else
        m_context->Erase(aisObj, Standard_False);
      break;
    case 2: // Hidden
      m_context->Erase(aisObj, Standard_False);
      break;
    }
  }

  m_context->UpdateCurrentViewer();
}

void OccView::hideSmootherVisualization() {
  if (m_context.IsNull())
    return;

  for (auto obj : m_smootherObjects) {
    m_context->Remove(obj, Standard_False);
  }
  m_smootherObjects.clear();
}

void OccView::updateSmootherVisualization() {
  qDebug() << "OccView: updateSmootherVisualization starting...";
  hideSmootherVisualization();

  if (!m_topologyModel) {
    qDebug() << "OccView: No topology model!";
    return;
  }

  const auto &faces = m_topologyModel->getFaces();
  qDebug() << "OccView: Creating TFI mesh for" << faces.size() << "faces";

  for (auto const &[id, face] : faces) {
    qDebug() << "OccView: Face ID:" << id;
    createTfiMesh(id);
  }

  if (m_view) {
    m_view->Redraw();
  }
  qDebug() << "OccView: updateSmootherVisualization done.";
}

void OccView::createTfiMesh(int faceId) {
  if (!m_topologyModel || m_context.IsNull())
    return;
  TopoFace *face = m_topologyModel->getFace(faceId);
  if (!face)
    return;

  // 1. Get ordered half-edges
  std::vector<TopoHalfEdge *> loop;
  TopoHalfEdge *startHe = face->getBoundary();
  if (!startHe)
    return;
  TopoHalfEdge *current = startHe;
  do {
    loop.push_back(current);
    if (!current->next) {
      qDebug() << "OccView: Half-edge next is NULL!";
      break;
    }
    current = current->next;
  } while (current != startHe && current != nullptr && loop.size() < 100);

  qDebug() << "OccView: Face" << faceId << "boundary loop size:" << loop.size();

  if (loop.size() != 4) {
    qDebug() << "OccView: Face" << faceId
             << "does not have 4 edges. Skipping TFI mesh.";
    return;
  }

  // 2. Extract corners and subdivision counts
  if (loop[0]->origin == nullptr || loop[1]->origin == nullptr ||
      loop[2]->origin == nullptr || loop[3]->origin == nullptr) {
    qDebug() << "OccView: One or more loop node origins are NULL!";
    return;
  }
  if (loop[0]->parentEdge == nullptr || loop[1]->parentEdge == nullptr) {
    qDebug() << "OccView: One or more loop parent edges are NULL!";
    return;
  }

  gp_Pnt c0 = loop[0]->origin->getPosition();
  gp_Pnt c1 = loop[1]->origin->getPosition();
  gp_Pnt c2 = loop[2]->origin->getPosition();
  gp_Pnt c3 = loop[3]->origin->getPosition();

  int M = loop[0]->parentEdge->getSubdivisions();
  int N = loop[1]->parentEdge->getSubdivisions();
  if (M < 1)
    M = 1;
  if (N < 1)
    N = 1;

  // 3. Generate Grid Points using TFI
  auto getPoint = [&](double u, double v) {
    // Linear boundary interpolation (straight edges for now)
    gp_Pnt s0_u = gp_Pnt(c0.XYZ() * (1.0 - u) + c1.XYZ() * u);
    gp_Pnt s2_u = gp_Pnt(c3.XYZ() * (1.0 - u) + c2.XYZ() * u);
    gp_Pnt s3_v = gp_Pnt(c0.XYZ() * (1.0 - v) + c3.XYZ() * v);
    gp_Pnt s1_v = gp_Pnt(c1.XYZ() * (1.0 - v) + c2.XYZ() * v);

    gp_XYZ p_uv = (s0_u.XYZ() * (1.0 - v) + s2_u.XYZ() * v +
                   s3_v.XYZ() * (1.0 - u) + s1_v.XYZ() * u) -
                  (c0.XYZ() * (1.0 - u) * (1.0 - v) + c1.XYZ() * u * (1.0 - v) +
                   c2.XYZ() * u * v + c3.XYZ() * (1.0 - u) * v);
    return gp_Pnt(p_uv);
  };

  // 3.1 Validate subdivisions
  if (M < 1)
    M = 1;
  if (N < 1)
    N = 1;
  if (M > 200 || N > 200) { // Safety cap
    qDebug() << "OccView: Subdivisions too high (" << M << "x" << N
             << "). Capping to 200.";
    M = std::min(M, 200);
    N = std::min(N, 200);
  }

  int nbNodes = (M + 1) * (N + 1);
  int nbTriangles = 2 * M * N;
  Handle(Poly_Triangulation) triangulation =
      new Poly_Triangulation(nbNodes, nbTriangles, Standard_False);

  // Fill nodes
  for (int j = 0; j <= N; ++j) {
    double v = (double)j / N;
    for (int i = 0; i <= M; ++i) {
      double u = (double)i / M;
      triangulation->SetNode(j * (M + 1) + i + 1, getPoint(u, v));
    }
  }

  // Fill triangles
  int triIdx = 1;
  for (int j = 0; j < N; ++j) {
    for (int i = 0; i < M; ++i) {
      int n1 = j * (M + 1) + i + 1;
      int n2 = n1 + 1;
      int n3 = (j + 1) * (M + 1) + i + 1;
      int n4 = n3 + 1;
      triangulation->SetTriangle(triIdx++, Poly_Triangle(n1, n2, n4));
      triangulation->SetTriangle(triIdx++, Poly_Triangle(n1, n4, n3));
    }
  }

  // 4. Create and Display Face Visualization (White)
  Handle(AIS_Triangulation) aisMesh = new AIS_Triangulation(triangulation);
  aisMesh->SetColor(Quantity_NOC_WHITE);
  m_context->Display(aisMesh, Standard_False);
  m_smootherObjects.append(aisMesh);

  // 5. Create and Display Grid Lines (Blue)
  Quantity_Color blue(Quantity_NOC_BLUE);

  // Vertical lines (constant u)
  for (int i = 0; i <= M; ++i) {
    double u = (double)i / M;
    bool isBoundary = (i == 0 || i == M);
    for (int j = 0; j < N; ++j) {
      gp_Pnt p1 = getPoint(u, (double)j / N);
      gp_Pnt p2 = getPoint(u, (double)(j + 1) / N);
      if (p1.SquareDistance(p2) < 1e-7)
        continue;

      try {
        Handle(Geom_Line) geomLine = new Geom_Line(p1, gp_Dir(gp_Vec(p1, p2)));
        Handle(AIS_Line) aisLine = new AIS_Line(geomLine);
        aisLine->SetPoints(new Geom_CartesianPoint(p1),
                           new Geom_CartesianPoint(p2));
        aisLine->SetColor(blue);
        aisLine->SetWidth(isBoundary ? (m_edgeWidth > 1.0 ? m_edgeWidth : 2.0)
                                     : 1.0);
        m_context->Display(aisLine, Standard_False);
        m_smootherObjects.append(aisLine);
      } catch (Standard_ConstructionError &) {
        // Skip degenerate segment
      }
    }
  }

  // Horizontal lines (constant v)
  for (int j = 0; j <= N; ++j) {
    double v = (double)j / N;
    bool isBoundary = (j == 0 || j == N);
    for (int i = 0; i < M; ++i) {
      gp_Pnt p1 = getPoint((double)i / M, v);
      gp_Pnt p2 = getPoint((double)(i + 1) / M, v);
      if (p1.SquareDistance(p2) < 1e-7)
        continue;

      try {
        Handle(Geom_Line) geomLine = new Geom_Line(p1, gp_Dir(gp_Vec(p1, p2)));
        Handle(AIS_Line) aisLine = new AIS_Line(geomLine);
        aisLine->SetPoints(new Geom_CartesianPoint(p1),
                           new Geom_CartesianPoint(p2));
        aisLine->SetColor(blue);
        aisLine->SetWidth(isBoundary ? (m_edgeWidth > 1.0 ? m_edgeWidth : 2.0)
                                     : 1.0);
        m_context->Display(aisLine, Standard_False);
        m_smootherObjects.append(aisLine);
      } catch (Standard_ConstructionError &) {
        // Skip degenerate segment
      }
    }
  }
}

void OccView::runEllipticSolver(const SmootherConfig &config) {
  if (!m_topologyModel || m_context.IsNull()) {
    qDebug() << "OccView::runEllipticSolver: Model or Context is null";
    return;
  }

  qDebug() << "OccView::runEllipticSolver: Starting solver...";
  hideSmootherVisualization();

  // --- Helper 1: Build a single Shape from a list of IDs ---
  auto buildTargetShape = [&](const QList<int> &ids,
                              bool isEdge) -> TopoDS_Shape {
    if (ids.isEmpty())
      return TopoDS_Shape();

    TopoDS_Compound comp;
    BRep_Builder B;
    B.MakeCompound(comp);
    bool added = false;

    const auto *map = isEdge ? m_edgeMap : m_faceMap;
    if (!map)
      return TopoDS_Shape();

    for (int id : ids) {
      if (id > 0 && id <= map->Extent()) {
        B.Add(comp, map->FindKey(id));
        added = true;
      }
    }
    return added ? comp : TopoDS_Shape();
  };

  // --- Helper 2: Project point to a shape (Wire or Shell) ---
  auto projectToShape = [&](const gp_Pnt &p, const TopoDS_Shape &s) -> gp_Pnt {
    if (s.IsNull())
      return p;
    // Note: Precision set to 1e-3, infinite range
    BRepExtrema_DistShapeShape extrema(BRepBuilderAPI_MakeVertex(p).Vertex(),
                                       s);
    if (extrema.IsDone() && extrema.NbSolution() > 0) {
      // Find closest solution
      // BRepExtrema sorts solutions by distance, so index 1 is closest
      return extrema.PointOnShape2(1);
    }
    return p;
  };

  const auto &faces = m_topologyModel->getFaces();
  qDebug() << "OccView::runEllipticSolver: Processing" << faces.size()
           << "faces.";

  for (auto const &[faceId, face] : faces) {
    // 1. Get ordered half-edges
    std::vector<TopoHalfEdge *> loop;
    TopoHalfEdge *startHe = face->getBoundary();
    if (!startHe) {
      qDebug() << "  Face" << faceId << "has no boundary.";
      continue;
    }

    TopoHalfEdge *current = startHe;
    int safety = 0;
    do {
      loop.push_back(current);
      current = current->next;
      safety++;
    } while (current != startHe && current != nullptr && safety < 100);

    if (loop.size() != 4) {
      qDebug() << "  Face" << faceId
               << "is not a Quad (Loop size:" << loop.size() << "). Skipping.";
      continue;
    }

    // 2. Identify Surface Constraint
    TopoDS_Shape surfaceConstraint;
    NodeConstraint nc0 = getNodeConstraint(loop[0]->origin->getID());

    // Check if constraint is generic geometry (not specifically an edge group)
    if (nc0.type == ConstraintGeometry && !nc0.isEdgeGroup) {
      surfaceConstraint = buildTargetShape(nc0.geometryIds, false);
      qDebug() << "  Face" << faceId << "constrained to"
               << nc0.geometryIds.size() << "CAD faces.";
    }

    // 3. Discretize Boundaries
    std::vector<std::vector<gp_Pnt>> boundaries(4);

    for (int k = 0; k < 4; ++k) {
      TopoEdge *edge = loop[k]->parentEdge;
      TopoNode *nStart = loop[k]->origin;
      TopoNode *nEnd = loop[(k + 1) % 4]->origin;

      int segments = edge->getSubdivisions();
      if (segments < 1)
        segments = 1;
      boundaries[k].resize(segments + 1);

      // Detect Edge Constraint
      TopoDS_Shape edgeConstraint;
      NodeConstraint c1 = getNodeConstraint(nStart->getID());
      NodeConstraint c2 = getNodeConstraint(nEnd->getID());

      if (c1.type == ConstraintGeometry && c1.isEdgeGroup &&
          c2.type == ConstraintGeometry && c2.isEdgeGroup &&
          c1.geometryIds == c2.geometryIds) {
        edgeConstraint = buildTargetShape(c1.geometryIds, true);
      }

      // Generate Points
      gp_Pnt pStart = nStart->getPosition();
      gp_Pnt pEnd = nEnd->getPosition();

      for (int i = 0; i <= segments; ++i) {
        double t = (double)i / segments;
        gp_XYZ xyz = pStart.XYZ() * (1.0 - t) + pEnd.XYZ() * t;
        gp_Pnt pLin(xyz);

        if (!edgeConstraint.IsNull()) {
          boundaries[k][i] = projectToShape(pLin, edgeConstraint);
        } else if (!surfaceConstraint.IsNull()) {
          boundaries[k][i] = projectToShape(pLin, surfaceConstraint);
        } else {
          boundaries[k][i] = pLin;
        }
      }
    }

    // 4. Initialize Grid (TFI)
    int M = loop[0]->parentEdge->getSubdivisions();
    int N = loop[1]->parentEdge->getSubdivisions();
    if (M < 1)
      M = 1;
    if (N < 1)
      N = 1;

    std::vector<std::vector<gp_Pnt>> grid(M + 1, std::vector<gp_Pnt>(N + 1));
    std::vector<std::vector<bool>> isFixed(M + 1,
                                           std::vector<bool>(N + 1, false));

    for (int i = 0; i <= M; ++i) {
      for (int j = 0; j <= N; ++j) {
        double u = (double)i / M;
        double v = (double)j / N;

        gp_XYZ pBottom = boundaries[0][i].XYZ();
        gp_XYZ pRight = boundaries[1][j].XYZ();
        gp_XYZ pTop = boundaries[2][M - i].XYZ();
        gp_XYZ pLeft = boundaries[3][N - j].XYZ();

        gp_XYZ cSW = boundaries[0][0].XYZ();
        gp_XYZ cSE = boundaries[0][M].XYZ();
        gp_XYZ cNE = boundaries[2][0].XYZ();
        gp_XYZ cNW = boundaries[2][M].XYZ();

        gp_XYZ pTFI = (1.0 - v) * pBottom + v * pTop + (1.0 - u) * pLeft +
                      u * pRight -
                      ((1.0 - u) * (1.0 - v) * cSW + u * (1.0 - v) * cSE +
                       u * v * cNE + (1.0 - u) * v * cNW);

        grid[i][j] = gp_Pnt(pTFI);

        // Project interior points
        if (!surfaceConstraint.IsNull()) {
          if (i > 0 && i < M && j > 0 && j < N) {
            grid[i][j] = projectToShape(grid[i][j], surfaceConstraint);
          }
        }

        if (i == 0 || i == M || j == 0 || j == N) {
          isFixed[i][j] = true;
        }
      }
    }

    // 5. Smooth with Projection
    EllipticSolver::Params params;
    params.iterations = config.faceIters;
    params.relaxation = config.faceRelax;
    params.bcRelaxation = config.faceBCRelax;

    auto constraintFunc = [&](int i, int j, const gp_Pnt &p) -> gp_Pnt {
      if (i == 0 || i == M || j == 0 || j == N)
        return p;
      return projectToShape(p, surfaceConstraint);
    };

    EllipticSolver::smoothGrid(grid, isFixed, params, constraintFunc);

    // 6. Visualization
    int nbNodes = (M + 1) * (N + 1);
    int nbTriangles = 2 * M * N;
    Handle(Poly_Triangulation) triangulation =
        new Poly_Triangulation(nbNodes, nbTriangles, Standard_False);

    // Fill Nodes
    for (int j = 0; j <= N; ++j) {
      for (int i = 0; i <= M; ++i) {
        triangulation->SetNode(j * (M + 1) + i + 1, grid[i][j]);
      }
    }

    // Fill Triangles
    int triIdx = 1;
    for (int j = 0; j < N; ++j) {
      for (int i = 0; i < M; ++i) {
        int n1 = j * (M + 1) + i + 1;
        int n2 = n1 + 1;
        int n3 = (j + 1) * (M + 1) + i + 1;
        int n4 = n3 + 1;
        triangulation->SetTriangle(triIdx++, Poly_Triangle(n1, n2, n4));
        triangulation->SetTriangle(triIdx++, Poly_Triangle(n1, n4, n3));
      }
    }

    // --- Create Shaded Mesh Visualization ---
    TopoDS_Face visFace;
    BRep_Builder B;
    B.MakeFace(visFace);                  // Create empty face
    B.UpdateFace(visFace, triangulation); // Assign triangulation

    Handle(AIS_ColoredShape) aisMesh = new AIS_ColoredShape(visFace);
    aisMesh->SetColor(Quantity_NOC_WHITE);
    // Mode 1 is Shaded. This ensures it's filled, not just wireframe.
    m_context->Display(aisMesh, 1, -1, Standard_False);
    m_smootherObjects.append(aisMesh);

    // --- Overlay Grid Lines (Blue) ---
    Quantity_Color gridColor(Quantity_NOC_BLUE1);

    // Vertical Lines
    for (int i = 0; i <= M; ++i) {
      for (int j = 0; j < N; ++j) {
        gp_Pnt p1 = grid[i][j];
        gp_Pnt p2 = grid[i][j + 1];
        if (p1.SquareDistance(p2) > 1e-10) {
          Handle(AIS_Line) line = new AIS_Line(new Geom_CartesianPoint(p1),
                                               new Geom_CartesianPoint(p2));
          line->SetColor(gridColor);
          m_context->Display(line, Standard_False);
          m_smootherObjects.append(line);
        }
      }
    }
    // Horizontal Lines
    for (int j = 0; j <= N; ++j) {
      for (int i = 0; i < M; ++i) {
        gp_Pnt p1 = grid[i][j];
        gp_Pnt p2 = grid[i + 1][j];
        if (p1.SquareDistance(p2) > 1e-10) {
          Handle(AIS_Line) line = new AIS_Line(new Geom_CartesianPoint(p1),
                                               new Geom_CartesianPoint(p2));
          line->SetColor(gridColor);
          m_context->Display(line, Standard_False);
          m_smootherObjects.append(line);
        }
      }
    }
  }

  if (m_view) {
    m_view->Redraw();
  }
  qDebug() << "OccView::runEllipticSolver: Finished.";
}