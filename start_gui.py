import os
import subprocess
import sys
import platform


def main():
    # Define paths
    project_root = os.path.dirname(os.path.abspath(__file__))
    source_dir = os.path.join(project_root, "TopoLink")
    build_dir = os.path.join(project_root, "build")

    # Create build directory if it doesn't exist
    if not os.path.exists(build_dir):
        os.makedirs(build_dir)

    # CMake configuration
    print("Configuring with CMake...")
    # Add CMAKE_PREFIX_PATH if user has custom install locations for Qt/OCCT
    # For now, rely on find_package default search paths
    cmake_cmd = ["cmake", "-S", source_dir, "-B", build_dir]

    # Check for brew locations on Mac if likely missing
    prefix_paths = []
    
    # Add current Python environment prefix (crucial for conda)
    prefix_paths.append(sys.prefix)
    
    # Add CONDA_PREFIX if it exists
    conda_prefix = os.environ.get('CONDA_PREFIX')
    if conda_prefix and conda_prefix not in prefix_paths:
        prefix_paths.append(conda_prefix)

    if platform.system() == "Darwin":
        # Common brew paths
        qt_path = "/usr/local/opt/qt@5"
        if not os.path.exists(qt_path):
            qt_path = "/opt/homebrew/opt/qt@5"  # Apple Silicon

        occ_path = "/usr/local/opt/opencascade"
        if not os.path.exists(occ_path):
            occ_path = "/opt/homebrew/opt/opencascade"

        if os.path.exists(qt_path):
            prefix_paths.append(qt_path)
        if os.path.exists(occ_path):
            prefix_paths.append(occ_path)

    if prefix_paths:
        cmake_cmd.append(f"-DCMAKE_PREFIX_PATH={';'.join(prefix_paths)}")

    try:
        subprocess.check_call(cmake_cmd)
    except subprocess.CalledProcessError:
        print("CMake configuration failed!")
        sys.exit(1)

    # Build
    print("Building application...")
    try:
        subprocess.check_call(["cmake", "--build", build_dir])
    except subprocess.CalledProcessError:
        print("Build failed!")
        sys.exit(1)

    # Run
    print("Running application...")
    executable_name = "MeshingApp"
    if platform.system() == "Windows":
        executable_name += ".exe"
    elif platform.system() == "Darwin":
        executable_name = "MeshingApp.app/Contents/MacOS/MeshingApp"
        # Often .app bundles are created on mac in build dir
        # Check if straight executable or bundle

    executable_path = os.path.join(build_dir, executable_name)

    # Fallback if specific path logic above is slightly off for simple CMake install
    if not os.path.exists(executable_path):
        # Try finding it
        for root, dirs, files in os.walk(build_dir):
            if "MeshingApp" in files:
                executable_path = os.path.join(root, "MeshingApp")
                break

    if os.path.exists(executable_path):
        try:
            subprocess.check_call([executable_path])
        except subprocess.CalledProcessError:
            print("Execution failed!")
            sys.exit(1)
    else:
        print(f"Executable not found at expected path: {executable_path}")
        sys.exit(1)


if __name__ == "__main__":
    main()
