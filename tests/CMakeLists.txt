enable_testing()
add_subdirectory(raytest)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Test for the raynoise tool
add_test(
  NAME RayNoiseTest
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/raynoise/test_raynoise.py
          --raynoise_exe "$<TARGET_FILE:raycloud::raynoise>"
          --data_dir "${CMAKE_CURRENT_SOURCE_DIR}/raynoise/data"
          --output_dir "${CMAKE_CURRENT_BINARY_DIR}/raynoise_test_outputs"
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
# Ensure raynoise executable is built before the test runs
set_property(TEST RayNoiseTest PROPERTY REQUIRED_FILES $<TARGET_FILE:raycloud::raynoise>)
