cmake_minimum_required(VERSION 3.0)

project(b_sphlib)

add_library(
  b_sphlib_sha2
  STATIC
  sha2.c
  sph_sha2.h
  sph_types.h
)
target_include_directories(b_sphlib_sha2 PUBLIC .)
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
