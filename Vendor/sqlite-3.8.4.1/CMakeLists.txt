cmake_minimum_required(VERSION 3.0)

include(CheckCCompilerFlag)

project(b_sqlite)

add_library(b_sqlite sqlite3.c sqlite3.h sqlite3ext.h)
target_include_directories(b_sqlite PUBLIC .)
target_link_libraries(b_sqlite PRIVATE ${CMAKE_DL_LIBS})

# If CMAKE_C_FLAGS is set to include -Wall, for example,
# some compilers find "errors" in sqlite3.c. B developers
# don't care about warnings in b_sqlite. Suppress them.

check_c_compiler_flag(-w HAVE_LOWER_W)
if (HAVE_LOWER_W)
  set_property(
    TARGET b_sqlite
    APPEND
    PROPERTY COMPILE_OPTIONS -w
  )
endif ()
