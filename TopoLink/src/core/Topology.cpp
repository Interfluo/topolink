#include "Topology.h"
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <set>
#include <unordered_set>

#include <gp_Pnt.hxx>

Topology::Topology() : _nextId(1) {}

Topology::~Topology() {}

int Topology::generateID() { return _nextId++; }

// ---------------------------------------------------------------------------
// Half-Edge Helpers
// ---------------------------------------------------------------------------

void Topology::resetHalfEdgeLoop(TopoHalfEdge *start) {
  if (!start)
    return;
  std::vector<TopoHalfEdge *> loop;
  TopoHalfEdge *curr = start;
  int safety = 0;
  do {
    loop.push_back(curr);
    curr = curr->next;
    if (++safety > kHalfEdgeLoopLimit)
      break;
  } while (curr && curr != start);

  for (auto *he : loop) {
    he->face = nullptr;
    he->next = nullptr;
    he->prev = nullptr;
  }
}

std::vector<TopoHalfEdge *>
Topology::buildHalfEdgeLoop(TopoFace *face,
                            const std::vector<TopoEdge *> &edges) {
  std::vector<TopoHalfEdge *> loopHEs;
  loopHEs.reserve(edges.size());

  for (size_t i = 0; i < edges.size(); ++i) {
    TopoEdge *currEdge = edges[i];
    TopoEdge *nextEdge = edges[(i + 1) % edges.size()];

    TopoNode *n1 = currEdge->getStartNode();
    TopoNode *n2 = currEdge->getEndNode();
    TopoNode *nextN1 = nextEdge->getStartNode();
    TopoNode *nextN2 = nextEdge->getEndNode();

    int n1Id = n1->getID();
    int n2Id = n2->getID();
    int nextN1Id = nextN1->getID();
    int nextN2Id = nextN2->getID();

    TopoNode *common = nullptr;
    if (n2Id == nextN1Id || n2Id == nextN2Id) {
      common = n2;
    } else if (n1Id == nextN1Id || n1Id == nextN2Id) {
      common = n1;
    } else {
      qDebug() << "Topology Error: Disconnected edges in face creation at index"
               << i << "(Nodes" << n1Id << "-" << n2Id << "vs" << nextN1Id
               << "-" << nextN2Id << ")";
      continue;
    }

    TopoHalfEdge *he = (common == n2) ? currEdge->getForwardHalfEdge()
                                      : currEdge->getBackwardHalfEdge();
    if (!he)
      continue;

    // --- FIX STARTS HERE ---
    // Instead of checking if the edge is used at all, check if THIS specific
    // half-edge direction is already taken by another face.
    if (he->face && he->face != face) {
      qDebug() << "Topology Error: Half-edge direction already owned. Edge"
               << currEdge->getID() << "Direction"
               << (common == n2 ? "Forward" : "Backward")
               << "is already owned by Face" << he->face->getID();
      return {};
    }
    // --- FIX ENDS HERE ---

    he->face = face;
    he->origin->setOut(he);

    // Safeguard: Ensure we don't add the same HE twice
    bool alreadyPresent = false;
    for (auto *existing : loopHEs) {
      if (existing == he) {
        alreadyPresent = true;
        break;
      }
    }

    if (!alreadyPresent) {
      loopHEs.push_back(he);
    } else {
      qDebug() << "Topology Warning: Duplicate half-edge in loop.";
      break;
    }
  }

  // --- VALIDATION FIX ---
  if (loopHEs.size() != edges.size()) {
    qDebug() << "Topology Error: Failed to build complete loop. Edges may be "
                "unordered or disconnected.";
    // Cleanup partially assigned faces to prevent dangling pointers
    for (auto *he : loopHEs) {
      if (he->face == face)
        he->face = nullptr;
    }
    return {};
  }

  // Link next/prev
  if (!loopHEs.empty()) {
    for (size_t i = 0; i < loopHEs.size(); ++i) {
      TopoHalfEdge *curr = loopHEs[i];
      TopoHalfEdge *next = loopHEs[(i + 1) % loopHEs.size()];
      curr->next = next;
      next->prev = curr;
    }
  }

  return loopHEs;
}

void Topology::removeEdgeFromChord(TopoEdge *edge) {
  if (!edge)
    return;
  DimensionChord *chord = edge->getChord();
  if (!chord)
    return;
  auto &reg = chord->registeredEdges;
  reg.erase(std::remove(reg.begin(), reg.end(), edge), reg.end());
}

