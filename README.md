# TopoLink

A Qt5/OpenCASCADE-based mesh visualization tool with STEP file import capabilities.

## Features

- **STEP Import**: Load CAD models from `.stp` / `.step` files
- **Interactive Selection**: Select faces, edges, and vertices with visual feedback
- **3D Navigation**: Rotate, pan, and zoom the view
- **Console Logging**: View selected element IDs and import status

## Requirements

- **Qt5** (Widgets, Core, Gui)
- **OpenCASCADE** (OCCT 7.x)
- **CMake** 3.16+
- **C++17** compatible compiler

### macOS (Homebrew)

```bash
brew install qt@5 opencascade
```

## Building

The simplest way to build and run is via the Python helper:

```bash
python3 start_gui.py
```

Or manually:

```bash
mkdir build && cd build
cmake ../TopoLink
cmake --build .
./MeshingApp
```

## Keyboard Shortcuts

| Action | Shortcut |
|--------|----------|
| Import STP | `Ctrl+O` |
| Fit View | `F` |
| Align to Axis | `A` |

## Mouse Controls

| Action | Control |
|--------|---------|
| Rotate | Left-click + drag |
| Pan | Middle-click + drag or Shift + left-click |
| Zoom | Scroll wheel |
| Select | Left-click (on model) |

## Project Structure

```
TopoLink/
├── CMakeLists.txt       # Build configuration
├── src/
│   ├── main.cpp         # Application entry point
│   └── gui/
│       ├── MainWindow.cpp/h  # Main application window
│       └── OccView.cpp/h     # OpenCASCADE 3D viewer widget
```

## License

MIT
