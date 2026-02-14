#include "MeshExporter.h"
#include "Smoother.h"
#include "TopoEdge.h"
#include "TopoFace.h"
#include "Topology.h"
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <unordered_map>

bool MeshExporter::exportToVTK(const QString &filename, const Topology *topo,
                               const Smoother *smoother) {
  if (!topo || !smoother)
    return false;

  QFile file(filename);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qDebug() << "Failed to open file for VTK export:" << filename;
    return false;
  }

  QTextStream out(&file);

  const QMap<int, Smoother::SmoothedFace> &smoothedFaces =
      smoother->getSmoothedFaces();

  // 1. Collect all unique points and build cells
  std::vector<gp_Pnt> allPoints;
  auto pointHash = [](const gp_Pnt &p) {
    return std::hash<double>{}(p.X()) ^ (std::hash<double>{}(p.Y()) << 1) ^
           (std::hash<double>{}(p.Z()) << 2);
  };
  auto pointEqual = [](const gp_Pnt &p1, const gp_Pnt &p2) {
    return p1.IsEqual(p2, 1e-9);
  };
  std::unordered_map<gp_Pnt, int, decltype(pointHash), decltype(pointEqual)>
      pointMap(1000, pointHash, pointEqual);

  struct Quad {
    int p[4];
    int faceGroupId;
  };
  std::vector<Quad> allQuads;

  // Map of pointIndex -> best EdgeGroupID
  // We'll update this as we process boundaries
  std::unordered_map<int, int> pointToEdgeGroup;

  for (auto it = smoothedFaces.begin(); it != smoothedFaces.end(); ++it) {
    int faceId = it.key();
    const Smoother::SmoothedFace &sf = it.value();
    const auto &grid = sf.grid;
    if (grid.empty() || grid[0].empty())
      continue;

    int M = grid.size() - 1;
    int N = grid[0].size() - 1;

    // Get Face Group ID
    int faceGroupId = 0;
    TopoFaceGroup *fg = topo->getGroupForFace(faceId);
    if (fg)
      faceGroupId = fg->id;

    // Grid of global point indices for this face
    std::vector<std::vector<int>> gridIndices(M + 1, std::vector<int>(N + 1));

    for (int i = 0; i <= M; ++i) {
      for (int j = 0; j <= N; ++j) {
        const gp_Pnt &p = grid[i][j];
        if (pointMap.find(p) == pointMap.end()) {
          int newIdx = (int)allPoints.size();
          pointMap[p] = newIdx;
          allPoints.push_back(p);
          gridIndices[i][j] = newIdx;
        } else {
          gridIndices[i][j] = pointMap[p];
        }
      }
    }

    // Identify boundary edges for this face to tag points
    TopoFace *face = topo->getFace(faceId);
    if (face) {
      TopoHalfEdge *startHe = face->getBoundary();
      if (startHe) {
        std::vector<TopoHalfEdge *> loop;
        TopoHalfEdge *curr = startHe;
        do {
          loop.push_back(curr);
          curr = curr->next;
        } while (curr != startHe && curr && loop.size() < 4);

        if (loop.size() == 4) {
          // loop[0]: bottom (j=0), loop[1]: right (i=M), loop[2]: top (j=N,
          // reversed), loop[3]: left (i=0, reversed)
          auto tagBoundary = [&](int iStart, int iEnd, int jStart, int jEnd,
                                 TopoEdge *edge) {
            TopoEdgeGroup *eg = topo->getGroupForEdge(edge->getID());
            if (eg) {
              for (int i = iStart; i <= iEnd; ++i) {
                for (int j = jStart; j <= jEnd; ++j) {
                  pointToEdgeGroup[gridIndices[i][j]] = eg->id;
                }
              }
            }
          };

          tagBoundary(0, M, 0, 0, loop[0]->parentEdge); // Bottom
          tagBoundary(M, M, 0, N, loop[1]->parentEdge); // Right
          tagBoundary(0, M, N, N, loop[2]->parentEdge); // Top
          tagBoundary(0, 0, 0, N, loop[3]->parentEdge); // Left
        }
      }
    }

    // Create Quads
    for (int i = 0; i < M; ++i) {
      for (int j = 0; j < N; ++j) {
        Quad q;
        q.p[0] = gridIndices[i][j];
        q.p[1] = gridIndices[i + 1][j];
        q.p[2] = gridIndices[i + 1][j + 1];
        q.p[3] = gridIndices[i][j + 1];
        q.faceGroupId = faceGroupId;
        allQuads.push_back(q);
      }
    }
  }

  // 2. Write VTK Header
  out << "# vtk DataFile Version 3.0\n";
  out << "Topolink Mesh Export\n";
  out << "ASCII\n";
  out << "DATASET UNSTRUCTURED_GRID\n";

  // 3. Write Points
  out << "POINTS " << allPoints.size() << " double\n";
  for (const auto &p : allPoints) {
    out << p.X() << " " << p.Y() << " " << p.Z() << "\n";
  }

  // 4. Write Cells
  out << "CELLS " << allQuads.size() << " " << (allQuads.size() * 5) << "\n";
  for (const auto &q : allQuads) {
    out << "4 " << q.p[0] << " " << q.p[1] << " " << q.p[2] << " " << q.p[3]
        << "\n";
  }

  out << "CELL_TYPES " << allQuads.size() << "\n";
  for (size_t i = 0; i < allQuads.size(); ++i) {
    out << "9\n"; // VTK_QUAD
  }

  // 5. Write Cell Data (Face Groups)
  out << "CELL_DATA " << allQuads.size() << "\n";
  out << "SCALARS topo_face_group_id int 1\n";
  out << "LOOKUP_TABLE default\n";
  for (const auto &q : allQuads) {
    out << q.faceGroupId << "\n";
  }

  // 6. Write Point Data (Edge Groups)
  out << "POINT_DATA " << allPoints.size() << "\n";
  out << "SCALARS topo_edge_group_id int 1\n";
  out << "LOOKUP_TABLE default\n";
  for (size_t i = 0; i < allPoints.size(); ++i) {
    auto it = pointToEdgeGroup.find((int)i);
    if (it != pointToEdgeGroup.end()) {
      out << it->second << "\n";
    } else {
      out << "0\n";
    }
  }

  file.close();
  return true;
}
