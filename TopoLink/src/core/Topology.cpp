#include "Topology.h"
#include <QJsonArray>
#include <QJsonObject>
#include <algorithm>
#include <iostream>
#include <set>

Topology::Topology() : _nextId(1) {}

Topology::~Topology() {
  // Unique pointers will clean up map contents automatically
}

int Topology::generateID() { return _nextId++; }

// Node Management
TopoNode *Topology::createNode(const gp_Pnt &position) {
  int id = generateID();
  return createNodeWithID(id, position);
}

TopoNode *Topology::createNodeWithID(int id, const gp_Pnt &position) {
  TopoNode *node = _nodePool.allocate(id, position);
  _nodes[id] = node;
  if (id >= _nextId)
    _nextId = id + 1;
  return node;
}

TopoNode *Topology::getNode(int id) const {
  auto it = _nodes.find(id);
  if (it != _nodes.end()) {
    return it->second;
  }
  return nullptr;
}

void Topology::deleteNode(int id) {
  // 1. Find edges connected to this node
  std::vector<int> edgesToDelete;
  TopoNode *node = getNode(id);
  if (!node)
    return;

  // Use raw pointer map
  for (const auto &pair : _edges) {
    if (pair.second->getStartNode() == node ||
        pair.second->getEndNode() == node) {
      edgesToDelete.push_back(pair.first);
    }
  }

  // 2. Delete connected edges (this will cascade to faces)
  for (int edgeId : edgesToDelete) {
    deleteEdge(edgeId);
  }

  // 3. Delete the node itself
  _nodes.erase(id);
  _nodePool.deallocate(node);
}

void Topology::updateNodePosition(int id, const gp_Pnt &pos) {
  TopoNode *node = getNode(id);
  if (node) {
    node->setPosition(pos);
  }
}

const std::map<int, TopoNode *> &Topology::getNodes() const { return _nodes; }

bool Topology::mergeNodes(int keepId, int removeId) {
  // Validate: both nodes must exist and be different
  if (keepId == removeId)
    return false;

  TopoNode *keepNode = getNode(keepId);
  TopoNode *removeNode = getNode(removeId);
  if (!keepNode || !removeNode)
    return false;

  // Track edges to delete (self-loops after rewiring)
  std::vector<int> edgesToDelete;
  std::vector<int> selfLoopEdges;

  // Update all edges referencing removeNode to point to keepNode
  for (auto &pair : _edges) {
    TopoEdge *edge = pair.second;
    bool modified = false;

    if (edge->getStartNode() == removeNode) {
      edge->setStartNode(keepNode);
      modified = true;
    }
    if (edge->getEndNode() == removeNode) {
      edge->setEndNode(keepNode);
      modified = true;
    }

    // Check for self-loop (both endpoints now the same)
    if (modified && edge->getStartNode() == edge->getEndNode()) {
      std::cout << "Topology::mergeNodes: Edge " << pair.first
                << " became self-loop." << std::endl;
      selfLoopEdges.push_back(pair.first);
      edgesToDelete.push_back(pair.first);
    }
  }

  // Update Half-Edge origins
  for (auto &edgePair : _edges) {
    TopoEdge *e = edgePair.second;
    if (e->getForwardHalfEdge()) {
      if (e->getForwardHalfEdge()->origin == removeNode) {
        e->getForwardHalfEdge()->origin = keepNode;
      }
    }
    if (e->getBackwardHalfEdge()) {
      if (e->getBackwardHalfEdge()->origin == removeNode) {
        e->getBackwardHalfEdge()->origin = keepNode;
      }
    }
  }

  // Handle Self-Loops: standard cascade will delete faces if needed
  // We no longer manually "removeEdge" to repair faces because
  // for multi-domain structured meshing, collapsing an edge should delete the
  // face.

  // Handle Duplicate Edges
  std::map<std::pair<int, int>, TopoEdge *> seenEdgesMap;
  std::vector<int> duplicateEdges;

  for (auto &pair : _edges) {
    if (std::find(edgesToDelete.begin(), edgesToDelete.end(), pair.first) !=
        edgesToDelete.end()) {
      continue;
    }

    TopoEdge *edge = pair.second;
    int n1 = edge->getStartNode()->getID();
    int n2 = edge->getEndNode()->getID();

    auto normalizedPair = std::make_pair(std::min(n1, n2), std::max(n1, n2));

    if (seenEdgesMap.count(normalizedPair) > 0) {
      TopoEdge *keeper = seenEdgesMap[normalizedPair];
      TopoEdge *duplicate = edge;

      for (auto &facePair : _faces) {
        facePair.second->replaceEdge(duplicate, keeper);
      }

      duplicateEdges.push_back(pair.first);
      edgesToDelete.push_back(pair.first);
    } else {
      seenEdgesMap[normalizedPair] = edge;
    }
  }

  // Final cleanup of degenerate faces (e.g. if a face now has < 3 unique edges)
  std::vector<int> facesToDelete;
  for (auto &facePair : _faces) {
    std::set<TopoEdge *> uniqueEdges;
    for (auto *e : facePair.second->getEdges()) {
      uniqueEdges.insert(e);
    }
    if (uniqueEdges.size() < 3) {
      std::cout << "Topology::mergeNodes: Face " << facePair.first
                << " became degenerate with " << uniqueEdges.size()
                << " unique edges." << std::endl;
      facesToDelete.push_back(facePair.first);
    }
  }

  for (int faceId : facesToDelete) {
    std::cout << "Topology::mergeNodes: Deleting degenerate face " << faceId
              << std::endl;
    deleteFace(faceId);
  }

  for (int edgeId : edgesToDelete) {
    deleteEdge(edgeId);
  }

  // Final Step: Rebuild Half-Edges for all surviving faces
  for (auto &facePair : _faces) {
    rebuildFaceHalfEdges(facePair.first);
  }

  // Rebuild optimized lookup map since edge endpoints changed
  rebuildEdgeLookup();

  deleteNode(removeId); // Use deleteNode logic to clean up map and pool
  return true;
}

