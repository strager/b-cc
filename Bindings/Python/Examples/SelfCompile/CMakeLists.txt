cmake_minimum_required(VERSION 3.0)

include(../../../../CMake/b_python.cmake)

project(b_Python_SelfCompile)

foreach (NAME SelfCompile SelfCompileTwisted)
  b_add_python_executable(
    "b_Python_${NAME}"
    "Source/${NAME}.py"
  )
  b_add_python_executable(
    "b_Python_Test${NAME}"
    "${b_SelfCompile_SOURCE_DIR}/Tests/TestSelfCompile.py"
  )
  b_add_python_test(
    "b_Python_Test${NAME}"
    MODULES b_Python
    PACKAGES b_PythonPackage
  )
  add_dependencies(
    "b_Python_Test${NAME}"
    "b_Python_${NAME}"
  )
  set_property(
    TEST "b_Python_Test${NAME}"
    APPEND PROPERTY
    ENVIRONMENT
    "SELF_COMPILE_EXE=$<TARGET_PROPERTY:b_Python_${NAME},LOCATION>"
    "CODE_DIR=${b_SelfCompile_BINARY_DIR}/TestSources"
  )
endforeach ()
