cmake_minimum_required(VERSION 3.0)

include(CMake/b_copy_file.cmake)
include(CMake/b_disable_gcc_warnings.cmake)

project(b)
set(CTEST_NEW_FORMAT True)
include(CTest)  # Requires project().

# b_Python needs PIC for ELF platforms.
set(CMAKE_POSITION_INDEPENDENT_CODE True)

# Must be first.
add_subdirectory(CMake)

add_subdirectory(Examples/JoinFiles)
add_subdirectory(Examples/SelfCompile)

# Must be after Examples.
add_subdirectory(Bindings/Python)

add_subdirectory(Tests/Unit)
add_subdirectory(Vendor/gtest-1.7.0)
add_subdirectory(Vendor/sphlib-3.0/c)
add_subdirectory(Vendor/sqlite-3.8.4.1)

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

# Omissions from CMakeLists.txt of gtest.
target_include_directories(
  gtest
  INTERFACE
  ${gtest_SOURCE_DIR}/include
)
b_disable_gcc_warnings(
    TARGET gtest
    PRIVATE_WARNINGS missing-field-initializers
    WARNINGS long-long variadic-macros
)

find_package(Threads REQUIRED)

add_library(
  b
  Headers/B/AnswerContext.h
  Headers/B/AnswerFuture.h
  Headers/B/Attributes.h
  Headers/B/Database.h
  Headers/B/Error.h
  Headers/B/FileQuestion.h
  Headers/B/Main.h
  Headers/B/Memory.h
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
  Source/Process.c
  Source/QuestionAnswer.c
  Source/RunLoop.c
  Source/RunLoopKqueue.c
  Source/RunLoopSigchld.c
  Source/RunLoopUtil.c
  Source/SQLite3.c
  Source/Serialize.c
  Source/UUID.c
)
target_include_directories(b PUBLIC Headers)
target_include_directories(b PRIVATE PrivateHeaders)
target_link_libraries(b PUBLIC b_sqlite)
target_link_libraries(
  b
  PRIVATE
  ${CMAKE_THREAD_LIBS_INIT}
  b_sphlib_sha2
)

# Compile as C99.
check_c_compiler_flag(-std=c99 HAVE_STD_C99)
if (HAVE_STD_C99)
  set_property(
    TARGET b
    APPEND
    PROPERTY COMPILE_OPTIONS -std=c99
  )
endif ()

# TODO(#29): Kill with fire.
b_disable_gcc_warnings(
  TARGET b
  PRIVATE_WARNINGS extended-offsetof main
)

target_compile_definitions(
  b
  PRIVATE

  # For glibc. Without this, many definitions (functions,
  # macros, structs, etc.) are missing from headers when
  # compiling with a non-GNU -std.
  #
  # Ideally we would use _DEFAULT_SOURCE, but that doesn't
  # exist in older glibc versions.
  -D_POSIX_SOURCE
  -D_POSIX_C_SOURCE=200809L

  # For Mac OS X, to include BSD extensions such as flock().
  # Required if _POSIX_SOURCE is defined.
  -D_DARWIN_C_SOURCE
)
