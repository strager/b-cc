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

function (B_ADD_PYTHON_TEST NAME)
  cmake_parse_arguments("" "" "" MODULES ${ARGN})
  get_target_property(LOCATION "${NAME}" LOCATION)
  add_test(NAME "${NAME}" COMMAND "${LOCATION}")
  add_dependencies("${NAME}" b_RunTestWrapper)
  set_property(
    TEST "${NAME}"
    APPEND PROPERTY
    ENVIRONMENT "B_RUN_TEST_SH=${B_RUN_TEST_WRAPPER}"
  )
  foreach (MODULE ${_MODULES})
    set_property(
      TEST "${NAME}"
      APPEND PROPERTY
      ENVIRONMENT
      "PYTHONPATH=\${PYTHONPATH}:$<TARGET_FILE_DIR:${MODULE}>"
    )
  endforeach ()
endfunction ()
