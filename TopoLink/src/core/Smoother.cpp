#include "Smoother.h"
#include "EllipticSolver.h"
#include "GraphSolver.h"
#include "TopoEdge.h"
#include "TopoFace.h"
#include "TopoHalfEdge.h"
#include "TopoNode.h"
#include "Topology.h"
#include <algorithm>

// OCCT Includes
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRep_Builder.hxx>
#include <QDebug>
#include <QFile>
#include <QList>
#include <QMap>
#include <QMutexLocker>
#include <QSet>
#include <QTextStream>
#include <QtConcurrent>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_XYZ.hxx>

Smoother::Smoother(Topology *topology)
    : QObject(nullptr), m_topology(topology) {}

Smoother::~Smoother() {}

void Smoother::setConfig(const SmootherConfig &config) { m_config = config; }

void Smoother::setConstraints(const QMap<int, Constraint> &constraints) {
  m_constraints = constraints;
}

void Smoother::setGeometryMaps(const void *faceMap, const void *edgeMap) {
  m_geoFaceMap = faceMap;
  m_geoEdgeMap = edgeMap;
}

const QMap<int, Smoother::SmoothedEdge> &Smoother::getSmoothedEdges() const {
  return m_smoothedEdges;
}

const QMap<int, Smoother::SmoothedFace> &Smoother::getSmoothedFaces() const {
  return m_smoothedFaces;
}

void Smoother::run() {
  if (!m_topology)
    return;

  m_convergenceHistory.clear();

  qDebug() << "Smoother: Starting edge smoothing...";
  smoothEdges();

  qDebug() << "Smoother: Starting face smoothing...";
  smoothFaces();

  qDebug() << "Smoother: Process complete.";
}

// -----------------------------------------------------------------------------
// Edge Smoothing
// -----------------------------------------------------------------------------
void Smoother::smoothEdges() {
  m_smoothedEdges.clear();

  const auto &edges = m_topology->getEdges();
  QList<int> edgeIds;
  for (auto const &[id, edge] : edges) {
    edgeIds.append(id);
  }

  QtConcurrent::blockingMap(edgeIds, [this, &edges](int id) {
    auto it = edges.find(id);
    if (it != edges.end()) {
      smoothSingleEdge(id, it->second);
    }
  });
}

void Smoother::smoothSingleEdge(int edgeId, TopoEdge *edge) {
  int subdivisions = edge->getSubdivisions();
  if (subdivisions < 1)
    subdivisions = 1;

  int numPoints = subdivisions + 1;
  std::vector<gp_Pnt> points(numPoints);

  TopoNode *nStart = edge->getStartNode();
  TopoNode *nEnd = edge->getEndNode();

  points[0] = nStart->getPosition();
  points[subdivisions] = nEnd->getPosition();

  // Check for Edge Constraints (Geometry Projection)
  TopoDS_Shape edgeConstraint;
  {
    // Constraints map access should be safe as it's read-only after setup
    if (m_constraints.contains(nStart->getID()) &&
        m_constraints.contains(nEnd->getID())) {
      const Constraint &c1 = m_constraints[nStart->getID()];
      const Constraint &c2 = m_constraints[nEnd->getID()];

      if (c1.type == ConstraintGeometry && c1.isEdgeGroup &&
          c2.type == ConstraintGeometry && c2.isEdgeGroup) {

        // Compute intersection of geometry IDs (common curve)
        QList<int> commonIds;
        for (int id : c1.geometryIds) {
          if (c2.geometryIds.contains(id)) {
            commonIds.append(id);
          }
        }

        if (!commonIds.isEmpty()) {
          edgeConstraint = buildTargetShape(commonIds, true);
        }
      }
    }
  }

  // Initialize internal points linearly
  for (int i = 1; i < subdivisions; ++i) {
    double t = (double)i / subdivisions;
    gp_XYZ xyz = points[0].XYZ() * (1.0 - t) + points[subdivisions].XYZ() * t;
    points[i] = gp_Pnt(xyz);
  }

  if (!edgeConstraint.IsNull()) {
    qDebug() << "Smoother: Edge" << edgeId << "found explicit edge constraint.";
  } else {
    // Fallback: Check for Surface Constraint on connected faces
    QList<int> faceGeoIds;
    auto checkFace = [&](TopoHalfEdge *he) {
      if (he && he->face) {
        std::string gidStr = m_topology->getFaceGeometryID(he->face->getID());
        qDebug() << "Smoother: Edge" << edgeId << "checking face"
                 << he->face->getID()
                 << "gidStr:" << QString::fromStdString(gidStr);
        if (!gidStr.empty()) {
          QStringList parts = QString::fromStdString(gidStr).split(",");
          for (const QString &part : parts) {
            bool ok;
            int gid = part.toInt(&ok);
            if (ok && !faceGeoIds.contains(gid))
              faceGeoIds.append(gid);
          }
        }
      } else {
        qDebug() << "Smoother: Edge" << edgeId
                 << "half-edge checking failed (he or face null)";
      }
    };
    checkFace(edge->getForwardHalfEdge());
    checkFace(edge->getBackwardHalfEdge());

    if (!faceGeoIds.isEmpty()) {
      edgeConstraint = buildTargetShape(faceGeoIds, false);
      if (edgeConstraint.IsNull()) {
        qDebug() << "Smoother: Edge" << edgeId
                 << "fallback to face constraint failed to build shape from ids"
                 << faceGeoIds;
      } else {
        qDebug() << "Smoother: Edge" << edgeId
                 << "found surface constraint from adjacent faces."
                 << faceGeoIds;
      }
    } else {
      qDebug() << "Smoother: Edge" << edgeId
               << "skipped. No edge constraint (Nodes" << nStart->getID()
               << "->" << nEnd->getID() << ") and no face fallback found.";
    }
  }

  // Always run smoothing (constrained or unconstrained)
  if (true) {
    std::vector<double> convergence;
    convergence.reserve(m_config.edgeIters);

    for (int it = 0; it < m_config.edgeIters; ++it) {
      std::vector<gp_Pnt> nextPoints = points;
      double maxDisp = 0.0;

      for (int i = 1; i < subdivisions; ++i) {
        gp_XYZ target = (points[i - 1].XYZ() + points[i + 1].XYZ()) * 0.5;
        gp_XYZ refined = points[i].XYZ() * (1.0 - m_config.edgeRelax) +
                         target * m_config.edgeRelax;

        // If constraint exists, project. Otherwise keep refined point
        // (Laplacian)
        if (!edgeConstraint.IsNull()) {
          nextPoints[i] = projectToShape(gp_Pnt(refined), edgeConstraint);
        } else {
          nextPoints[i] = gp_Pnt(refined);
        }

        double distSq = points[i].SquareDistance(nextPoints[i]);
        if (distSq > maxDisp)
          maxDisp = distSq;
      }
      points = nextPoints;
      double currentError = std::sqrt(maxDisp);
      convergence.push_back(currentError);
      emit iterationCompleted(-edgeId, it, currentError);
      if (currentError < 1e-9)
        break;
    }

    QMutexLocker locker(&m_mutex);
    m_convergenceHistory[-edgeId] = convergence;
  }

  SmoothedEdge se;
  se.points = points;

  QMutexLocker locker(&m_mutex);
  m_smoothedEdges.insert(edgeId, se);
}