// Edge Management
TopoEdge *Topology::createEdge(TopoNode *start, TopoNode *end) {
  int id = generateID();
  return createEdgeWithID(id, start, end);
}

TopoEdge *Topology::createEdgeWithID(int id, TopoNode *start, TopoNode *end) {
  if (!start || !end)
    return nullptr;
  TopoEdge *edge = _edgePool.allocate(id, start, end);
  _edges[id] = edge;

  // Create Half-Edges immediately (Boundary HEs)
  TopoHalfEdge *he1 = createHalfEdge();
  TopoHalfEdge *he2 = createHalfEdge();
  he1->origin = start;
  he1->parentEdge = edge;
  he2->origin = end;
  he2->parentEdge = edge;
  edge->setHalfEdges(he1, he2);

  // Set default out pointer for nodes if not already set
  if (!start->getOut())
    start->setOut(he1);
  if (!end->getOut())
    end->setOut(he2);

  // Update Optimized Lookup
  int n1 = start->getID();
  int n2 = end->getID();
  auto key = std::make_pair(std::min(n1, n2), std::max(n1, n2));
  _edgeLookup[key] = edge;

  if (id >= _nextId)
    _nextId = id + 1;
  return edge;
}

TopoEdge *Topology::getEdge(TopoNode *n1, TopoNode *n2) const {
  if (!n1 || !n2)
    return nullptr;
  auto key = std::make_pair(std::min(n1->getID(), n2->getID()),
                            std::max(n1->getID(), n2->getID()));
  auto it = _edgeLookup.find(key);
  if (it != _edgeLookup.end()) {
    return it->second;
  }
  return nullptr;
}

TopoEdge *Topology::getEdge(int n1Id, int n2Id) const {
  TopoNode *n1 = getNode(n1Id);
  TopoNode *n2 = getNode(n2Id);
  if (!n1 || !n2)
    return nullptr;
  return getEdge(n1, n2);
}

TopoEdge *Topology::getEdge(int id) const {
  auto it = _edges.find(id);
  if (it != _edges.end()) {
    return it->second;
  }
  return nullptr;
}

