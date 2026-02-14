# TopoLink Project: Roadmap and Technical Tasks

This document outlines the development roadmap and a prioritized list of technical tasks for the TopoLink project.

---

## 1. Project Roadmap

### 1.1. Core Views
-   **[Done]** **Geometry View**: CAD import/export, group creation and management, selection modes.
-   **[In Progress]** **Topology View**: Draggable nodes, edge/face creation, group editor, topology-geometry constraint logic, edge subdivision editor.
-   **[Not Started]** **Smoother View**: TFI initialization, elliptic smoother implementation, geometry projection, grid visualization, and export.

### 1.2. UI/UX Ideas
-   **Key-bindings**:
    -   Selection modes (`q`=node, `w`=edge, `e`=face).
    -   View switching (`1`=geometry, `2`=topology, `3`=smoother).
    -   Node merging (`m` key).

---

## 2. Technical Task List

This section details outstanding bugs, planned enhancements, and architectural changes.

### Phase 1: Immediate Bug Fixes

#### Task 1.1: Fix Dangling Pointers in `deleteFace`
-   **File**: `TopoLink/src/core/Topology.cpp`
-   **Function**: `Topology::deleteFace`
-   **Problem**: `deleteFace` does not remove the face pointer from `TopoFaceGroup` collections, leading to dangling pointers and instability. This is inconsistent with the behavior of `deleteEdge`.
-   **Action**: Before removing the face from the main `_faces` map, iterate through all `_faceGroups` and erase any pointers to the face being deleted. The implementation should mirror the logic in `Topology::deleteEdge`.

#### Task 1.2: Fix Node Selection Highlighting
-   **File**: `TopoLink/src/gui/OccView.cpp`
-   **Problem**: Node selection highlighting is visually broken. While the underlying selection for operations like 'move' and 'merge' works correctly, the visual feedback is wrong. Clicking one node can cause other nodes to appear highlighted, or the highlight style is not applied correctly. This seems to be a conflict between the `Prs3d_PointAspect` used to render nodes as spheres and the `AIS_InteractiveContext`'s highlighting mechanism.
-   **Action**: Investigate the interaction between `AIS_Point`, `AIS_ColoredShape`, `Prs3d_PointAspect`, and the context's highlight `Prs3d_Drawer`. The current implementation using `AIS_Point` with a local `PointAspect` still fails to highlight correctly. A robust solution needs to be found that allows the selection highlight style to override the base appearance for point-based objects.

### Phase 2: Short-Term Performance Enhancements

#### **[Done]** Task 2.1: Implement O(log N) Edge Lookup Map
-   **File**: `TopoLink/src/core/Topology.cpp`
-   **Problem**: `getEdge(start_node, end_node)` and similar functions perform a linear scan over all edges (O(N)), causing a severe performance bottleneck.
-   **Action**:
    1.  Add a new member to the `Topology` class: `std::map<std::pair<int, int>, TopoEdge*> _edgeLookup;`.
    2.  Update `addEdge` to populate this map with a pointer to the created edge, using the start and end node IDs as the key.
    3.  Update `deleteEdge` to remove the corresponding entry from the map.
    4.  Refactor `getEdge` to use the `_edgeLookup` map for O(log N) lookups.

#### **[In Progress]** Task 2.2: Implement O(1) Cached Adjacency Lists
-   **File**: `TopoLink/src/core/TopoNode.h`
-   **Problem**: Queries for edges connected to a specific node require iterating through the global edge list.
-   **Action**:
    1.  Add a new member to the `TopoNode` class: `std::vector<TopoEdge*> _connected_edges;`.
    2.  Modify the topology-building and edge-creation logic to populate this vector for each affected node.
    3.  Update edge deletion logic to remove the edge pointer from the `_connected_edges` vector of its associated nodes.
    4.  Refactor `getConnectedEdges(node)` to directly return the content of this cached vector.

### Phase 3: Long-Term Architectural Rework

#### **[In Progress]** Task 3.1: Migrate Core Topology to a Half-Edge Data Structure
-   **Objective**: Replace the current entity-based model with a half-edge (or winged-edge) data structure. This will provide O(1) constant-time complexity for all fundamental adjacency queries, eliminating performance bottlenecks and creating a robust foundation for advanced modeling operations.