// -----------------------------------------------------------------------------
// Face Smoothing
// -----------------------------------------------------------------------------
void Smoother::smoothFaces() {
  m_smoothedFaces.clear();
  QSet<int> processedFaces;

  qDebug() << "Smoother: Starting Group-Based Face Smoothing...";

  // 1. Process Face Groups
  const auto &faceGroups = m_topology->getFaceGroups();

  for (const auto &[groupId, groupPtr] : faceGroups) {
    if (groupPtr) {
      smoothFaceGroup(groupPtr.get(), processedFaces);
    }
  }

  // 2. Process Remaining (Ungrouped) Faces
  // Iterate all faces, check if processed
  QList<TopoFace *> remainingFaces;
  for (const auto &[id, face] : m_topology->getFaces()) {
    if (!processedFaces.contains(id)) {
      remainingFaces.append(face);
    }
  }

  QtConcurrent::blockingMap(remainingFaces, [this](TopoFace *face) {
    smoothSingleFace(face->getID(), face);
  });
}

void Smoother::smoothFaceGroup(const TopoFaceGroup *group,
                               QSet<int> &processedFaces) {
  if (!group || group->faces.empty())
    return;

  qDebug() << "Smoother: Processing Face Group" << group->name.c_str() << "with"
           << group->faces.size() << "faces";

  // 1. Identify Group Constraint (Whole Surface)
  TopoDS_Shape groupConstraint;
  if (!group->geometryID.empty()) {
    QList<int> ids;
    QStringList parts = QString::fromStdString(group->geometryID).split(",");
    for (const QString &part : parts) {
      bool ok;
      int gid = part.toInt(&ok);
      if (ok)
        ids.append(gid);
    }
    groupConstraint = buildTargetShape(ids, false);
  }

  // 2. Build Graph
  // We need to map (FaceID, i, j) -> GraphNodeIndex
  // Shared edges (internal to group) must map to the SAME GraphNodeIndex.

  struct GridPoint {
    int faceId;
    int i, j;
    bool operator<(const GridPoint &other) const {
      if (faceId != other.faceId)
        return faceId < other.faceId;
      if (i != other.i)
        return i < other.i;
      return j < other.j;
    }
  };

  // Union-Find / Merging for shared nodes
  // Actually, simpler: Pre-calculate the grid sizes.
  // For each face, determine M and N.
  // Node Identity:
  // - Internal nodes: Unique to (Face, i, j)
  // - Edge nodes: Identify by (EdgeID, index).
  //   If Edge is part of an EdgeGroup -> Fixed.
  //   If Edge is shared with another face in THIS group -> Free (Shared).
  //   If Edge is boundary (no other face in group) -> Fixed.
  // - Corner nodes: Identify by NodeID -> Fixed (usually).

  // Let's use a map to assign indices.
  std::map<int, int> nodeToGraphIdx; // TopoNodeID -> GraphIdx
  std::map<std::pair<int, int>, int>
      edgeToGraphIdx; // (EdgeID, subIdx) -> GraphIdx
  // Internal nodes: just append.

  std::vector<GraphSolver::Node> graphNodes;

  // Helper to get/create index
  auto getGraphIndex =
      [&](int faceId, int i, int j, int M, int N, TopoFace *face,
          const std::vector<std::vector<gp_Pnt>> &boundaries) -> int {
    // Is Corner?
    if ((i == 0 && j == 0) || (i == M && j == 0) || (i == M && j == N) ||
        (i == 0 && j == N)) {
      // It's a TopoNode.
      // Find which one.
      // loop order: bottom (0->M), right (0->N), top (M->0), left (N->0)
      // Node 0: (0,0) -> loop[0]->origin
      // Node 1: (M,0) -> loop[1]->origin
      // Node 2: (M,N) -> loop[2]->origin
      // Node 3: (0,N) -> loop[3]->origin
      // We need to access the boundary loop again.
      // Use existing `boundaries` to get position? No, we need topology for
      // connectivity. Let's assume fixed for now for corners?
      return -1; // Treat corners as fixed, don't include in graph solver?
                 // Wait, if we don't include them, neighbors can't reference
                 // them. We MUST include fixed nodes in the graph so neighbors
                 // can pull from them.
    }

    // Is Edge?
    // ...
    return -1;
  };

  // RESTART STRATEGY for simplicity:
  // 1. Gather all nodes (TopoNodes) and discretized Edge points.
  // 2. Identify which are fixed (Boundary of group or EdgeGroup).
  // 3. Create GraphNodes for all of them.
  // 4. Create GraphNodes for all internal face points.
  // 5. Build connectivity.

  // Map global identifiers to graph indices
  std::map<int, int> topoNodeToIdx;
  std::map<std::pair<int, int>, int>
      topoEdgePointToIdx; // (EdgeID, index) -> Idx
  std::map<std::tuple<int, int, int>, int>
      faceInternalToIdx; // (FaceID, i, j) -> Idx

  // 0. Prepare Faces (calc grids)
  struct FaceData {
    TopoFace *face;
    int M, N;
    std::vector<std::vector<gp_Pnt>> tfiGrid; // Initial TFI
    std::vector<TopoHalfEdge *> loop;
  };
  QList<FaceData> faceDataList;

  for (TopoFace *face : group->faces) {
    processedFaces.insert(face->getID());

    // Get loop
    TopoHalfEdge *startHe = face->getBoundary();
    std::vector<TopoHalfEdge *> loop;
    TopoHalfEdge *current = startHe;
    int safety = 0;
    do {
      loop.push_back(current);
      current = current->next;
      safety++;
    } while (current != startHe && current != nullptr && safety < 100);

    if (loop.size() != 4)
      continue; // Skip non-quads

    // Get Subdivision counts from edges
    int M = loop[0]->parentEdge->getSubdivisions();
    int N = loop[1]->parentEdge->getSubdivisions();

    // Build Initial TFI Grid (reuse smoothSingleFace logic primarily)
    // ... (We need the boundaries)
    std::vector<std::vector<gp_Pnt>> boundaries(4);
    for (int k = 0; k < 4; ++k) {
      TopoEdge *edge = loop[k]->parentEdge;
      std::vector<gp_Pnt> edgePoints;
      // Check smoothed edges first?
      // Since we run smoothEdges() before, we have initial guesses.
      {
        QMutexLocker locker(&m_mutex);
        if (m_smoothedEdges.contains(edge->getID())) {
          edgePoints = m_smoothedEdges[edge->getID()].points;
        } else {
          // Linear fallback
          int subs = edge->getSubdivisions();
          edgePoints.resize(subs + 1);
          TopoNode *ns = edge->getStartNode();
          TopoNode *ne = edge->getEndNode();
          for (int p = 0; p <= subs; ++p) {
            double t = (double)p / subs;
            gp_XYZ xyz = ns->getPosition().XYZ() * (1.0 - t) +
                         ne->getPosition().XYZ() * t;
            edgePoints[p] = gp_Pnt(xyz);
          }
        }
      }

      // Orient
      bool isForward = (loop[k]->origin == edge->getStartNode());
      if (isForward) {
        boundaries[k] = edgePoints;
      } else {
        boundaries[k].resize(edgePoints.size());
        std::reverse_copy(edgePoints.begin(), edgePoints.end(),
                          boundaries[k].begin());
      }
    }

    // Generate TFI
    std::vector<std::vector<gp_Pnt>> grid(M + 1, std::vector<gp_Pnt>(N + 1));
    for (int i = 0; i <= M; ++i) {
      for (int j = 0; j <= N; ++j) {
        double u = (double)i / M;
        double v = (double)j / N;
        gp_XYZ pBottom = boundaries[0][i].XYZ();
        gp_XYZ pRight = boundaries[1][j].XYZ();
        gp_XYZ pTop_val =
            boundaries[2][M - i].XYZ(); // Reversed relative to grid indices
        gp_XYZ pLeft_val = boundaries[3][N - j].XYZ(); // Reversed

        gp_XYZ cSW = boundaries[0][0].XYZ();
        gp_XYZ cSE = boundaries[0][M].XYZ();
        gp_XYZ cNE = boundaries[2][0].XYZ();
        gp_XYZ cNW = boundaries[2][M].XYZ();

        gp_XYZ pTFI = (1.0 - v) * pBottom + v * pTop_val +
                      (1.0 - u) * pLeft_val + u * pRight -
                      ((1.0 - u) * (1.0 - v) * cSW + u * (1.0 - v) * cSE +
                       u * v * cNE + (1.0 - u) * v * cNW);
        grid[i][j] = gp_Pnt(pTFI);
      }
    }

    faceDataList.append({face, M, N, grid, loop});
  }

  // 1. Create Nodes
  // Helper to add node if unique
  auto addNode = [&](gp_Pnt p, bool fixed) -> int {
    GraphSolver::Node n;
    n.pos = p;
    // If explicitly marked fixed by caller, always fixed.
    // If caller marked false (free), we might still fix it later based on
    // connectivity. But graph solver needs initial state. Let's assume passed
    // 'fixed' is the START state.
    n.isFixed = fixed;
    graphNodes.push_back(n);
    return graphNodes.size() - 1;
  };

  // TopoNodes (Corners)
  // Default: FREE (Internal nodes should move).
  // Will verify/fix connectivity later.
  // Exception: If explicit node constraint exists in m_constraints?
  for (const auto &fd : faceDataList) {
    for (auto he : fd.loop) {
      TopoNode *node = he->origin;
      int nid = node->getID();
      if (topoNodeToIdx.find(nid) == topoNodeToIdx.end()) {
        // Check explicit constraints?
        // If constrained to a specific vertex/edge -> FIXED (in context of
        // surface smoothing) If constrained to same surface -> FREE (floats on
        // surface) For safety, let's treat explicit node constraints as FIXED
        // for now.
        bool isExplicitlyConstrained =
            m_constraints.contains(nid) &&
            (m_constraints[nid].type == ConstraintFixed);

        topoNodeToIdx[nid] =
            addNode(node->getPosition(), isExplicitlyConstrained);
      }
    }
  }

  // Edges
  // For each edge involved:
  // Determine if it is FIXED (Boundary of group / Explicit Edge Group) or FREE
  // (Shared internal). Note: If an edge is shared by faces OUTSIDE the group,
  // it must be FIXED. So: Edge is FREE iff (GroupForEdge == ThisGroup) OR (All
  // adjacent faces are in ThisGroup)? User said "internal edges". Simpler rule:
  // Is the edge in an explicit Edge Group? If so, FIXED. Is the edge on the
  // boundary of the aggregate mesh? If so, FIXED. Otherwise, FREE.

  QSet<int> groupFaceIds;
  for (auto f : group->faces)
    groupFaceIds.insert(f->getID());

  for (const auto &fd : faceDataList) {
    for (int k = 0; k < 4; k++) {
      TopoEdge *edge = fd.loop[k]->parentEdge;
      int eid = edge->getID();
      int Mn = edge->getSubdivisions(); // this is M or N matching the face side

      // Check edge status
      bool isEdgeFixed = true; // Assume fixed unless proven free

      // Check Explicit Group
      TopoEdgeGroup *eg = m_topology->getGroupForEdge(eid);
      if (eg && eg->name != "Unused") {
        isEdgeFixed = true;
      } else {
        // Check connectivity
        // If it has a twin, and that twin's face is in this group, then it is
        // shared internal -> FREE.
        TopoHalfEdge *he = fd.loop[k];
        TopoHalfEdge *twin = he->twin;
        if (twin && twin->face && groupFaceIds.contains(twin->face->getID())) {
          isEdgeFixed = false; // Internal to group, free to move
        }
      }

      // If Edge is Fixed, its endpoints MUST be Fixed.
      if (isEdgeFixed) {
        int startIdx = topoNodeToIdx[edge->getStartNode()->getID()];
        int endIdx = topoNodeToIdx[edge->getEndNode()->getID()];
        graphNodes[startIdx].isFixed = true;
        graphNodes[endIdx].isFixed = true;
        qDebug() << "Smoother: Node" << edge->getStartNode()->getID()
                 << "fixed by Edge" << eid;
        qDebug() << "Smoother: Node" << edge->getEndNode()->getID()
                 << "fixed by Edge" << eid;
      } else {
        qDebug() << "Smoother: Edge" << eid << "is FREE (Internal)";
      }

      // Create nodes for edge interiors (1 to subs-1)
      for (int i = 1; i < Mn; ++i) {
        if (topoEdgePointToIdx.find({eid, i}) == topoEdgePointToIdx.end()) {
          // Initial pos? Use smoothed edge entry
          gp_Pnt pos;
          {
            QMutexLocker locker(&m_mutex);
            if (m_smoothedEdges.contains(eid)) {
              pos = m_smoothedEdges[eid].points[i];
            } else {
              double t = (double)i / Mn;
              gp_XYZ xyz =
                  edge->getStartNode()->getPosition().XYZ() * (1.0 - t) +
                  edge->getEndNode()->getPosition().XYZ() * t;
              pos = gp_Pnt(xyz);
            }
          }
          topoEdgePointToIdx[{eid, i}] = addNode(pos, isEdgeFixed);
        }
      }
    }
  }

  // Debug final node states
  for (auto const &[nid, idx] : topoNodeToIdx) {
    qDebug() << "Smoother: Node" << nid
             << "Final Fixed State:" << graphNodes[idx].isFixed
             << "Neighbors:" << graphNodes[idx].neighbors.size();
  }

  // Face Internals (Always Free)
  for (const auto &fd : faceDataList) {
    for (int i = 1; i < fd.M; ++i) {
      for (int j = 1; j < fd.N; ++j) {
        faceInternalToIdx[{fd.face->getID(), i, j}] =
            addNode(fd.tfiGrid[i][j], false);
      }
    }
  }

  // 2. Build Connectivity (Neighbors)
  for (const auto &fd : faceDataList) {
    int fid = fd.face->getID();

    auto getNodeIdx = [&](int i, int j) -> int {
      // Internal
      if (i > 0 && i < fd.M && j > 0 && j < fd.N) {
        return faceInternalToIdx[{fid, i, j}];
      }

      // Boundary
      // Map (i,j) to Edge/Node
      if (i == 0 && j == 0)
        return topoNodeToIdx[fd.loop[0]->origin->getID()]; // SW
      if (i == fd.M && j == 0)
        return topoNodeToIdx[fd.loop[1]->origin->getID()]; // SE
      if (i == fd.M && j == fd.N)
        return topoNodeToIdx[fd.loop[2]->origin->getID()]; // NE
      if (i == 0 && j == fd.N)
        return topoNodeToIdx[fd.loop[3]->origin->getID()]; // NW

      // Edges
      // Bottom: i varies 0->M, j=0. Corresponds to loop[0].
      if (j == 0) {
        TopoEdge *edge = fd.loop[0]->parentEdge;
        bool fwd = (fd.loop[0]->origin == edge->getStartNode());
        int k = i; // 0 to M
        int idx = fwd ? k : (fd.M - k);
        return topoEdgePointToIdx[{edge->getID(), idx}];
      }
      // Right: i=M, j varies 0->N. Corresponds to loop[1].
      if (i == fd.M) {
        TopoEdge *edge = fd.loop[1]->parentEdge;
        bool fwd = (fd.loop[1]->origin == edge->getStartNode());
        int k = j;
        int idx = fwd ? k : (fd.N - k);
        return topoEdgePointToIdx[{edge->getID(), idx}];
      }
      // Top: i varies M->0, j=N. Corresponds to loop[2].
      if (j == fd.N) {
        TopoEdge *edge = fd.loop[2]->parentEdge;
        bool fwd = (fd.loop[2]->origin == edge->getStartNode());
        int k = fd.M - i; // loop runs M -> 0
        int idx = fwd ? k : (fd.M - k);
        return topoEdgePointToIdx[{edge->getID(), idx}];
      }
      // Left: i=0, j varies N->0. Corresponds to loop[3]
      if (i == 0) {
        TopoEdge *edge = fd.loop[3]->parentEdge;
        bool fwd = (fd.loop[3]->origin == edge->getStartNode());
        int k = fd.N - j;
        int idx = fwd ? k : (fd.N - k);
        return topoEdgePointToIdx[{edge->getID(), idx}];
      }
      return -1;
    };

    // Add 4-Neighbors
    for (int i = 0; i <= fd.M; ++i) {
      for (int j = 0; j <= fd.N; ++j) {
        int curr = getNodeIdx(i, j);
        if (curr == -1)
          continue;

        // We only add neighbors TO this node?
        // GraphSolver expects bidirectional?
        // We just need to ensure everyone has their neighbors listed.
        // It's easier to iterate valid grid links and add to both.

        if (i < fd.M) {
          int right = getNodeIdx(i + 1, j);
          if (right != -1) {
            // Avoid duplicates? std::vector iteration check is slow.
            // But mesh is small.
            // Better: only add if not present?
            // Let's rely on logic.
            // Since nodes are shared, we visit edges multiple times.
            // Use a set or unique vector in GraphNode.
          }
        }
        // ... This is getting verbose to implement safely in replacement.
        // Simpler: Just push_back. GraphSolver averages them all.
        // Repeated neighbors simply weight that connection higher.
        // Ideally we want 1 weight per link.

        // Directions: +i, -i, +j, -j.
        int n_i[4] = {i + 1, i - 1, i, i};
        int n_j[4] = {j, j, j + 1, j - 1};

        for (int d = 0; d < 4; ++d) {
          int ni = n_i[d];
          int nj = n_j[d];
          if (ni >= 0 && ni <= fd.M && nj >= 0 && nj <= fd.N) {
            int neighbor = getNodeIdx(ni, nj);
            if (neighbor != -1) {
              graphNodes[curr].neighbors.push_back(neighbor);
            }
          }
        }
      }
    }
  }

  // Deduplicate neighbors (optional but good for perf/correctness)
  for (auto &node : graphNodes) {
    std::sort(node.neighbors.begin(), node.neighbors.end());
    node.neighbors.erase(
        std::unique(node.neighbors.begin(), node.neighbors.end()),
        node.neighbors.end());
    // Remove self-loops if any
    node.neighbors.erase(std::remove(node.neighbors.begin(),
                                     node.neighbors.end(),
                                     (&node - &graphNodes[0])),
                         node.neighbors.end());
  }

  // 3. Solve
  GraphSolver::Params params;
  params.iterations = m_config.faceIters;
  params.relaxation = m_config.faceRelax;

  // Constraint Function
  auto constraintFunc = [&](int idx, const gp_Pnt &p) -> gp_Pnt {
    return projectToShape(p, groupConstraint);
  };

  auto convergence =
      GraphSolver::smoothGraph(graphNodes, params, constraintFunc);

  // 4. Write Back
  // Update m_smoothedEdges (for the shared/free edges)
  // Update m_smoothedFaces (grids)

  // Edges
  QMutexLocker locker(&m_mutex);
  for (auto const &[key, idx] : topoEdgePointToIdx) {
    int eid = key.first;
    int ptIdx = key.second;
    if (!m_smoothedEdges.contains(eid)) {
      // Should verify subs...
      int subs = m_topology->getEdge(eid)->getSubdivisions();
      m_smoothedEdges[eid].points.resize(subs + 1);
      // set ends
      m_smoothedEdges[eid].points[0] =
          m_topology->getEdge(eid)->getStartNode()->getPosition();
      m_smoothedEdges[eid].points[subs] =
          m_topology->getEdge(eid)->getEndNode()->getPosition();
    }
    m_smoothedEdges[eid].points[ptIdx] = graphNodes[idx].pos;
  }

  // Faces
  for (const auto &fd : faceDataList) {
    SmoothedFace sf;
    sf.grid.resize(fd.M + 1, std::vector<gp_Pnt>(fd.N + 1));
    sf.surface = groupConstraint;

    for (int i = 0; i <= fd.M; ++i) {
      for (int j = 0; j <= fd.N; ++j) {
        // Re-resolve index
        // Note: this repeats the lookup logic.
        // Ideally we cached it in fd.gridIndices[i][j]
        // ...
        // Copied lookup logic for brevity:
        int idx = -1;
        if (i > 0 && i < fd.M && j > 0 && j < fd.N)
          idx = faceInternalToIdx[{fd.face->getID(), i, j}];
        else {
          // Reuse the lambda logic essentially
          // ...
          // Actually, we can just read back?
          // Let's assume re-running lookup is okay or copy helper.
          // Helper is inside smoothFaceGroup.
          // We can't access it here easily without copy paste.
          // OK, let's just make the helper capture variables and reuse it.
          // (The lambda `getNodeIdx` captures maps)
        }
        // WAIT, I can just use `getNodeIdx` from above if I structure code
        // to keep it in scope. Yes.
      }
    }

    // Re-run population
    auto getNodeIdx = [&](int i, int j) -> int {
      if (i > 0 && i < fd.M && j > 0 && j < fd.N)
        return faceInternalToIdx[{fd.face->getID(), i, j}];
      if (i == 0 && j == 0)
        return topoNodeToIdx[fd.loop[0]->origin->getID()];
      if (i == fd.M && j == 0)
        return topoNodeToIdx[fd.loop[1]->origin->getID()];
      if (i == fd.M && j == fd.N)
        return topoNodeToIdx[fd.loop[2]->origin->getID()];
      if (i == 0 && j == fd.N)
        return topoNodeToIdx[fd.loop[3]->origin->getID()];

      if (j == 0) {
        TopoEdge *e = fd.loop[0]->parentEdge;
        bool f = (fd.loop[0]->origin == e->getStartNode());
        return topoEdgePointToIdx[{e->getID(), f ? i : (fd.M - i)}];
      }
      if (i == fd.M) {
        TopoEdge *e = fd.loop[1]->parentEdge;
        bool f = (fd.loop[1]->origin == e->getStartNode());
        return topoEdgePointToIdx[{e->getID(), f ? j : (fd.N - j)}];
      }
      if (j == fd.N) {
        TopoEdge *e = fd.loop[2]->parentEdge;
        bool f = (fd.loop[2]->origin == e->getStartNode());
        return topoEdgePointToIdx[{e->getID(), f ? (fd.M - i) : i}];
      } // fd.M-i is k.  idx = fwd ? k : M-k => f ? M-i : i
      if (i == 0) {
        TopoEdge *e = fd.loop[3]->parentEdge;
        bool f = (fd.loop[3]->origin == e->getStartNode());
        return topoEdgePointToIdx[{e->getID(), f ? (fd.N - j) : j}];
      }
      return -1;
    };

    for (int i = 0; i <= fd.M; ++i) {
      for (int j = 0; j <= fd.N; ++j) {
        int idx = getNodeIdx(i, j);
        if (idx != -1) {
          sf.grid[i][j] = graphNodes[idx].pos;
        }
      }
    }

    m_smoothedFaces[fd.face->getID()] = sf;
    m_convergenceHistory[fd.face->getID()] = convergence;
  }
}

