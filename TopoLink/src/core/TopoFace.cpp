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

void TopoFace::removeEdge(TopoEdge *edge) {
  _edges.erase(std::remove(_edges.begin(), _edges.end(), edge), _edges.end());
}

TopoHalfEdge *TopoFace::getBoundary() const { return _boundary; }

void TopoFace::setBoundary(TopoHalfEdge *he) { _boundary = he; }

void TopoFace::setMetadata(const std::string &key, const std::string &value) {
  _metadata[key] = value;
}

std::string TopoFace::getMetadata(const std::string &key) const {
  auto it = _metadata.find(key);
  if (it != _metadata.end()) {
    return it->second;
  }
  return "";
}

bool TopoFace::hasMetadata(const std::string &key) const {
  return _metadata.find(key) != _metadata.end();
}
