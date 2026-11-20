set(DOQE_COMMIT unknown)
set(DOQE_DIRTY false)

if(GIT_EXECUTABLE)
  execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --show-prefix
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE root_result OUTPUT_VARIABLE prefix
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  if(root_result EQUAL 0 AND prefix STREQUAL "")
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
      WORKING_DIRECTORY "${SOURCE_DIR}"
      RESULT_VARIABLE commit_result OUTPUT_VARIABLE commit
      OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(commit_result EQUAL 0)
      set(DOQE_COMMIT "${commit}")
      execute_process(COMMAND "${GIT_EXECUTABLE}" --no-optional-locks status --porcelain --untracked-files=normal
        WORKING_DIRECTORY "${SOURCE_DIR}"
        OUTPUT_VARIABLE changes COMMAND_ERROR_IS_FATAL ANY)
      if(NOT changes STREQUAL "")
        set(DOQE_DIRTY true)
      endif()
    endif()
  endif()
endif()

configure_file("${CMAKE_CURRENT_LIST_DIR}/build_version.hpp.in" "${VERSION_HEADER}" @ONLY)
