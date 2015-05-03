function (B_COPY_FILE SOURCE DEST)
  add_custom_command(
    OUTPUT "${DEST}"
    COMMAND cmake -E copy "${SOURCE}" "${DEST}"
    DEPENDS "${SOURCE}"
    COMMENT "Copying ${SOURCE}"
  )
endfunction ()
