cmake_minimum_required(VERSION 3.0)

project(b_CMake)

set(RT_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/RunTest.sh")
set(RT_BINARY "${CMAKE_CURRENT_BINARY_DIR}/RunTest.sh")
b_copy_file("${RT_SOURCE}" "${RT_BINARY}")
add_custom_target(
  b_RunTestWrapper
  DEPENDS "${RT_BINARY}"
  SOURCES "${RT_SOURCE}"
)

set(B_RUN_TEST_WRAPPER "${RT_BINARY}" PARENT_SCOPE)
