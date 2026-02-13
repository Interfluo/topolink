#ifndef TOPOFACE_H
#define TOPOFACE_H

#include "TopoEdge.h"
#include <map>
#include <string>
#include <vector>

// Forward declaration
struct TopoHalfEdge;

class TopoFace {
public:
  TopoFace(int id, const std::vector<TopoEdge *> &edges);
  ~TopoFace();

  int getID() const;
  const std::vector<TopoEdge *> &getEdges() const;

  // Legacy edge management (Deprecated in favor of Half-Edge ops)
  void replaceEdge(TopoEdge *oldEdge, TopoEdge *newEdge);
  void removeEdge(TopoEdge *edge);

  // Half-Edge Access
  TopoHalfEdge *getBoundary() const;
  void setBoundary(TopoHalfEdge *he);

  // Metadata
  void setMetadata(const std::string &key, const std::string &value);
  std::string getMetadata(const std::string &key) const;
  bool hasMetadata(const std::string &key) const;

private:
  int _id;
  std::vector<TopoEdge *> _edges;
  TopoHalfEdge *_boundary; // One of the half-edges surrounding the face
  std::map<std::string, std::string> _metadata;
};

#endif // TOPOFACE_H