void Smoother::smoothSingleFace(int faceId, TopoFace *face) {
  // 1. Get ordered boundary loop
  TopoHalfEdge *startHe = face->getBoundary();
  if (!startHe)
    return;

  std::vector<TopoHalfEdge *> loop;
  TopoHalfEdge *current = startHe;
  int safety = 0;
  do {
    loop.push_back(current);
    current = current->next;
    safety++;
  } while (current != startHe && current != nullptr && safety < 100);

  if (loop.size() != 4) {
    // Only Quads supported for now
    return;
  }

  // 2. Identify Face/Surface Constraint
  TopoDS_Shape surfaceConstraint;

  // Check Topology Face Group
  std::string gidStr = m_topology->getFaceGeometryID(face->getID());
  if (!gidStr.empty()) {
    QList<int> ids;
    QStringList parts = QString::fromStdString(gidStr).split(",");
    for (const QString &part : parts) {
      bool ok;
      int gid = part.toInt(&ok);
      if (ok)
        ids.append(gid);
    }
    if (!ids.isEmpty()) {
      surfaceConstraint = buildTargetShape(ids, false);
    }
  }

  // Fallback: Check first node constraint
  if (surfaceConstraint.IsNull() &&
      m_constraints.contains(loop[0]->origin->getID())) {
    const Constraint &nc0 = m_constraints[loop[0]->origin->getID()];
    if (nc0.type == ConstraintGeometry && !nc0.isEdgeGroup) {
      surfaceConstraint = buildTargetShape(nc0.geometryIds, false);
    }
  }

  // 3. Collect Boundary Points from SmoothedEdges
  std::vector<std::vector<gp_Pnt>> boundaries(4);

  for (int k = 0; k < 4; ++k) {
    TopoEdge *edge = loop[k]->parentEdge;
    std::vector<gp_Pnt> edgePoints;

    {
      QMutexLocker locker(&m_mutex);
      if (m_smoothedEdges.contains(edge->getID())) {
        edgePoints = m_smoothedEdges[edge->getID()].points;
      }
    }

    if (edgePoints.empty()) {
      // Fallback if missing
      int subs = edge->getSubdivisions();
      edgePoints.resize(subs + 1);
      TopoNode *ns = edge->getStartNode();
      TopoNode *ne = edge->getEndNode();
      for (int p = 0; p <= subs; ++p) {
        double t = (double)p / subs;
        gp_XYZ xyz =
            ns->getPosition().XYZ() * (1.0 - t) + ne->getPosition().XYZ() * t;
        edgePoints[p] = gp_Pnt(xyz);
      }
    }

    // Determine direction
    bool isForward = (loop[k]->origin == edge->getStartNode());
    if (isForward) {
      boundaries[k] = edgePoints;
    } else {
      boundaries[k].resize(edgePoints.size());
      std::reverse_copy(edgePoints.begin(), edgePoints.end(),
                        boundaries[k].begin());
    }
  }

  // 4. Initialize Grid (TFI)
  int M = boundaries[0].size() - 1; // Bottom edge subdivisions
  int N = boundaries[1].size() - 1; // Right edge subdivisions

  std::vector<std::vector<gp_Pnt>> grid(M + 1, std::vector<gp_Pnt>(N + 1));
  std::vector<std::vector<bool>> isFixed(M + 1,
                                         std::vector<bool>(N + 1, false));

  for (int i = 0; i <= M; ++i) {
    for (int j = 0; j <= N; ++j) {
      double u = (double)i / M;
      double v = (double)j / N;

      gp_XYZ pBottom = boundaries[0][i].XYZ();
      gp_XYZ pRight = boundaries[1][j].XYZ();
      gp_XYZ pTop_val = boundaries[2][M - i].XYZ();
      gp_XYZ pLeft_val = boundaries[3][N - j].XYZ();

      gp_XYZ cSW = boundaries[0][0].XYZ(); // Node 0
      gp_XYZ cSE = boundaries[0][M].XYZ(); // Node 1
      gp_XYZ cNE = boundaries[2][0].XYZ(); // Node 2 (Start of B2)
      gp_XYZ cNW = boundaries[2][M].XYZ(); // Node 3 (End of B2)

      gp_XYZ pTFI = (1.0 - v) * pBottom + v * pTop_val + (1.0 - u) * pLeft_val +
                    u * pRight -
                    ((1.0 - u) * (1.0 - v) * cSW + u * (1.0 - v) * cSE +
                     u * v * cNE + (1.0 - u) * v * cNW);

      grid[i][j] = gp_Pnt(pTFI);

      if (i == 0 || i == M || j == 0 || j == N) {
        isFixed[i][j] = true;
      } else if (!surfaceConstraint.IsNull()) {
        grid[i][j] = projectToShape(grid[i][j], surfaceConstraint);
      }
    }
  }

  // 5. Smooth with Elliptic Solver
  EllipticSolver::Params params;
  params.iterations = m_config.faceIters;
  params.relaxation = m_config.faceRelax;
  params.bcRelaxation = m_config.faceBCRelax;

  auto constraintFunc = [&](int i, int j, const gp_Pnt &p) -> gp_Pnt {
    if (i == 0 || i == M || j == 0 || j == N)
      return p;
    return projectToShape(p, surfaceConstraint);
  };

  auto progressFunc = [&](int it, double error) {
    emit iterationCompleted(faceId, it, error);
  };

  std::vector<double> convergence = EllipticSolver::smoothGrid(
      grid, isFixed, params, constraintFunc, progressFunc);

  {
    QMutexLocker locker(&m_mutex);
    m_convergenceHistory[faceId] = convergence;
  }

  SmoothedFace sf;
  sf.grid = grid;
  sf.surface = surfaceConstraint;

  {
    QMutexLocker locker(&m_mutex);
    m_smoothedFaces.insert(faceId, sf);
  }
}

