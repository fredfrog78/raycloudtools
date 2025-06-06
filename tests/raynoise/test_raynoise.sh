#!/bin/bash
set -e # Exit immediately if a command exits with a non-zero status.

# Define expected uncertainty values (adjust precision as needed)
# Point 1: total_variance = 0.00065916358
# Point 2: total_variance = 0.00109416128
EXPECTED_UNCERTAINTY_1="0.000659" # Using fewer decimal places for comparison
EXPECTED_UNCERTAINTY_2="0.001094" # Using fewer decimal places for comparison

# Path to the raynoise executable (assuming it's in the build path)
# This might need adjustment based on the build system / CTest environment
RAYNOISE_EXE="${RAYNOISE_EXE:-./raynoise}" # Default if not set by CTest

INPUT_FILE="tests/raynoise/sample_input.ply"
OUTPUT_FILE="tests/raynoise/sample_output.ply"
LOG_FILE="tests/raynoise/test_raynoise.log"

echo "Running raynoise test..." > $LOG_FILE
echo "Executable: $RAYNOISE_EXE" >> $LOG_FILE
echo "Input: $INPUT_FILE" >> $LOG_FILE
echo "Output: $OUTPUT_FILE" >> $LOG_FILE

# Run raynoise
$RAYNOISE_EXE -i $INPUT_FILE -o $OUTPUT_FILE >> $LOG_FILE 2>&1

if [ ! -f "$OUTPUT_FILE" ]; then
    echo "ERROR: Output file $OUTPUT_FILE not created." | tee -a $LOG_FILE
    exit 1
fi

echo "Output file created. Verifying contents..." >> $LOG_FILE

# Very basic check for uncertainty values.
# This assumes ASCII PLY output and that uncertainty is the last property.
# A more robust check would parse the PLY properly.
# For now, we use grep and awk. The property order in output PLY needs to be known.
# Assuming output PLY has x, y, z, ox, oy, oz, intensity, uncertainty

# Read the data lines from the output PLY, skipping the header
# The number of header lines might vary slightly if properties are added.
# Let's assume the header for 2 points and 8 properties (x,y,z,ox,oy,oz,intensity,uncertainty)
# ply, format, element vertex 2, 8*property, element face, property list, end_header = 14 lines
# This is fragile. A better way is to find "end_header" and take lines after it.

UNCERTAINTY_VALUES=$(awk '/end_header/{flag=1; next}flag' $OUTPUT_FILE | awk '{print $NF}')
echo "Extracted uncertainty values:" >> $LOG_FILE
echo "$UNCERTAINTY_VALUES" >> $LOG_FILE

VAL1=$(echo "$UNCERTAINTY_VALUES" | awk 'NR==1')
VAL2=$(echo "$UNCERTAINTY_VALUES" | awk 'NR==2')

echo "Value 1: $VAL1, Expected: $EXPECTED_UNCERTAINTY_1" >> $LOG_FILE
echo "Value 2: $VAL2, Expected: $EXPECTED_UNCERTAINTY_2" >> $LOG_FILE

# Compare with a tolerance
TOLERANCE=0.000001

# Check Point 1
ABS_DIFF_1=$(echo "$VAL1 - $EXPECTED_UNCERTAINTY_1" | bc -l | awk '{if ($1 < 0) print -$1; else print $1}')
if (( $(echo "$ABS_DIFF_1 > $TOLERANCE" | bc -l) )); then
    echo "ERROR: Uncertainty for point 1 ($VAL1) is not within tolerance of expected ($EXPECTED_UNCERTAINTY_1)." | tee -a $LOG_FILE
    exit 1
fi

# Check Point 2
ABS_DIFF_2=$(echo "$VAL2 - $EXPECTED_UNCERTAINTY_2" | bc -l | awk '{if ($1 < 0) print -$1; else print $1}')
if (( $(echo "$ABS_DIFF_2 > $TOLERANCE" | bc -l) )); then
    echo "ERROR: Uncertainty for point 2 ($VAL2) is not within tolerance of expected ($EXPECTED_UNCERTAINTY_2)." | tee -a $LOG_FILE
    exit 1
fi

echo "Uncertainty values match expected values within tolerance." | tee -a $LOG_FILE
echo "Test PASSED." | tee -a $LOG_FILE

# Clean up output file
# rm $OUTPUT_FILE

exit 0
