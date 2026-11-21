# Version text and build provenance are private details of the CLI command.
doqe_add_module(cli version.cpp)

add_custom_target(doqe_build_version
  COMMAND "${CMAKE_COMMAND}"
    "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
    "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
    "-DVERSION_HEADER=${CMAKE_CURRENT_BINARY_DIR}/build_version.hpp"
    "-DDOQE_VERSION=${PROJECT_VERSION}"
    -P "${PROJECT_SOURCE_DIR}/cmake/build-version.cmake"
  BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/build_version.hpp"
  VERBATIM)

add_executable(doqe main.cpp)
set_target_properties(doqe PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
add_dependencies(doqe doqe_build_version)
target_include_directories(doqe PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(doqe PRIVATE doqe_cli)
