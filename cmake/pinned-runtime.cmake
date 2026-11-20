set(libcxx_headers "${DOQE_LLVM_ROOT}/include/c++/v1")
add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>"
  "$<$<COMPILE_LANGUAGE:CXX>:-nostdinc++>"
  "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-isystem \"${libcxx_headers}\">"
  "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include \"${CMAKE_CURRENT_LIST_DIR}/check-libcxx.hpp\">")
add_link_options("$<$<LINK_LANGUAGE:CXX>:-nostdlib++>")

# Older build directories may still cache the former runtime flags and archives.
set(CMAKE_CXX_STANDARD_LIBRARIES "")
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  add_compile_options(-Werror=unguarded-availability -mmacosx-version-min=26.0)
  add_link_options(-mmacosx-version-min=26.0)
  # dyld coalesced executable weak C++ definitions with system ones, causing
  # doqe_tests to abort when typed operator new ran before its initializer.
  add_link_options("$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:LINKER:-no_exported_symbols>")
  link_libraries("$<$<LINK_LANGUAGE:CXX>:${libcxx_archive}>"
    "$<$<LINK_LANGUAGE:CXX>:${libcxxabi_archive}>")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(target_headers "${DOQE_LLVM_ROOT}/include/${llvm_target}/c++/v1")
  add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:SHELL:-isystem \"${target_headers}\">")
  add_link_options("$<$<LINK_LANGUAGE:CXX>:--rtlib=compiler-rt;--unwindlib=none>")
  link_libraries("$<$<LINK_LANGUAGE:CXX>:$<LINK_GROUP:RESCAN,${libcxx_archive},${libcxxabi_archive},${libunwind_archive}>>")
endif()
