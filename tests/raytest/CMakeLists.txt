# Setup of GTEST changed at CMake 3.5.
cmake_minimum_required(VERSION 3.5)

find_package(GTest)
# Eigen required to support some tests - NDT in particular
find_package(Eigen3 QUIET)

set(SOURCES
  raytests.cpp
  testmain.cpp
)

add_executable(raytest ${SOURCES})
if(DEFINED CMAKE_DEBUG_POSTFIX)
  # Only set the property if CMAKE_DEBUG_POSTFIX does not contain a semicolon (i.e., is not a list)
  # and also handle the case where it might be an empty string (which is valid).
  if(NOT CMAKE_DEBUG_POSTFIX MATCHES ";")
    set_target_properties(raytest PROPERTIES DEBUG_POSTFIX "${CMAKE_DEBUG_POSTFIX}")
  else()
    message(WARNING "CMAKE_DEBUG_POSTFIX in raytest scope appears to be a list: [${CMAKE_DEBUG_POSTFIX}]. Not setting DEBUG_POSTFIX for raytest target to avoid errors. This might be an issue in the root CMake project setup.")
  endif()
# If CMAKE_DEBUG_POSTFIX is not defined at all, we simply don't set the property.
# This is generally fine, as no postfix will be applied.
endif()
set_target_properties(raytest PROPERTIES FOLDER tests)

target_include_directories(raytest
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/raylib>    
)

target_include_directories(raytest SYSTEM
  PRIVATE
    "${GTEST_INCLUDE_DIRS}"
)

target_link_libraries(raytest PUBLIC raylib GTest::Main)

if(Eigen3_FOUND)
  target_link_libraries(raytest PRIVATE Eigen3::Eigen)
endif(Eigen3_FOUND)

add_test(NAME raytest COMMAND raytest --gtest_output=xml:test-reports/
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

source_group("source" REGULAR_EXPRESSION ".*$")
