#ifndef TOPOEDGE_H
#define TOPOEDGE_H

#include "MetadataHolder.h"

class TopoNode;
struct TopoHalfEdge;
struct DimensionChord;

class TopoEdge : public MetadataHolder {
public:
  TopoEdge(int id, TopoNode *start, TopoNode *end);
  ~TopoEdge();

  int getID() const;
  TopoNode *getStartNode() const;
  TopoNode *getEndNode() const;

  // Node setters (for merge operations)
  void setStartNode(TopoNode *node);
  void setEndNode(TopoNode *node);

  // Half-Edge Access
  TopoHalfEdge *getForwardHalfEdge() const;  // he1
  TopoHalfEdge *getBackwardHalfEdge() const; // he2
  void setHalfEdges(TopoHalfEdge *he1, TopoHalfEdge *he2);

  DimensionChord *getChord() const;
  void setChord(DimensionChord *chord);

  // Subdivisions (Delegated to Chord)
  int getSubdivisions() const;
  void setSubdivisions(int subdivisions);

private:
  int _id;

  TopoNode *_start;
  TopoNode *_end;

  // Half-Edges
  TopoHalfEdge *_he1; // Forward (start -> end)
  TopoHalfEdge *_he2; // Backward (end -> start)

  DimensionChord *_chord;
  int _subdivisions;
};

#endif // TOPOEDGE_H
