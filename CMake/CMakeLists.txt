cmake_minimum_required(VERSION 3.0)

project(b_CMake)

set(RT_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/RunTest.sh")
set(RT_BINARY "${CMAKE_CURRENT_BINARY_DIR}/RunTest.sh")
add_custom_command(
  OUTPUT "${RT_BINARY}"
  COMMAND cmake -E copy "${RT_SOURCE}" "${RT_BINARY}"
  DEPENDS "${RT_SOURCE}"
  COMMENT "Copying RunTest.sh"
)
add_custom_target(
  b_RunTestWrapper
  DEPENDS "${RT_BINARY}"
  SOURCES "${RT_SOURCE}"
)

set(B_RUN_TEST_WRAPPER "${RT_BINARY}" PARENT_SCOPE)
