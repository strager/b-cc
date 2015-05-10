function (B_COPY_FILE SOURCE DEST)
  get_filename_component(ABS_SOURCE "${SOURCE}" ABSOLUTE)
  get_filename_component(ABS_DEST "${DEST}" ABSOLUTE)
  add_custom_command(
    OUTPUT "${DEST}"
    COMMAND cmake -E copy "${ABS_SOURCE}" "${ABS_DEST}"
    DEPENDS "${SOURCE}"
    COMMENT "Copying ${SOURCE}"
  )
endfunction ()