-   **Implementation Plan**:

    **Step 1: Define Core Data Structures**
    -   Create new classes for the half-edge elements. The `Topology` class will retain ownership via `std::unique_ptr`, so all internal pointers should be raw.
    -   **`HE_Vertex`**:
        -   `glm::vec3 position`
        -   `HE_Edge* out` (One of the outgoing half-edges)
        -   *(Retain other data: constraints, ID, etc.)*
    -   **`HE_Edge` (The Half-Edge)**:
        -   `HE_Vertex* vert` (The vertex this edge points to)
        -   `HE_Edge* pair` (The opposite half-edge)
        -   `HE_Edge* next` (The next half-edge in the face loop)
        -   `HE_Face* face` (`nullptr` if this is a boundary edge)
        -   *(Retain other data: subdivision count, etc.)*
    -   **`HE_Face`**:
        -   `HE_Edge* edge` (One of the half-edges bounding the face)
        -   *(Retain other data: group associations, etc.)*

    **Step 2: Modify `Topology` Class Storage**
    -   Replace the existing `_nodes`, `_edges`, `_faces` maps with maps that store the new `HE_*` types:
        -   `std::map<int, std::unique_ptr<HE_Vertex>> _vertices;`
        -   `std::map<int, std::unique_ptr<HE_Edge>> _half_edges;`
        -   `std::map<int, std::unique_ptr<HE_Face>> _faces;`

    **Step 3: Implement a Half-Edge Builder**
    -   Create a builder utility responsible for constructing the half-edge graph from a simple list of vertices and face-vertex-indices. This is the most critical part of the migration.
    -   **Procedure**:
        1.  **Instantiate Vertices**: Create all `HE_Vertex` objects from the input vertex list.
        2.  **Instantiate Faces & Half-Edges**:
            -   Use a temporary `std::map<std::pair<int, int>, HE_Edge*>` to track created half-edges and find their pairs.
            -   For each face defined by vertex indices (e.g., `v1, v2, v3`):
                -   Create a new `HE_Face`.
                -   For each edge segment (e.g., `v1-v2`, `v2-v3`, `v3-v1`):
                    -   Create a new `HE_Edge` for that segment.
                    -   Assign its `face` and `vert` (end vertex) pointers.
                    -   Add it to the temporary map with key `<start_vid, end_vid>`.
        3.  **Stitch Pointers**:
            -   **`next` pointers**: Iterate through the half-edges of each new face and link them into a circular list using their `next` pointers. Set the `HE_Face::edge` pointer.
            -   **`pair` pointers**: Iterate through the temporary map. For each half-edge from `A` to `B`, find its counterpart from `B` to `A`. Link them using their `pair` pointers.
            -   **Boundary Edges**: If a counterpart from `B` to `A` is not found, it's a boundary edge. Create a new half-edge for the boundary, set its `face` to `nullptr`, and link the `pair` pointers.
            -   **`out` pointers**: For each `HE_Vertex`, assign its `out` pointer to one of the half-edges that originates from it.

    **Step 4: Refactor Public API**
    -   Deprecate and replace the old, slow query methods with new, efficient ones that leverage the half-edge graph.
    -   **Examples**:
        -   `getVertexNeighborVertices(HE_Vertex* v)`: Circulate around `v` using `v->out->pair->next`.
        -   `getFaceNeighborFaces(HE_Face* f)`: Circulate around `f`'s edges, get each `edge->pair`, and then `edge->pair->face`.
        -   All such queries should become O(k) where k is the number of adjacent elements, not O(N).

    **Step 5: Update High-Level Logic**
    -   Refactor all higher-level application code (UI, tools, etc.) that previously used the old topology API to use the new, efficient half-edge queries. This includes selection, merging, and smoothing algorithms.

### Phase 4: Topology-Geometry Constraint System

-   **Objective**: Implement the logic that governs how topology nodes are constrained by the underlying geometry groups. This system will determine the degrees of freedom for each node (fixed, sliding on an edge, or sliding on a face) based on its topological neighborhood.

