enable_testing()
add_subdirectory(raytest)

# Test for the raynoise tool
# Assumes 'raynoise' target exists and builds the raynoise executable.
add_test(
  NAME RayNoiseTest
  # Pass the location of the raynoise executable to the script via an environment variable.
  # The script is expected to be in ${CMAKE_CURRENT_SOURCE_DIR}/raynoise/test_raynoise.sh
  # ${CMAKE_CURRENT_SOURCE_DIR} here refers to the "tests" directory.
  COMMAND ${CMAKE_COMMAND} -E env "RAYNOISE_EXE=$<TARGET_FILE:raynoise>" ${CMAKE_SOURCE_DIR}/tests/raynoise/test_raynoise.sh
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR} # Run tests from the build directory.
)

# To make sure the test runs only if raynoise can be built,
# you can set a test property. This is more robust.
# This requires raynoise to be a known CMake target.
set_property(TEST RayNoiseTest PROPERTY REQUIRED_FILES $<TARGET_FILE:raynoise>)
