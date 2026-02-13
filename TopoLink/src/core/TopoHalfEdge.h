#ifndef TOPOHALFEDGE_H
#define TOPOHALFEDGE_H

// Forward declarations
class TopoNode;
class TopoEdge;
class TopoFace;

struct TopoHalfEdge {
  TopoNode *origin = nullptr;   // The vertex this half-edge originates from
  TopoHalfEdge *twin = nullptr; // The opposite half-edge
  TopoHalfEdge *next = nullptr; // The next half-edge in the loop
  TopoHalfEdge *prev = nullptr; // The previous half-edge in the loop
  TopoFace *face = nullptr; // The face this half-edge belongs to (loops CCW)
  TopoEdge *parentEdge =
      nullptr; // The parent edge entity containing this half-edge
};

#endif // TOPOHALFEDGE_H
