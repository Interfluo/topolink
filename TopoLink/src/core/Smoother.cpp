#include "Smoother.h"
#include "EllipticSolver.h"
#include "TopoEdge.h"
#include "TopoFace.h"
#include "TopoHalfEdge.h"
#include "TopoNode.h"
#include "Topology.h"

// OCCT Includes
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRep_Builder.hxx>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_XYZ.hxx>

Smoother::Smoother(Topology *topology) : m_topology(topology) {}

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

  for (const auto &[edgeId, edge] : m_topology->getEdges()) {
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
    // An edge is constrained if BOTH its start and end nodes share the SAME
    // EdgeGroup Geometry constraint.
    TopoDS_Shape edgeConstraint;
    if (m_constraints.contains(nStart->getID()) &&
        m_constraints.contains(nEnd->getID())) {
      const Constraint &c1 = m_constraints[nStart->getID()];
      const Constraint &c2 = m_constraints[nEnd->getID()];

      if (c1.type == ConstraintGeometry && c1.isEdgeGroup &&
          c2.type == ConstraintGeometry && c2.isEdgeGroup &&
          c1.geometryIds == c2.geometryIds) {

        edgeConstraint = buildTargetShape(c1.geometryIds, true);
      }
    }

    // Initialize internal points linearly
    for (int i = 1; i < subdivisions; ++i) {
      double t = (double)i / subdivisions;
      gp_XYZ xyz = points[0].XYZ() * (1.0 - t) + points[subdivisions].XYZ() * t;
      points[i] = gp_Pnt(xyz);
    }

    // Iterative Smoothing for Edges
    // If constrained, we project after smoothing.
    // If not constrained, it's just a straight line (already set by
    // initialization). Technically, a straight line is the solution to the
    // Laplacian D2=0. But if constrained to a curved surface/edge, we need to
    // iterate.

    if (!edgeConstraint.IsNull()) {
      // Found edge constraint
    } else {
      // Fallback: Check for Surface Constraint on connected faces
      QList<int> faceGeoIds;
      auto checkFace = [&](TopoHalfEdge *he) {
        if (he && he->face) {
          std::string gidStr = m_topology->getFaceGeometryID(he->face->getID());
          if (!gidStr.empty()) {
            QStringList parts = QString::fromStdString(gidStr).split(",");
            for (const QString &part : parts) {
              bool ok;
              int gid = part.toInt(&ok);
              if (ok && !faceGeoIds.contains(gid))
                faceGeoIds.append(gid);
            }
          }
        }
      };
      checkFace(edge->getForwardHalfEdge());
      checkFace(edge->getBackwardHalfEdge());

      if (!faceGeoIds.isEmpty()) {
        edgeConstraint = buildTargetShape(faceGeoIds, false);
      }
    }

    if (!edgeConstraint.IsNull()) {
      std::vector<double> convergence;
      convergence.reserve(m_config.edgeIters);

      for (int it = 0; it < m_config.edgeIters; ++it) {
        std::vector<gp_Pnt> nextPoints = points;
        double maxDisp = 0.0;

        for (int i = 1; i < subdivisions; ++i) {
          // Laplacian smoothing: average of neighbors
          gp_XYZ target = (points[i - 1].XYZ() + points[i + 1].XYZ()) * 0.5;

          // Relaxation
          gp_XYZ refined = points[i].XYZ() * (1.0 - m_config.edgeRelax) +
                           target * m_config.edgeRelax;

          // Project
          nextPoints[i] = projectToShape(gp_Pnt(refined), edgeConstraint);

          double distSq = points[i].SquareDistance(nextPoints[i]);
          if (distSq > maxDisp)
            maxDisp = distSq;
        }
        points = nextPoints;
        double currentError = std::sqrt(maxDisp);
        convergence.push_back(currentError);
        if (currentError < 1e-9)
          break;
      }
      m_convergenceHistory[-edgeId] = convergence;
    } else {
      // If not constrained to geometry, checking if it's "Fixed" (User defined
      // shape?) For now, if no geometry constraint, it's a straight line. We
      // could add "Surface" constraint logic here if edges lie on a face but
      // not an edge.
    }

    SmoothedEdge se;
    se.points = points;
    m_smoothedEdges.insert(edgeId, se);
  }
}

