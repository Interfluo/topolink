#ifndef TOPOLOGY_H
#define TOPOLOGY_H

#include "DimensionChord.h" // Needed for pool
#include "ObjectPool.h"
#include "TopoEdge.h"
#include "TopoFace.h"
#include "TopoHalfEdge.h"
#include "TopoNode.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

class QJsonObject;

// Grouping structures
struct TopoEdgeGroup {
  int id;
  std::string name;       // Added for UI sync
  std::string geometryID; // Link to geometry constraint
  std::vector<TopoEdge *> edges;
};

struct TopoFaceGroup {
  int id;
  std::string name;       // Added for UI sync
  std::string geometryID; // Link to geometry constraint
  std::vector<TopoFace *> faces;
};

class Topology {
public:
  static constexpr int kHalfEdgeLoopLimit = 1000;

  Topology();
  ~Topology();

  // Serialization
  QJsonObject toJson() const;
  void fromJson(const QJsonObject &json);

  // Node Management
  TopoNode *createNode(const gp_Pnt &position);
  TopoNode *createNodeWithID(int id, const gp_Pnt &position);
  TopoNode *getNode(int id) const;
  void deleteNode(int id);
  bool mergeNodes(int keepId, int removeId);
  void updateNodePosition(int id, const gp_Pnt &pos);
  const std::map<int, TopoNode *> &getNodes() const;

  // Edge Management
  TopoEdge *createEdge(TopoNode *start, TopoNode *end);
  TopoEdge *createEdgeWithID(int id, TopoNode *start, TopoNode *end);
  TopoEdge *getEdge(int id) const;
  TopoEdge *getEdge(TopoNode *start, TopoNode *end) const;
  TopoEdge *getEdge(int n1Id, int n2Id) const;
  void deleteEdge(int id);
  TopoNode *splitEdge(int edgeId, double t);
  void rebuildEdgeLookup();

  const std::map<int, TopoEdge *> &getEdges() const;

  // Edge Dimensions
  std::set<int> getUniqueEdgeSubdivisions() const;
  void setSubdivisionsForEdges(const std::vector<int> &edgeIDs,
                               int subdivisions);
  void propagateSubdivisions(int edgeId, int subdivisions);

  // Face Management
  TopoFace *createFace(const std::vector<TopoEdge *> &edges);
  TopoFace *createFaceWithID(int id, const std::vector<TopoEdge *> &edges);
  TopoFace *getFace(int id) const;
  void deleteFace(int id);
  void rebuildFaceHalfEdges(int faceId);
  const std::map<int, TopoFace *> &getFaces() const;

  // Half-Edge Internal Management
  TopoHalfEdge *createHalfEdge();
  void deleteHalfEdge(TopoHalfEdge *he);

  // Dimension Chord Management
  DimensionChord *createChord(int segments);
  void deleteChord(DimensionChord *chord);

  // Group Management
  TopoEdgeGroup *createEdgeGroup(const std::string &name,
                                 const std::string &geometryID);
  TopoFaceGroup *createFaceGroup(const std::string &name,
                                 const std::string &geometryID);
  void addEdgeToGroup(int groupID, TopoEdge *edge);
  void addFaceToGroup(int groupID, TopoFace *face);
  TopoEdgeGroup *getEdgeGroup(int id) const;
  TopoFaceGroup *getFaceGroup(int id) const;
  TopoEdgeGroup *getEdgeGroupByName(const std::string &name) const;
  TopoFaceGroup *getFaceGroupByName(const std::string &name) const;
  TopoEdgeGroup *getGroupForEdge(int edgeId) const;
  TopoFaceGroup *getGroupForFace(int faceId) const;
  std::string getFaceGeometryID(int faceId) const;
  void clearGroups();

private:
  int _nextId;
  int generateID();

  // Half-Edge Helpers
  void resetHalfEdgeLoop(TopoHalfEdge *start);
  std::vector<TopoHalfEdge *>
  buildHalfEdgeLoop(TopoFace *face, const std::vector<TopoEdge *> &edges);
  void removeEdgeFromChord(TopoEdge *edge);

  // Primary Storage (Owning Pools)
  ObjectPool<TopoNode> _nodePool;
  ObjectPool<TopoEdge> _edgePool;
  ObjectPool<TopoFace> _facePool;
  ObjectPool<TopoHalfEdge> _halfEdgePool;
  ObjectPool<DimensionChord> _chordPool;

  // ID Lookups (Non-owning)
  std::map<int, TopoNode *> _nodes;
  std::map<int, TopoEdge *> _edges;
  std::map<int, TopoFace *> _faces;
  std::map<std::pair<int, int>, TopoEdge *> _edgeLookup;

  std::map<int, std::unique_ptr<TopoEdgeGroup>> _edgeGroups;
  std::map<int, std::unique_ptr<TopoFaceGroup>> _faceGroups;
};

#endif // TOPOLOGY_H
