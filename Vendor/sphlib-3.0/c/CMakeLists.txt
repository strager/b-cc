cmake_minimum_required(VERSION 3.0)

include(CheckCCompilerFlag)

project(b_sphlib)

add_library(
  b_sphlib_sha2
  STATIC
  sha2.c
  sph_sha2.h
  sph_types.h
)
target_include_directories(b_sphlib_sha2 PUBLIC .)
check_c_compiler_flag(
  -Wlong-long
  HAVE_W_LONG_LONG
)
if (HAVE_W_LONG_LONG)
  set_property(
    TARGET b_sphlib_sha2
    APPEND
    PROPERTY COMPILE_OPTIONS -Wno-long-long
  )
  set_property(
    TARGET b_sphlib_sha2
    APPEND
    PROPERTY INTERFACE_COMPILE_OPTIONS -Wno-long-long
  )
endif ()
add_library(
  b_sphlib_utest
  STATIC
  utest.c
  utest.h
)
target_include_directories(b_sphlib_utest PUBLIC .)
add_executable(
  b_sphlib_test_sha2
  test_sha2.c
)
target_link_libraries(
  b_sphlib_test_sha2
  b_sphlib_sha2
  b_sphlib_utest
)
target_include_directories(b_sphlib_test_sha2 PUBLIC .)
add_test(
  NAME b_sphlib_test_sha2
  COMMAND $<TARGET_FILE:b_sphlib_test_sha2>
)
