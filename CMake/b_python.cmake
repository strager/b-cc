include("${CMAKE_CURRENT_LIST_DIR}/b_copy_file.cmake")
include(CMakeParseArguments)

function (B_ADD_PYTHON_EXECUTABLE NAME SOURCE)
  get_filename_component(SOURCE_NAME "${SOURCE}" NAME)
  set(DEST "${CMAKE_CURRENT_BINARY_DIR}/${SOURCE_NAME}")
  b_copy_file("${SOURCE}" "${DEST}")
  add_custom_target(
    "${NAME}"
    ALL
    DEPENDS "${DEST}"
    SOURCES "${SOURCE}"
  )
  set_target_properties(
    "${NAME}"
    PROPERTIES
    LOCATION "${DEST}"
    OUTPUT_NAME "${SOURCE_NAME}"
  )
endfunction ()

function (B_ADD_PYTHON_PACKAGE NAME)
  cmake_parse_arguments("" "" SOURCE_BASE "" ${ARGN})
  set(RAW_SOURCES "${_UNPARSED_ARGUMENTS}")
  if (NOT _SOURCE_BASE)
    message(SEND_ERROR "SOURCE_BASE must be set")
    return ()
  endif ()
  set(SOURCES)
  set(DESTS)
  set(DEST_DIR "${CMAKE_CURRENT_BINARY_DIR}/${NAME}")
  foreach (RAW_SOURCE ${RAW_SOURCES})
    set(SOURCE "${_SOURCE_BASE}/${RAW_SOURCE}")
    set(DEST "${DEST_DIR}/${RAW_SOURCE}")
    b_copy_file("${SOURCE}" "${DEST}")
    list(APPEND SOURCES "${SOURCE}")
    list(APPEND DESTS "${DEST}")
  endforeach ()
  add_custom_target(
    "${NAME}"
    ALL
    DEPENDS ${DESTS}
    SOURCES ${SOURCES}
  )
  set_target_properties(
    "${NAME}"
    PROPERTIES
    LOCATION "${DEST_DIR}"
  )
endfunction ()

function (B_APPEND_PYTHON_PATH TEST_NAME)
  # TODO(strager): Error on ":".
  string(REPLACE ";" : PYTHONPATH "${ARGN}")
  set_property(
    TEST "${TEST_NAME}"
    APPEND PROPERTY
    ENVIRONMENT
    "PYTHONPATH=\${PYTHONPATH}:${PYTHONPATH}"
  )
endfunction ()

function (B_ADD_PYTHON_TEST NAME)
  cmake_parse_arguments(
    ""
    ""
    ""
    "MODULES;PACKAGES"
    ${ARGN}
  )
  if (DEFINED _UNPARSED_ARGUMENTS)
    message(
      SEND_ERROR
      "Unexpected arguments: ${_UNPARSED_ARGUMENTS}"
    )
    return ()
  endif ()
  get_target_property(LOCATION "${NAME}" LOCATION)
  add_test(NAME "${NAME}" COMMAND "${LOCATION}")
  add_dependencies("${NAME}" b_RunTestWrapper)
  set_property(
    TEST "${NAME}"
    APPEND PROPERTY
    ENVIRONMENT "B_RUN_TEST_SH=${B_RUN_TEST_WRAPPER}"
  )
  foreach (MODULE ${_MODULES})
    b_append_python_path(
      "${NAME}"
      "$<TARGET_FILE_DIR:${MODULE}>"
    )
    add_dependencies("${NAME}" "${MODULE}")
  endforeach ()
  foreach (PACKAGE ${_PACKAGES})
    get_target_property(LOCATION "${PACKAGE}" LOCATION)
    b_append_python_path("${NAME}" "${LOCATION}")
    add_dependencies("${NAME}" "${PACKAGE}")
  endforeach ()
  if (DEFINED _MODULES OR DEFINED _PACKAGES)
    add_dependencies("${NAME}" ${_MODULES} ${_PACKAGES})
  endif ()
endfunction ()
