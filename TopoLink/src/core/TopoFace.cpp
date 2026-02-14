#include "TopoFace.h"

TopoFace::TopoFace(int id, const std::vector<TopoEdge *> &edges)
    : _id(id), _edges(edges), _boundary(nullptr) {}

TopoFace::~TopoFace() {}

int TopoFace::getID() const { return _id; }

const std::vector<TopoEdge *> &TopoFace::getEdges() const { return _edges; }

void TopoFace::replaceEdge(TopoEdge *oldEdge, TopoEdge *newEdge) {
  for (auto &edge : _edges) {
    if (edge == oldEdge) {
      edge = newEdge;
    }
  }
}

void TopoFace::splitEdge(TopoEdge *oldEdge, TopoEdge *newEdge1,
                         TopoEdge *newEdge2) {
  for (auto it = _edges.begin(); it != _edges.end(); ++it) {
    if (*it == oldEdge) {
      // Find the direction of the old edge in the face's half-edge loop
      // to determine the order of newEdge1 and newEdge2.
      // However, our model simply keeps an unordered (or rather, ordered by
      // creation) vector of edges, and rebuilds the DCEL. So we just need
      // to replace one edge with two.
      it = _edges.erase(it);
      it = _edges.insert(it, newEdge1);
      _edges.insert(it + 1, newEdge2);
      break;
    }
  }
}

void TopoFace::removeEdge(TopoEdge *edge) {
  _edges.erase(std::remove(_edges.begin(), _edges.end(), edge), _edges.end());
}

TopoHalfEdge *TopoFace::getBoundary() const { return _boundary; }

void TopoFace::setBoundary(TopoHalfEdge *he) { _boundary = he; }