void Topology::deleteEdge(int id) {
  TopoEdge *edge = getEdge(id);
  if (!edge)
    return;

  // 1. Find faces using this edge
  std::vector<int> facesToDelete;
  for (const auto &pair : _faces) {
    const auto &faceEdges = pair.second->getEdges();
    if (std::find(faceEdges.begin(), faceEdges.end(), edge) !=
        faceEdges.end()) {
      facesToDelete.push_back(pair.first);
    }
  }

  // 2. Delete connected faces
  for (int faceId : facesToDelete) {
    deleteFace(faceId);
  }

  // 3. Remove from edge groups
  for (auto &groupPair : _edgeGroups) {
    auto &edges = groupPair.second->edges;
    edges.erase(std::remove(edges.begin(), edges.end(), edge), edges.end());
  }

  // 4. Remove from Optimized Lookup
  int n1 = edge->getStartNode()->getID();
  int n2 = edge->getEndNode()->getID();
  auto key = std::make_pair(std::min(n1, n2), std::max(n1, n2));
  _edgeLookup.erase(key);

  // 5. Delete the Half-Edges
  if (edge->getForwardHalfEdge())
    deleteHalfEdge(edge->getForwardHalfEdge());
  if (edge->getBackwardHalfEdge())
    deleteHalfEdge(edge->getBackwardHalfEdge());

  // 6. Delete the edge
  _edges.erase(id);
  _edgePool.deallocate(edge);
}

void Topology::rebuildEdgeLookup() {
  _edgeLookup.clear();
  for (auto const &[id, edge] : _edges) {
    int n1 = edge->getStartNode()->getID();
    int n2 = edge->getEndNode()->getID();
    auto key = std::make_pair(std::min(n1, n2), std::max(n1, n2));
    _edgeLookup[key] = edge;
  }
}

const std::map<int, TopoEdge *> &Topology::getEdges() const { return _edges; }

// Edge Dimensions
std::set<int> Topology::getUniqueEdgeSubdivisions() const {
  std::set<int> uniqueSubdivs;
  for (const auto &pair : _edges) {
    uniqueSubdivs.insert(pair.second->getSubdivisions());
  }
  return uniqueSubdivs;
}

void Topology::setSubdivisionsForEdges(const std::vector<int> &edgeIDs,
                                       int subdivisions) {
  for (int id : edgeIDs) {
    auto it = _edges.find(id);
    if (it != _edges.end()) {
      it->second->setSubdivisions(subdivisions);
    }
  }
}

// Face Management
TopoFace *Topology::createFace(const std::vector<TopoEdge *> &edges) {
  int id = generateID();
  return createFaceWithID(id, edges);
}

TopoFace *Topology::createFaceWithID(int id,
                                     const std::vector<TopoEdge *> &edges) {
  if (edges.empty())
    return nullptr;

  TopoFace *face = _facePool.allocate(id, edges);
  _faces[id] = face;
  if (id >= _nextId)
    _nextId = id + 1;

  std::vector<TopoHalfEdge *> loopHEs;
  loopHEs.reserve(edges.size());

  for (size_t i = 0; i < edges.size(); ++i) {
    TopoEdge *currEdge = edges[i];
    TopoEdge *nextEdge = edges[(i + 1) % edges.size()];

    // Determine common node to infer direction
    TopoNode *n1 = currEdge->getStartNode();
    TopoNode *n2 = currEdge->getEndNode();
    TopoNode *nextN1 = nextEdge->getStartNode();
    TopoNode *nextN2 = nextEdge->getEndNode();

    TopoNode *common = nullptr;
    if (n2 == nextN1 || n2 == nextN2) {
      common = n2; // Forward: n1 -> n2 -> ...
    } else if (n1 == nextN1 || n1 == nextN2) {
      common = n1; // Backward: n2 -> n1 -> ...
    } else {
      continue;
    }

    // Use Existing HalfEdge
    TopoHalfEdge *he = (common == n2) ? currEdge->getForwardHalfEdge()
                                      : currEdge->getBackwardHalfEdge();
    if (!he)
      continue;

    he->face = face;
    he->origin->setOut(he); // Update out pointer to be face-aligned

    loopHEs.push_back(he);
  }

  // Link Half-Edges (Next/Prev)
  if (!loopHEs.empty()) {
    for (size_t i = 0; i < loopHEs.size(); ++i) {
      TopoHalfEdge *curr = loopHEs[i];
      TopoHalfEdge *next = loopHEs[(i + 1) % loopHEs.size()];

      curr->next = next;
      next->prev = curr;
    }
    face->setBoundary(loopHEs[0]);
  }

  return face;
}

TopoFace *Topology::getFace(int id) const {
  auto it = _faces.find(id);
  if (it != _faces.end()) {
    return it->second;
  }
  return nullptr;
}

