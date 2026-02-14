# Edge Split Algorithm - Full Cascading Implementation

## Current Status
The basic edge splitting works:
- ✅ Splits the selected edge into 2 edges with a new node
- ✅ Finds parallel edges via BFS through quad faces
- ✅ Updates edge groups
- ❌ Does NOT create connecting edges
- ❌ Does NOT subdivide faces properly
- ❌ Does NOT cascade to adjacent faces

## Required Algorithm

When splitting edge E in a quad face F:

### Step 1: Find parallel edge in same face
- In quad ABCD with edge AB being split:
- Find opposite edge CD (via next->next in half-edge loop)
- Split CD at same parameter t
- Creates nodes N1 (on AB) and N2 (on CD)

### Step 2: Create connecting edge
- Create edge N1-N2 connecting the two new nodes

### Step 3: Subdivide the face
- Delete old face F
- Create 2 new quad faces:
  - Face F1: [A-N1, N1-N2, N2-C, C-A] (or similar based on orientation)
  - Face F2: [N1-B, B-D, D-N2, N2-N1]
- Preserve face group: add F1 and F2 to same group as F
- Preserve edge groups: add new edges to same groups as parent edges

### Step 4: Cascade to adjacent faces
- The split edge AB has 2 half-edges (forward and backward)
- Forward half-edge belongs to face F (already handled)
- Backward half-edge belongs to adjacent face F_adj
- **Recursively apply steps 1-3 to F_adj**
- Continue until no more adjacent faces

## Implementation Challenges

1. **Visited tracking**: Need to track which faces have been processed to avoid infinite loops
2. **Edge matching**: When cascading, need to find which edge in the adjacent face corresponds to the split edge
3. **Orientation**: Edge direction matters for building correct face loops
4. **Group preservation**: Must track which group each face/edge belonged to

## Pseudo-code

```cpp
void splitEdgeWithCascade(int edgeId, double t) {
  std::set<int> processedFaces;
  std::map<int, int> oldEdgeToNewEdges; // Maps old edge ID to new edge IDs

  // Start with initial edge's faces
  TopoEdge *startEdge = getEdge(edgeId);
  std::queue<std::pair<TopoFace*, TopoEdge*>> facesToProcess;

  if (startEdge->forwardHE && startEdge->forwardHE->face)
    facesToProcess.push({startEdge->forwardHE->face, startEdge});
  if (startEdge->backwardHE && startEdge->backwardHE->face)
    facesToProcess.push({startEdge->backwardHE->face, startEdge});

  while (!facesToProcess.empty()) {
    auto [face, edgeToSplit] = facesToProcess.front();
    facesToProcess.pop();

    if (processedFaces.count(face->getID())) continue;
    processedFaces.insert(face->getID());

    // Process this face
    processFaceSplit(face, edgeToSplit, t, facesToProcess, oldEdgeToNewEdges);
  }
}

void processFaceSplit(TopoFace *face, TopoEdge *edge, double t,
                     queue &facesToProcess, map &edgeMap) {
  // 1. Find opposite edge in quad
  TopoEdge *oppositeEdge = findOppositeEdge(face, edge);

  // 2. Split both edges
  auto [n1, e1a, e1b] = splitEdgeAtT(edge, t);
  auto [n2, e2a, e2b] = splitEdgeAtT(oppositeEdge, t);

  // 3. Create connecting edge
  TopoEdge *connecting = createEdge(n1, n2);

  // 4. Find perpendicular edges
  auto [perp1, perp2] = findPerpendicularEdges(face, edge, oppositeEdge);

  // 5. Create 2 new faces
  TopoFace *f1 = createFace({e1a, perp1, e2a, connecting});
  TopoFace *f2 = createFace({e1b, connecting, e2b, perp2});

  // 6. Preserve groups
  copyFaceGroups(face, {f1, f2});
  copyEdgeGroups(edge, {e1a, e1b});
  copyEdgeGroups(oppositeEdge, {e2a, e2b});

  // 7. Delete old face
  deleteFace(face);

  // 8. Queue adjacent faces for processing
  queueAdjacentFaces(e1a, e1b, e2a, e2b, facesToProcess, processedFaces);
}
```

## Recommendation

This is a complex algorithm that needs careful implementation. The current simple approach won't work for full mesh subdivision.

**Options:**
1. Implement the full cascading algorithm (complex, ~200 lines)
2. Keep current simple split (just splits one edge, user manually connects)
3. Implement quad-only subdivision without cascading (splits one face at a time)

Which would you prefer?
