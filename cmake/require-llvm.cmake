function(doqe_check_compiler_environment)
  foreach(variable IN ITEMS CPATH C_INCLUDE_PATH CPLUS_INCLUDE_PATH OBJC_INCLUDE_PATH LIBRARY_PATH)
    if(DEFINED ENV{${variable}})
      message(FATAL_ERROR "doqe requires the ${variable} environment variable to be unset")
    endif()
  endforeach()
  foreach(variable IN ITEMS CMAKE_C_COMPILER_LAUNCHER CMAKE_CXX_COMPILER_LAUNCHER
      CMAKE_C_LINKER_LAUNCHER CMAKE_CXX_LINKER_LAUNCHER)
    if(DEFINED ENV{${variable}})
      message(FATAL_ERROR "doqe requires the ${variable} environment variable to be unset")
    endif()
    if(DEFINED ${variable})
      message(FATAL_ERROR "doqe requires ${variable} to be unset")
    endif()
  endforeach()
endfunction()

function(doqe_verify_llvm)
  # SHA-256 of the compiler and static runtime from the verified release tarballs.
  set(llvm_libdir "${DOQE_LLVM_ROOT}/lib")
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    set(clang_sha256 a22ab66d53cf70dfee8544eabdeb36f075bf942861cb8d59f98a3fadc7a65c26)
    set(libcxx_sha256 bbab7514c4ead42c32e18ff18c1006c9d8696f968fa895a34b6d0a545d5e9726)
    set(libcxxabi_sha256 8c7604eda506797adfd63d6556e75990100e017baedb8460fa45b876e68fff1c)
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(clang_sha256 2033aa32d783adec2a3d30a5d9229b60d9a7594ad73e19e87c3fd2b5ed21a9c2)
    set(libcxx_sha256 91ad2e76b5c991ea3700f4a9909f0ad7ec1b5a87ba05db6afbe69f186e818724)
    set(libcxxabi_sha256 3ab150f0530e938d706b5d63b26711df9d299dad3f5e77231cf616e1a7f0d47e)
    set(libunwind_sha256 ed6b04b2ebd75062dc297ccd1e0047fe80c0dcb286d0b1bb85f7cc446f6e4252)
    set(llvm_target x86_64-unknown-linux-gnu)
    string(APPEND llvm_libdir "/${llvm_target}")
  else()
    message(FATAL_ERROR "No doqe LLVM pin for ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
  endif()

  doqe_require_llvm(CXX clang++)
  doqe_require_llvm(C clang)
  doqe_require_llvm_digest("${actual_CXX_compiler}" "${clang_sha256}")
  if(NOT actual_C_compiler STREQUAL actual_CXX_compiler)
    doqe_require_llvm_digest("${actual_C_compiler}" "${clang_sha256}")
  endif()

  execute_process(COMMAND "${CMAKE_CXX_COMPILER}" --version
    OUTPUT_VARIABLE version COMMAND_ERROR_IS_FATAL ANY)
  string(REGEX MATCH "^[^\r\n]*" banner "${version}")
  if(NOT banner STREQUAL
      "clang version 23.1.5 (https://github.com/llvm/llvm-project 392dfff97d29b59d77e5cac6d4b6ef2585fe7117)")
    message(FATAL_ERROR "Unexpected upstream LLVM Clang version banner: ${banner}")
  endif()

  file(REAL_PATH "${llvm_libdir}/libc++.a" libcxx_archive)
  file(REAL_PATH "${llvm_libdir}/libc++abi.a" libcxxabi_archive)
  doqe_require_llvm_digest("${libcxx_archive}" "${libcxx_sha256}")
  doqe_require_llvm_digest("${libcxxabi_archive}" "${libcxxabi_sha256}")
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    file(REAL_PATH "${llvm_libdir}/libunwind.a" libunwind_archive)
    doqe_require_llvm_digest("${libunwind_archive}" "${libunwind_sha256}")
  endif()
  return(PROPAGATE libcxx_archive libcxxabi_archive libunwind_archive llvm_target)
endfunction()

function(doqe_require_llvm language driver)
  set(compiler "${CMAKE_${language}_COMPILER}")
  get_filename_component(driver_name "${compiler}" NAME)
  file(REAL_PATH "${compiler}" actual_compiler)
  file(REAL_PATH "${DOQE_LLVM_ROOT}/bin/${driver}" pinned_compiler)
  if(NOT CMAKE_${language}_COMPILER_ID STREQUAL "Clang"
      OR NOT CMAKE_${language}_COMPILER_VERSION STREQUAL "23.1.5"
      OR NOT driver_name STREQUAL driver
      OR NOT actual_compiler STREQUAL pinned_compiler)
    message(FATAL_ERROR
      "doqe requires upstream LLVM Clang 23.1.5 at ${DOQE_LLVM_ROOT}/bin/${driver}; "
      "found ${CMAKE_${language}_COMPILER_ID} ${CMAKE_${language}_COMPILER_VERSION} "
      "at ${compiler} (resolved to ${actual_compiler})")
  endif()

  file(REAL_PATH "${DOQE_LLVM_ROOT}" actual_root)
  get_filename_component(compiler_bin "${actual_compiler}" DIRECTORY)
  get_filename_component(compiler_root "${compiler_bin}" DIRECTORY)
  if(NOT actual_root STREQUAL compiler_root)
    message(FATAL_ERROR "DOQE_LLVM_ROOT must resolve to the installation containing the compiler.\n"
      "Resolved root: ${actual_root}\nResolved compiler: ${actual_compiler}")
  endif()
  set(actual_${language}_compiler "${actual_compiler}" PARENT_SCOPE)
endfunction()

function(doqe_require_llvm_digest path pinned_sha256)
  if(NOT EXISTS "${path}" OR IS_DIRECTORY "${path}")
    message(FATAL_ERROR "doqe requires a pinned LLVM 23.1.5 file at ${path}")
  endif()
  file(SHA256 "${path}" actual_sha256)
  if(NOT actual_sha256 STREQUAL pinned_sha256)
    message(FATAL_ERROR "doqe requires upstream LLVM Clang 23.1.5: SHA-256 mismatch for "
      "${path} (expected ${pinned_sha256}, found ${actual_sha256})")
  endif()
endfunction()