void Topology::deleteFace(int id) {
  std::cout << "Topology::deleteFace: " << id << std::endl;
  TopoFace *face = getFace(id);
  if (face) {
    // Reset half-edges belonging to this face to be boundary HEs
    // DO NOT delete them here; they are owned by the edges.
    TopoHalfEdge *start = face->getBoundary();
    if (start) {
      std::vector<TopoHalfEdge *> loop;
      TopoHalfEdge *curr = start;
      do {
        loop.push_back(curr);
        curr = curr->next;
      } while (curr && curr != start);

      for (auto *he : loop) {
        he->face = nullptr;
        he->next = nullptr;
        he->prev = nullptr;
      }
    }

    _faces.erase(id);
    _facePool.deallocate(face);
  }
}

void Topology::rebuildFaceHalfEdges(int faceId) {
  TopoFace *face = getFace(faceId);
  if (!face)
    return;

  // 1. Reset existing half-edges for this face to be boundary HEs
  TopoHalfEdge *start = face->getBoundary();
  if (start) {
    std::vector<TopoHalfEdge *> loop;
    TopoHalfEdge *curr = start;
    do {
      loop.push_back(curr);
      curr = curr->next;
    } while (curr && curr != start);

    for (auto *he : loop) {
      he->face = nullptr;
      he->next = nullptr;
      he->prev = nullptr;
    }
  }
  face->setBoundary(nullptr);

  // 2. Rebuild from edges vector using existing HEs
  std::vector<TopoEdge *> edges = face->getEdges();
  if (edges.empty())
    return;

  std::vector<TopoHalfEdge *> loopHEs;
  for (size_t i = 0; i < edges.size(); ++i) {
    TopoEdge *currEdge = edges[i];
    TopoEdge *nextEdge = edges[(i + 1) % edges.size()];

    TopoNode *n1 = currEdge->getStartNode();
    TopoNode *n2 = currEdge->getEndNode();
    TopoNode *nextN1 = nextEdge->getStartNode();
    TopoNode *nextN2 = nextEdge->getEndNode();

    TopoNode *common = nullptr;
    if (n2 == nextN1 || n2 == nextN2) {
      common = n2;
    } else if (n1 == nextN1 || n1 == nextN2) {
      common = n1;
    }

    if (!common)
      continue;

    TopoHalfEdge *he = (common == n2) ? currEdge->getForwardHalfEdge()
                                      : currEdge->getBackwardHalfEdge();
    if (!he)
      continue;

    he->face = face;
    he->origin->setOut(he);
    loopHEs.push_back(he);
  }

  if (!loopHEs.empty()) {
    for (size_t i = 0; i < loopHEs.size(); ++i) {
      TopoHalfEdge *curr = loopHEs[i];
      TopoHalfEdge *next = loopHEs[(i + 1) % loopHEs.size()];
      curr->next = next;
      next->prev = curr;
    }
    face->setBoundary(loopHEs[0]);
  }
}

const std::map<int, TopoFace *> &Topology::getFaces() const { return _faces; }

// Half-Edge Internal Management
TopoHalfEdge *Topology::createHalfEdge() { return _halfEdgePool.allocate(); }

void Topology::deleteHalfEdge(TopoHalfEdge *he) {
  _halfEdgePool.deallocate(he);
}

// Dimension Chord Management
DimensionChord *Topology::createChord(int segments) {
  DimensionChord *chord = _chordPool.allocate();
  chord->segments = segments;
  return chord;
}

void Topology::deleteChord(DimensionChord *chord) {
  _chordPool.deallocate(chord);
}

// Group Management
TopoEdgeGroup *Topology::createEdgeGroup(const std::string &geometryID) {
  int id = generateID();
  auto group = std::make_unique<TopoEdgeGroup>();
  group->id = id;
  group->geometryID = geometryID;
  TopoEdgeGroup *ptr = group.get();
  _edgeGroups[id] = std::move(group);
  return ptr;
}

TopoFaceGroup *Topology::createFaceGroup(const std::string &geometryID) {
  int id = generateID();
  auto group = std::make_unique<TopoFaceGroup>();
  group->id = id;
  group->geometryID = geometryID;
  TopoFaceGroup *ptr = group.get();
  _faceGroups[id] = std::move(group);
  return ptr;
}

void Topology::addEdgeToGroup(int groupID, TopoEdge *edge) {
  auto it = _edgeGroups.find(groupID);
  if (it != _edgeGroups.end() && edge) {
    it->second->edges.push_back(edge);
  }
}

