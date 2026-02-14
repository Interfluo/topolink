#ifndef TOPOFACE_H
#define TOPOFACE_H

#include "MetadataHolder.h"
#include "TopoEdge.h"
#include <vector>

// Forward declaration
struct TopoHalfEdge;

class TopoFace : public MetadataHolder {
public:
  TopoFace(int id, const std::vector<TopoEdge *> &edges);
  ~TopoFace();

  int getID() const;
  const std::vector<TopoEdge *> &getEdges() const;

  void replaceEdge(TopoEdge *oldEdge, TopoEdge *newEdge);
  void splitEdge(TopoEdge *oldEdge, TopoEdge *newEdge1, TopoEdge *newEdge2);
  void removeEdge(TopoEdge *edge);

  // Half-Edge Access
  TopoHalfEdge *getBoundary() const;
  void setBoundary(TopoHalfEdge *he);

private:
  int _id;
  std::vector<TopoEdge *> _edges;
  TopoHalfEdge *_boundary; // One of the half-edges surrounding the face
};

#endif // TOPOFACE_H
