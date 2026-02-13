# GUI Pages

This directory contains the implementation of the wizard pages used in the main application workflow.

## Structure

The application uses a 3-stage workflow managed by a `QStackedWidget` in the `MainWindow`.

1. **GeometryPage** (`GeometryPage.h/cpp`):
   - Handles the import and definition of geometry groups (Face/Edge groups).
   - Allows users to assign colors and render modes to specific geometric entities.
   - Refactored from the legacy `GeometryGroupPanel`.

2. **TopologyPage** (`TopologyPage.h/cpp`):
   - **[TODO]** Will contain tools for defining the topology grouping (nodes, connections) on top of the geometry.
   - currently a placeholder.

3. **SmootherPage** (`SmootherPage.h/cpp`):
   - **[TODO]** Will contain the elliptic smoother settings and export functionality.
   - currently a placeholder.

## Usage

Pages are instantiated in `MainWindow` and added to the stack. Navigation is controlled by the "Next/Back" buttons in the main workflow dock.
