enable_testing()
add_subdirectory(raytest)

# Test for the raynoise tool
# Note: find_package(Python3 ...) removed.
# The command is now for the C++ test executable "raynoise_cpp_test".
# This executable is assumed to be defined elsewhere in this CMakeLists.txt or an included one.
add_test(
  NAME RayNoiseTest
  COMMAND $<TARGET_FILE:raynoise_cpp_test> "$<TARGET_FILE:raynoise>" "${CMAKE_CURRENT_SOURCE_DIR}/raynoise/data" "${CMAKE_CURRENT_BINARY_DIR}/raynoise_test_outputs"
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
# Ensure raynoise executable is built before the test runs
set_property(TEST RayNoiseTest PROPERTY REQUIRED_FILES $<TARGET_FILE:raynoise>)
