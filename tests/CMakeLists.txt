enable_testing()
add_subdirectory(raytest)

# Define the C++ test executable for raynoise
add_executable(raynoise_cpp_test
  raynoise/test_raynoise_cpp.cpp
  raynoise/raynoise_test_utils.cpp
)
# Optional: Set properties for the test executable, like FOLDER
# set_target_properties(raynoise_cpp_test PROPERTIES FOLDER "tests/raynoise")
# Optional: Link libraries if raynoise_test_utils.cpp or test_raynoise_cpp.cpp ended up needing them
# (e.g., if std::filesystem was used, or if it needs parts of raylib for some reason)
# target_link_libraries(raynoise_cpp_test PRIVATE raylib)


# Test for the raynoise tool
add_test(
  NAME RayNoiseTest
  COMMAND $<TARGET_FILE:raynoise_cpp_test> "$<TARGET_FILE:raynoise>" "${CMAKE_CURRENT_SOURCE_DIR}/raynoise/data" "${CMAKE_CURRENT_BINARY_DIR}/raynoise_test_outputs"
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
# Ensure raynoise executable is built before the test runs
set_property(TEST RayNoiseTest PROPERTY REQUIRED_FILES $<TARGET_FILE:raynoise>)
