# Edge Splitting with Parallel Propagation

## Overview

The edge splitting functionality now properly propagates splits across parallel edges in quad-only structured meshes. This ensures topological consistency when splitting edges that are part of a quad mesh domain.

## Key Implementation Details

### Algorithm Phases

The `Topology::splitEdge()` function operates in 5 distinct phases:

#### Phase 1: Parallel Edge Discovery (Lines 608-650)
- Uses BFS traversal starting from the selected edge
- Propagates through quad faces only (structured mesh assumption)
- Finds opposite edges via half-edge connectivity: `next->next` in a quad gives the opposite edge
- Uses `visited` set to prevent infinite loops
- Builds a list of all edges that need to be split together

#### Phase 2: Node and Edge Creation (Lines 664-709)
- Creates all new nodes and edges for each edge in the split set
- Each edge split creates:
  - 1 new node at parameter `t` along the edge
  - 2 new edges connecting (start → newNode) and (newNode → end)
- Inherits properties:
  - Subdivision count from parent edge
  - Constraint target ID from start node
- Tracks affected faces for later rebuilding

#### Phase 3: Face Updates (Lines 711-726)
- Updates all affected faces to use new edges
- Calls `TopoFace::splitEdge()` to replace old edge with two new edges
- Collects unique set of faces that need half-edge rebuilding
- Defers half-edge rebuild until all faces are updated

#### Phase 4: Edge Group Updates (Lines 728-739)
- Finds each old edge in edge groups
- Replaces with two new edges in sequence
- Preserves ordering within the group

#### Phase 5: Cleanup (Lines 741-747)
- Deletes all old edges that were split
- Returns the new node created from the initial edge

## Differences from Original Implementation

### ✅ Improvements

1. **Parallel Propagation**: Now splits all parallel edges across quad domains instead of just the selected edge
2. **Infinite Loop Protection**: Uses `visited` set and BFS traversal pattern
3. **Batch Processing**: Creates all entities before updating faces, avoiding half-built states
4. **Face Rebuild Deferred**: Rebuilds half-edges only after all face edge lists are updated
5. **Topological Consistency**: All parallel edges split at same parameter `t`

### ⚠️ Potential Edge Cases to Test

#### 1. **Non-Quad Faces at Boundaries**
- If an edge is adjacent to a non-quad face, propagation stops at that boundary
- The non-quad face still gets updated but doesn't contribute to further propagation
- **Test**: Mixed quad/triangle meshes at domain boundaries

#### 2. **Edge Group Ordering**
- Edge groups maintain insertion order
- After split, two new edges are inserted where the old one was
- **Test**: Verify group ordering matches expected topology traversal

#### 3. **Branching Propagation Paths**
- In complex quad meshes, propagation can branch and rejoin
- The `visited` set prevents double-processing
- **Test**: Grid meshes where parallel edges form closed loops

#### 4. **T-Junctions**
- If one side of a quad has already been split, creating a T-junction
- The opposite edge will still split, potentially creating mismatched subdivisions
- **Consideration**: May need additional logic to detect and handle pre-existing splits

#### 5. **Constraint Propagation**
- New nodes inherit constraint from start node
- For edges with different start/end constraints, this may need refinement
- **Test**: Edges on constraint curves or surfaces

#### 6. **Half-Edge Validity After Mass Updates**
- With many edges splitting simultaneously, half-edge pointers could become stale
- The batch rebuild should handle this, but verify no dangling pointers
- **Test**: Large grid meshes with splits propagating across many quads

#### 7. **Edge Direction in Groups**
- Edge groups don't store edge direction
- After split, the two new edges might have different orientations
- **Test**: Verify meshing respects edge directions from groups

## Usage Example

```cpp
Topology topo;

// Create a 2x2 quad mesh
// ... setup nodes and edges ...

// Split a horizontal edge at 30% from start
int edgeId = horizontalEdge->getID();
TopoNode* newNode = topo.splitEdge(edgeId, 0.3);

// Result: All horizontal edges in the mesh are split at 0.3
// New nodes are created along each parallel edge
// All faces are updated with the new edges
```

## Testing Recommendations

### Unit Tests (see test_edge_split.cpp)
- ✅ Single quad split
- ✅ Multi-quad propagation
- ✅ Edge group preservation
- ✅ Subdivision inheritance
- ⚠️ Non-quad boundary handling
- ⚠️ T-junction scenarios
- ⚠️ Circular/grid propagation patterns

### Integration Tests
- Split edge in actual imported geometry
- Verify mesher handles split edges correctly
- Check smoothing behavior with split edges
- Validate edge groups in constraint system

### Visual Tests (in GUI)
- Preview should show all parallel splits
- New nodes should align properly
- Face half-edges should rebuild correctly
- No rendering artifacts or missing faces

## Known Limitations

1. **Quad-Only Assumption**: Only propagates through 4-edge faces
2. **Parameter Uniformity**: All parallel edges split at same `t` value
3. **No Undo Support**: Would need to track all created entities for rollback
4. **No Merge Detection**: Doesn't check if split creates coincident nodes

## Future Enhancements

1. **Adaptive Parameter**: Allow different `t` values based on edge length ratios
2. **Smart T-Junction Handling**: Detect existing splits and adjust accordingly
3. **Constraint-Aware Splitting**: Project new nodes onto constraint surfaces
4. **Group Direction Preservation**: Store and maintain edge orientations in groups
5. **Undo/Redo Support**: Command pattern for operation history
