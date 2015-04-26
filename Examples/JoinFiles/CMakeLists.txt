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