-   **Implementation Plan**:

    **Step 1: Data Model for Group Linking**
    -   In the `TopoFaceGroup` class, add a member to store a reference to its associated `GeoFaceGroup` (e.g., `int geoFaceGroupId;` or `GeoFaceGroup* geoFaceGroup;`).
    -   In the `TopoEdgeGroup` class, add a similar member to link to a `GeoEdgeGroup`.
    -   The relationship is many-to-one (many TopoGroups can link to one GeoGroup).

    **Step 2: Implement Node Constraint Solver**
    -   Create a new function or class, e.g., `ConstraintSolver`, with a method: `determineNodeConstraint(TopoNode* node)`.
    -   **Procedure**:
        1.  Given a `TopoNode`, find all adjacent `TopoFace`s using the topology query API (e.g., via a vertex circulator in the half-edge structure).
        2.  For each adjacent face, identify which `TopoFaceGroup` it belongs to.
        3.  From each `TopoFaceGroup`, retrieve the linked `GeoFaceGroup`.
        4.  Collect the set of unique `GeoFaceGroup`s adjacent to the node.
        5.  The size of this unique set determines the constraint level:
            -   **`size >= 3`**: The node is **Fixed** (0-DOF). It lies at the junction of three or more geometric regions.
            -   **`size == 2`**: The node is **Edge-Constrained** (1-DOF). It can slide along the geometric edge shared by the two geometry groups.
            -   **`size == 1`**: The node is **Face-Constrained** (2-DOF). It can slide on the surface of the single geometry group.
            -   **`size == 0`**: The node is **Unconstrained**.
    -   The result of this function should be an enum or struct that can be stored in the `TopoNode` and accessed by other systems (e.g., `enum class Constraint { FIXED, EDGE, FACE, UNCONSTRAINED };`).

    **Step 3: Integrate Constraint Logic into Node Manipulation**
    -   Modify the smoother and interactive node-dragging tools.
    -   After calculating a new trial position for a node, call a projection function based on the node's constraint level.
    -   **Projection Functions**:
        -   `projectOnFace(new_pos, geo_face)`: Projects the point onto the surface of the geometry face.
        -   `projectOnEdge(new_pos, geo_edge)`: Projects the point onto the geometry edge curve.
        -   For fixed nodes, the position is simply not updated (or reset to its original fixed position).

    **Step 4: Implement Geometry Group Validation Logic**
    -   Create a new function, e.g., `validateGeometryGroups()`, to be run when geometry groups are created or modified.
    -   **Sub-Check 1: Group Contiguity**:
        -   For each `GeoFaceGroup`, perform a graph traversal (like BFS or DFS) starting from an arbitrary face.
        -   Verify that all faces within the group are visited, ensuring the group is a single connected patch.
    -   **Sub-Check 2: Boundary Edge Grouping**:
        -   For each `GeoFaceGroup`, identify all its boundary edges (edges adjacent to only one face within the group).
        -   For each boundary edge, verify that it is a member of a valid `GeoEdgeGroup`. This ensures that all geometric seams and borders are explicitly defined as edge groups.



# suggested data structures
Here is a comprehensive technical specification for the **Dynamic Structured Surface Meshing Kernel**. This document is formatted for immediate hand-off to a C++ development team.

---

# Technical Specification: SS-Mesh (Structured Surface Meshing) Kernel

## 1. Executive Summary

This project aims to build a robust, dynamic half-edge data structure for structured surface meshing. The system must support real-time topological modifications (extrude, merge, split, delete) while strictly adhering to underlying CAD geometry constraints and propagating mesh dimensions across structured grids (quad-dominant).

**Key Design Philosophy:**

* **Explicit Connectivity:** Do not compute adjacency on the fly; store it.
* **Constraint-Based:** Topology "lives" on Geometry.
* **Event-Driven Updates:** Dimension changes propagate instantly via pre-linked lists, not tree searches.

---

## 2. Architecture Overview

The system is divided into three distinct layers:

1. **Geometry Layer (Constraint):** The immutable CAD B-Rep (Boundary Representation).
2. **Topology Layer (Mesh):** The mutable half-edge graph.
3. **Dimension Layer (Solver):** The "Union-Find" system managing structured edge counts.

### 2.1 The Geometry Layer

* **Purpose:** Defines *where* mesh nodes are allowed to exist.
* **Storage:** `std::vector` or similar linear storage; generally static during meshing.

```cpp
enum class GeoType { VERTEX, CURVE, SURFACE };

struct GeoBase {
    int id;
    GeoType type;
    bool isBoundary; // True if this limits the domain (e.g., outer loop)
};

struct GeoNode : GeoBase { 
    Vector3 position; 
};

struct GeoEdge : GeoBase { 
    // Parametric curve definition (t -> Vector3)
    std::vector<GeoNode*> ends; 
};

struct GeoFace : GeoBase { 
    // Parametric surface definition (u,v -> Vector3)
    std::vector<GeoEdge*> boundaryLoops; 
};

```

---

## 3. The Topology Layer (Data Structures)

* **Pattern:** **Non-Manifold Half-Edge (DCEL)**.
* **Optimization:** **Explicit Parallel Linking** for O(1) dimension propagation.

### 3.1 TopoNode

Represents a vertex in the mesh.

```cpp
enum class NodeFreedom {
    LOCKED,         // Pinned to GeoNode
    SLIDING_CURVE,  // Pinned to GeoEdge (t-value)
    SLIDING_SURF,   // Pinned to GeoFace (u,v-value)
    FREE            // 3D Space (rare in this specific app)
};

struct TopoNode {
    int id;
    Vector3 pos;    // Cache for rendering
    double t;       // Parametric coord (if on curve)
    Vector2 uv;     // Parametric coord (if on surface)

    TopoHalfEdge* out;  // One outgoing half-edge
    GeoBase* geoLink;   // The "parent" geometry constraint
    NodeFreedom state;  // Cached state

    // Recalculates 'state' based on adjacent faces' geoLinks
    void updateFreedom(); 
};

```

### 3.2 TopoEdge (The "Rib")

Represents the connection between two nodes. Contains the logic for the structured grid.

```cpp
struct TopoEdge {
    int id;
    TopoHalfEdge* he1; // Forward half
    TopoHalfEdge* he2; // Backward half

    // --- Structured Grid Optimization ---
    // These pointers form a linked list of edges that MUST have the same dimension.
    // They are "parallel" in the quad grid sense.
    TopoEdge* parallelNext = nullptr; 
    TopoEdge* parallelPrev = nullptr;

    // Pointer to the shared dimension controller
    DimensionChord* chord; 

    bool isDegenerate() const { return he1->origin == he2->origin; }
};

```

### 3.3 TopoHalfEdge (The "Dart")

Directed edge component.

```cpp
struct TopoHalfEdge {
    TopoNode* origin;
    TopoHalfEdge* twin; 
    TopoHalfEdge* next; // CCW traversal
    TopoHalfEdge* prev; // CCW traversal
    TopoFace* face;     // nullptr if Wire/Boundary
    TopoEdge* parent;
};

```

### 3.4 DimensionChord (The "Brain")

Manages the element count for a "strip" of quads.

```cpp
struct DimensionChord {
    int segments;       // e.g., 10 divisions
    bool userLocked;    // True if user explicitly typed a number
    std::vector<TopoEdge*> registeredEdges; // Back-pointer for debug/highlighting
    
    // Merges 'other' into 'this' (Union operation)
    void merge(DimensionChord* other);
};

```

---

## 4. Algorithmic Specifications

### 4.1 Constraint Resolution (Node Freedom)

* **Trigger:** Node creation, merge, or move.
* **Logic:**
1. Collect all incident `TopoFace`s.
2. Check their `geoLink` (GeoFace).
3. If all incident faces belong to the **same** GeoFace  `SLIDING_SURF`.
4. If incident faces belong to **different** GeoFaces  Node is on the boundary `GeoEdge` between them  `SLIDING_CURVE`.
5. If node is explicitly snapped to a `GeoNode`  `LOCKED`.