// ---------------------------------------------------------------------------
// Node Management
// ---------------------------------------------------------------------------

TopoNode *Topology::createNode(const gp_Pnt &position) {
  int id = generateID();
  return createNodeWithID(id, position);
}

TopoNode *Topology::createNodeWithID(int id, const gp_Pnt &position) {
  auto it = _nodes.find(id);
  if (it != _nodes.end()) {
    it->second->setPosition(position);
    return it->second;
  }

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
  TopoNode *node = getNode(id);
  if (!node)
    return;

  // Find edges connected to this node
  std::vector<int> edgesToDelete;
  for (const auto &pair : _edges) {
    if (pair.second->getStartNode() == node ||
        pair.second->getEndNode() == node) {
      edgesToDelete.push_back(pair.first);
    }
  }

  // Delete connected edges (cascades to faces)
  for (int edgeId : edgesToDelete) {
    deleteEdge(edgeId);
  }

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

// ---------------------------------------------------------------------------
// mergeNodes
// ---------------------------------------------------------------------------

bool Topology::mergeNodes(int keepId, int removeId) {
  if (keepId == removeId)
    return false;

  TopoNode *keepNode = getNode(keepId);
  TopoNode *removeNode = getNode(removeId);
  if (!keepNode || !removeNode)
    return false;

  std::unordered_set<int> edgesToDelete;

  // 1. Rewire all edges referencing removeNode → keepNode
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

    if (modified && edge->getStartNode() == edge->getEndNode()) {
      edgesToDelete.insert(pair.first);
    }
  }

  // 2. Update half-edge origins
  for (auto &edgePair : _edges) {
    TopoEdge *e = edgePair.second;
    if (e->getForwardHalfEdge() &&
        e->getForwardHalfEdge()->origin == removeNode) {
      e->getForwardHalfEdge()->origin = keepNode;
    }
    if (e->getBackwardHalfEdge() &&
        e->getBackwardHalfEdge()->origin == removeNode) {
      e->getBackwardHalfEdge()->origin = keepNode;
    }
  }

  // 3. Find and mark duplicate edges
  std::map<std::pair<int, int>, TopoEdge *> seenEdgesMap;
  std::unordered_set<int> affectedFaceIds;

  for (auto &pair : _edges) {
    if (edgesToDelete.count(pair.first))
      continue;

    TopoEdge *edge = pair.second;
    int n1 = edge->getStartNode()->getID();
    int n2 = edge->getEndNode()->getID();
    auto normalizedPair = std::make_pair(std::min(n1, n2), std::max(n1, n2));

    if (seenEdgesMap.count(normalizedPair) > 0) {
      TopoEdge *keeper = seenEdgesMap[normalizedPair];
      TopoEdge *duplicate = edge;

      // Transfer group membership from duplicate to keeper
      for (auto &groupPair : _edgeGroups) {
        auto &group = groupPair.second;
        auto &edges = group->edges;
        bool dupInGroup =
            std::find(edges.begin(), edges.end(), duplicate) != edges.end();
        if (dupInGroup) {
          // Add keeper if not present
          if (std::find(edges.begin(), edges.end(), keeper) == edges.end()) {
            edges.push_back(keeper);
          }
        }
      }

      // Transfer group membership from duplicate to keeper
      for (auto &groupPair : _edgeGroups) {
        auto &group = groupPair.second;
        auto &edges = group->edges;
        bool dupInGroup =
            std::find(edges.begin(), edges.end(), duplicate) != edges.end();
        if (dupInGroup) {
          // Add keeper if not present
          if (std::find(edges.begin(), edges.end(), keeper) == edges.end()) {
            edges.push_back(keeper);
          }
        }
      }

      // Replace duplicate with keeper only in faces that reference the
      // duplicate, using DCEL for O(1) lookup
      TopoHalfEdge *he1 = duplicate->getForwardHalfEdge();
      TopoHalfEdge *he2 = duplicate->getBackwardHalfEdge();
      if (he1 && he1->face) {
        he1->face->replaceEdge(duplicate, keeper);
        affectedFaceIds.insert(he1->face->getID());
      }
      if (he2 && he2->face) {
        he2->face->replaceEdge(duplicate, keeper);
        affectedFaceIds.insert(he2->face->getID());
      }

      edgesToDelete.insert(pair.first);
    } else {
      seenEdgesMap[normalizedPair] = edge;
    }
  }

  // 4. Find degenerate faces:
  //    - Does NOT have exactly 4 unique surviving edges (Strict Quad Domain),
  //    OR
  //    - References any edge that will be deleted (self-loops, duplicates), OR
  //    - Has duplicate references to the same surviving edge (loop corruption)
  std::vector<int> facesToDelete;
  for (auto &facePair : _faces) {
    const auto &faceEdges = facePair.second->getEdges();
    bool refsDeletedEdge = false;
    std::set<TopoEdge *> uniqueEdges;
    for (auto *e : faceEdges) {
      if (edgesToDelete.count(e->getID())) {
        refsDeletedEdge = true;
        break;
      }
      uniqueEdges.insert(e);
    }
    // Strict Quad Domain: Must have exactly 4 unique edges.
    // Also, must not have internal duplicates (faceEdges.size() ==
    // uniqueEdges.size())
    if (refsDeletedEdge || uniqueEdges.size() != 4 ||
        faceEdges.size() != uniqueEdges.size()) {
      facesToDelete.push_back(facePair.first);
    }
  }

  // Phase 4: Delete all old edges
  // ... (bypass deleteEdge cascade since faces are handled above)
  for (int edgeId : edgesToDelete) {
    TopoEdge *edge = getEdge(edgeId);
    if (!edge)
      continue;

    // Remove from groups
    for (auto &groupPair : _edgeGroups) {
      auto &edges = groupPair.second->edges;
      edges.erase(std::remove(edges.begin(), edges.end(), edge), edges.end());
    }

    // Remove from lookup
    int n1 = edge->getStartNode()->getID();
    int n2 = edge->getEndNode()->getID();
    auto key = std::make_pair(std::min(n1, n2), std::max(n1, n2));
    _edgeLookup.erase(key);

    // Clean up chord registration
    removeEdgeFromChord(edge);

    // Delete half-edges
    if (edge->getForwardHalfEdge())
      deleteHalfEdge(edge->getForwardHalfEdge());
    if (edge->getBackwardHalfEdge())
      deleteHalfEdge(edge->getBackwardHalfEdge());

    _edges.erase(edgeId);
    _edgePool.deallocate(edge);
  }

  // 7. Rebuild half-edges only for affected surviving faces
  for (int faceId : affectedFaceIds) {
    if (_faces.count(faceId)) {
      rebuildFaceHalfEdges(faceId);
    }
  }

  // 8. Rebuild edge lookup (endpoints changed)
  rebuildEdgeLookup();

  // 9. Remove the merged-away node (direct cleanup, no cascade needed)
  _nodes.erase(removeId);
  _nodePool.deallocate(removeNode);

  return true;
}

