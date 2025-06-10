import argparse
import os
import subprocess
import sys
import math
import numpy as np
from plyfile import PlyData

# Define a global tolerance for floating point comparisons
FLOAT_TOLERANCE = 1e-6 # Same as in the shell script

# Expected uncertainty property names in the output PLY
UNCERTAINTY_FIELDS = [
    'total_variance',
    'range_variance',
    'angular_variance',
    'aoi_variance',
    'mixed_pixel_variance'
]

# Define Test Cases
# Each test case is a dictionary:
#   'name': str, unique name for the test
#   'input_file': str, name of the input PLY file in the data_dir
#   'args': list of str, additional command line arguments for raynoise
#   'points_to_check': list of dicts, where each dict is:
#       'index': int, 0-based index of the point in the output PLY
#       'expected_values': dict, mapping field_name to expected float value

TEST_CASES = [
    {
        'name': "Basic_P1",
        'input_file': "test_basic.ply",
        'args': ["--c_aoi", "0", "--penalty_mixed", "0", "--c_intensity", "0.5", "--epsilon", "0.01"],
        'points_to_check': [
            {
                'index': 0,
                'expected_values': {
                    'total_variance': 0.0006591635802469136,
                    'range_variance': 0.0006469135802469136,
                    'angular_variance': 0.00001225,
                    'aoi_variance': 0.0,
                    'mixed_pixel_variance': 0.0
                }
            }
        ]
    },
    {
        'name': "Basic_P2",
        'input_file': "test_basic.ply",
        'args': ["--c_aoi", "0", "--penalty_mixed", "0", "--c_intensity", "0.5", "--epsilon", "0.01"],
        'points_to_check': [
            {
                'index': 1,
                'expected_values': {
                    'total_variance': 0.001090108682800641,
                    'range_variance': 0.001041108682800641,
                    'angular_variance': 0.000049,
                    'aoi_variance': 0.0,
                    'mixed_pixel_variance': 0.0
                }
            }
        ]
    },
    {
        'name': "AoI_P1_check",
        'input_file': "test_aoi.ply",
        'args': ["--c_intensity", "0", "--penalty_mixed", "0"],
        'points_to_check': [
            {
                'index': 0,
                'expected_values': {
                    'total_variance': 0.1398728609039869,
                    'range_variance': 0.0004,
                    'angular_variance': 0.0000245,
                    'aoi_variance': 0.1394483609039869,
                    'mixed_pixel_variance': 0.0
                }
            }
        ]
    },
    {
        'name': "AoI_P2_check",
        'input_file': "test_aoi.ply",
        'args': ["--c_intensity", "0", "--penalty_mixed", "0"],
        'points_to_check': [
            {
                'index': 1,
                'expected_values': {
                    'total_variance': 0.09942215099009901,
                    'range_variance': 0.0004,
                    'angular_variance': 0.00001225,
                    'aoi_variance': 0.09900990099009901,
                    'mixed_pixel_variance': 0.0
                }
            }
        ]
    },
    {
        'name': "Mixed_P_test",
        'input_file': "test_mixed.ply",
        'args': ["--c_intensity", "0", "--c_aoi", "0"],
        'points_to_check': [
            {
                'index': 0,
                'expected_values': {
                    'total_variance': 0.5004275625,
                    'range_variance': 0.0004,
                    'angular_variance': 0.0000275625,
                    'aoi_variance': 0.0,
                    'mixed_pixel_variance': 0.5
                }
            }
        ]
    },
    {
        'name': "Mixed_SF1",
        'input_file': "test_mixed.ply",
        'args': ["--c_intensity", "0", "--c_aoi", "0"],
        'points_to_check': [
            {
                'index': 1,
                'expected_values': {
                    'total_variance': 0.0004123725,
                    'range_variance': 0.0004,
                    'angular_variance': 0.0000123725,
                    'aoi_variance': 0.0,
                    'mixed_pixel_variance': 0.0
                }
            }
        ]
    }
]

