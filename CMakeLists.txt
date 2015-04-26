cmake_minimum_required(VERSION 3.0)
enable_testing()

add_subdirectory(Examples/JoinFiles)
add_subdirectory(Test/Unit)
add_subdirectory(Vendor/gmock-1.7.0)
add_subdirectory(Vendor/sqlite-3.8.4.1)

# Omissions from CMakeLists.txt of gmock and gtest.
target_include_directories(
  gmock
  INTERFACE
  ${gmock_SOURCE_DIR}/include
  ${gtest_SOURCE_DIR}/include
)
target_include_directories(
  gtest
  INTERFACE
  ${gtest_SOURCE_DIR}/include
)

project(b)

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
  Headers/B/Serialize.h
  Headers/B/UUID.h
  PrivateHeaders/B/Private/AnswerContext.h
  PrivateHeaders/B/Private/AnswerFuture.h
  PrivateHeaders/B/Private/Assertions.h
  PrivateHeaders/B/Private/Database.h
  PrivateHeaders/B/Private/Log.h
  PrivateHeaders/B/Private/Math.h
  PrivateHeaders/B/Private/Memory.h
  PrivateHeaders/B/Private/Mutex.h
  PrivateHeaders/B/Private/Queue.h
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
  Source/SQLite3.c
  Source/Serialize.c
  Source/UUID.c
)
target_include_directories(b PUBLIC Headers)
target_include_directories(b PRIVATE PrivateHeaders)
target_link_libraries(b PUBLIC b_sqlite)
