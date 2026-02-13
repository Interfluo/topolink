#!/usr/bin/env python3
import os
import shutil
import argparse

def remove_path(path):
    """Removes a file or directory."""
    if not os.path.exists(path):
        return
    
    try:
        if os.path.isdir(path):
            print(f"Removing directory: {path}")
            shutil.rmtree(path)
        else:
            print(f"Removing file: {path}")
            os.remove(path)
    except Exception as e:
        print(f"Error removing {path}: {e}")

def main():
    parser = argparse.ArgumentParser(description="Clean up project artifacts.")
    parser.add_argument("--all", action="store_true", help="Clean everything (build, pycache, junk, pngs)")
    parser.add_argument("--build", action="store_true", help="Remove build directory")
    parser.add_argument("--pycache", action="store_true", help="Remove __pycache__ and .pyc files")
    parser.add_argument("--junk", action="store_true", help="Remove macOS metadata (._* and .DS_Store)")
    parser.add_argument("--png", action="store_true", help="Remove .png files (legacy behavior)")
    
    args = parser.parse_args()

    # If no arguments provided, or --all is set, clean everything
    clean_all = args.all or not any([args.build, args.pycache, args.junk, args.png])

    # 1. Clean build directory at root
    if clean_all or args.build:
        remove_path("build")

    # 2. Recursive cleaning
    print("Starting recursive cleanup...")
    for dirpath, dirnames, filenames in os.walk(".", topdown=False):
        # Do not descend into build if we are about to delete it, or it was deleted
        if "build" in dirpath.split(os.sep):
            continue

        # Clean directories
        for d in list(dirnames):
            if clean_all or args.pycache:
                if d == "__pycache__":
                    remove_path(os.path.join(dirpath, d))
                    dirnames.remove(d)

        # Clean files
        for f in filenames:
            path = os.path.join(dirpath, f)
            
            if clean_all or args.pycache:
                if f.endswith(".pyc") or f.endswith(".pyo"):
                    remove_path(path)
                    continue

            if clean_all or args.junk:
                if f.startswith("._") or f == ".DS_Store":
                    remove_path(path)
                    continue

            if clean_all or args.png:
                if f.endswith(".png"):
                    remove_path(path)
                    continue

    print("Cleanup complete.")

if __name__ == "__main__":
    main()
