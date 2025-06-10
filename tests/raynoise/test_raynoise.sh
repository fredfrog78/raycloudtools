#!/bin/bash
set -e # Exit immediately if a command exits with a non-zero status.
# set -x # Debug mode

TOLERANCE=0.000001 # Tolerance for floating point comparisons

# Path to the raynoise executable (assuming it's in the build path or CTest sets it)
RAYNOISE_EXE="${RAYNOISE_EXE:-raynoise}" # Default if not set by CTest in local run
BASE_TEST_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )" # Get dir of this script
DATA_DIR="${BASE_TEST_DIR}/data"

OUTPUT_BASE_NAME="test_output"
LOG_FILE="${BASE_TEST_DIR}/test_raynoise.log"

# Clean up previous logs and outputs
rm -f $LOG_FILE
rm -f ${BASE_TEST_DIR}/${OUTPUT_BASE_NAME}_*.ply

echo "Running raynoise tests..." | tee -a $LOG_FILE
echo "Using raynoise executable: $RAYNOISE_EXE" | tee -a $LOG_FILE
echo "Using data directory: $DATA_DIR" | tee -a $LOG_FILE
echo "Tolerance: $TOLERANCE" | tee -a $LOG_FILE

TOTAL_TESTS=0
PASSED_TESTS=0

# Function to compare two floating point numbers
# compare_floats val1 val2 tolerance
# Returns 0 if |val1 - val2| <= tolerance, 1 otherwise
compare_floats() {
    local val1=$1
    local val2=$2
    local tol=$3
    local diff=$(echo "$val1 - $val2" | bc -l)
    local abs_diff=$(echo "if($diff < 0) -$diff else $diff" | bc -l)
    if (( $(echo "$abs_diff <= $tol" | bc -l) )); then
        return 0 # True, difference is within tolerance
    else
        return 1 # False, difference is outside tolerance
    fi
}