### 4.2 The "Control-Drag" Extrusion (Creation)

* **Context:** User drags an Edge to create a Face.
* **Procedure:**
1. **Generate:** 2 new Nodes, 3 new Edges, 1 new Face.
2. **Link Topology:** Connect `Next`, `Prev`, `Twin`, `Face` pointers.
3. **Link Parallelism (Critical):**
* The new edge *parallel* to the source edge gets `source->chord`.
* Set `newEdge->parallelPrev = source`.
* Set `source->parallelNext = newEdge`.


4. **Create Perpendiculars:**
* The two new "side" edges get a *new* `DimensionChord`.
* Link them to each other (`side1->parallelNext = side2`).





### 4.3 Robust Merge (The "Zipper")

* **Context:** Dragging Node A onto Node B.
* **Procedure:**
1. **Geometry Check:** If `A.constraint > B.constraint` (e.g., A is Fixed, B is Surface), fail or swap.
2. **Rewire:** Iterate all `HalfEdges` originating at A; set origin to B.
3. **Detect Collapse:** Check for edges where `he->origin == he->twin->origin`.
4. **Handle Collapse:**
* If the collapsed edge was part of a Face, that Face effectively loses an edge.
* **Structured Rule:** A quad becoming a triangle is valid only if it is a "corner collapse" (3 nodes merge to 1) or if the face is deleted.


5. **Merge Chords:** If two edges become parallel due to the merge (e.g., closing a Y-shape into an I-shape), their `DimensionChords` must be merged using the `Union` operation.



### 4.4 Cascade Delete

* **Procedure:**
1. **Tombstone:** Mark entity as `isDeleted`.
2. **Propagate:**
* Node dies  Kill incident Edges.
* Edge dies  Kill incident Faces.


3. **Repair Parallel Links:**
* If `Edge B` is deleted in chain `A -> B -> C`:
* Link `A -> C`.
* If `B` was the head, `C` becomes head.


4. **Memory:** Return objects to Memory Pool.



---

## 5. Memory Management

**Requirement:** Use **Arena/Pool Allocation**.

* Standard `new/delete` is too slow and causes fragmentation for mesh elements.
* Implement `ObjectPool<T>` blocks (e.g., 4KB chunks).
* **Pointers:** Use raw pointers for internal topology links (fastest dereference). Use `IDs` or `Handles` for external API references (safer).

---

## 6. Unit Testing Plan

### 6.1 Low-Level Tests (The "Mechanics")

1. **Cycle Test:** Traverse `Next` 4 times on a quad; assert you return to start.
2. **Twin Test:** Assert `he->twin->twin == he`.
3. **Chord Propagation:**
* Create a 1x5 strip of quads.
* Change dimension on Edge 1.
* Assert Edges 2, 3, 4, 5, 6 update automatically.



### 6.2 Interaction Tests (The "Stress")

1. **The "Bowtie" Merge:** Merge two opposite corners of a quad. Assert system rejects or handles degenerate face correctly without crashing.
2. **Boundary Slide:** Move a node constrained to a `GeoEdge`. Assert it clamps to the curve parameter  and does not drift into the face interior.
3. **Delete Middle:** Delete a central face in a 3x3 grid. Assert surrounding edges become "Wire" (boundary) edges.

---

## 7. Iterative Execution Plan

### Phase 1: The Skeleton (Weeks 1-2)

* Implement `ObjectPool`.
* Implement `TopoNode`, `TopoEdge`, `TopoFace`, `TopoHalfEdge` structs.
* Implement basic "Link" functions (`setTwin`, `makeFace`).
* *Deliverable:* A hardcoded single quad that renders.

### Phase 2: The Constraint Engine (Weeks 3-4)

* Implement `Geo*` classes.
* Implement `updateFreedom()` logic.
* Implement "Snap to Geometry" math (Project point to curve/surface).
* *Deliverable:* A quad where nodes stick to a defined parametric surface.

### Phase 3: Dynamic Operations (Weeks 5-6)

