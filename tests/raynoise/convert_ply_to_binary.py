import argparse
import numpy as np
from plyfile import PlyData, PlyElement

def convert_ply_ascii_to_binary(input_filepath, output_filepath):
    """
    Converts a PLY file from ASCII to binary_little_endian format.
    Reads vertex properties: x, y, z, ox, oy, oz, red, green, blue, alpha, time.
    Calculates nx = ox - x, ny = oy - y, nz = oz - z.
    Writes vertex properties: x, y, z, time, nx, ny, nz, red, green, blue, alpha
    in binary_little_endian format.
    """
    try:
        # Read the ASCII PLY file
        plydata_ascii = PlyData.read(input_filepath)

        if 'vertex' not in plydata_ascii:
            raise ValueError(f"Input PLY file {input_filepath} does not contain 'vertex' element.")

        vertex_ascii = plydata_ascii['vertex']

        num_vertices = len(vertex_ascii.data)

        # Check for required input fields
        required_input_fields = ['x', 'y', 'z', 'ox', 'oy', 'oz', 'red', 'green', 'blue', 'alpha', 'time']
        for field in required_input_fields:
            if field not in vertex_ascii.data.dtype.names:
                raise ValueError(f"Input PLY file {input_filepath} is missing required field: {field} in vertex element.")

        # Create a new numpy structured array for the binary data
        # Order for raylib PLY (based on rayply.cpp writeRayCloudChunkStart):
        # x, y, z (float32 or float64 depending on RAYLIB_DOUBLE_RAYS)
        # time (float64)
        # nx, ny, nz (float32) - these represent the vector from end to start (start - end), so ray = start - end.
        #                         readPly expects normal = start - end. So if we have start=origin, end=pos, then normal = origin - pos.
        #                         The test files have ox,oy,oz as absolute origin. So origin-pos = (ox-x, oy-y, oz-z).
        # red, green, blue, alpha (uint8)

        # Assuming RAYLIB_DOUBLE_RAYS = 0 (coords are float32) for this script's output
        # This matches the previous assumptions for raynoise's own output PLY.
        vertex_binary_data = np.empty(num_vertices, dtype=[
            ('x', 'f4'), ('y', 'f4'), ('z', 'f4'),      # End point position
            ('time', 'f8'),                            # Time
            ('nx', 'f4'), ('ny', 'f4'), ('nz', 'f4'),  # Ray vector (start - end)
            ('red', 'u1'), ('green', 'u1'), ('blue', 'u1'), ('alpha', 'u1') # Color
        ])

        # Populate data
        vertex_binary_data['x'] = vertex_ascii.data['x'].astype(np.float32)
        vertex_binary_data['y'] = vertex_ascii.data['y'].astype(np.float32)
        vertex_binary_data['z'] = vertex_ascii.data['z'].astype(np.float32)
        vertex_binary_data['time'] = vertex_ascii.data['time'].astype(np.float64)

        # Calculate nx, ny, nz as origin - position
        vertex_binary_data['nx'] = vertex_ascii.data['ox'].astype(np.float32) - vertex_ascii.data['x'].astype(np.float32)
        vertex_binary_data['ny'] = vertex_ascii.data['oy'].astype(np.float32) - vertex_ascii.data['y'].astype(np.float32)
        vertex_binary_data['nz'] = vertex_ascii.data['oz'].astype(np.float32) - vertex_ascii.data['z'].astype(np.float32)

        vertex_binary_data['red'] = vertex_ascii.data['red'].astype(np.uint8)
        vertex_binary_data['green'] = vertex_ascii.data['green'].astype(np.uint8)
        vertex_binary_data['blue'] = vertex_ascii.data['blue'].astype(np.uint8)
        vertex_binary_data['alpha'] = vertex_ascii.data['alpha'].astype(np.uint8)

        # Create a new PlyElement for the binary data
        vertex_binary_el = PlyElement.describe(vertex_binary_data, 'vertex')

        # Create a new PlyData object and write as binary_little_endian
        plydata_binary = PlyData([vertex_binary_el], byte_order='<')
        plydata_binary.write(output_filepath)

        print(f"Successfully converted {input_filepath} to {output_filepath} (binary_little_endian with nx,ny,nz ray vectors).")

    except Exception as e:
        print(f"Error during conversion of {input_filepath}: {e}")
        raise # Re-raise the exception to make the script exit with an error status

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="Convert ASCII PLY to binary_little_endian PLY, transforming ox,oy,oz to nx,ny,nz ray vectors."
    )
    parser.add_argument("input_file", help="Path to the input ASCII PLY file (must contain x,y,z, ox,oy,oz, rgba, time).")
    parser.add_argument("output_file", help="Path to the output binary PLY file.")

    args = parser.parse_args()

    try:
        convert_ply_ascii_to_binary(args.input_file, args.output_file)
    except Exception:
        # Error message already printed by the function
        print("Conversion failed.")
        exit(1)
