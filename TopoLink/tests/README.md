# TopoLink Tests

This directory contains the unit testing framework for the TopoLink application.

## Framework

We use **GoogleTest** (fetched automatically via CMake) for unit testing.

## Running Tests

To run the tests:

1. Configure the project with testing enabled (default in root CMakeLists.txt):
   
   **Note**: The test suite is currently disabled by default in `CMakeLists.txt` to ensure build stability on all platforms. To enable it, uncomment `add_subdirectory(tests)` at the end of `TopoLink/CMakeLists.txt`.

   ```bash
   mkdir build && cd build
   cmake ..
   ```

2. Build the project:
   ```bash
   make
   ```

3. Run the tests:
   ```bash
   ./tests/unit_tests
   ```
   Or using CTest:
   ```bash
   ctest
   ```

## Adding New Tests

1. Create a new `.cpp` file in this directory (e.g., `test_geometry.cpp`).
2. Add the file to `CMakeLists.txt` in the `add_executable` source list.
3. Write standard GoogleTest tests:
   ```cpp
   #include <gtest/gtest.h>
   
   TEST(Category, TestName) {
       EXPECT_EQ(1, 1);
   }
   ```