* Implement `ExtrudeNode` and `ExtrudeEdge`.
* Implement the `DimensionChord` system and "Parallel Linking" logic.
* *Deliverable:* Ability to "draw" a strip of quads and resize the grid dynamically.

### Phase 4: Destructive Operations (Weeks 7-8)

* Implement `SmartMerge` (The most complex task).
* Implement `CascadeDelete`.
* *Deliverable:* Full "Undo/Redo" capable meshing session (Create, Drag, Snap, Delete).


# Test for topology datastructure
This document outlines a comprehensive Unit Test Suite for the **Structured Surface Meshing Kernel**. It is organized by complexity, starting with atomic topology checks and moving to complex destructive operations.

These tests are designed to be implemented in a framework like **GoogleTest (gtest)**.

### 1. Level 0: Core Topology Sanity (The "Skeleton")

*Objective: Verify the fundamental Half-Edge linkage logic is unbreakable.*

* **`Topo_HalfEdge_Twin_Integrity`**
* **Action:** Create two nodes and one edge connecting them.
* **Assert:** `he->twin->twin == he` and `he->origin != he->twin->origin`.


* **`Topo_Face_Winding_Order`**
* **Action:** Create a single Quad face manually.
* **Assert:** Walking `he->next` four times returns to the start `he`.
* **Assert:** `he->prev->next == he`.


* **`Topo_Edge_Parallel_Links`**
* **Action:** Create two disconnected edges. Manually link them as `parallelNext`.
* **Assert:** `e1->parallelNext == e2` and `e2->parallelPrev == e1`.



---

### 2. Level 1: Creation & Extrusion (The "Builder")

*Objective: Ensure constructive operations generate valid connectivity and dimension links.*

* **`Create_PullNode_GeneratesWire`**
* **Pre:** One `TopoNode` (A).
* **Action:** Pull Node A to create Node B.
* **Assert:** A new `TopoEdge` exists. `Face` is `nullptr` (it is a wire).


* **`Create_PullEdge_GeneratesQuad`**
* **Pre:** Edge E1 (A-B).
* **Action:** Pull E1 to create Edge E2 (C-D).
* **Assert:**
* 4 Edges total form a loop.
* 1 `TopoFace` created.
* **Critical:** `E1->chord` and `E2->chord` are the *same* pointer (Dimension sync).
* **Critical:** `SideEdge1->chord` and `SideEdge2->chord` are the *same* pointer.




* **`Create_MultiEdge_Pull_Strip`**
* **Pre:** A chain of 3 edges (A-B-C-D).
* **Action:** Select all 3 and pull to extrude a strip of 3 faces.
* **Assert:**
* 3 new Faces created.
* The new "top" edges are parallel-linked to the original "bottom" edges.
* All 4 new "side" edges (verticals) share the same `DimensionChord` (if the pull was uniform).





---

### 3. Level 2: Dimensioning & Propagation (The "Brain")

*Objective: Verify the Union-Find `Chord` system updates the grid instantly.*

* **`Dim_Propagate_Linear_Strip`**
* **Pre:** A 1x10 strip of quad faces.
* **Action:** Change dimension of Edge 1 from 5 to 12.
* **Assert:** Traverse `parallelNext` for all 10 steps. Assert *all* parallel edges now report dimension 12.


* **`Dim_Propagate_Ring_Loop`**
* **Pre:** A 2x2 grid curved into a cylinder (Faces 0 and 3 share an edge).
* **Action:** Change dimension of a longitudinal edge.
* **Assert:** The change propagates around the ring and stops correctly without infinite looping (check `visited` flags or rely on shared `Chord` pointer equality).


* **`Dim_Sync_On_Merge`**
* **Pre:** Two separate strips. Strip A has dim 10. Strip B has dim 5.
* **Action:** Merge an edge from Strip A with an edge from Strip B.
* **Assert:** Both strips create a single `Chord`. The dimension is unified (e.g., to `max(10, 5) = 10`).



---

### 4. Level 3: Topology Subdivision (The "Split")

*Objective: Ensure splitting an edge propagates the split across the structured grid to maintain Quad topology.*

