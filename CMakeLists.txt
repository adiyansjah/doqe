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
get_directory_property(doqe_toolchain_link_options LINK_OPTIONS)

find_package(Git QUIET)
include(cmake/modules.cmake)
foreach(module IN LISTS doqe_modules)
  add_subdirectory("src/${module}")
endforeach()
cmake_language(DEFER CALL doqe_check_module_links)

include(CTest)
if(BUILD_TESTING)
  include(FetchContent)
  set(BUILD_GMOCK OFF)
  set(INSTALL_GTEST OFF)
  FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG 52eb8108c5bdec04579160ae17225d66034bd723)
  FetchContent_MakeAvailable(googletest)

  add_executable(doqe_tests tests/cli/version_test.cpp tests/base/status_test.cpp
    tests/base/memory_pool_test.cpp tests/column/column_test.cpp tests/integration/unwind_test.cpp
    tests/integration/throw_for_unwind_test.cpp)
  set_source_files_properties(tests/cli/version_test.cpp PROPERTIES
    INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/src/cli")
  set_source_files_properties(tests/base/memory_pool_test.cpp PROPERTIES
    INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/src/base")
  set_source_files_properties(tests/column/column_test.cpp PROPERTIES
    INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/src/column")
  add_library(doqe_base_test STATIC src/base/memory_pool.cpp)
  target_compile_definitions(doqe_base_test PRIVATE DOQE_TEST_HOOKS)
  target_link_libraries(doqe_base_test PRIVATE doqe_base)
  target_link_libraries(doqe_tests PRIVATE doqe_cli GTest::gtest_main doqe_base_test doqe_base doqe_column)
  add_test(NAME Base.ProductionArchiveHasNoTestHooks COMMAND "${CMAKE_COMMAND}"
    "-DLLVM_NM=${DOQE_LLVM_ROOT}/bin/llvm-nm" "-DARCHIVE=$<TARGET_FILE:doqe_base>"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/check-base-symbols.cmake")
  include(GoogleTest)
  gtest_discover_tests(doqe_tests)
  add_test(NAME CLI.Version COMMAND "${CMAKE_COMMAND}"
    "-DDOQE=$<TARGET_FILE:doqe>" -DARGUMENT=--version -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/check-cli.cmake")
  add_test(NAME CLI.UnknownArgument COMMAND "${CMAKE_COMMAND}"
    "-DDOQE=$<TARGET_FILE:doqe>" -DARGUMENT=--unknown -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/check-cli.cmake")
  add_test(NAME Runtime.SharedDependencies COMMAND "${CMAKE_COMMAND}"
    "-DHOST_SYSTEM=${CMAKE_SYSTEM_NAME}"
    "-DSANITIZED=${DOQE_ENABLE_SANITIZERS}"
    "-DLLVM_NM=${DOQE_LLVM_ROOT}/bin/llvm-nm"
    "-DDOQE=$<TARGET_FILE:doqe>" "-DDOQE_TESTS=$<TARGET_FILE:doqe_tests>"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/check-runtime-dependencies.cmake")
endif()

include(cmake/module-boundaries.cmake)
include(cmake/quality.cmake)