// -----------------------------------------------------------------------------
// Face Smoothing
// -----------------------------------------------------------------------------
void Smoother::smoothFaces() {
  m_smoothedFaces.clear();

  for (const auto &[faceId, face] : m_topology->getFaces()) {
    // 1. Get ordered boundary loop
    TopoHalfEdge *startHe = face->getBoundary();
    if (!startHe)
      continue;

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
      continue;
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
    // We need to orient them correctly based on HalfEdge direction
    std::vector<std::vector<gp_Pnt>> boundaries(4);

    for (int k = 0; k < 4; ++k) {
      TopoEdge *edge = loop[k]->parentEdge;
      // Check if we have smoothed data for this edge
      if (!m_smoothedEdges.contains(edge->getID())) {
        // Should not happen if smoothEdges ram first, but safe fallback
        int subs = edge->getSubdivisions();
        boundaries[k].resize(subs + 1);
        // ... fill linearly ... (omitted for brevity, assume exists)
        continue;
      }

      const std::vector<gp_Pnt> &edgePoints =
          m_smoothedEdges[edge->getID()].points;

      // Determine direction
      // loop[k]->origin is the start of this half-edge.
      // edge->getStartNode() is the storage start.
      bool isForward = (loop[k]->origin == edge->getStartNode());

      if (isForward) {
        boundaries[k] = edgePoints;
      } else {
        // Reverse copy
        boundaries[k].resize(edgePoints.size());
        std::reverse_copy(edgePoints.begin(), edgePoints.end(),
                          boundaries[k].begin());
      }
    }

    // 4. Initialize Grid (TFI)
    int M = boundaries[0].size() - 1; // Bottom edge subdivisions
    int N = boundaries[1].size() - 1; // Right edge subdivisions

    // Ensure opposite sides match (Topology enforces this but good to be safe)
    // If not matching, TFI logic is more complex usually involves re-sampling.
    // For now assume strictly valid Quad topology where opposite edges have
    // same subs.

    std::vector<std::vector<gp_Pnt>> grid(M + 1, std::vector<gp_Pnt>(N + 1));
    std::vector<std::vector<bool>> isFixed(M + 1,
                                           std::vector<bool>(N + 1, false));

    for (int i = 0; i <= M; ++i) {
      for (int j = 0; j <= N; ++j) {
        double u = (double)i / M;
        double v = (double)j / N;

        gp_XYZ pBottom = boundaries[0][i].XYZ();
        // Right boundary has N segments.
        gp_XYZ pRight = boundaries[1][j].XYZ();
        // Top boundary has M segments. Indexed M-i because "Top" typically runs
        // Right->Left in loop? Wait, standard loop: Bottom(0), Right(1),
        // Top(2), Left(3). Bottom: 0->1. Right: 1->2. Top: 2->3. Left: 3->0. In
        // TFI (u,v): (u,0) is Bottom. (1,v) is Right. (u,1) is Top. (0,v) is
        // Left.

        // Top boundary from loop[2] goes from Node 2 to Node 3.
        // In TFI grid, top row goes from (0,1) to (1,1). (Node 3 is (0,1), Node
        // 2 is (1,1)?? No) Let's standardise: Node 0 (0,0) -> Node 1 (1,0)
        // [Bottom] Node 1 (1,0) -> Node 2 (1,1) [Right] Node 2 (1,1) -> Node 3
        // (0,1) [Top - Reversed relative to u?] Node 3 (0,1) -> Node 0 (0,0)
        // [Left - Reversed relative to v?]

        // loop[2] corresponds to Top edge. Its points are oriented Node 2 ->
        // Node 3. TFI formula expects Top(u) where u goes 0->1. But Top edge
        // vector goes (1,1) -> (0,1). So u=0 corresponds to "Right" side of Top
        // edge ??? No.

        // Let's re-verify TFI params.
        // P(u,v).
        // u=0: Left Edge. v=0: Bottom Edge.
        // Bottom(u): b[0]...b[M].
        // Right(v):  r[0]...r[N].
        // Top(u):    t[0]...t[M].  <-- This must correspond to Grid[i][N].
        // Left(v):   l[0]...l[N].  <-- This must correspond to Grid[0][j].

        // Boundary 0: Node 0 -> Node 1. Matches Bottom(u).
        // Boundary 1: Node 1 -> Node 2. Matches Right(v).
        // Boundary 2: Node 2 -> Node 3. This is opposed to Bottom.
        //    Grid[i][N] is Top edge. u goes 0 (Left) to 1 (Right).
        //    Boundary 2 starts at Node 2 (Right side) and goes to Node 3 (Left
        //    side). So Boundary 2 corresponds to decreasing u. Top(u) =
        //    Boundary 2 [ M - i ].

        // Boundary 3: Node 3 -> Node 0.
        //    Grid[0][j] is Left edge. v goes 0 (Bottom) to 1 (Top).
        //    Boundary 3 starts at Node 3 (Top) and goes to Node 0 (Bottom).
        //    So Boundary 3 corresponds to decreasing v.
        //    Left(v) = Boundary 3 [ N - j ].

        gp_XYZ pTop_val = boundaries[2][M - i].XYZ();
        gp_XYZ pLeft_val = boundaries[3][N - j].XYZ();

        // Corners
        gp_XYZ cSW = boundaries[0][0].XYZ(); // Node 0
        gp_XYZ cSE = boundaries[0][M].XYZ(); // Node 1
        gp_XYZ cNE = boundaries[2][0].XYZ(); // Node 2 (Start of B2)
        gp_XYZ cNW = boundaries[2][M].XYZ(); // Node 3 (End of B2)

        gp_XYZ pTFI = (1.0 - v) * pBottom + v * pTop_val +
                      (1.0 - u) * pLeft_val + u * pRight -
                      ((1.0 - u) * (1.0 - v) * cSW + u * (1.0 - v) * cSE +
                       u * v * cNE + (1.0 - u) * v * cNW);

        grid[i][j] = gp_Pnt(pTFI);

        // Mark boundaries as fixed (since they were smoothed in stage 1)
        if (i == 0 || i == M || j == 0 || j == N) {
          isFixed[i][j] = true;
        } else if (!surfaceConstraint.IsNull()) {
          // Initial projection of interior to surface
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

    std::vector<double> convergence =
        EllipticSolver::smoothGrid(grid, isFixed, params, constraintFunc);
    m_convergenceHistory[faceId] = convergence;

    SmoothedFace sf;
    sf.grid = grid;
    sf.surface = surfaceConstraint;
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
