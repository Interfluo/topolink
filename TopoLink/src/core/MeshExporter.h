#ifndef MESHEXPORTER_H
#define MESHEXPORTER_H

#include <QMap>
#include <QString>
#include <functional>
#include <gp_Pnt.hxx>
#include <vector>

class Topology;
class Smoother;

/**
 * @brief Utility class to export smoothed meshes to various formats.
 */
class MeshExporter {
public:
  /**
   * @brief Exports the smoothed mesh from the Smoother to a VTK Legacy ASCII
   * file.
   *
   * @param filename Path to the output .vtk file.
   * @param topo Reference to the topology model (for group information).
   * @param smoother Reference to the smoother containing the results.
   * @return true if successful, false otherwise.
   */
  static bool exportToVTK(const QString &filename, const Topology *topo,
                          const Smoother *smoother);

private:
  struct PointHash {
    size_t operator()(const gp_Pnt &p) const {
      // Simple hash for gp_Pnt using coordinates
      // Using a tolerance might be better, but smoother points should be
      // bit-identical for shared boundaries.
      size_t h1 = std::hash<double>{}(p.X());
      size_t h2 = std::hash<double>{}(p.Y());
      size_t h3 = std::hash<double>{}(p.Z());
      return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
  };

  struct PointEqual {
    bool operator()(const gp_Pnt &p1, const gp_Pnt &p2) const {
      return p1.IsEqual(p2, 1e-9);
    }
  };
};

#endif // MESHEXPORTER_H
