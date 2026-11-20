cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/llvm.cmake")
endif()

include(cmake/require-llvm.cmake)
doqe_check_compiler_environment()

# Load the toolchain before compiler identification so configuration can reject
# a missing LLVM root or a toolchain-provided launcher first.
project(doqe VERSION 0.1.0 LANGUAGES NONE)
doqe_check_compiler_environment()
if(BUILD_SHARED_LIBS)
  message(FATAL_ERROR "BUILD_SHARED_LIBS=ON requires an exported-symbols ADR. See docs/arch/README.md.")
endif()
if(NOT DOQE_LLVM_ROOT)
  message(FATAL_ERROR "DOQE_LLVM_ROOT is empty; use the doqe toolchain file cmake/llvm.cmake")
endif()

# Identify the compilers without linking before the pin is checked.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
enable_language(C CXX)
doqe_verify_llvm()
unset(CMAKE_TRY_COMPILE_TARGET_TYPE)

if(NOT CMAKE_GENERATOR STREQUAL "Ninja")
  message(FATAL_ERROR "doqe requires the single-config Ninja generator")
endif()
execute_process(COMMAND "${CMAKE_MAKE_PROGRAM}" --version
  OUTPUT_VARIABLE ninja_version OUTPUT_STRIP_TRAILING_WHITESPACE
  COMMAND_ERROR_IS_FATAL ANY)
if(ninja_version VERSION_LESS 1.12.1)
  message(FATAL_ERROR "doqe requires Ninja 1.12.1 or newer; found ${ninja_version}")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
include(cmake/pinned-runtime.cmake)
include(cmake/sanitizers.cmake)

find_package(Git QUIET)
add_custom_target(doqe_build_version
  COMMAND "${CMAKE_COMMAND}"
    "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
    "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
    "-DVERSION_HEADER=${CMAKE_CURRENT_BINARY_DIR}/build_version.hpp"
    "-DDOQE_VERSION=${PROJECT_VERSION}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/build-version.cmake"
  BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/build_version.hpp"
  VERBATIM)

add_executable(doqe src/main.cpp src/version.cpp)
add_dependencies(doqe doqe_build_version)
target_include_directories(doqe PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

include(CTest)
if(BUILD_TESTING)
  include(FetchContent)
  set(BUILD_GMOCK OFF)
  set(INSTALL_GTEST OFF)
  FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG 52eb8108c5bdec04579160ae17225d66034bd723)
  FetchContent_MakeAvailable(googletest)

  add_executable(doqe_tests tests/version_test.cpp tests/unwind_test.cpp
    tests/throw_for_unwind_test.cpp src/version.cpp)
  target_include_directories(doqe_tests PRIVATE src)
  target_link_libraries(doqe_tests PRIVATE GTest::gtest_main)
  include(GoogleTest)
  gtest_discover_tests(doqe_tests)
  add_test(NAME CLI.Version COMMAND "${CMAKE_COMMAND}"
    "-DDOQE=$<TARGET_FILE:doqe>" -DARGUMENT=--version -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-cli.cmake")
  add_test(NAME CLI.UnknownArgument COMMAND "${CMAKE_COMMAND}"
    "-DDOQE=$<TARGET_FILE:doqe>" -DARGUMENT=--unknown -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-cli.cmake")
  add_test(NAME Runtime.SharedDependencies COMMAND "${CMAKE_COMMAND}"
    "-DHOST_SYSTEM=${CMAKE_SYSTEM_NAME}"
    "-DSANITIZED=${DOQE_ENABLE_SANITIZERS}"
    "-DLLVM_NM=${DOQE_LLVM_ROOT}/bin/llvm-nm"
    "-DDOQE=$<TARGET_FILE:doqe>" "-DDOQE_TESTS=$<TARGET_FILE:doqe_tests>"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-runtime-dependencies.cmake")
endif()

include(cmake/quality.cmake)
