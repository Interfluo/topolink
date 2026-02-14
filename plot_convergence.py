import sys
import os
import csv
import matplotlib.pyplot as plt

def main():
    filename = "convergence.log"
    if not os.path.exists(filename):
        print(f"Error: {filename} not found.")
        sys.exit(1)

    iterations = []
    face_data = {}

    try:
        with open(filename, 'r') as f:
            reader = csv.reader(f)
            headers = next(reader)
            
            # Initialize lists for each face column
            for h in headers[1:]:
                face_data[h] = []

            for row in reader:
                if not row: continue
                iterations.append(int(row[0]))
                for i, val in enumerate(row[1:]):
                    key = headers[i+1]
                    if val:
                        face_data[key].append(float(val))
                    else:
                        face_data[key].append(None) # Handle variable lengths if any
    except Exception as e:
        print(f"Error reading {filename}: {e}")
        sys.exit(1)

    if not iterations:
        print("No data found.")
        sys.exit(0)

    plt.figure(figsize=(10, 6))
    for face_id, values in face_data.items():
        # clean Nones if any (though smoother saves padded data now, actually saving maxIters rows)
        # The C++ code pads with empty strings if a face finished early? No, it pads based on maxIters.
        # But wait, C++ code:
        # if (i < it->second.size()) out << val
        # else out << "" (implicit empty between commas? No, loop just puts commas)
        
        # Let's check C++ logic again. 
        # for (auto it...) { out << ","; if (i < size) out << val; }
        # So yes, empty string between commas.
        
        valid_iters = []
        valid_vals = []
        for i, v in enumerate(values):
            if v is not None:
                valid_iters.append(iterations[i])
                valid_vals.append(v)
        
        if valid_vals:
            linestyle = '--' if 'Edge' in face_id else '-'
            plt.plot(valid_iters, valid_vals, label=face_id, linestyle=linestyle)

    plt.title("Smoother Convergence (Max Displacement)")
    plt.xlabel("Iteration")
    plt.ylabel("Max Displacement")
    plt.yscale("log")
    plt.grid(True, which="both", ls="-", alpha=0.5)
    plt.legend()
    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    main()
