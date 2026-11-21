set(libcxx_headers "${DOQE_LLVM_ROOT}/include/c++/v1")
add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>"
  "$<$<COMPILE_LANGUAGE:CXX>:-nostdinc++>"
  "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-isystem \"${libcxx_headers}\">"
  "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include \"${CMAKE_CURRENT_LIST_DIR}/check-libcxx.hpp\">")
add_link_options("$<$<LINK_LANGUAGE:CXX>:-nostdlib++>")

# Put the runtimes after all project archives on final C++ link lines.
# The normal variable overrides any stale cache value from older builds.
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(CMAKE_CXX_STANDARD_LIBRARIES "\"${libcxx_archive}\" \"${libcxxabi_archive}\"")
  add_compile_options(-Werror=unguarded-availability -mmacosx-version-min=26.0)
  add_link_options(-mmacosx-version-min=26.0)
  # dyld coalesced executable weak C++ definitions with system ones, causing
  # doqe_tests to abort when typed operator new ran before its initializer.
  add_link_options("$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:LINKER:-no_exported_symbols>")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(CMAKE_CXX_STANDARD_LIBRARIES
    "-Wl,--start-group \"${libcxx_archive}\" \"${libcxxabi_archive}\" \"${libunwind_archive}\" -Wl,--end-group")
  set(target_headers "${DOQE_LLVM_ROOT}/include/${llvm_target}/c++/v1")
  add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:SHELL:-isystem \"${target_headers}\">")
  add_link_options("$<$<LINK_LANGUAGE:CXX>:--rtlib=compiler-rt;--unwindlib=none>")
endif()
