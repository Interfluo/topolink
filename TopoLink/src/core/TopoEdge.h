#ifndef TOPOEDGE_H
#define TOPOEDGE_H

#include <map>
#include <string>

class TopoNode;
struct TopoHalfEdge;
struct DimensionChord;

class TopoEdge {
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

  // Structured Grid / Parallelism
  TopoEdge *getParallelNext() const;
  TopoEdge *getParallelPrev() const;
  void setParallelNext(TopoEdge *next);
  void setParallelPrev(TopoEdge *prev);

  DimensionChord *getChord() const;
  void setChord(DimensionChord *chord);

  // Subdivisions (Delegated to Chord)
  int getSubdivisions() const;
  void setSubdivisions(int subdivisions);

  // Metadata
  void setMetadata(const std::string &key, const std::string &value);
  std::string getMetadata(const std::string &key) const;
  bool hasMetadata(const std::string &key) const;

private:
  int _id;

  TopoNode *_start;
  TopoNode *_end;

  // Half-Edges
  TopoHalfEdge *_he1; // Forward (start -> end)
  TopoHalfEdge *_he2; // Backward (end -> start)

  // Parallel Linking for Structured Grid
  TopoEdge *_parallelNext;
  TopoEdge *_parallelPrev;
  DimensionChord *_chord;
  int _subdivisions;

  std::map<std::string, std::string> _metadata;
};

#endif // TOPOEDGE_H
