#ifndef DIMENSIONCHORD_H
#define DIMENSIONCHORD_H

#include <vector>

// Forward declaration
class TopoEdge;

struct DimensionChord {
  int segments = 11;       // Default divisions
  bool userLocked = false; // True if user explicitly typed a number
  std::vector<TopoEdge *>
      registeredEdges; // Back-pointer for debug/highlighting

  // Helper to merge another chord into this one
  // In a real implementation, this would update all edges pointing to 'other'
  // to point to 'this', and then delete 'other'.
  // void merge(DimensionChord* other);
};

#endif // DIMENSIONCHORD_H
