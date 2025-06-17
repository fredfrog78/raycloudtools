enable_testing()
add_subdirectory(raytest)

# Create the output directory for raynoise PLY files generated during GTests
# The GTests (running as part of 'raytest' executable) will be configured to use this path.
# CMAKE_CURRENT_BINARY_DIR here is typically <build_dir>/tests
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/raynoise_gtest_outputs")

# The add_executable for raynoise_cpp_test and add_test for RayNoiseTest are removed.
# Configuration for raytest to find raynoise executable and data will be in tests/raytest/CMakeLists.txt
