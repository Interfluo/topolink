### Functional Specification: 3D Meshing Application Controls

**1. UI Layout & HUD**

* **Viewport:** Central 3D rendering area.
* **Sidebar (Left):**
* **Menus:** Top section.
* **Groups:** Tree/List view of object groups below menus.


* **Heads-Up Display (HUD) / Toolbar:**
* **Location:** Floating or fixed panel (refer to wireframe).
* **Row 1 (Workbench Selector):** Three icons corresponding to `F1` (Geometry), `F2` (Topology), `F3` (Smoother/Mesh).
* *Icons:* Surface (Geometry), Grid/Nodes (Topology), Deformed Grid (Mesh).


* **Row 2 (Selection Mode):** Three icons corresponding to `Q` (Node), `W` (Edge), `E` (Face).
* *State:* Icons visually toggle to reflect active state.
* *Constraint:* Entire row disabled/greyed out when `F3` (Smoother Workbench) is active.





---

**2. Input Map: Keyboard & Mouse**

| Context | Input | Action | Notes |
| --- | --- | --- | --- |
| **View** | `Ctrl` + `F` | Zoom to Fit | Fits all visible geometry to viewport. |
|  | `Right Click` + `Drag` | Rotate View | Orbit camera around pivot. |
|  | `Ctrl` + `A` | Align to Nearest Axis | Snaps view to closest X, Y, or Z orthogonal view. |
| **Selection** | `Left Click` | Select | Click to select entity; Drag to box select. |
|  | `Shift` + `Left Click` + `Drag` | Add to Selection | Adds box selection to currently selected items. |
|  | `Z` | Cycle Selection | Cycles through previous available objects under cursor (depth sorting). |
|  | `C` | Select Adjacents | Selects adjacent faces/edges *not* belonging to the same face. |
|  | `Q` | Toggle Node Mode | Toggle Node selection filter. |
|  | `W` | Toggle Edge Mode | Toggle Edge selection filter. |
|  | `E` | Toggle Face Mode | Toggle Face selection filter. |
| **Editing** | `Ctrl` + `Left Click` | Add Node | Creates a new node at cursor location. |
|  | `Ctrl` + `Left Click` + `Drag` | Extrude | Extrudes the selected topology. |
|  | `M` | Merge Topology | Merges selected topology to the object currently under the cursor. |
|  | `Right Click` | Context Menu | Opens context-sensitive menu. |

---

**3. Workbench Logic (State Machine)**

| Key | Workbench Name | Behavior |
| --- | --- | --- |
| **F1** | **Geometry** | Standard geometry editing mode. |
| **F2** | **Topology** | Topology structure editing mode. |
| **F3** | **Smoother** | Mesh smoothing/relaxation mode. <br>

<br>**Constraint:** Selection Mode keys (`Q`, `W`, `E`) are disabled. |

---

**4. Solver Configuration Parameters**
*Default values indicated in parentheses.*

| Parameter | Default | Type | Description |
| --- | --- | --- | --- |
| **Edge Iterations** | `1000` | Int | Max iterations for edge solver. |
| **Edge Relaxation** | `0.9` | Float | Relaxation factor for edge positions. |
| **Edge BC Relaxation** | `0.1` | Float | Relaxation factor for boundary condition edges. |
| **Face Iterations** | `1000` | Int | Max iterations for face solver. |
| **Face Relaxation** | `0.9` | Float | Relaxation factor for face internals. |
| **Face BC Relaxation** | `0.1` | Float | Relaxation factor for face boundary conditions. |
| **Singularity Relaxation** | `0.1` | Float | Damping factor for singularity points. |
| **Growth Rate Relaxation** | `0.002` | Float | Control for mesh element size transition. |
| **Implicit Solver Sub-iters** | `4` | Int | Sub-iterations per global step. |
| **Surface Proj. Frequency** | `1` | Int | Frequency of projecting nodes back to geometry surface. |
