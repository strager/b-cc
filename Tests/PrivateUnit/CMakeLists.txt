cmake_minimum_required(VERSION 3.0)
enable_testing()

project(b_TestUnit)

add_library(b_PrivateUnitUtil
            STATIC
            Environ.c
            Environ.h
            Executable.h)

function (ADD_UNIT_TEST NAME)
  add_executable("${NAME}" "${NAME}.cc")
  target_link_libraries(
    "${NAME}"
    b
    b_PrivateUnitUtil
    gtest
    gtest_main
  )
  # FIXME(strager): It'd be nice to use b_SOURCE_DIR here.
  target_include_directories(
    "${NAME}"
    PRIVATE
    ../../PrivateHeaders
  )
  add_test(NAME "${NAME}" COMMAND "$<TARGET_FILE:${NAME}>")
endfunction ()

ADD_UNIT_TEST(TestAnswerFuture)
ADD_UNIT_TEST(TestRunLoop)
