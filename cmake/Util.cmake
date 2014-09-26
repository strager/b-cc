find_package(PkgConfig)
include(CheckCCompilerFlag)

if (CMAKE_VERSION VERSION_LESS 3.0.0)
    # CMake 3 introduced target_include_directories,
    # INCLUDE_DIRECTORIES, and
    # INTERFACE_INCLUDE_DIRECTORIES.  Let's just globally
    # include b's include directories.
    macro (target_include_directories TARGET)
        set(_TID_PRIVATE_INCLUDES False)
        foreach (_TID_ARG ${ARGN})
            if (_TID_ARG STREQUAL SYSTEM)
                # TODO(strager)
                # Skip.
            elseif (_TID_ARG STREQUAL BEFORE)
                # TODO(strager)
                # Skip.
            elseif (_TID_ARG STREQUAL INTERFACE)
                set(_TID_PRIVATE_INCLUDES False)
            elseif (_TID_ARG STREQUAL PUBLIC)
                set(_TID_PRIVATE_INCLUDES False)
            elseif (_TID_ARG STREQUAL PRIVATE)
                set(_TID_PRIVATE_INCLUDES True)
            else ()
                if (_TID_PRIVATE_INCLUDES)
                    set_property(
                        TARGET "${TARGET}"
                        APPEND_STRING
                        PROPERTY COMPILE_FLAGS
                        " -I${CMAKE_CURRENT_SOURCE_DIR}/${_TID_ARG}"
                    )
                else ()
                    include_directories("${_TID_ARG}")
                endif ()
            endif ()
        endforeach ()
    endmacro ()
endif ()

function (append_with_space OUT)
    set(LIST "${${OUT}}")
    foreach (ITEM ${ARGN})
        set(LIST "${LIST} ${ITEM}")
    endforeach ()
    set("${OUT}" "${LIST}" PARENT_SCOPE)
endfunction ()

function (append_compiler_flags)
    set(CFLAGS "${CMAKE_C_FLAGS}")
    append_with_space(CFLAGS ${ARGN})
    set(CMAKE_C_FLAGS "${CFLAGS}" PARENT_SCOPE)

    set(CXXFLAGS "${CMAKE_CXX_FLAGS}")
    append_with_space(CXXFLAGS ${ARGN})
    set(CMAKE_CXX_FLAGS "${CXXFLAGS}" PARENT_SCOPE)
endfunction ()

function (depend_pkgconfig TARGET VARIABLE_PREFIX)
    set(MANUAL_HELP "Set ${VARIABLE_PREFIX}_FOUND=1 and
the following variables as required:

    ${VARIABLE_PREFIX}_LIBRARIES
    ${VARIABLE_PREFIX}_LIBRARY_DIRS
    ${VARIABLE_PREFIX}_LDFLAGS
    ${VARIABLE_PREFIX}_LDFLAGS_OTHER
    ${VARIABLE_PREFIX}_INCLUDE_DIRS
    ${VARIABLE_PREFIX}_CFLAGS
    ${VARIABLE_PREFIX}_CFLAGS_OTHER
")
    if (NOT "${VARIABLE_PREFIX}_FOUND")
        if (
                NOT PKGCONFIG_FOUND
                # CMake <3.0.0's find_package doesn't seem
                # to set PKGCONFIG_FOUND.
                AND NOT DEFINED PKG_CONFIG_FOUND
        )
            message(
                SEND_ERROR
                "CMake PkgConfig find package not available.  ${MANUAL_HELP}"
            )
            return ()
        endif ()
        if (NOT PKG_CONFIG_FOUND)
            message(
                SEND_ERROR
                "pkg-config not found.  ${MANUAL_HELP}"
            )
            return ()
        endif ()

        pkg_search_module("${VARIABLE_PREFIX}" ${ARGN})

        if (NOT "${VARIABLE_PREFIX}_FOUND")
            message(SEND_ERROR "
Could not find required packages: ${ARGN}.  Check that it is
installed and registered with pkg-config.
")
            return ()
        endif ()
    endif ()

    target_include_directories(
        "${TARGET}"
        PRIVATE "${${VARIABLE_PREFIX}_INCLUDE_DIRS}"
    )
    set_property(
        TARGET "${TARGET}"
        APPEND_STRING
        PROPERTY COMPILE_FLAGS
        " ${${VARIABLE_PREFIX}_CFLAGS} ${${VARIABLE_PREFIX}_CFLAGS_OTHER}"
    )
    set_property(
        TARGET "${TARGET}"
        APPEND_STRING
        PROPERTY LINK_FLAGS
        " ${${VARIABLE_PREFIX}_LDFLAGS} ${${VARIABLE_PREFIX}_LDFLAGS_OTHER}"
    )

    foreach (LIB ${${VARIABLE_PREFIX}_LIBRARIES})
        find_library(
            LIB_PATH
            NAMES "${LIB}"
            HINTS ${${VARIABLE_PREFIX}_LIBRARY_DIRS}
        )
        target_link_libraries(
            "${TARGET}"
            PRIVATE "${LIB_PATH}"
        )
    endforeach ()
endfunction ()

function (add_empty_c_library TARGET)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/empty.c" "")
    add_library(
        "${TARGET}"
        ${ARGN}
        "${CMAKE_CURRENT_BINARY_DIR}/empty.c"
    )
    check_c_compiler_flag(
        -Wempty-translation-unit
        HAVE_W_EMPTY_TRANSLATION_UNIT
    )
    if (HAVE_W_EMPTY_TRANSLATION_UNIT)
        set_property(
            TARGET "${TARGET}"
            APPEND_STRING
            PROPERTY COMPILE_FLAGS
            " -Wno-empty-translation-unit"
        )
    endif ()
endfunction ()

function (
    check_c_source_compiles_with_flags
    SOURCE
    OUT
)
    append_with_space(CMAKE_C_FLAGS ${ARGN})
    check_c_source_compiles("${SOURCE}" "${OUT}")
    set("${OUT}" "${${OUT}}" PARENT_SCOPE)
endfunction ()

function (
    check_cxx_source_compiles_with_flags
    SOURCE
    OUT
)
    append_with_space(CMAKE_CXX_FLAGS ${ARGN})
    check_cxx_source_compiles("${SOURCE}" "${OUT}")
    set("${OUT}" "${${OUT}}" PARENT_SCOPE)
endfunction ()
