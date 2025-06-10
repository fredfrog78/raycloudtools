import argparse
import sys
from plyfile import PlyData, PlyElement
import numpy as np

def parse_ply_point_uncertainties(ply_filepath, point_index):
    """
    Reads a binary PLY file, extracts specific uncertainty fields for a given point index,
    and prints them to stdout space-separated.

    Expected properties: total_variance, range_variance, angular_variance,
                         aoi_variance, mixed_pixel_variance (all assumed double/float64).
    """
    try:
        plydata = PlyData.read(ply_filepath)

        if 'vertex' not in plydata:
            print(f"Error: 'vertex' element not found in PLY file '{ply_filepath}'.", file=sys.stderr)
            return False

        vertex_element = plydata['vertex']

        if point_index < 0 or point_index >= len(vertex_element.data):
            print(f"Error: Point index {point_index} is out of bounds (0-{len(vertex_element.data)-1}).", file=sys.stderr)
            return False

        point_data = vertex_element.data[point_index]

        required_fields = [
            'total_variance', 'range_variance', 'angular_variance',
            'aoi_variance', 'mixed_pixel_variance'
        ]

        output_values = []
        for field in required_fields:
            if field not in vertex_element.data.dtype.names: # Check against dtype.names for actual data fields
                print(f"Error: Required property '{field}' not found in PLY file's vertex data fields '{ply_filepath}'. Available fields: {list(vertex_element.data.dtype.names)}", file=sys.stderr)
                return False

            value = point_data[field]
            # Ensure values are float for formatting, then format to string
            # Numpy floats (like float32, float64) can be formatted.
            try:
                # Format to 15 decimal places to ensure sufficient precision for bc
                output_values.append(f"{float(value):.15f}")
            except ValueError:
                # Should not happen if PLY field is numeric, but as a fallback
                print(f"Warning: Could not convert value for field '{field}' to float for formatting. Using raw string.", file=sys.stderr)
                output_values.append(str(value))

        print(" ".join(output_values))
        return True

    except FileNotFoundError:
        print(f"Error: File not found: {ply_filepath}", file=sys.stderr)
        return False
    except Exception as e:
        print(f"Error parsing PLY file {ply_filepath} for point {point_index}: {e}", file=sys.stderr)
        return False

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="Parse specific uncertainty fields from a binary PLY file for a given point."
    )
    parser.add_argument("ply_file", help="Path to the binary PLY file.")
    parser.add_argument("point_index", type=int, help="0-based index of the point to extract data for.")

    args = parser.parse_args()

    if not parse_ply_point_uncertainties(args.ply_file, args.point_index):
        sys.exit(1)
    sys.exit(0)
