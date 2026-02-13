#pragma once

#include <QComboBox>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QList>
#include <QMap>
#include <QMenu>
#include <QMouseEvent>
#include <QObject>
#include <QPair>
#include <QPushButton>
#include <QWheelEvent>
#include <QWidget>

#include <AIS_ColoredShape.hxx>
#include <AIS_InteractiveContext.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <V3d_View.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

class Topology;
class TopoFace;

class OccView : public QWidget {
  Q_OBJECT
public:
  explicit OccView(QWidget *parent = nullptr);
  ~OccView() override;

  // Accessor for the context (needed to display shapes)
  Handle(AIS_InteractiveContext) getContext() const { return m_context; }

  // Fit the camera to the scene
  void fitAll();

  // Topology model reference
  void setTopologyModel(Topology *topo) { m_topologyModel = topo; }

  // Align view to closest axis (Ctrl+A)
  void alignToClosestAxis();

  // Set appearance for edge/face groups
  void setEdgeGroupAppearance(const QList<int> &ids, const QColor &color,
                              int renderMode);
  void setFaceGroupAppearance(const QList<int> &ids, const QColor &color,
                              int renderMode);

  // Store shape data for appearance updates
  void setShapeData(const TopoDS_Shape &shape,
                    const TopTools_IndexedMapOfShape &faceMap,
                    const TopTools_IndexedMapOfShape &edgeMap);

  // Store AIS shape for appearance updates
  void setAisShape(const Handle(AIS_ColoredShape) & aisShape) {
    m_aisShape = aisShape;
    if (!m_aisShape.IsNull()) {
      m_aisShape->SetOwnDeviationCoefficient(m_linearDeflection);
      m_aisShape->SetOwnDeviationAngle(m_angularDeflection);

      // Deactivate selection if we are already in topology mode
      if (m_interactionMode == Mode_Topology && !m_context.IsNull()) {
        m_context->Deactivate(m_aisShape);
      }
    }
  }

  // Refresh display after appearance changes
  void refreshDisplay();
  void reset(); // Reset the view (clear shapes and nodes)

  void init(); // Initialize the OCCT viewer

  // Interaction Mode
  enum InteractionMode {
    Mode_Geometry, // Original selection mode
    Mode_Topology  // Node creation/editing mode
  };
  void setInteractionMode(InteractionMode mode);
  InteractionMode getInteractionMode() const { return m_interactionMode; }

  // Workbench Management
  void setWorkbench(int index); // 0=F1, 1=F2, 2=F3

  // Topology Selection Mode
  enum TopologySelectionMode { SelNodes, SelEdges, SelFaces };
  void setTopologySelectionMode(int mode); // Use int for Qt signals
  void clearTopologySelection();
  void cycleSelection();
  void selectAdjacents();

  // Selection Queries
  QList<int> getSelectedNodeIds() const;
  QList<QPair<int, int>> getSelectedEdgeIds() const;
  QList<int> getSelectedFaceIds() const;

  // Topology Edits
  int addTopologyNode(const gp_Pnt &p);
  void removeTopologyNode(int id);
  void removeTopologyEdge(int n1, int n2);
  void removeTopologyFace(int id);
  void addTopologyFace(const QList<int> &nodeIds);
  void createFaceVisual(int faceId, const QList<int> &nodeIds);
  void refreshFaceVisual(int faceId, const QList<int> &nodeIds);
  gp_Pnt getTopologyNodePosition(int id) const;
  void mergeTopologyNodeEdges(int keepId, int removeId);
  void addEdge(int node1, int node2);
  void moveTopologyNode(int id, const gp_Pnt &p);

  // Restoration API (for loading projects)
  void restoreTopologyNode(int id, const gp_Pnt &p);
  void restoreTopologyEdge(int id, int n1, int n2);
  void restoreTopologyFace(int id, const QList<int> &nodeIds);
  void finalizeRestoration();

  // Topology Entity Highlight
  void highlightTopologyFace(int faceId, bool highlight);
  void highlightTopologyEdge(int n1, int n2, bool highlight);
  void highlightTopologyNode(int id, bool highlight);

  // Smoother Visualization API
  void updateSmootherVisualization();
  void hideSmootherVisualization();
  void createTfiMesh(int faceId);

  // Topology Group Appearance
  void setTopologyFaceGroupAppearance(const QList<int> &ids,
                                      const QColor &color, int renderMode);
  void setTopologyEdgeGroupAppearance(const QList<int> &ids,
                                      const QColor &color, int renderMode);

  // Constraints
  enum ConstraintType {
    ConstraintNone,
    ConstraintFixed,
    ConstraintEdge,
    ConstraintFace,
    ConstraintGeometry
  };
  struct NodeConstraint {
    ConstraintType type = ConstraintNone;
    QList<int> geometryIds; // OCCT face or edge IDs
    bool isEdgeGroup = false;
    gp_Pnt origin;
    gp_Dir dir;
  };