// ---------------------------------------------------------------------------
// Edge Management
// ---------------------------------------------------------------------------

TopoEdge *Topology::createEdge(TopoNode *start, TopoNode *end) {
  int id = generateID();
  return createEdgeWithID(id, start, end);
}

TopoEdge *Topology::createEdgeWithID(int id, TopoNode *start, TopoNode *end) {
  if (!start || !end)
    return nullptr;

  auto it = _edges.find(id);
  if (it != _edges.end()) {
    // Edge exists, verify/update nodes if possible, but typically IDs are
    // stable. For now, just return existing to maintain idempotency.
    return it->second;
  }

  TopoEdge *edge = _edgePool.allocate(id, start, end);
  _edges[id] = edge;

  // Create half-edges
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

  // Update optimized lookup
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

  // 1. Find faces via DCEL half-edge face pointers (O(1))
  std::vector<int> facesToDelete;
  TopoHalfEdge *he1 = edge->getForwardHalfEdge();
  TopoHalfEdge *he2 = edge->getBackwardHalfEdge();
  if (he1 && he1->face) {
    facesToDelete.push_back(he1->face->getID());
  }
  if (he2 && he2->face &&
      (facesToDelete.empty() || he2->face->getID() != facesToDelete[0])) {
    facesToDelete.push_back(he2->face->getID());
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

  // 4. Remove from optimized lookup
  int n1 = edge->getStartNode()->getID();
  int n2 = edge->getEndNode()->getID();
  auto key = std::make_pair(std::min(n1, n2), std::max(n1, n2));
  _edgeLookup.erase(key);

  // 5. Clean up chord registration
  removeEdgeFromChord(edge);

  // 6. Delete the half-edges
  if (edge->getForwardHalfEdge())
    deleteHalfEdge(edge->getForwardHalfEdge());
  if (edge->getBackwardHalfEdge())
    deleteHalfEdge(edge->getBackwardHalfEdge());

  // 7. Delete the edge
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

// ---------------------------------------------------------------------------
// Edge Dimensions
// ---------------------------------------------------------------------------

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

void Topology::propagateSubdivisions(int edgeId, int subdivisions) {
  TopoEdge *startEdge = getEdge(edgeId);
  if (!startEdge)
    return;

  std::unordered_set<int> visited;
  std::vector<int> queue;
  queue.push_back(edgeId);
  visited.insert(edgeId);

  size_t head = 0;
  while (head < queue.size()) {
    int currId = queue[head++];
    TopoEdge *currEdge = getEdge(currId);
    if (!currEdge)
      continue;

    currEdge->setSubdivisions(subdivisions);

    // Find "parallel" edges via adjacent faces
    std::vector<TopoHalfEdge *> hes = {currEdge->getForwardHalfEdge(),
                                       currEdge->getBackwardHalfEdge()};

    for (TopoHalfEdge *he : hes) {
      if (!he || !he->face)
        continue;

      // Only propagate through QUAD faces
      auto faceEdges = he->face->getEdges();
      if (faceEdges.size() == 4) {
        // Find the opposite edge in the quad
        TopoHalfEdge *curr = he;
        // next -> next is the opposite half-edge in a quad
        if (curr->next && curr->next->next) {
          TopoEdge *oppositeEdge = curr->next->next->parentEdge;
          if (oppositeEdge &&
              visited.find(oppositeEdge->getID()) == visited.end()) {
            visited.insert(oppositeEdge->getID());
            queue.push_back(oppositeEdge->getID());
          }
        }
      }
    }
  }
}

TopoNode *Topology::splitEdge(int edgeId, double t) {
  TopoEdge *startEdge = getEdge(edgeId);
  if (!startEdge) {
    qDebug() << "splitEdge: Edge" << edgeId << "not found";
    return nullptr;
  }

  qDebug() << "splitEdge: Starting split of edge" << edgeId << "at t =" << t;

  // Write to file to confirm code is running
  std::ofstream logFile("/tmp/edge_split_log.txt", std::ios::app);
  logFile << "=== SPLIT EDGE CALLED ===" << std::endl;
  logFile << "Edge ID: " << edgeId << ", t: " << t << std::endl;
  // Keep logFile open for logging throughout the function

  // Find all parallel edges that need to be split together
  // using the same propagation logic as propagateSubdivisions
  std::unordered_set<int> visited;
  std::vector<int> edgesToSplit;
  std::vector<int> queue;

  queue.push_back(edgeId);
  visited.insert(edgeId);

  size_t head = 0;
  while (head < queue.size()) {
    int currId = queue[head++];
    TopoEdge *currEdge = getEdge(currId);
    if (!currEdge)
      continue;

    edgesToSplit.push_back(currId);

    // Find parallel edges via adjacent quad faces
    std::vector<TopoHalfEdge *> hes = {currEdge->getForwardHalfEdge(),
                                       currEdge->getBackwardHalfEdge()};

    qDebug() << "  Edge" << currId
             << "- Forward HE:" << (hes[0] ? "exists" : "null")
             << "Backward HE:" << (hes[1] ? "exists" : "null");

    for (TopoHalfEdge *he : hes) {
      if (!he) {
        qDebug() << "    Half-edge is null, skipping";
        continue;
      }
      if (!he->face) {
        qDebug() << "    Half-edge has no face, skipping";
        continue;
      }

      // Only propagate through QUAD faces (structured mesh assumption)
      auto faceEdges = he->face->getEdges();
      qDebug() << "    Face" << he->face->getID() << "has" << faceEdges.size()
               << "edges";

      if (faceEdges.size() == 4) {
        // Find the opposite edge in the quad
        TopoHalfEdge *curr = he;
        // In a quad: next->next is the opposite half-edge
        qDebug() << "      Checking for opposite edge: next ="
                 << (curr->next ? "exists" : "null");
        if (curr->next) {
          qDebug() << "        next->next ="
                   << (curr->next->next ? "exists" : "null");
        }

        if (curr->next && curr->next->next) {
          TopoEdge *oppositeEdge = curr->next->next->parentEdge;
          if (oppositeEdge) {
            qDebug() << "        Found opposite edge" << oppositeEdge->getID();
            if (visited.find(oppositeEdge->getID()) == visited.end()) {
              qDebug() << "        Adding opposite edge"
                       << oppositeEdge->getID() << "to queue";
              visited.insert(oppositeEdge->getID());
              queue.push_back(oppositeEdge->getID());
            } else {
              qDebug() << "        Opposite edge already visited";
            }
          }
        }
      }
    }
  }

  qDebug() << "splitEdge: Found" << edgesToSplit.size()
           << "parallel edges to split";

  logFile << "Found " << edgesToSplit.size() << " parallel edges to split"
          << std::endl;

  // Track all edges and their split results for batch processing
  struct EdgeSplitData {
    int oldEdgeId;
    TopoEdge *oldEdge;
    TopoNode *newNode;
    TopoEdge *newEdge1;
    TopoEdge *newEdge2;
    std::vector<int> affectedFaceIds;
  };
  std::vector<EdgeSplitData> splitData;
  splitData.reserve(edgesToSplit.size());

  // Phase 1: Create all new nodes and edges
  for (int eid : edgesToSplit) {
    TopoEdge *oldEdge = getEdge(eid);
    if (!oldEdge)
      continue;

    EdgeSplitData data;
    data.oldEdgeId = eid;
    data.oldEdge = oldEdge;

    TopoNode *start = oldEdge->getStartNode();
    TopoNode *end = oldEdge->getEndNode();

    // Calculate new node position at parameter t
    gp_Pnt p1 = start->getPosition();
    gp_Pnt p2 = end->getPosition();
    gp_Vec v(p1, p2);
    gp_Pnt pNew = p1.Translated(v * t);

    logFile << "    Split Edge " << eid << " (Nodes " << start->getID() << "->"
            << end->getID() << ") t=" << t << std::endl;
    logFile << "      p1: " << p1.X() << ", " << p1.Y() << ", " << p1.Z()
            << std::endl;
    logFile << "      p2: " << p2.X() << ", " << p2.Y() << ", " << p2.Z()
            << std::endl;
    logFile << "      pNew: " << pNew.X() << ", " << pNew.Y() << ", "
            << pNew.Z() << std::endl;

    // Create new node
    data.newNode = createNode(pNew);
    // Transfer constraint if any
    data.newNode->setConstraintTargetID(start->getConstraintTargetID());

    // Create two new edges
    data.newEdge1 = createEdge(start, data.newNode);
    data.newEdge2 = createEdge(data.newNode, end);

    // Inherit subdivisions
    int subdivs = oldEdge->getSubdivisions();
    data.newEdge1->setSubdivisions(subdivs);
    data.newEdge2->setSubdivisions(subdivs);

    // Find affected faces
    TopoHalfEdge *he1 = oldEdge->getForwardHalfEdge();
    TopoHalfEdge *he2 = oldEdge->getBackwardHalfEdge();

    if (he1 && he1->face) {
      data.affectedFaceIds.push_back(he1->face->getID());
    }
    if (he2 && he2->face) {
      data.affectedFaceIds.push_back(he2->face->getID());
    }

    splitData.push_back(data);
  }

  // Phase 2: Subdivide faces (create connecting edges and new faces)
  std::set<int> facesToDelete;
  std::map<int, std::vector<int>> faceGroupMap; // old face ID -> group IDs

  // Track which faces have which edge groups
  for (const auto &[fid, faceGroup] : _faceGroups) {
    for (TopoFace *face : faceGroup->faces) {
      faceGroupMap[face->getID()].push_back(fid);
    }
  }

  // Group splits by face
  std::map<int, std::vector<int>> faceToSplitIndices;
  for (size_t i = 0; i < splitData.size(); i++) {
    for (int fid : splitData[i].affectedFaceIds) {
      faceToSplitIndices[fid].push_back(i);
    }
  }

  logFile << "Processing " << faceToSplitIndices.size() << " faces"
          << std::endl;

  // Process each affected face
  for (const auto &[fid, splitIndices] : faceToSplitIndices) {
    TopoFace *face = getFace(fid);
    if (!face) {
      logFile << "  Face " << fid << " not found!" << std::endl;
      continue;
    }

    const auto &faceEdges = face->getEdges();
    logFile << "  Face " << fid << " has " << faceEdges.size() << " edges and "
            << splitIndices.size() << " splits" << std::endl;

    // Only handle quad faces with 2 splits (parallel edges)
    if (faceEdges.size() == 4 && splitIndices.size() == 2) {
      logFile << "  -> Subdividing quad face " << fid << std::endl;

      const auto &split0 = splitData[splitIndices[0]];
      const auto &split1 = splitData[splitIndices[1]];

      // Create connecting edge
      TopoEdge *connectingEdge = createEdge(split0.newNode, split1.newNode);
      logFile << "  Created connecting edge " << connectingEdge->getID()
              << std::endl;

      // Find perpendicular edges (not being split)
      std::vector<TopoEdge *> perpEdges;
      for (TopoEdge *e : faceEdges) {
        if (e != split0.oldEdge && e != split1.oldEdge) {
          perpEdges.push_back(e);
        }
      }

      logFile << "  Found " << perpEdges.size() << " perpendicular edges"
              << std::endl;

      // Robustly identify the two loops for the new faces
      // We have:
      // - split0 (with newEdge1, newEdge2, newNode)
      // - split1 (with newEdge1, newEdge2, newNode)
      // - connectingEdge (between split0.newNode and split1.newNode)
      // - 2 perpendicular edges from the original face (perpEdges)

      // Helper to find the perp edge connected to a specific node
      auto findConnectedPerp = [&](TopoNode *node) -> TopoEdge * {
        for (TopoEdge *e : perpEdges) {
          if (e->getStartNode() == node || e->getEndNode() == node) {
            return e;
          }
        }
        return nullptr;
      };

      // Helper to find which new edge of a split connects to a specific node
      auto findConnectedSplitEdge = [&](const EdgeSplitData &split,
                                        TopoNode *node) -> TopoEdge * {
        if (split.newEdge1->getStartNode() == node ||
            split.newEdge1->getEndNode() == node)
          return split.newEdge1;
        if (split.newEdge2->getStartNode() == node ||
            split.newEdge2->getEndNode() == node)
          return split.newEdge2;
        return nullptr;
      };

      // --- FACE 1 Construction ---
      // Trace from split0.StartNode
      TopoNode *nodeA_Start = split0.oldEdge->getStartNode();
      TopoEdge *edgeA_Seg =
          split0.newEdge1; // The segment connected to StartNode usually
      // Verify geometry just in case (e.g. if newEdge1 isn't strictly
      // start->new)
      if (edgeA_Seg->getStartNode() != nodeA_Start &&
          edgeA_Seg->getEndNode() != nodeA_Start) {
        edgeA_Seg = split0.newEdge2;
      }

      TopoEdge *perp1 = findConnectedPerp(nodeA_Start);
      if (!perp1) {
        logFile << "  ERROR: Could not find perp edge connected to "
                << nodeA_Start->getID() << std::endl;
        continue;
      }

      TopoNode *nodeB_Start = (perp1->getStartNode() == nodeA_Start)
                                  ? perp1->getEndNode()
                                  : perp1->getStartNode();
      TopoEdge *edgeB_Seg = findConnectedSplitEdge(split1, nodeB_Start);
      if (!edgeB_Seg) {
        logFile << "  ERROR: Could not find split1 edge connected to "
                << nodeB_Start->getID() << std::endl;
        continue;
      }

      // --- FACE 2 Construction ---
      // Trace from split0.EndNode
      TopoNode *nodeA_End = split0.oldEdge->getEndNode();
      TopoEdge *edgeA_Seg2 =
          (edgeA_Seg == split0.newEdge1) ? split0.newEdge2 : split0.newEdge1;

      TopoEdge *perp2 = findConnectedPerp(nodeA_End);
      if (!perp2) {
        logFile << "  ERROR: Could not find perp edge connected to "
                << nodeA_End->getID() << std::endl;
        // cleanup face1?
        continue;
      }

      TopoNode *nodeB_End = (perp2->getStartNode() == nodeA_End)
                                ? perp2->getEndNode()
                                : perp2->getStartNode();
      TopoEdge *edgeB_Seg2 = findConnectedSplitEdge(split1, nodeB_End);
      if (!edgeB_Seg2) {
        logFile << "  ERROR: Could not find split1 edge connected to "
                << nodeB_End->getID() << std::endl;
        continue;
      }

      std::vector<TopoEdge *> face1Edges = {edgeA_Seg, connectingEdge,
                                            edgeB_Seg, perp1};
      std::vector<TopoEdge *> face2Edges = {edgeA_Seg2, perp2, edgeB_Seg2,
                                            connectingEdge};

      // CRITICAL FIX: Delete the old face NOW to unbind its half-edges from the
      // perpendicular edges. If we don't do this, createFace() will fail
      // because the edges are still "owned" by the old face. We have already
      // extracted all necessary data (perpEdges, faceGroupMap).
      std::vector<int> currentGroups;
      if (faceGroupMap.count(fid)) {
        currentGroups = faceGroupMap[fid];
      }

      logFile << "  Deleting old face " << fid
              << " to release edge ownership..." << std::endl;
      deleteFace(fid);

      TopoFace *newFace1 = createFace(face1Edges);
      TopoFace *newFace2 = createFace(face2Edges);

      if (newFace1 && newFace2) {
        logFile << "  Created new faces " << newFace1->getID() << " and "
                << newFace2->getID() << std::endl;

        // Preserve face groups
        for (int groupId : currentGroups) {
          addFaceToGroup(groupId, newFace1);
          addFaceToGroup(groupId, newFace2);
          logFile << "  Added new faces to group " << groupId << std::endl;
        }

        // facesToDelete.insert(fid); // No longer needed, already deleted
      } else {
        logFile << "  ERROR: Failed to create faces" << std::endl;
      }
    }
  }

  // Delete old faces
  for (int fid : facesToDelete) {
    logFile << "Deleting old face " << fid << std::endl;
    deleteFace(fid);
  }

  // Phase 5: Delete all old edges
  // CRITICAL: Ensure faces are gone first (done above).
  for (const auto &data : splitData) {
    logFile << "Deleting old edge " << data.oldEdgeId << std::endl;
    // Verify edge exists before delete
    if (getEdge(data.oldEdgeId)) {
      deleteEdge(data.oldEdgeId);
    } else {
      logFile << "  Warning: Edge " << data.oldEdgeId << " already deleted?"
              << std::endl;
    }
  }

  // Phase 4: Update edge groups
  for (const auto &data : splitData) {
    for (auto &groupPair : _edgeGroups) {
      auto &group = groupPair.second;
      auto it =
          std::find(group->edges.begin(), group->edges.end(), data.oldEdge);
      if (it != group->edges.end()) {
        it = group->edges.erase(it);
        it = group->edges.insert(it, data.newEdge1);
        group->edges.insert(it + 1, data.newEdge2);
      }
    }
  }

  qDebug() << "splitEdge: Successfully split" << splitData.size() << "edges";
  qDebug() << "splitEdge: Total edges now:" << _edges.size()
           << "Total nodes now:" << _nodes.size();

  logFile << "=== SPLIT COMPLETE ===" << std::endl;
  logFile << "Total edges: " << _edges.size()
          << ", Total nodes: " << _nodes.size() << std::endl;
  logFile.close();

  // Return the new node created from the initial edge split
  return splitData.empty() ? nullptr : splitData[0].newNode;
}

// ---------------------------------------------------------------------------
// Face Management
// ---------------------------------------------------------------------------

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

  auto loopHEs = buildHalfEdgeLoop(face, edges);
  if (!loopHEs.empty()) {
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
  TopoFace *face = getFace(id);
  if (!face)
    return;

  // Reset half-edges belonging to this face (don't delete them, edges own
  // them)
  resetHalfEdgeLoop(face->getBoundary());

  // Remove from face groups
  for (auto &groupPair : _faceGroups) {
    auto &faces = groupPair.second->faces;
    faces.erase(std::remove(faces.begin(), faces.end(), face), faces.end());
  }

  _faces.erase(id);
  _facePool.deallocate(face);
}

void Topology::rebuildFaceHalfEdges(int faceId) {
  TopoFace *face = getFace(faceId);
  if (!face)
    return;

  // 1. Reset existing half-edge loop
  resetHalfEdgeLoop(face->getBoundary());
  face->setBoundary(nullptr);

  // 2. Rebuild from edges vector
  std::vector<TopoEdge *> edges = face->getEdges();
  if (edges.empty())
    return;

  auto loopHEs = buildHalfEdgeLoop(face, edges);
  if (!loopHEs.empty()) {
    face->setBoundary(loopHEs[0]);
  }
}

const std::map<int, TopoFace *> &Topology::getFaces() const { return _faces; }

// ---------------------------------------------------------------------------
// Half-Edge & Chord Management
// ---------------------------------------------------------------------------

TopoHalfEdge *Topology::createHalfEdge() { return _halfEdgePool.allocate(); }

void Topology::deleteHalfEdge(TopoHalfEdge *he) {
  _halfEdgePool.deallocate(he);
}

DimensionChord *Topology::createChord(int segments) {
  DimensionChord *chord = _chordPool.allocate();
  chord->segments = segments;
  return chord;
}

void Topology::deleteChord(DimensionChord *chord) {
  _chordPool.deallocate(chord);
}

// ---------------------------------------------------------------------------
// Group Management
// ---------------------------------------------------------------------------

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
  if (it != _faceGroups.end()) {
    return it->second.get();
  }
  return nullptr;
}

std::string Topology::getFaceGeometryID(int faceId) const {
  TopoFace *face = getFace(faceId);
  if (!face)
    return "";

  for (const auto &groupPair : _faceGroups) {
    const auto &group = groupPair.second;
    for (TopoFace *f : group->faces) {
      if (f == face) {
        return group->geometryID;
      }
    }
  }
  return "";
}

void Topology::clearGroups() {
  _edgeGroups.clear();
  _faceGroups.clear();
}

// ---------------------------------------------------------------------------
// Serialization
// ---------------------------------------------------------------------------

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

  // Dimension Chords — collect unique chords from edges and assign IDs
  std::map<DimensionChord *, int> chordIds;
  int nextChordId = 1;
  for (const auto &[id, edge] : _edges) {
    DimensionChord *chord = edge->getChord();
    if (chord && chordIds.find(chord) == chordIds.end()) {
      chordIds[chord] = nextChordId++;
    }
  }

  QJsonObject chordsObj;
  for (const auto &[chord, chordId] : chordIds) {
    QJsonObject c;
    c["segments"] = chord->segments;
    c["user_locked"] = chord->userLocked;
    chordsObj[QString::number(chordId)] = c;
  }
  root["dimension_chords"] = chordsObj;

  // Edges
  QJsonObject edgesObj;
  for (const auto &[id, edge] : _edges) {
    QJsonObject e;
    QJsonArray nodeIds;
    nodeIds.append(edge->getStartNode()->getID());
    nodeIds.append(edge->getEndNode()->getID());
    e["node_ids"] = nodeIds;
    e["subdivisions"] = edge->getSubdivisions();
    if (edge->getChord() && chordIds.count(edge->getChord())) {
      e["chord_id"] = chordIds[edge->getChord()];
    }
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

  // Reset pools (now properly calls destructors for live objects)
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

  // 2. Dimension Chords
  std::map<int, DimensionChord *> chordMap;
  if (json.contains("dimension_chords")) {
    QJsonObject chordsObj = json["dimension_chords"].toObject();
    for (auto it = chordsObj.begin(); it != chordsObj.end(); ++it) {
      int chordId = it.key().toInt();
      QJsonObject c = it.value().toObject();
      DimensionChord *chord = createChord(c["segments"].toInt(11));
      chord->userLocked = c["user_locked"].toBool(false);
      chordMap[chordId] = chord;
    }
  }

  // 3. Edges
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

        // Restore chord assignment
        if (e.contains("chord_id")) {
          int chordId = e["chord_id"].toInt();
          auto cit = chordMap.find(chordId);
          if (cit != chordMap.end()) {
            edge->setChord(cit->second);
            cit->second->registeredEdges.push_back(edge);
          }
        }
      }
    }
  }

  // 4. Faces
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

  // 5. Edge Groups
  if (json.contains("topo_edge_groups")) {
    QJsonObject groupsObj = json["topo_edge_groups"].toObject();
    for (auto it = groupsObj.begin(); it != groupsObj.end(); ++it) {
      int id = it.key().toInt();
      QJsonObject g = it.value().toObject();
      TopoEdgeGroup *group =
          createEdgeGroup(g["geometry_id"].toString().toStdString());

      if (group->id != id) {
        auto itGroup = _edgeGroups.find(group->id);
        if (itGroup != _edgeGroups.end()) {
          std::unique_ptr<TopoEdgeGroup> ptr = std::move(itGroup->second);
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

  // 6. Face Groups
  if (json.contains("topo_face_groups")) {
    QJsonObject groupsObj = json["topo_face_groups"].toObject();
    for (auto it = groupsObj.begin(); it != groupsObj.end(); ++it) {
      int id = it.key().toInt();
      QJsonObject g = it.value().toObject();
      TopoFaceGroup *group =
          createFaceGroup(g["geometry_id"].toString().toStdString());

      if (group->id != id) {
        auto itGroup = _faceGroups.find(group->id);
        if (itGroup != _faceGroups.end()) {
          std::unique_ptr<TopoFaceGroup> ptr = std::move(itGroup->second);
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
