cmake_minimum_required(VERSION 3.0)
enable_testing()

include(CheckCXXCompilerFlag)

project(b_TestUnit)

add_library(
  b_UnitUtil
  STATIC
  Util/Environ.c
  Util/Environ.h
  Util/Executable.h
  Util/TemporaryDirectory.h
)

function (ADD_UNIT_TEST NAME)
  add_executable("${NAME}" "${NAME}.cc")
  target_link_libraries(
    "${NAME}"
    b
    b_UnitUtil
    gtest
    gtest_main
  )
  # FIXME(strager): Should we expose an INTERFACE library
  # for the private headers?
  target_include_directories(
    "${NAME}"
    PRIVATE
    ../../PrivateHeaders
  )
  add_test(
    NAME "${NAME}"
    COMMAND "${B_RUN_TEST_WRAPPER}" "$<TARGET_FILE:${NAME}>"
  )
  add_dependencies("${NAME}" b_RunTestWrapper)
endfunction ()

ADD_UNIT_TEST(TestAnswerFuture)
ADD_UNIT_TEST(TestFileQuestion)
ADD_UNIT_TEST(TestRunLoop)
ADD_UNIT_TEST(TestSerialize)
ADD_UNIT_TEST(TestUUID)
