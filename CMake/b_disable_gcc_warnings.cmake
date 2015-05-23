if (NOT CMAKE_VERSION VERSION_LESS 3.1)
  cmake_policy(SET CMP0054 OLD)
endif ()

include(CMakeParseArguments)
include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

function (B_TARGET_LANGUAGE TARGET OUTVAR)
  # HACK(strager): The LANGUAGE source property doesn't seem
  # to work (always empty), and neither does LINKER_LANGUAGE
  # (always empty). Make a guess based on the SOURCES list.
  get_target_property(TARGET_SOURCES "${TARGET}" SOURCES)
  if (TARGET_SOURCES MATCHES "\\.(cc|cpp)(;$)")
    set("${OUTVAR}" CXX PARENT_SCOPE)
  else ()
    set("${OUTVAR}" CXX PARENT_SCOPE)
  endif ()
endfunction ()

function (B_DISABLE_GCC_WARNING_ TARGET WARNING LANG PUBLIC)
  set(WARNING_VAR "${WARNING}")
  string(REPLACE - _ WARNING_VAR "${WARNING_VAR}")
  string(REPLACE + X WARNING_VAR "${WARNING_VAR}")
  string(TOUPPER "${WARNING_VAR}" WARNING_VAR)
  set(WARNING_VAR "HAVE_W_${WARNING_VAR}")
  if (LANG STREQUAL C)
    check_c_compiler_flag("-W${WARNING}" "${WARNING_VAR}")
  elseif (LANG STREQUAL CXX)
    check_cxx_compiler_flag(
      "-W${WARNING}"
      "${WARNING_VAR}"
    )
  else ()
    message(
      SEND_ERROR "${TARGET} has unknown language ${LANG}"
    )
    return ()
  endif ()
  if ("${WARNING_VAR}")
    set_property(
      TARGET "${TARGET}"
      APPEND
      PROPERTY COMPILE_OPTIONS "-Wno-${WARNING}"
    )
    if (PUBLIC)
      set_property(
        TARGET "${TARGET}"
        APPEND
        PROPERTY INTERFACE_COMPILE_OPTIONS "-Wno-${WARNING}"
      )
    endif ()
  endif ()
endfunction ()

function (B_DISABLE_GCC_WARNINGS)
  cmake_parse_arguments(
    ""
    ""
    TARGET
    "PRIVATE_WARNINGS;WARNINGS"
    "${ARGN}"
  )
  if (NOT TARGET "${_TARGET}")
    message(SEND_ERROR "${_TARGET} is not a target")
    return ()
  endif ()
  b_target_language("${_TARGET}" LANG)
  if (NOT LANG)
    message(
      SEND_ERROR "Could not determine language of ${_TARGET}"
    )
    return ()
  endif ()
  foreach (WARNING ${_WARNINGS})
    b_disable_gcc_warning_(
      "${_TARGET}"
      "${WARNING}"
      "${LANG}"
      True
    )
  endforeach ()
  foreach (WARNING ${_PRIVATE_WARNINGS})
    b_disable_gcc_warning_(
      "${_TARGET}"
      "${WARNING}"
      "${LANG}"
      False
    )
  endforeach ()
endfunction ()
