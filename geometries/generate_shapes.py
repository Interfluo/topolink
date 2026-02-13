import os
from OCC.Core.BRepPrimAPI import BRepPrimAPI_MakeBox, BRepPrimAPI_MakeCylinder, BRepPrimAPI_MakeSphere
from OCC.Core.STEPControl import STEPControl_Writer, STEPControl_AsIs
from OCC.Core.IGESControl import IGESControl_Writer
from OCC.Core.Interface import Interface_Static_SetCVal
from OCC.Core.IFSelect import IFSelect_RetDone

def export_step(shape, filename):
    """Exports a shape to a STEP file."""
    writer = STEPControl_Writer()
    Interface_Static_SetCVal("write.step.schema", "AP203")
    writer.Transfer(shape, STEPControl_AsIs)
    status = writer.Write(filename)
    if status == IFSelect_RetDone:
        print(f"Successfully exported {filename}")
    else:
        print(f"Error: Failed to export {filename}")

def export_iges(shape, filename):
    """Exports a shape to an IGES file."""
    writer = IGESControl_Writer()
    writer.AddShape(shape)
    writer.ComputeModel()
    status = writer.Write(filename)
    if status:
        print(f"Successfully exported {filename}")
    else:
        print(f"Error: Failed to export {filename}")

def main():
    # Ensure the output directory exists
    output_dir = os.path.dirname(os.path.abspath(__file__))
    os.makedirs(output_dir, exist_ok=True)

    shapes = {
        "cube": BRepPrimAPI_MakeBox(10.0, 10.0, 10.0).Shape(),
        "cylinder": BRepPrimAPI_MakeCylinder(5.0, 20.0).Shape(),
        "sphere": BRepPrimAPI_MakeSphere(5.0).Shape(),
    }

    for name, shape in shapes.items():
        step_file = os.path.join(output_dir, f"{name}.stp")
        iges_file = os.path.join(output_dir, f"{name}.igs")
        
        export_step(shape, step_file)
        export_iges(shape, iges_file)

if __name__ == "__main__":
    print("Generating basic geometries...")
    try:
        main()
        print("\nAll geometries generated successfully.")
    except ImportError:
        print("\nError: pythonocc-core is not installed.")
        print("Please install it using conda or pip:")
        print("  conda install -c conda-forge pythonocc-core")
        print("  OR")
        print("  pip install pythonocc-core")
