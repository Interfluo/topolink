cmake_minimum_required(VERSION 3.16)
project(MeshingApp LANGUAGES CXX)

# Default to Release build if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
# Find Qt
find_package(Qt5 COMPONENTS Widgets Core Gui Concurrent REQUIRED)

# Find OpenCASCADE
# Ensure 'CASROOT' environment variable is set or specify path here
find_package(OpenCASCADE REQUIRED)

if (APPLE)
    # Homebrew location fallback if OpenCASCADE_LIBRARY_DIR is not set
    if (NOT DEFINED OpenCASCADE_LIBRARY_DIR)
        set(OpenCASCADE_LIBRARY_DIR "/opt/homebrew/opt/opencascade/lib")
    endif()
endif()

link_directories(${OpenCASCADE_LIBRARY_DIR})


# Define Sources
set(SOURCES
    src/main.cpp
    src/gui/MainWindow.cpp
    src/gui/OccView.cpp
    src/gui/BannerWidget.cpp
    src/gui/pages/GeometryPage.cpp
    src/gui/pages/TopologyPage.cpp
    src/gui/pages/SmootherPage.cpp
    src/gui/pages/ConvergencePlot.cpp
    src/gui/GroupDelegates.cpp
    src/core/TopoNode.cpp
    src/core/TopoEdge.cpp
    src/core/TopoFace.cpp
    src/core/Topology.cpp
    src/core/EllipticSolver.cpp
    src/core/Smoother.cpp
    src/core/MeshExporter.cpp
    src/gui/ProjectManager.cpp
)

set(HEADERS
    src/gui/MainWindow.h
    src/gui/OccView.h
    src/gui/BannerWidget.h
    src/gui/pages/GeometryPage.h
    src/gui/pages/TopologyPage.h
    src/gui/pages/SmootherPage.h
    src/gui/pages/ConvergencePlot.h
    src/gui/GroupDelegates.h
    src/gui/EntityOwner.h
    src/core/TopoNode.h
    src/core/TopoEdge.h
    src/core/TopoFace.h
    src/core/Topology.h
    src/core/Smoother.h
    src/core/MeshExporter.h
    src/gui/ProjectManager.h
    src/gui/SplitEdgeDialog.h
)


# macOS App Icon
if(APPLE)
    set(MACOSX_BUNDLE_ICON_FILE MeshingApp.icns)
    set(APP_ICON_MACOSX ${CMAKE_CURRENT_SOURCE_DIR}/resources/MeshingApp.icns)
    set_source_files_properties(${APP_ICON_MACOSX} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
endif()

# Create Executable (macOS bundle)
add_executable(MeshingApp MACOSX_BUNDLE
    ${SOURCES}
    ${HEADERS}
    ${APP_ICON_MACOSX}
    resources.qrc
)

# Link Libraries
target_link_libraries(MeshingApp PRIVATE
    Qt5::Widgets
    Qt5::Concurrent
    TKMath TKernel TKService TKV3d TKOpenGl TKBrep TKPrim 
    TKTopAlgo TKGeomAlgo TKGeomBase TKDESTEP TKXSBase TKMesh
)


# Include Directories
target_include_directories(MeshingApp PRIVATE 
    src/gui 
    src/core
    ${OpenCASCADE_INCLUDE_DIR}
)




add_subdirectory(tests)
