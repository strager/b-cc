cmake_minimum_required(VERSION 3.0)

project(b_sqlite)

add_library(b_sqlite sqlite3.c sqlite3.h sqlite3ext.h)
target_include_directories(b_sqlite PUBLIC .)
