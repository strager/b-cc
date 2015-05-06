cmake_minimum_required(VERSION 3.0)

include(../../../../CMake/b_python.cmake)

project(b_Python_JoinFiles)

b_add_python_executable(
  b_Python_JoinFiles
  Source/JoinFiles.py
)

b_add_python_executable(
  b_Python_TestJoinFiles
  "${b_JoinFiles_SOURCE_DIR}/Tests/TestJoinFiles.py"
)
b_add_python_test(b_Python_TestJoinFiles MODULES b_Python)
add_dependencies(b_Python_TestJoinFiles b_Python_JoinFiles)
set_property(
  TEST b_Python_TestJoinFiles
  APPEND PROPERTY
  ENVIRONMENT
  JOIN_FILES_EXE=$<TARGET_PROPERTY:b_Python_JoinFiles,LOCATION>
)
