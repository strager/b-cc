cmake_minimum_required(VERSION 2.8)
enable_testing()

project(b)

include(cmake/Util.cmake)

find_package(Threads REQUIRED)
include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
include(CheckIncludeFile)

function (add_gtest_test NAME)
    add_executable(
        "${NAME}"
        "test/gtest/${NAME}.cc"
    )
    target_link_libraries("${NAME}" b b_mocks b_gmock)
    add_test(
        NAME "${NAME}"
        COMMAND
            "${CMAKE_CURRENT_BINARY_DIR}/RunTest.sh"
            "$<TARGET_FILE:${NAME}>"
    )
    add_dependencies("${NAME}" TestWrapper)
endfunction ()

set(
    SQLITE3_STATIC
    True
    CACHE BOOL "Compile static sqlite3 bundled with B"
)
set(
    STATIC_ANALYSIS
    False
    CACHE BOOL "Only compile source code needed for static analysis"
)

if (STATIC_ANALYSIS AND CMAKE_VERSION VERSION_LESS 3.0.0)
    message(
        FATAL_ERROR
        "CMake >= 3.0.0 is required for static analysis builds"
    )
endif ()

check_c_compiler_flag(-pedantic HAVE_PEDANTIC)
if (HAVE_PEDANTIC)
    check_c_source_compiles_with_flags("
enum Enum {
    A,
};
int main() {
}
" HAVE_PEDANTIC_ALLOWING_TRAILING_COMMAS -Werror -pedantic)
    if (HAVE_PEDANTIC_ALLOWING_TRAILING_COMMAS)
        append_compiler_flags(-pedantic)
    endif ()
endif ()

check_c_compiler_flag(-Wall HAVE_W_ALL)
if (HAVE_W_ALL)
    append_compiler_flags(-Wall)
endif ()

check_c_compiler_flag(-Wextra HAVE_W_EXTRA)
if (HAVE_W_EXTRA)
    append_compiler_flags(-Wextra)
endif ()

check_c_compiler_flag(-Werror HAVE_W_ERROR)
if (HAVE_W_ERROR)
    append_compiler_flags(-Werror)
endif ()

check_c_compiler_flag(
    -Wconditional-uninitialized
    HAVE_W_CONDITIONAL_UNINITIALIZED
)
if (HAVE_W_CONDITIONAL_UNINITIALIZED)
    append_compiler_flags(-Wconditional-uninitialized)
endif ()

check_c_compiler_flag(-fno-exceptions HAVE_F_NO_EXCEPTIONS)

# WONTFIX by GCC:
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=25509
check_c_source_compiles("
__attribute__((warn_unused_result)) int
foo(void) {
    return 1;
}
int
main() {
    return 0;
}
" HAVE_ATTRIBUTE_WARN_UNUSED_RESULT)
if (HAVE_ATTRIBUTE_WARN_UNUSED_RESULT)
    check_c_source_compiles("
__attribute__((warn_unused_result)) int
foo(void) {
    return 1;
}
int
main() {
    (void) foo();
    return 0;
}
" HAVE_SUPPRESSABLE_ATTRIBUTE_WARN_UNUSED_RESULT)
    if (NOT HAVE_SUPPRESSABLE_ATTRIBUTE_WARN_UNUSED_RESULT)
        append_compiler_flags(-Wno-unused-result)
    endif ()
endif ()

# FIXME(strager): We should figure out a better name than
# 'main' so we don't have to worry about this warning. =]
check_c_source_compiles("
int
main() {
    int main = 0;
    return main;
}
" HAVE_SANE_MAIN_TREATMENT)
if (NOT HAVE_SANE_MAIN_TREATMENT)
    check_c_compiler_flag(
        -Wno-main
        HAVE_W_NO_MAIN
    )
    if (HAVE_W_NO_MAIN)
        append_compiler_flags(-Wno-main)
    else ()
        message(SEND_ERROR "C compiler does not support 'main' as a variable name")
    endif ()
endif ()

# TODO(strager): Use configured header file.
check_include_file(valgrind/valgrind.h HAVE_VALGRIND_H)
if (HAVE_VALGRIND_H)
    append_compiler_flags(-DB_CONFIG_VALGRIND)
endif ()

if (STATIC_ANALYSIS)
    # FIXME(strager): This is ld64/Mach-O-specific.
    set(
        IGNORE_UNDEFINED_LINKER_FLAGS
        "-Wl,-undefined -Wl,dynamic_lookup"
    )
    append_with_space(
        CMAKE_EXE_LINKER_FLAGS
        "${IGNORE_UNDEFINED_LINKER_FLAGS}"
    )
    append_with_space(
        CMAKE_SHARED_LINKER_FLAGS
        "${IGNORE_UNDEFINED_LINKER_FLAGS}"
    )
endif ()

check_c_compiler_flag(-std=c11 HAVE_STD_C11)
check_c_compiler_flag(-std=c1x HAVE_STD_C1X)
if (HAVE_STD_C11)
    append_with_space(CMAKE_C_FLAGS -std=c11)
elseif (HAVE_STD_C1X)
    append_with_space(CMAKE_C_FLAGS -std=c1x)
endif ()

check_cxx_compiler_flag(-std=c++11 HAVE_STD_CXX11)
check_cxx_compiler_flag(-std=c++0x HAVE_STD_CXX0X)
if (HAVE_STD_CXX11)
    append_with_space(CMAKE_CXX_FLAGS -std=c++11)
elseif (HAVE_STD_CXX0X)
    append_with_space(CMAKE_CXX_FLAGS -std=c++0x)
endif ()

# For glibc.  Without this, many definitions (functions,
# macros, structs, etc.) are missing from headers when
# compiling with a non-GNU -std.
#
# Ideally we would use _DEFAULT_SOURCE, but that doesn't
# exist in older glibc versions.
add_definitions(-D_POSIX_SOURCE -D_POSIX_C_SOURCE=200112L)

# For Mac OS X, to include BSD extensions such as flock().
# Required if _POSIX_SOURCE is defined.
add_definitions(-D_DARWIN_C_SOURCE)

# These checks must come after checking for -std=c++11.
# Otherwise, we may pick a really old libc++ without C++11
# support instead of a modern libstdc++.
set(CXX_SOURCE "
#include <limits>
#include <string>
int main() {
}
")
check_cxx_source_compiles_with_flags(
    "${CXX_SOURCE}"
    HAVE_STDLIB_LIBCXX
    -stdlib=libc++
)
check_cxx_source_compiles_with_flags(
    "${CXX_SOURCE}"
    HAVE_STDLIB_LIBSTDCXX
    -stdlib=libstdc++
)
if (HAVE_STDLIB_LIBCXX)
    append_with_space(CMAKE_CXX_FLAGS -stdlib=libc++)
elseif (HAVE_STDLIB_LIBSTDCXX)
    append_with_space(CMAKE_CXX_FLAGS -stdlib=libstdc++)
endif ()

check_c_compiler_flag(-fPIC HAVE_F_PIC)
if (HAVE_F_PIC)
    append_compiler_flags(-fPIC)
endif ()

if (SQLITE3_STATIC)
    set(VENDOR_SQLITE3 vendor/sqlite-3.8.4.1)
    if (STATIC_ANALYSIS)
        add_empty_c_library(b_sqlite3 STATIC)
    else ()
        add_library(
            b_sqlite3
            STATIC
            "${VENDOR_SQLITE3}/sqlite3.c"

            "${VENDOR_SQLITE3}/sqlite3.h"
            "${VENDOR_SQLITE3}/sqlite3ext.h"
        )
    endif ()

    target_include_directories(
        b_sqlite3
        PUBLIC "${VENDOR_SQLITE3}"
    )
    target_link_libraries(
        b_sqlite3
        LINK_PRIVATE "${CMAKE_DL_LIBS}"
    )

    # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=56273
    check_cxx_source_compiles("
#include <cstdio>

namespace {
    struct type {
        bool a;
        bool b;

        bool
        get_b() {
            return b;
        }
    };

    type stuff[9u];
}

void
foo() {
  for (unsigned i = 0u; i < 9u; i++) {
      if (!stuff[i].a) {
          continue;
      }
      fprintf(stdout, \"\");
      for (unsigned j = i + 1u; j < 9u; j++) {
          if (stuff[j].a && stuff[j].get_b()) {
              return;
          }
      }
  }
}

int
main() {
    foo();
    return 0;
}
" HAVE_NOT_GCC_BUG_56273)
    check_c_compiler_flag(
        -Wno-array-bounds
        HAVE_W_NO_ARRAY_BOUNDS
    )
    if (
            NOT HAVE_NOT_GCC_BUG_56273
            AND HAVE_W_NO_ARRAY_BOUNDS
    )
        set_property(
            TARGET b_sqlite3
            APPEND_STRING
            PROPERTY COMPILE_FLAGS
            " -Wno-array-bounds"
        )
    endif ()

    check_c_compiler_flag(
        -Wno-conditional-uninitialized
        HAVE_W_NO_CONDITIONAL_UNINITIALIZED
    )
    if (HAVE_W_NO_CONDITIONAL_UNINITIALIZED)
        set_property(
            TARGET b_sqlite3
            APPEND_STRING
            PROPERTY COMPILE_FLAGS
            " -Wno-conditional-uninitialized"
        )
    endif ()

    check_c_compiler_flag(
        -Wno-unused-const-variable
        HAVE_W_NO_UNUSED_CONST_VARIABLE
    )
    if (HAVE_W_NO_UNUSED_CONST_VARIABLE)
        set_property(
            TARGET b_sqlite3
            APPEND_STRING
            PROPERTY COMPILE_FLAGS
            " -Wno-unused-const-variable"
        )

        check_c_compiler_flag(
            -Wno-unused-const-variable
            HAVE_W_NO_UNUSED_CONST_VARIABLE
        )
    endif ()
endif ()

function (b_add_library NAME)
    add_library(
        "${NAME}"
        ${ARGN}

        src/Alloc.c
        src/AnswerContext.c
        src/Assert.c
        src/Database.c
        src/Deserialize.c
        src/Error.c
        src/File.cc
        src/FilePath.c
        src/Log.c
        src/Main.c
        src/Misc.c
        src/OS.c
        src/Process.c
        src/QuestionAnswer.c
        src/QuestionDispatch.c
        src/QuestionQueue.cc
        src/QuestionQueueRoot.c
        src/QuestionVTableSet.c
        src/RefCount.c
        src/SQLite3.c
        src/Serialize.c
        src/Thread.c
        src/UUID.c

        include/B/Alloc.h
        include/B/AnswerContext.h
        include/B/Assert.h
        include/B/Base.h
        include/B/Config.h
        include/B/CXX.h
        include/B/Database.h
        include/B/Deserialize.h
        include/B/Errno.h
        include/B/Error.h
        include/B/File.h
        include/B/FilePath.h
        include/B/Log.h
        include/B/Macro.h
        include/B/Main.h
        include/B/Private/Misc.h
        include/B/Private/OS.h
        include/B/Private/Queue.h
        include/B/Private/RefCount.h
        include/B/Private/SQLite3.h
        include/B/Private/Thread.h
        include/B/Process.h
        include/B/QuestionAnswer.h
        include/B/QuestionDispatch.h
        include/B/QuestionQueue.h
        include/B/QuestionVTableSet.h
        include/B/Serialize.h
        include/B/UUID.h
    )
    set_property(
        TARGET "${NAME}"
        PROPERTY LINKER_LANGUAGE C
    )
    target_include_directories("${NAME}" PUBLIC include)
    if (SQLITE3_STATIC)
        target_link_libraries(
            "${NAME}"
            LINK_PRIVATE b_sqlite3
        )
    else ()
        depend_pkgconfig("${NAME}" SQLITE3 sqlite3>=3.8.3)
    endif ()
    target_link_libraries(
        "${NAME}"
        LINK_PRIVATE "${CMAKE_THREAD_LIBS_INIT}"
    )
    if (HAVE_F_NO_EXCEPTIONS)
        append_target_compiler_flags(
            "${NAME}"
            -fno-exceptions
        )
    endif ()
endfunction ()

b_add_library(b)
if (BUILD_SHARED_LIBS)
    add_library(b_shared ALIAS b)
else ()
    b_add_library(b_shared SHARED)
    set_property(TARGET b_shared PROPERTY OUTPUT_NAME b)
endif ()

set(VENDOR_GMOCK vendor/gmock-1.7.0)
if (STATIC_ANALYSIS)
    add_empty_c_library(b_gmock STATIC)
else ()
    add_library(
        b_gmock
        STATIC
        "${VENDOR_GMOCK}/gtest/src/gtest-all.cc"
        "${VENDOR_GMOCK}/src/gmock-all.cc"
        "${VENDOR_GMOCK}/src/gmock_main.cc"

        "${VENDOR_GMOCK}/gtest/include/gtest/gtest-death-test.h"
        "${VENDOR_GMOCK}/gtest/include/gtest/gtest-message.h"
        "${VENDOR_GMOCK}/gtest/include/gtest/gtest-param-test.h"
        "${VENDOR_GMOCK}/gtest/include/gtest/gtest-printers.h"
        "${VENDOR_GMOCK}/gtest/include/gtest/gtest-spi.h"
        "${VENDOR_GMOCK}/gtest/include/gtest/gtest-test-part.h"
        "${VENDOR_GMOCK}/gtest/include/gtest/gtest-typed-test.h"
        "${VENDOR_GMOCK}/gtest/include/gtest/gtest.h"
        "${VENDOR_GMOCK}/gtest/include/gtest/gtest_pred_impl.h"
        "${VENDOR_GMOCK}/gtest/include/gtest/gtest_prod.h"
        "${VENDOR_GMOCK}/gtest/include/gtest/internal/gtest-death-test-internal.h"
        "${VENDOR_GMOCK}/gtest/include/gtest/internal/gtest-filepath.h"
        "${VENDOR_GMOCK}/gtest/include/gtest/internal/gtest-internal.h"
        "${VENDOR_GMOCK}/gtest/include/gtest/internal/gtest-linked_ptr.h"
        "${VENDOR_GMOCK}/gtest/include/gtest/internal/gtest-param-util-generated.h"
        "${VENDOR_GMOCK}/gtest/include/gtest/internal/gtest-param-util.h"
        "${VENDOR_GMOCK}/gtest/include/gtest/internal/gtest-port.h"
        "${VENDOR_GMOCK}/gtest/include/gtest/internal/gtest-string.h"
        "${VENDOR_GMOCK}/gtest/include/gtest/internal/gtest-tuple.h"
        "${VENDOR_GMOCK}/gtest/include/gtest/internal/gtest-type-util.h"
        "${VENDOR_GMOCK}/gtest/src/gtest-internal-inl.h"
        "${VENDOR_GMOCK}/include/gmock/gmock-actions.h"
        "${VENDOR_GMOCK}/include/gmock/gmock-cardinalities.h"
        "${VENDOR_GMOCK}/include/gmock/gmock-generated-actions.h"
        "${VENDOR_GMOCK}/include/gmock/gmock-generated-function-mockers.h"
        "${VENDOR_GMOCK}/include/gmock/gmock-generated-matchers.h"
        "${VENDOR_GMOCK}/include/gmock/gmock-generated-nice-strict.h"
        "${VENDOR_GMOCK}/include/gmock/gmock-matchers.h"
        "${VENDOR_GMOCK}/include/gmock/gmock-more-actions.h"
        "${VENDOR_GMOCK}/include/gmock/gmock-more-matchers.h"
        "${VENDOR_GMOCK}/include/gmock/gmock-spec-builders.h"
        "${VENDOR_GMOCK}/include/gmock/gmock.h"
        "${VENDOR_GMOCK}/include/gmock/internal/gmock-generated-internal-utils.h"
        "${VENDOR_GMOCK}/include/gmock/internal/gmock-internal-utils.h"
        "${VENDOR_GMOCK}/include/gmock/internal/gmock-port.h"
    )
endif ()
target_include_directories(
    b_gmock
    PUBLIC
        "${VENDOR_GMOCK}/gtest/include"
        "${VENDOR_GMOCK}/include"
    PRIVATE "${VENDOR_GMOCK}/gtest" "${VENDOR_GMOCK}"
)
set_property(
    TARGET b_gmock
    APPEND_STRING
    PROPERTY COMPILE_FLAGS
    " -Wno-missing-field-initializers"
)

check_cxx_source_compiles("
template<typename T1, typename T2>
struct Foo {
};

template<typename... T>
struct Bar {
    typedef Foo<T...> MyFoo;
};

int main() {
}
" HAVE_CXX11_VARIADIC_TEMPLATE_FORWARDING)
if (NOT HAVE_CXX11_VARIADIC_TEMPLATE_FORWARDING)
    set_property(
        TARGET b_gmock
        APPEND
        PROPERTY COMPILE_DEFINITIONS
        GTEST_LANG_CXX11=0
    )
endif ()

add_library(
    b_mocks
    STATIC
    test/gtest/Mocks.cc

    test/gtest/Mocks.h
)
target_link_libraries(b_mocks LINK_PRIVATE b b_gmock)

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/RunTest.sh"
    COMMAND
        cmake
        -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/RunTest.sh"
        "${CMAKE_CURRENT_BINARY_DIR}/RunTest.sh"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/RunTest.sh"
    COMMENT "Copying RunTest.sh"
)
add_custom_target(
    TestWrapper
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/RunTest.sh"
    SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/RunTest.sh"
)

add_gtest_test(TestAlloc)
add_gtest_test(TestAnswerContext)
add_gtest_test(TestDeserialize)
add_gtest_test(TestErrorHandler)
add_gtest_test(TestFile)
add_gtest_test(TestMain)
add_gtest_test(TestMisc)
add_gtest_test(TestProcess)
add_gtest_test(TestQuestionDispatch)
add_gtest_test(TestQuestionQueue)
add_gtest_test(TestSerialize)
add_gtest_test(TestUUID)

add_executable(TestProcessChild
               test/gtest/TestProcessChild.c)
add_dependencies(TestProcess TestProcessChild)

add_executable(ex1 ex/1/main.cc)
target_link_libraries(ex1 b)
set_property(
    TARGET ex1
    PROPERTY LINKER_LANGUAGE C
)
if (HAVE_F_NO_EXCEPTIONS)
    append_target_compiler_flags(ex1 -fno-exceptions)
endif ()