  void setNodeConstraint(int nodeId, ConstraintType type, int targetId = -1);
  void setNodeConstraints(const QMap<int, NodeConstraint> &constraints);
  NodeConstraint getNodeConstraint(int nodeId) const;

signals:
  void shapeSelected(const TopoDS_Shape &shape);
  void topologyNodeCreated(int id, double u, double v, int faceId);
  void topologyNodeSelected(int id);
  void topologyNodeMoved(int id, const gp_Pnt &p);
  void topologyNodeDeleted(int id);
  void topologyEdgeCreated(int n1, int n2, int id);
  void topologyNodesMerged(int keepId, int removeId);

  void topologyEdgeDeleted(int n1, int n2);
  void topologyFaceCreated(int id, const QList<int> &nodeIds);
  void topologyFaceDeleted(int id);
  void topologySelectionChanged();
  void workbenchRequested(int index);

public slots:
  void setSelectionMode(int mode);

protected:
  // Override standard Qt events
  QPaintEngine *paintEngine() const override {
    return nullptr;
  } // Disable Qt painting
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void showEvent(QShowEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

  // Mouse events
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;

private:
  Handle(V3d_View) m_view;
  Handle(AIS_InteractiveContext) m_context;
  Handle(V3d_Viewer) m_viewer;

  Standard_Integer myXmin;
  Standard_Integer myYmin;
  Standard_Integer myXmax;
  Standard_Integer myYmax;

  Standard_Integer myCurX;
  Standard_Integer myCurY;

  // Selection mode overlay
  QComboBox *m_selectionCombo = nullptr;
  // HUD Overlay
  QWidget *m_hudWidget = nullptr;
  QPushButton *m_btnF1 = nullptr;
  QPushButton *m_btnF2 = nullptr;
  QPushButton *m_btnF3 = nullptr;
  QPushButton *m_btnQ = nullptr;
  QPushButton *m_btnW = nullptr;
  QPushButton *m_btnE = nullptr;
  void createHUD();
  void updateHUDStates();
  void createOverlay();

  // Shape data for appearance control
  TopoDS_Shape m_shape;
  TopTools_IndexedMapOfShape *m_faceMap;
  TopTools_IndexedMapOfShape *m_edgeMap;
  Handle(AIS_ColoredShape) m_aisShape;

  // Interaction State
  InteractionMode m_interactionMode = Mode_Geometry;
  int m_workbenchIndex = 0; // 0=F1, 1=F2, 2=F3

  // Topology Nodes
  QMap<int, Handle(AIS_InteractiveObject)> m_topologyNodes;
  int m_nextNodeId = 1;
  Handle(AIS_InteractiveObject) m_selectedNode;
  int m_selectedNodeId = -1;
  int m_hoveredNodeId = -1;
  bool m_isDraggingNode = false;
  int m_draggedNodeId = -1;

  // Topology Edges
  QMap<QPair<int, int>, Handle(AIS_InteractiveObject)> m_topologyEdges;
  QMap<int, QPair<int, int>> m_edgeIdMap;           // ID -> Node Pair
  QMap<QPair<int, int>, int> m_nodePairToEdgeIdMap; // Node Pair -> ID
  int m_nextEdgeId = 1;
  bool m_isCreatingEdge = false;

  int m_edgeStartNodeId = -1;
  Handle(AIS_InteractiveObject) m_draggedEdge;

  void updateConnectedEdges(int nodeId);
  gp_Pnt applyConstraint(int nodeId, const gp_Pnt &newPos);

  // Topology Faces
  QMap<int, Handle(AIS_InteractiveObject)> m_topologyFaces;
  QMap<int, QList<int>> m_faceNodeMap; // Face ID -> List of Node IDs (ordered)
  int m_nextFaceId = 1;
  QMap<int, NodeConstraint> m_nodeConstraints;

  struct TopologyStyle {
    QColor color;
    int renderMode = 0; // 0: Shaded, 1: Translucent, 2: Hidden
  };
  QMap<int, TopologyStyle> m_faceStyles;
  QMap<int, TopologyStyle> m_edgeStyles;

  void updateConnectedFaces(int nodeId);
  Handle(AIS_InteractiveObject) buildFaceShape(const QList<int> &nodeIds);

  // Face Extrusion via Edge Pull
  QPair<int, int> m_selectedEdge = qMakePair(-1, -1);
  bool m_isExtrudingFace = false;
  QList<Handle(AIS_InteractiveObject)> m_facePreviewShapes;

  // Helper for ray casting
  bool castRayToMesh(int x, int y, gp_Pnt &intersection, TopoDS_Face &face);

  // Configuration
  void loadConfig();
  double m_linearDeflection = 0.1;
  double m_angularDeflection = 0.1;
  double m_nodeSize = 4.0;

  // Expanded visual configuration
  QColor m_bgGradientTop = Qt::black;
  QColor m_bgGradientBottom = Qt::black;
  QColor m_highlightColor = Qt::cyan;
  QColor m_selectionColor = Qt::magenta;
  double m_edgeWidth = 1.0;
  QColor m_edgeColor = Qt::black; // For default edges if we use them

  TopologySelectionMode m_topologySelectionMode = SelNodes;
  int m_geometrySelectionMode = 4; // Default to Face (TopAbs_FACE = 4)

  Topology *m_topologyModel = nullptr;
  QList<Handle(AIS_InteractiveObject)> m_smootherObjects;
};
