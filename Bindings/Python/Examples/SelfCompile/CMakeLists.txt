cmake_minimum_required(VERSION 3.0)

include(../../../../CMake/b_python.cmake)

project(b_Python_SelfCompile)

b_add_python_executable(
  b_Python_SelfCompile
  Source/SelfCompile.py
)

b_add_python_executable(
  b_Python_TestSelfCompile
  "${b_SelfCompile_SOURCE_DIR}/Tests/TestSelfCompile.py"
)
b_add_python_test(b_Python_TestSelfCompile MODULES b_Python)
add_dependencies(b_Python_TestSelfCompile b_Python_SelfCompile)
set_property(
  TEST b_Python_TestSelfCompile
  APPEND PROPERTY
  ENVIRONMENT
  SELF_COMPILE_EXE=$<TARGET_PROPERTY:b_Python_SelfCompile,LOCATION>
  "CODE_DIR=${b_SelfCompile_BINARY_DIR}/TestSources"
)
