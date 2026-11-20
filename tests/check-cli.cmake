execute_process(COMMAND "${DOQE}" "${ARGUMENT}"
  RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)

if(ARGUMENT STREQUAL "--version")
  if(NOT status STREQUAL "0" OR
      NOT output MATCHES "^doqe 0\\.1\\.0 \\(([0-9a-f]+(-dirty)?|unknown)\\)\n$")
    message(FATAL_ERROR "${DOQE} ${ARGUMENT} exited ${status}\n${output}${error}")
  endif()
elseif(NOT status STREQUAL "1")
  message(FATAL_ERROR "${DOQE} ${ARGUMENT} exited ${status}; expected 1\n${output}${error}")
endif()
