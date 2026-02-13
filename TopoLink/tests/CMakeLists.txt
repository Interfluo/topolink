include(FetchContent)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(unit_tests
    main_test.cpp
    core/TestTopology.cpp
    ../src/core/TopoNode.cpp
    ../src/core/TopoEdge.cpp
    ../src/core/TopoFace.cpp
    ../src/core/Topology.cpp
)

target_include_directories(unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core
    ${OpenCASCADE_INCLUDE_DIR}
)

target_link_libraries(unit_tests
    gtest
    gtest_main
    TKMath TKernel
    Qt5::Core
)

include(GoogleTest)
gtest_discover_tests(unit_tests)