* **`Split_Edge_Propagates_Across_Quad`**
* **Pre:** A single Quad (Edges Top, Bot, Left, Right).
* **Action:** Split "Top" edge (insert Node M).
* **Assert:**
* "Bottom" edge must also split (insert Node N).
* A new edge M-N is created.
* 1 Quad becomes 2 Quads.




* **`Split_Ring_Loop_Full_Circle`**
* **Pre:** A cylinder of 4 faces.
* **Action:** Split one of the ring edges.
* **Assert:** The split propagates through all 4 faces, creating a "cut" all the way around the cylinder, returning to the start.


* **`Split_Boundary_Constraint`**
* **Pre:** Quad edge lies on a `GeoEdge` (Curve).
* **Action:** Split the edge.
* **Assert:** The new Node M is constrained to `SLIDING_CURVE` on that specific `GeoEdge`, not just floating in 3D space.



---

### 5. Level 4: Destructive Merging (The "Zipper")

*Objective: This is the most critical section for stability. It handles "Pinching" and "Collapsing".*

* **`Merge_Nodes_SharedEdge_Collapse` (The Zipper)**
* **Pre:** Two nodes A and B connected by Edge E.
* **Action:** Drag A onto B (Merge).
* **Assert:**
* Edge E is deleted.
* Nodes A is deleted.
* Edges incident to A are rewired to B.
* No "degenerate" edges (length 0) remain in the active list.




* **`Merge_Nodes_Separate_Pinch`**
* **Pre:** Two nodes A and C in different, unconnected faces.
* **Action:** Merge A onto C.
* **Assert:**
* A is deleted.
* A's spoke edges now radiate from C.
* Topology is "Non-Manifold" (two cones touching at a vertex).




* **`Merge_Corner_Collapse_QuadToTri`**
* **Pre:** A single Quad A-B-C-D.
* **Action:** Merge A onto C (Diagonal).
* **Assert:**
* Strict Quad Enforcer: Should **FAIL/Reject** (if triangles are banned).
* OR: Quad splits into two Triangles.




* **`Merge_Hierarchy_Validation`**
* **Pre:** Node A is `FIXED` (Corner). Node B is `SLIDING_SURF`.
* **Action:** Try to Merge A onto B (Drag fixed node to floating node).
* **Assert:** **REJECT**. Fixed nodes cannot be eaten by floating nodes.
* **Action:** Try to Merge B onto A.
* **Assert:** **SUCCESS**. B moves to A's location and becomes `FIXED`.



---

### 6. Level 5: Deletion & Cleanup (The "Reaper")

*Objective: Ensure no dangling pointers or memory leaks occur during cascading deletes.*

* **`Delete_Node_Cascades_Faces`**
* **Pre:** A 3x3 grid of nodes (4 faces). Center Node exists.
* **Action:** Delete Center Node.
* **Assert:**
* Center Node deleted.
* 4 radiating edges deleted.
* 4 incident faces deleted.
* Outer boundary remains as a "Hole" (Wire edges).




* **`Delete_Edge_Stitches_Parallel_Links`**
* **Pre:** A strip of 3 quads (Edges E1, E2, E3, E4 are the "rungs").
* **Action:** Delete Face 2 (which owns Edge E2 and E3).
* **Assert:**
* E2 and E3 are NOT deleted (they are shared by Faces 1 and 3).
* Face 2 is deleted.
* **Scenario 2:** If we delete the *Edge* E2...
* Face 1 and Face 2 are deleted (merged into a big hole).
* Dimension Chords: Verify E1 is still linked to E3? (Logic specific: usually breaking continuity breaks the dimension link).







---

### 7. Level 6: Geometric Robustness (The "Math")

* **`Geo_Project_Node_To_Surface`**
* **Action:** Move a node constrained to `SLIDING_SURF`.
* **Assert:** New position is mathematically exactly on the underlying CAD surface (within tolerance).


* **`Geo_Project_Node_To_Curve_Ends`**
* **Action:** Drag a node constrained to `SLIDING_CURVE` past the end of the curve.
* **Assert:** Position clamps to the curve endpoint (t=0 or t=1). It does not "fall off" the geometry.