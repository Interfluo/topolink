#ifndef SMOOTHER_H
#define SMOOTHER_H

#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <map>
#include <vector>

#include "SmootherConfig.h"
#include "Topology.h"
#include <QPair>
#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>

#include <QMutex>
#include <QObject>

class Topology;
class TopoEdge;
class TopoFace;

/**
 * @brief Manages the edge and face smoothing process.
 */
class Smoother : public QObject {
  Q_OBJECT
public:
  struct SmoothedEdge {
    std::vector<gp_Pnt> points;
  };

  struct SmoothedFace {
    std::vector<std::vector<gp_Pnt>> grid;
    TopoDS_Shape surface; // Underlying geometry (optional) for reference
  };

  /**
   * @brief Constraint on a topology node/edge/face.
   * Copied/Adapted from OccView::NodeConstraint to avoid dependency loop.
   */
  enum ConstraintType {
    ConstraintNone,
    ConstraintFixed,
    ConstraintEdge,
    ConstraintFace,
    ConstraintGeometry
  };

  struct Constraint {
    ConstraintType type = ConstraintNone;
    QList<int> geometryIds; // Target CAD face/edge IDs
    bool isEdgeGroup = false;
    gp_Pnt origin; // Original position (for Fixed constraints)
  };

  Smoother(Topology *topology);
  ~Smoother();

  /**
   * @brief Sets the smoothing configuration.
   */
  void setConfig(const SmootherConfig &config);

  /**
   * @brief Sets constraints for nodes.
   * These should be populated before running the smoother.
   */
  void setConstraints(const QMap<int, Constraint> &constraints);

  /**
   * @brief Sets the geometry maps for constraint looking up.
   * Needed because core Topology doesn't know about CAD shapes.
   * Pointers are not owned by Smoother.
   */
  void setGeometryMaps(const void *faceMap, const void *edgeMap);

  /**
   * @brief Runs the full smoothing process (edges then faces).
   */
  void run();

  void saveConvergenceData(const QString &filename) const;

signals:
  void iterationCompleted(int id, int iteration, double error);

public:
  // Accessors for results
  const QMap<int, SmoothedEdge> &getSmoothedEdges() const;
  const QMap<int, SmoothedFace> &getSmoothedFaces() const;

private:
  void smoothEdges();
  void smoothFaces();

  void smoothSingleEdge(int edgeId, TopoEdge *edge);
  void smoothSingleFace(int faceId, TopoFace *face);
  void smoothFaceGroup(const TopoFaceGroup *group,
                       QSet<int> &processedFaces); // Added method

  // Helper to project point to shape
  gp_Pnt projectToShape(const gp_Pnt &p, const TopoDS_Shape &s);

  // Helper to build a TopoDS_Shape from geometry IDs
  TopoDS_Shape buildTargetShape(const QList<int> &ids, bool isEdge);

  const Topology *m_topology = nullptr;
  SmootherConfig m_config;
  QMap<int, Constraint> m_constraints;

  // Geometry Lookups (void* cast to TopTools_IndexedMapOfShape* internally)
  const void *m_geoFaceMap = nullptr;
  const void *m_geoEdgeMap = nullptr;

  // Results
  QMap<int, SmoothedEdge> m_smoothedEdges;
  QMap<int, SmoothedFace> m_smoothedFaces;

  // FaceID -> Vector of max displacement per iteration
  std::map<int, std::vector<double>> m_convergenceHistory;

  mutable QMutex m_mutex;
};

#endif // SMOOTHER_H
