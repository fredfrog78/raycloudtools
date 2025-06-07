import argparse
import numpy as np
from plyfile import PlyData, PlyElement

def convert_ply_ascii_to_binary(input_filepath, output_filepath):
    """
    Converts a PLY file from ASCII to binary_little_endian format.
    Assumes the PLY file has a 'vertex' element with specific properties:
    x, y, z (float32)
    ox, oy, oz (float32)
    red, green, blue, alpha (uint8)
    time (float64)
    """
    try:
        # Read the ASCII PLY file
        plydata_ascii = PlyData.read(input_filepath)

        if 'vertex' not in plydata_ascii:
            raise ValueError(f"Input PLY file {input_filepath} does not contain 'vertex' element.")

        vertex_ascii = plydata_ascii['vertex']

        num_vertices = len(vertex_ascii.data)

        # Define the dtype for the binary structured array
        # This must match the properties defined in the ASCII PLY header
        # and the desired output binary structure.
        defined_dtype = [
            ('x', 'f4'), ('y', 'f4'), ('z', 'f4'),
            ('ox', 'f4'), ('oy', 'f4'), ('oz', 'f4'),
            ('red', 'u1'), ('green', 'u1'), ('blue', 'u1'), ('alpha', 'u1'),
            ('time', 'f8')
        ]

        # Check if all expected fields exist in the source PLY's vertex element
        # and filter dtype for existing fields to avoid errors if some are missing
        # (though for this script, we expect them all to be there as per the problem description)
        final_dtype = []
        for field_name, field_type in defined_dtype:
            if field_name not in vertex_ascii.data.dtype.names:
                # If a field is critical and missing, it's an error.
                # For this script, we assume the input PLY files are structured as expected.
                raise ValueError(f"Input PLY file {input_filepath} is missing required field: '{field_name}' in vertex element.")
            final_dtype.append((field_name, field_type))

        vertex_binary_data = np.empty(num_vertices, dtype=final_dtype)

        # Copy data for all fields present in the final_dtype
        for field_name, _ in final_dtype:
            vertex_binary_data[field_name] = vertex_ascii.data[field_name]

        # Create a new PlyElement for the binary data
        vertex_binary_el = PlyElement.describe(vertex_binary_data, 'vertex')

        # Create a new PlyData object and write as binary_little_endian
        # text=False is default when byte_order is specified.
        plydata_binary = PlyData([vertex_binary_el], byte_order='<')
        plydata_binary.write(output_filepath)

        print(f"Successfully converted {input_filepath} (ASCII) to {output_filepath} (binary_little_endian).")

    except Exception as e:
        print(f"Error during conversion of {input_filepath}: {e}")
        # Potentially re-raise or exit with error code
        raise

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Convert ASCII PLY file to binary_little_endian PLY file.")
    parser.add_argument("input_file", help="Path to the input ASCII PLY file.")
    parser.add_argument("output_file", help="Path to the output binary PLY file.")

    args = parser.parse_args()

    try:
        convert_ply_ascii_to_binary(args.input_file, args.output_file)
    except Exception:
        # Error message already printed by the function
        print("Conversion failed.")
        exit(1)
