cmake_minimum_required(VERSION 2.8)

include(CheckCCompilerFlag)

project(b_JoinFiles)

add_executable(b_JoinFiles Source/Main.c)
target_link_libraries(b_JoinFiles b)

# Compile as C99.
check_c_compiler_flag(-std=c99 HAVE_STD_C99)
if (HAVE_STD_C99)
  set_property(
    TARGET b_JoinFiles
    APPEND
    PROPERTY COMPILE_OPTIONS -std=c99
  )
endif ()

# TODO(#29): Kill with fire.
check_c_compiler_flag(-Wno-main HAVE_W_NO_MAIN)
if (HAVE_W_NO_MAIN)
  set_property(
    TARGET b_JoinFiles
    APPEND
    PROPERTY COMPILE_OPTIONS -Wno-main
  )
endif ()

add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/TestJoinFiles.py"
  COMMAND
  cmake
  -E copy
  "${CMAKE_CURRENT_SOURCE_DIR}/Tests/TestJoinFiles.py"
  "${CMAKE_CURRENT_BINARY_DIR}/TestJoinFiles.py"
  DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/Tests/TestJoinFiles.py"
  COMMENT "Copying TestJoinFiles.py"
)
add_custom_target(
  TestJoinFiles
  ALL
  DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/TestJoinFiles.py"
  SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/Tests/TestJoinFiles.py"
)
add_dependencies(TestJoinFiles b_JoinFiles)
add_test(
  NAME TestJoinFiles
  COMMAND "${CMAKE_CURRENT_BINARY_DIR}/TestJoinFiles.py"
)
set_property(
  TEST TestJoinFiles
  APPEND PROPERTY
  ENVIRONMENT
  JOIN_FILES_EXE=$<TARGET_FILE:b_JoinFiles>
)