void Topology::addFaceToGroup(int groupID, TopoFace *face) {
  auto it = _faceGroups.find(groupID);
  if (it != _faceGroups.end() && face) {
    it->second->faces.push_back(face);
  }
}

TopoEdgeGroup *Topology::getEdgeGroup(int id) const {
  auto it = _edgeGroups.find(id);
  if (it != _edgeGroups.end()) {
    return it->second.get();
  }
  return nullptr;
}

TopoFaceGroup *Topology::getFaceGroup(int id) const {
  auto it = _faceGroups.find(id);
  return nullptr;
}

QJsonObject Topology::toJson() const {
  QJsonObject root;

  // Nodes
  QJsonObject nodesObj;
  for (const auto &[id, node] : _nodes) {
    QJsonObject n;
    n["target_id"] = QString::fromStdString(node->getConstraintTargetID());
    n["u"] = node->getU();
    n["v"] = node->getV();

    QJsonArray pos;
    pos.append(node->getPosition().X());
    pos.append(node->getPosition().Y());
    pos.append(node->getPosition().Z());
    n["position"] = pos;

    QString freedom;
    switch (node->getFreedom()) {
    case TopoNode::NodeFreedom::LOCKED:
      freedom = "LOCKED";
      break;
    case TopoNode::NodeFreedom::SLIDING_CURVE:
      freedom = "SLIDING_CURVE";
      break;
    case TopoNode::NodeFreedom::SLIDING_SURF:
      freedom = "SLIDING_SURF";
      break;
    case TopoNode::NodeFreedom::FREE:
      freedom = "FREE";
      break;
    }
    n["freedom"] = freedom;
    nodesObj[QString::number(id)] = n;
  }
  root["topo_nodes"] = nodesObj;

  // Edges
  QJsonObject edgesObj;
  for (const auto &[id, edge] : _edges) {
    QJsonObject e;
    QJsonArray nodeIds;
    nodeIds.append(edge->getStartNode()->getID());
    nodeIds.append(edge->getEndNode()->getID());
    e["node_ids"] = nodeIds;
    e["subdivisions"] = edge->getSubdivisions();
    edgesObj[QString::number(id)] = e;
  }
  root["topo_edges"] = edgesObj;

  // Faces
  QJsonObject facesObj;
  for (const auto &[id, face] : _faces) {
    QJsonObject f;
    QJsonArray edgeIds;
    for (TopoEdge *edge : face->getEdges()) {
      edgeIds.append(edge->getID());
    }
    f["edge_ids"] = edgeIds;
    facesObj[QString::number(id)] = f;
  }
  root["topo_faces"] = facesObj;

  // Edge Groups
  QJsonObject edgeGroupsObj;
  for (const auto &[id, group] : _edgeGroups) {
    QJsonObject g;
    g["geometry_id"] = QString::fromStdString(group->geometryID);
    QJsonArray edgeIds;
    for (TopoEdge *edge : group->edges) {
      edgeIds.append(edge->getID());
    }
    g["edge_ids"] = edgeIds;
    edgeGroupsObj[QString::number(id)] = g;
  }
  root["topo_edge_groups"] = edgeGroupsObj;

  // Face Groups
  QJsonObject faceGroupsObj;
  for (const auto &[id, group] : _faceGroups) {
    QJsonObject g;
    g["geometry_id"] = QString::fromStdString(group->geometryID);
    QJsonArray faceIds;
    for (TopoFace *face : group->faces) {
      faceIds.append(face->getID());
    }
    g["face_ids"] = faceIds;
    faceGroupsObj[QString::number(id)] = g;
  }
  root["topo_face_groups"] = faceGroupsObj;

  return root;
}

