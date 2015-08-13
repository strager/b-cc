cmake_minimum_required(VERSION 3.0)

include(../../CMake/b_disable_gcc_warnings.cmake)
include(../../CMake/b_python.cmake)
include(CheckCCompilerFlag)

project(b_Python)

find_package(PythonLibs 2.7)
if (NOT PYTHONLIBS_FOUND)
  message(WARNING "B Python support disabled")
  return ()
endif ()

add_library(
  b_Python
  SHARED
  PrivateHeaders/B/Py/Private/Answer.h
  PrivateHeaders/B/Py/Private/AnswerContext.h
  PrivateHeaders/B/Py/Private/AnswerFuture.h
  PrivateHeaders/B/Py/Private/Database.h
  PrivateHeaders/B/Py/Private/FileQuestion.h
  PrivateHeaders/B/Py/Private/Main.h
  PrivateHeaders/B/Py/Private/Process.h
  PrivateHeaders/B/Py/Private/Question.h
  PrivateHeaders/B/Py/Private/QuestionVTable.h
  PrivateHeaders/B/Py/Private/RunLoop.h
  PrivateHeaders/B/Py/Private/Serialize.h
  PrivateHeaders/B/Py/Private/Util.h
  Source/Answer.c
  Source/AnswerContext.c
  Source/AnswerFuture.c
  Source/Database.c
  Source/FileQuestion.c
  Source/Main.c
  Source/Module.c
  Source/Process.c
  Source/Question.c
  Source/QuestionVTable.c
  Source/RunLoop.c
  Source/RunLoopNative.c
  Source/Serialize.c
  Source/Util.c
)
target_include_directories(
  b_Python
  PRIVATE
  ${PYTHON_INCLUDE_DIRS}
  PrivateHeaders
)
target_link_libraries(
  b_Python
  LINK_PRIVATE
  ${PYTHON_LIBRARIES}
  b
)

# We are ssize_t-clean. (Make Python interfaces uses size_t
# instead of int.)
target_compile_definitions(
  b_Python
  PRIVATE
  PY_SSIZE_T_CLEAN
)

# Compile as C99.
check_c_compiler_flag(-std=c99 HAVE_STD_C99)
if (HAVE_STD_C99)
  set_property(
    TARGET b_Python
    APPEND
    PROPERTY COMPILE_OPTIONS -std=c99
  )
endif ()

# TODO(#29): Kill with fire.
b_disable_gcc_warnings(
  TARGET b_Python
  PRIVATE_WARNINGS extended-offsetof main
)

# Allow Python to import our module by naming it _b.so (even
# on OS X).
set_property(TARGET b_Python PROPERTY OUTPUT_NAME _b)
set_property(TARGET b_Python PROPERTY PREFIX "")
set_property(TARGET b_Python PROPERTY SUFFIX .so)

b_add_python_package(
  b_PythonPackage
  SOURCE_BASE Source
  b/__init__.py
  b/twisted.py
)

function (ADD_UNIT_TEST NAME)
  set(TARGET "b_Python_${NAME}")
  b_add_python_executable("${TARGET}" "Tests/${NAME}.py")
  b_add_python_test(
    "${TARGET}"
    MODULES b_Python
    PACKAGES b_PythonPackage
  )
endfunction ()

add_unit_test(TestDatabase)
add_unit_test(TestFileQuestion)
add_unit_test(TestMain)
add_unit_test(TestProcess)
add_unit_test(TestPythonFileQuestion)
add_unit_test(TestRunLoop)

# Must come after b_PythonPackage is defined.
add_subdirectory(Examples/JoinFiles)
add_subdirectory(Examples/SelfCompile)
