cmake_minimum_required(VERSION 3.0)

include(../../../CMake/b_disable_gcc_warnings.cmake)

project(b_sphlib)

add_library(
  b_sphlib_sha2
  STATIC
  sha2.c
  sph_sha2.h
  sph_types.h
)
target_include_directories(b_sphlib_sha2 PUBLIC .)
b_disable_gcc_warnings(
  TARGET b_sphlib_sha2
  PRIVATE_WARNINGS
  long-long
  shorten-64-to-32
  sign-conversion
  # Should be under WARNINGS, but we don't want -Wno-undef
  # for the B library.
  undef
)
add_library(
  b_sphlib_utest
  STATIC
  utest.c
  utest.h
)
target_include_directories(b_sphlib_utest PUBLIC .)
b_disable_gcc_warnings(
  TARGET b_sphlib_utest
  PRIVATE_WARNINGS sign-conversion
)
add_executable(
  b_sphlib_test_sha2
  test_sha2.c
)
target_link_libraries(
  b_sphlib_test_sha2
  b_sphlib_sha2
  b_sphlib_utest
)
b_disable_gcc_warnings(
  TARGET b_sphlib_test_sha2
  PRIVATE_WARNINGS long-long undef
)
target_include_directories(b_sphlib_test_sha2 PUBLIC .)
add_test(
  NAME b_sphlib_test_sha2
  COMMAND $<TARGET_FILE:b_sphlib_test_sha2>
)
