add_custom_target(format
  COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check-format.sh"
    "${GIT_EXECUTABLE}" "${DOQE_LLVM_ROOT}/bin/clang-format"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  VERBATIM)

add_custom_target(lint
  COMMAND "${CMAKE_COMMAND}"
    "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
    "-DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
    "-DCLANG_TIDY=${DOQE_LLVM_ROOT}/bin/clang-tidy"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check-lint.cmake"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  DEPENDS doqe_build_version
  VERBATIM)