def run_single_test_case(test_case, raynoise_exe, data_dir, output_dir):
    """
    Runs a single test case for raynoise.
    Returns True if the test passes, False otherwise.
    """
    test_name = test_case['name']
    input_filename = test_case['input_file']
    raynoise_args = test_case['args']
    points_to_check = test_case['points_to_check']

    input_ply_path = os.path.join(data_dir, input_filename)
    safe_output_filename = "".join(c if c.isalnum() else "_" for c in test_name) + ".ply"
    output_ply_path = os.path.join(output_dir, safe_output_filename)

    print(f"\n--- Running Test Case: {test_name} ---")
    print(f"Input PLY: {input_ply_path}")
    print(f"Output PLY: {output_ply_path}")

    command = [raynoise_exe, input_ply_path, output_ply_path] + raynoise_args
    print(f"Executing: {' '.join(command)}")

    try:
        process_result = subprocess.run(command, capture_output=True, text=True, check=False)
        if process_result.returncode != 0:
            print(f"ERROR: raynoise execution failed for {test_name}!")
            print(f"Return code: {process_result.returncode}")
            print(f"Stdout:\n{process_result.stdout}")
            print(f"Stderr:\n{process_result.stderr}")
            return False
        if process_result.stderr:
             print(f"raynoise stderr (even on success):\n{process_result.stderr}")

    except Exception as e:
        print(f"ERROR: Failed to run raynoise for {test_name}: {e}")
        return False

    if not os.path.exists(output_ply_path):
        print(f"ERROR: Output PLY file not found: {output_ply_path}")
        return False

    try:
        plydata = PlyData.read(output_ply_path)
        vertex_data = plydata['vertex'].data
    except Exception as e:
        print(f"ERROR: Failed to read or parse output PLY file {output_ply_path}: {e}")
        return False

    case_passed = True
    for point_check in points_to_check:
        point_idx = point_check['index']
        expected_vals = point_check['expected_values']

        print(f"  Checking Point Index: {point_idx}")

        if point_idx < 0 or point_idx >= len(vertex_data):
            print(f"    ERROR: Point index {point_idx} is out of bounds for output PLY (num_points: {len(vertex_data)}).")
            case_passed = False
            continue

        actual_point_data = vertex_data[point_idx]

        for field_name, expected_value in expected_vals.items():
            if field_name not in UNCERTAINTY_FIELDS:
                print(f"    WARNING: '{field_name}' not in defined UNCERTAINTY_FIELDS to check. Skipping.")
                continue

            if field_name not in actual_point_data.dtype.names:
                print(f"    ERROR: Expected field '{field_name}' not found in output PLY for point {point_idx}.")
                case_passed = False
                continue

            actual_value = actual_point_data[field_name]

            if math.isclose(actual_value, expected_value, rel_tol=FLOAT_TOLERANCE, abs_tol=FLOAT_TOLERANCE):
                print(f"    PASS: {field_name} - Actual: {actual_value:.15f}, Expected: {expected_value:.15f}")
            else:
                print(f"    FAIL: {field_name} - Actual: {actual_value:.15f}, Expected: {expected_value:.15f}, Diff: {abs(actual_value - expected_value):.15e}")
                case_passed = False

    if case_passed:
        print(f"--- Test Case {test_name}: PASSED ---")
    else:
        print(f"--- Test Case {test_name}: FAILED ---")
    return case_passed


def main():
    parser = argparse.ArgumentParser(description="Run tests for the raynoise tool.")
    parser.add_argument("--raynoise_exe", required=True, help="Path to the raynoise executable.")
    parser.add_argument("--data_dir", required=True, help="Path to the directory containing test PLY files.")
    parser.add_argument("--output_dir", default="test_outputs", help="Directory to store output PLY files from raynoise. Defaults to './test_outputs'.")

    args = parser.parse_args()

    if not os.path.isfile(args.raynoise_exe):
        print(f"Error: raynoise executable not found at {args.raynoise_exe}", file=sys.stderr)
        sys.exit(1)

    if not os.path.isdir(args.data_dir):
        print(f"Error: Test data directory not found at {args.data_dir}", file=sys.stderr)
        sys.exit(1)

    if not os.path.isdir(args.output_dir):
        try:
            os.makedirs(args.output_dir, exist_ok=True)
            print(f"Created output directory: {args.output_dir}")
        except OSError as e:
            print(f"Error: Could not create output directory {args.output_dir}: {e}", file=sys.stderr)
            sys.exit(1)

    print(f"Using raynoise executable: {args.raynoise_exe}")
    print(f"Using test data directory: {args.data_dir}")
    print(f"Using output directory: {args.output_dir}")
    print(f"Float comparison tolerance (rel and abs): {FLOAT_TOLERANCE}")

    total_cases = len(TEST_CASES)
    passed_cases = 0

    for test_case_dict in TEST_CASES:
        if run_single_test_case(test_case_dict, args.raynoise_exe, args.data_dir, args.output_dir):
            passed_cases += 1

    print("\n--- Test Summary ---")
    print(f"Total test cases: {total_cases}")
    print(f"Passed: {passed_cases}")
    print(f"Failed: {total_cases - passed_cases}")

    if passed_cases == total_cases:
        print("All tests PASSED successfully!")
        sys.exit(0)
    else:
        print("Some tests FAILED.")
        sys.exit(1)

if __name__ == "__main__":
    main()