void Topology::fromJson(const QJsonObject &json) {
  // Clear existing topology
  _nodes.clear();
  _edges.clear();
  _faces.clear();
  _edgeLookup.clear();
  _edgeGroups.clear();
  _faceGroups.clear();
  _nextId = 1;

  // Reset pools
  _nodePool.clear();
  _edgePool.clear();
  _facePool.clear();
  _halfEdgePool.clear();
  _chordPool.clear();

  // 1. Nodes
  if (json.contains("topo_nodes")) {
    QJsonObject nodesObj = json["topo_nodes"].toObject();
    for (auto it = nodesObj.begin(); it != nodesObj.end(); ++it) {
      int id = it.key().toInt();
      QJsonObject n = it.value().toObject();
      QJsonArray posArr = n["position"].toArray();
      gp_Pnt pos(posArr[0].toDouble(), posArr[1].toDouble(),
                 posArr[2].toDouble());

      TopoNode *node = createNodeWithID(id, pos);
      node->setConstraintTargetID(n["target_id"].toString().toStdString());
      node->setNormalizedUV(n["u"].toDouble(), n["v"].toDouble());

      QString freedom = n["freedom"].toString();
      if (freedom == "LOCKED")
        node->setFreedom(TopoNode::NodeFreedom::LOCKED);
      else if (freedom == "SLIDING_CURVE")
        node->setFreedom(TopoNode::NodeFreedom::SLIDING_CURVE);
      else if (freedom == "SLIDING_SURF")
        node->setFreedom(TopoNode::NodeFreedom::SLIDING_SURF);
      else
        node->setFreedom(TopoNode::NodeFreedom::FREE);
    }
  }

  // 2. Edges
  if (json.contains("topo_edges")) {
    QJsonObject edgesObj = json["topo_edges"].toObject();
    for (auto it = edgesObj.begin(); it != edgesObj.end(); ++it) {
      int id = it.key().toInt();
      QJsonObject e = it.value().toObject();
      QJsonArray nodeIds = e["node_ids"].toArray();
      TopoNode *n1 = getNode(nodeIds[0].toInt());
      TopoNode *n2 = getNode(nodeIds[1].toInt());
      if (n1 && n2) {
        TopoEdge *edge = createEdgeWithID(id, n1, n2);
        edge->setSubdivisions(e["subdivisions"].toInt());
      }
    }
  }

  // 3. Faces
  if (json.contains("topo_faces")) {
    QJsonObject facesObj = json["topo_faces"].toObject();
    for (auto it = facesObj.begin(); it != facesObj.end(); ++it) {
      int id = it.key().toInt();
      QJsonObject f = it.value().toObject();
      QJsonArray edgeIds = f["edge_ids"].toArray();
      std::vector<TopoEdge *> edges;
      for (const auto &val : edgeIds) {
        TopoEdge *edge = getEdge(val.toInt());
        if (edge)
          edges.push_back(edge);
      }
      if (!edges.empty()) {
        createFaceWithID(id, edges);
      }
    }
  }

  // 4. Edge Groups
  if (json.contains("topo_edge_groups")) {
    QJsonObject groupsObj = json["topo_edge_groups"].toObject();
    for (auto it = groupsObj.begin(); it != groupsObj.end(); ++it) {
      int id = it.key().toInt();
      QJsonObject g = it.value().toObject();
      TopoEdgeGroup *group =
          createEdgeGroup(g["geometry_id"].toString().toStdString());

      // Handle the case where the auto-generated ID is different
      if (group->id != id) {
        // Re-insert into map with correct ID
        std::unique_ptr<TopoEdgeGroup> ptr;
        auto itGroup = _edgeGroups.find(group->id);
        if (itGroup != _edgeGroups.end()) {
          ptr = std::move(itGroup->second);
          _edgeGroups.erase(itGroup);
          ptr->id = id;
          _edgeGroups[id] = std::move(ptr);
          group = _edgeGroups[id].get();
        }
      }

      QJsonArray edgeIds = g["edge_ids"].toArray();
      for (const auto &val : edgeIds) {
        addEdgeToGroup(id, getEdge(val.toInt()));
      }
    }
  }

  // 5. Face Groups
  if (json.contains("topo_face_groups")) {
    QJsonObject groupsObj = json["topo_face_groups"].toObject();
    for (auto it = groupsObj.begin(); it != groupsObj.end(); ++it) {
      int id = it.key().toInt();
      QJsonObject g = it.value().toObject();
      TopoFaceGroup *group =
          createFaceGroup(g["geometry_id"].toString().toStdString());

      if (group->id != id) {
        std::unique_ptr<TopoFaceGroup> ptr;
        auto itGroup = _faceGroups.find(group->id);
        if (itGroup != _faceGroups.end()) {
          ptr = std::move(itGroup->second);
          _faceGroups.erase(itGroup);
          ptr->id = id;
          _faceGroups[id] = std::move(ptr);
          group = _faceGroups[id].get();
        }
      }

      QJsonArray faceIds = g["face_ids"].toArray();
      for (const auto &val : faceIds) {
        addFaceToGroup(id, getFace(val.toInt()));
      }
    }
  }
}
