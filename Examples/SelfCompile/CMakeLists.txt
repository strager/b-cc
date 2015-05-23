cmake_minimum_required(VERSION 2.8)

include(../../CMake/b_disable_gcc_warnings.cmake)
include(CheckCCompilerFlag)

project(b_SelfCompile)

add_executable(b_SelfCompile Source/Main.c)
target_link_libraries(b_SelfCompile b)

# Compile as C99.
check_c_compiler_flag(-std=c99 HAVE_STD_C99)
if (HAVE_STD_C99)
  set_property(
    TARGET b_SelfCompile
    APPEND
    PROPERTY COMPILE_OPTIONS -std=c99
  )
endif ()

# TODO(#29): Kill with fire.
b_disable_gcc_warnings(
  TARGET b_SelfCompile
  PRIVATE_WARNINGS main
)

target_compile_definitions(
  b_SelfCompile
  PRIVATE

  # For glibc. Without this, many definitions (functions,
  # macros, structs, etc.) are missing from headers when
  # compiling with a non-GNU -std.
  #
  # Ideally we would use _DEFAULT_SOURCE, but that doesn't
  # exist in older glibc versions.
  -D_POSIX_SOURCE
  -D_POSIX_C_SOURCE=200809L
)

set(
  SOURCE_FILES
  Examples/SelfCompile/Source/Main.c
  Headers/B/AnswerContext.h
  Headers/B/AnswerFuture.h
  Headers/B/Attributes.h
  Headers/B/Database.h
  Headers/B/Error.h
  Headers/B/FileQuestion.h
  Headers/B/Main.h
  Headers/B/Memory.h
  Headers/B/Process.h
  Headers/B/QuestionAnswer.h
  Headers/B/RunLoop.h
  Headers/B/Serialize.h
  Headers/B/UUID.h
  PrivateHeaders/B/Private/AnswerContext.h
  PrivateHeaders/B/Private/AnswerFuture.h
  PrivateHeaders/B/Private/Assertions.h
  PrivateHeaders/B/Private/Callback.h
  PrivateHeaders/B/Private/Config.h
  PrivateHeaders/B/Private/Database.h
  PrivateHeaders/B/Private/Log.h
  PrivateHeaders/B/Private/Main.h
  PrivateHeaders/B/Private/Math.h
  PrivateHeaders/B/Private/Memory.h
  PrivateHeaders/B/Private/Mutex.h
  PrivateHeaders/B/Private/Queue.h
  PrivateHeaders/B/Private/RunLoopUtil.h
  PrivateHeaders/B/Private/SQLite3.h
  Source/AnswerContext.c
  Source/AnswerFuture.c
  Source/Assertions.c
  Source/Database.c
  Source/FileQuestion.c
  Source/Main.c
  Source/Memory.c
  Source/Mutex.c
  Source/QuestionAnswer.c
  Source/RunLoop.c
  Source/RunLoopKqueue.c
  Source/RunLoopSigchld.c
  Source/RunLoopUtil.c
  Source/SQLite3.c
  Source/Serialize.c
  Source/UUID.c
  Vendor/sphlib-3.0/c/md_helper.c
  Vendor/sphlib-3.0/c/sha2.c
  Vendor/sphlib-3.0/c/sph_sha2.h
  Vendor/sphlib-3.0/c/sph_types.h
  Vendor/sqlite-3.8.4.1/sqlite3.c
  Vendor/sqlite-3.8.4.1/sqlite3.h
)
set(CODE_DIR "${CMAKE_CURRENT_BINARY_DIR}/TestSources")
set(CODE_FILES)
foreach (FILE ${SOURCE_FILES})
  b_copy_file(
    "${b_SOURCE_DIR}/${FILE}"
    "${CODE_DIR}/${FILE}"
  )
  list(APPEND CODE_FILES "${CODE_DIR}/${FILE}")
endforeach ()
add_custom_target(
  TestSelfCompileSources
  DEPENDS ${CODE_FILES}
)
b_add_python_executable(
  TestSelfCompile
  Tests/TestSelfCompile.py
)
add_dependencies(TestSelfCompile TestSelfCompileSources)
b_add_python_test(TestSelfCompile)
set_property(
  TEST TestSelfCompile
  APPEND PROPERTY
  ENVIRONMENT
  SELF_COMPILE_EXE=$<TARGET_FILE:b_SelfCompile>
  "CODE_DIR=${CODE_DIR}"
)
