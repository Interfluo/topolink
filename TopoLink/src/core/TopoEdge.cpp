#include "TopoEdge.h"
#include "DimensionChord.h"
#include "TopoHalfEdge.h"

TopoEdge::TopoEdge(int id, TopoNode *start, TopoNode *end)
    : _id(id), _start(start), _end(end), _he1(nullptr), _he2(nullptr),
      _chord(nullptr), _subdivisions(11) {}

TopoEdge::~TopoEdge() {}

int TopoEdge::getID() const { return _id; }

TopoNode *TopoEdge::getStartNode() const { return _start; }

TopoNode *TopoEdge::getEndNode() const { return _end; }

void TopoEdge::setStartNode(TopoNode *node) { _start = node; }

void TopoEdge::setEndNode(TopoNode *node) { _end = node; }

int TopoEdge::getSubdivisions() const {
  if (_chord)
    return _chord->segments;
  return _subdivisions;
}

void TopoEdge::setSubdivisions(int subdivisions) {
  _subdivisions = subdivisions;
  if (_chord) {
    _chord->segments = subdivisions;
    _chord->userLocked = true;
  }
}

TopoHalfEdge *TopoEdge::getForwardHalfEdge() const { return _he1; }
TopoHalfEdge *TopoEdge::getBackwardHalfEdge() const { return _he2; }

void TopoEdge::setHalfEdges(TopoHalfEdge *he1, TopoHalfEdge *he2) {
  _he1 = he1;
  _he2 = he2;
  if (_he1 && _he2) {
    _he1->twin = _he2;
    _he2->twin = _he1;
  }
}

DimensionChord *TopoEdge::getChord() const { return _chord; }
void TopoEdge::setChord(DimensionChord *chord) { _chord = chord; }