# Function to run a single test case for a specific point
# run_test <test_name> <input_ply> <raynoise_args> <point_index_to_check> <expected_total_v> <expected_range_v> <expected_angular_v> <expected_aoi_v> <expected_mixed_v>
run_test() {
    local test_name=$1
    local input_ply=$2
    local raynoise_args="$3"
    local point_idx=$4 # 0-based index of the point to check in the output PLY data lines
    local expected_total_v=$5
    local expected_range_v=$6
    local expected_angular_v=$7
    local expected_aoi_v=$8
    local expected_mixed_v=$9

    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    local output_ply="${BASE_TEST_DIR}/${OUTPUT_BASE_NAME}_${test_name}_pt${point_idx}.ply"

    echo "-----------------------------------------------------" | tee -a $LOG_FILE
    echo "Running test: $test_name, Point Index: $point_idx" | tee -a $LOG_FILE
    echo "Input: $input_ply" | tee -a $LOG_FILE
    echo "Output: $output_ply" | tee -a $LOG_FILE
    echo "Raynoise Args: $raynoise_args" | tee -a $LOG_FILE

    local cmd="$RAYNOISE_EXE $input_ply $output_ply $raynoise_args"
    echo "Executing: $cmd" | tee -a $LOG_FILE
    eval $cmd >> $LOG_FILE 2>&1

    if [ ! -f "$output_ply" ]; then
        echo "ERROR: Output file $output_ply not created." | tee -a $LOG_FILE
        echo "Test FAILED: $test_name, Point Index: $point_idx" | tee -a $LOG_FILE
        return 1
    fi

    echo "Output file created. Verifying contents..." | tee -a $LOG_FILE

    local python_parser_script="${BASE_TEST_DIR}/parse_binary_ply.py"
    local parsed_values

    # Execute python script, redirecting its stderr to LOG_FILE, and capturing its stdout
    if ! parsed_values=$(python3 "$python_parser_script" "$output_ply" "$point_idx" 2>> "$LOG_FILE"); then
        echo "ERROR: Python PLY parser script failed (exit code) for $output_ply point $point_idx. See log for python errors." | tee -a $LOG_FILE
        echo "Test FAILED (parsing script error): $test_name, Point Index: $point_idx" | tee -a $LOG_FILE
        return 1
    fi

    # Check if parsed_values is empty (e.g. if python script printed error and exited non-zero but somehow that wasn't caught by 'if !')
    if [ -z "$parsed_values" ]; then
        echo "ERROR: Python PLY parser script returned empty output for $output_ply point $point_idx." | tee -a $LOG_FILE
        echo "Test FAILED (empty parser output): $test_name, Point Index: $point_idx" | tee -a $LOG_FILE
        return 1
    fi

    read -r actual_total_v actual_range_v actual_angular_v actual_aoi_v actual_mixed_v <<< "$parsed_values"

    # This check is now for values read by the python script
    if [ -z "$actual_total_v" ] || [ -z "$actual_range_v" ] || [ -z "$actual_angular_v" ] || [ -z "$actual_aoi_v" ] || [ -z "$actual_mixed_v" ] || \
       ! [[ "$actual_total_v" =~ ^-?[0-9]+(\.[0-9]+)?(\.?[0-9]+e[\+\-][0-9]+)?$ ]] || \
       ! [[ "$actual_range_v" =~ ^-?[0-9]+(\.[0-9]+)?(\.?[0-9]+e[\+\-][0-9]+)?$ ]] || \
       ! [[ "$actual_angular_v" =~ ^-?[0-9]+(\.[0-9]+)?(\.?[0-9]+e[\+\-][0-9]+)?$ ]] || \
       ! [[ "$actual_aoi_v" =~ ^-?[0-9]+(\.[0-9]+)?(\.?[0-9]+e[\+\-][0-9]+)?$ ]] || \
       ! [[ "$actual_mixed_v" =~ ^-?[0-9]+(\.[0-9]+)?(\.?[0-9]+e[\+\-][0-9]+)?$ ]] ; then
        echo "ERROR: Failed to parse all five uncertainty values correctly using Python script for point ${point_idx} from $output_ply." | tee -a $LOG_FILE
        echo "Raw parsed output: '$parsed_values'" | tee -a $LOG_FILE
        echo "Individual values: Total='${actual_total_v}', Range='${actual_range_v}', Angular='${actual_angular_v}', AoI='${actual_aoi_v}', Mixed='${actual_mixed_v}'" | tee -a $LOG_FILE
        echo "Test FAILED (parsing values): $test_name, Point Index: $point_idx" | tee -a $LOG_FILE
        return 1
    fi

    echo "Parsed values (via Python): Total=${actual_total_v}, Range=${actual_range_v}, Angular=${actual_angular_v}, AoI=${actual_aoi_v}, Mixed=${actual_mixed_v}" | tee -a $LOG_FILE
    echo "Expected values: Total=${expected_total_v}, Range=${expected_range_v}, Angular=${expected_angular_v}, AoI=${expected_aoi_v}, Mixed=${expected_mixed_v}" | tee -a $LOG_FILE

    local all_match=true
    compare_floats $actual_total_v $expected_total_v $TOLERANCE || { echo "MISMATCH: total_variance"; all_match=false; }
    compare_floats $actual_range_v $expected_range_v $TOLERANCE || { echo "MISMATCH: range_variance"; all_match=false; }
    compare_floats $actual_angular_v $expected_angular_v $TOLERANCE || { echo "MISMATCH: angular_variance"; all_match=false; }
    compare_floats $actual_aoi_v $expected_aoi_v $TOLERANCE || { echo "MISMATCH: aoi_variance"; all_match=false; }
    compare_floats $actual_mixed_v $expected_mixed_v $TOLERANCE || { echo "MISMATCH: mixed_pixel_variance"; all_match=false; }

    if $all_match; then
        echo "Test PASSED: $test_name, Point Index: $point_idx" | tee -a $LOG_FILE
        PASSED_TESTS=$((PASSED_TESTS + 1))
        return 0
    else
        echo "Test FAILED: $test_name, Point Index: $point_idx (Values do not match expected within tolerance)" | tee -a $LOG_FILE
        return 1
    fi
}