void Smoother::saveConvergenceData(const QString &filename) const {
  QFile file(filename);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qDebug() << "Failed to open convergence log file:" << filename;
    return;
  }

  QTextStream out(&file);
  out << "Iteration";
  for (auto it = m_convergenceHistory.begin(); it != m_convergenceHistory.end();
       ++it) {
    if (it->first < 0)
      out << ",Edge_" << -it->first;
    else
      out << ",Face_" << it->first;
  }
  out << "\n";

  // Find max iterations
  size_t maxIters = 0;
  for (auto const &[id, data] : m_convergenceHistory) {
    if (data.size() > maxIters)
      maxIters = data.size();
  }

  for (size_t i = 0; i < maxIters; ++i) {
    out << i;
    for (auto it = m_convergenceHistory.begin();
         it != m_convergenceHistory.end(); ++it) {
      out << ",";
      if (i < it->second.size()) {
        out << it->second[i];
      }
    }
    out << "\n";
  }
  file.close();
  qDebug() << "Convergence data saved to" << filename;
}

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------
TopoDS_Shape Smoother::buildTargetShape(const QList<int> &ids, bool isEdge) {
  if (ids.isEmpty())
    return TopoDS_Shape();
  if (!m_geoFaceMap || !m_geoEdgeMap)
    return TopoDS_Shape();

  TopoDS_Compound comp;
  BRep_Builder B;
  B.MakeCompound(comp);
  bool added = false;

  const TopTools_IndexedMapOfShape *map =
      static_cast<const TopTools_IndexedMapOfShape *>(isEdge ? m_geoEdgeMap
                                                             : m_geoFaceMap);

  for (int id : ids) {
    if (id > 0 && id <= map->Extent()) {
      B.Add(comp, map->FindKey(id));
      added = true;
    }
  }
  return added ? comp : TopoDS_Shape();
}

gp_Pnt Smoother::projectToShape(const gp_Pnt &p, const TopoDS_Shape &s) {
  if (s.IsNull())
    return p;
  // Note: Projecting to a compound of edges or faces
  // BRepExtrema works well generally.

  BRepExtrema_DistShapeShape extrema(BRepBuilderAPI_MakeVertex(p).Vertex(), s);
  if (extrema.IsDone() && extrema.NbSolution() > 0) {
    // Solution 1 is closest
    return extrema.PointOnShape2(1);
  }
  return p;
}