# --- Test Case Definitions ---

# Test Case 1: Basic test (verifying range and angular, isolating others)
INPUT_BASIC="${DATA_DIR}/test_basic.ply"
ARGS_BASIC="--c_aoi 0 --penalty_mixed 0 --c_intensity 0.5 --epsilon 0.01" # Default intensity params
# P1 (idx 0) in test_basic.ply: total_v=0.00065916, range_v=0.00064691, angular_v=0.00001225, aoi_v=0.0, mixed_pixel_v=0.0
run_test "Basic_P1" "$INPUT_BASIC" "$ARGS_BASIC" 0 0.0006591635802469136 0.0006469135802469136 0.00001225 0.0 0.0
# P2 (idx 1) in test_basic.ply: total_v=0.00109416, range_v=0.00104516, angular_v=0.000049,   aoi_v=0.0, mixed_pixel_v=0.0
run_test "Basic_P2" "$INPUT_BASIC" "$ARGS_BASIC" 1 0.001090108682800641 0.001041108682800641 0.000049 0.0 0.0

# Test Case 2: Angle of Incidence test (isolating intensity and mixed pixel)
INPUT_AOI="${DATA_DIR}/test_aoi.ply"
ARGS_AOI="--c_intensity 0 --penalty_mixed 0 --c_aoi 0.1 --epsilon_aoi 0.01" # Default AoI params
# P1_check (idx 0) in test_aoi.ply: total_v=0.13987286, range_v=0.0004, angular_v=0.0000245, aoi_v=0.13944836, mixed_pixel_v=0.0
run_test "AoI_P1" "$INPUT_AOI" "$ARGS_AOI" 0 0.1398728609039869 0.0004 0.0000245 0.1394483609039869 0.0
# P2_check (idx 1) in test_aoi.ply: total_v=0.09942215, range_v=0.0004, angular_v=0.00001225, aoi_v=0.09900990, mixed_pixel_v=0.0
run_test "AoI_P2" "$INPUT_AOI" "$ARGS_AOI" 1 0.09942215099009901 0.0004 0.00001225 0.09900990099009901 0.0

# Test Case 3: Mixed Pixel test (isolating intensity and AoI)
INPUT_MIXED="${DATA_DIR}/test_mixed.ply"
ARGS_MIXED="--c_intensity 0 --c_aoi 0 --k_mixed 8 --depth_thresh_mixed 0.05 --min_front_mixed 1 --min_behind_mixed 1 --penalty_mixed 0.5" # Default mixed pixel params (aggressive)
# P_test (idx 0) in test_mixed.ply: total_v=0.50042756, range_v=0.0004, angular_v=0.00002756, aoi_v=0.0, mixed_pixel_v=0.5
run_test "Mixed_P_test" "$INPUT_MIXED" "$ARGS_MIXED" 0 0.5004275625 0.0004 0.0000275625 0.0 0.5
# SF1 (idx 1) in test_mixed.ply: total_v=0.00041237, range_v=0.0004, angular_v=0.00001237, aoi_v=0.0, mixed_pixel_v=0.0
run_test "Mixed_SF1" "$INPUT_MIXED" "$ARGS_MIXED" 1 0.0004123725 0.0004 0.0000123725 0.0 0.0

# --- Summary ---
echo "-----------------------------------------------------" | tee -a $LOG_FILE
echo "Total tests run: $TOTAL_TESTS" | tee -a $LOG_FILE
echo "Tests passed: $PASSED_TESTS" | tee -a $LOG_FILE
if [ "$PASSED_TESTS" -eq "$TOTAL_TESTS" ]; then
    echo "All tests PASSED successfully!" | tee -a $LOG_FILE
    exit 0
else
    echo "Some tests FAILED! Check $LOG_FILE for details." | tee -a $LOG_FILE
    exit 1
fi
