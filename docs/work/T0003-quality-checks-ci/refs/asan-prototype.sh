#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C
# Run from the T0003 tree. Only the scratch copy is edited.
pin="$HOME/opt/llvm-23.1.5"
host=$(uname -s)
printf 'Date: %s\nMachine: %s\n' "$(date -u +%FT%TZ)" "$(hostname)"
uname -smr
if [[ $host == Darwin ]]; then sw_vers; xcrun --show-sdk-version; else cat /etc/debian_version; fi
"$pin/bin/clang++" --version
cmake --version | sed -n '1p'
ninja --version
echo 'Build settings: asan-ubsan, explicit sized deallocation, stack-use-after-return detection'
printf 'Commit: %s\n' "${1:?Supply source commit, branch and dirty status}"
scratch=$(mktemp -d "${TMPDIR:-/tmp}/doqe-asan-prototype.XXXXXX")
echo "Scratch: $scratch"
tar -cf - CMakeLists.txt CMakePresets.json cmake src tests .clang-tidy | tar -xf - -C "$scratch"
cd "$scratch"
export ASAN_OPTIONS=alloc_dealloc_mismatch=1:detect_stack_use_after_return=1
if [[ $host == Darwin ]]; then
  /usr/bin/python3 - <<'PY'
from pathlib import Path
p = Path("cmake/pinned-runtime.cmake")
text = p.read_text()
text = text.replace('  link_libraries(', '''  add_link_options("LINKER:-rpath,${DOQE_LLVM_ROOT}/lib")
  set(libcxx_archive "${DOQE_LLVM_ROOT}/lib/libc++.dylib")
  set(libcxxabi_archive "${DOQE_LLVM_ROOT}/lib/libc++abi.dylib")
  link_libraries(''', 1)
p.write_text(text)
# The pre-change runtime test forbids all shared libc++. Require the prototype's
# direct pinned install names here so the full test suite exercises the trial.
p = Path("tests/check-runtime-dependencies.cmake")
text = p.read_text().replace('  set(inspect otool -L)', '''  set(inspect otool -L)
  set(forbidden "libstdc\\\\+\\\\+|libgcc_s")''')
text = text.replace('  if(NOT SANITIZED', '''  if(HOST_SYSTEM STREQUAL "Darwin")
    foreach(library IN ITEMS libc++ libc++abi)
      string(FIND "${dependencies}" "@rpath/${library}.1.dylib" pinned)
      if(pinned LESS 0)
        message(FATAL_ERROR "Missing pinned ${library}")
      endif()
    endforeach()
    string(FIND "${dependencies}" "/usr/lib/libc++" system)
    if(NOT system LESS 0)
      message(FATAL_ERROR "Unexpected system libc++ dependency")
    endif()
  endif()
  if(NOT SANITIZED''')
p.write_text(text)
PY
  shasum -a 256 "$pin/lib/libc++.dylib" "$pin/lib/libc++abi.dylib"
fi
cmake --preset asan-ubsan -DCMAKE_CXX_FLAGS=-fsized-deallocation >configure.log 2>&1 ||
  { tail -60 configure.log; exit 1; }
if ! cmake --build --preset asan-ubsan >build.log 2>&1; then
  grep -m 1 -F 'ERROR: AddressSanitizer:' build.log || true
  tail -25 build.log
  exit 1
fi
ctest --preset asan-ubsan
if [[ $host == Darwin ]]; then
  resource=$("$pin/bin/clang++" -print-resource-dir)
  asan="$resource/lib/darwin/libclang_rt.asan_osx_dynamic.dylib"
  otool -L build/asan-ubsan/doqe build/asan-ubsan/doqe_tests "$asan" "$pin/lib/libc++.dylib" "$pin/lib/libc++abi.dylib"
  xcrun dyld_info -fixups build/asan-ubsan/doqe_tests "$asan" >fixups.log
  grep -E 'doqe_tests|libclang_rt.*(arm64|:)|(__Znwm|__Znam|___cxa_throw|__ZNSt3__113runtime_errorC1EPKc)' fixups.log | sed -n '1,25p'
  DYLD_PRINT_LIBRARIES=1 build/asan-ubsan/doqe_tests --gtest_filter=Runtime.UnwindsAcrossFunctionBoundary >images.log 2>&1
  grep -E 'libc\+\+|libunwind|ASan|PASSED' images.log
else
  resource=$("$pin/bin/clang++" -print-resource-dir)
  "$pin/bin/llvm-nm" -A --defined-only \
    "$resource/lib/x86_64-unknown-linux-gnu/libclang_rt.asan_cxx.a" \
    "$pin/lib/x86_64-unknown-linux-gnu/libc++abi.a" \
    build/asan-ubsan/doqe_tests >symbols.log
  grep -E ' [TW] (_Znwm|_Znam|_ZdlPvm|_ZdlPv|_ZdaPv)$' symbols.log
  ninja -C build/asan-ubsan -t commands doqe_tests | tail -1 >link.log
  cp link.log map-link.sh
  sed -i 's@ -o doqe_tests @ -Wl,-Map,prototype.map -o doqe_tests @' map-link.sh
  (cd build/asan-ubsan && bash ../../map-link.sh)
  grep -E 'asan_new_delete.cpp.o|cxa_new_delete.cpp.o|operator (new|delete)' build/asan-ubsan/prototype.map | sed -n '1,35p'
fi
cp tests/version_test.cpp original.cpp
for seed in use-after-free heap-overflow signed-overflow new-free array-delete sized-delete; do
  case "$seed" in
    use-after-free) diagnostic='AddressSanitizer: heap-use-after-free'; body='volatile int *value = new int{42}; delete value; EXPECT_EQ(*value, 42);' ;;
    heap-overflow) diagnostic='AddressSanitizer: heap-buffer-overflow'; body='volatile int *value = new int[1]{}; EXPECT_EQ(value[1], 0); delete[] value;' ;;
    signed-overflow) diagnostic='runtime error: signed integer overflow'; body='volatile int value = INT_MAX; EXPECT_LT(value + 1, 0);' ;;
    new-free) diagnostic='AddressSanitizer: alloc-dealloc-mismatch (operator new vs free)'; body='std::free(new int{42});' ;;
    array-delete) diagnostic='AddressSanitizer: alloc-dealloc-mismatch (operator new [] vs operator delete)'; body='::operator delete(static_cast<void *>(new int[2]{}));' ;;
    sized-delete) diagnostic='AddressSanitizer: new-delete-type-mismatch'; body='::operator delete(::operator new(32), std::size_t{16});' ;;
  esac
  cp original.cpp tests/version_test.cpp
  printf '\n#include <climits>\n#include <cstdlib>\n#include <new>\nTEST(Seed, Violation) { %s }\n' "$body" >> tests/version_test.cpp
  cmake --build --preset asan-ubsan >"$seed-build.log" 2>&1 || { tail -60 "$seed-build.log"; exit 1; }
  status=0
  ctest --preset asan-ubsan -R '^Seed.Violation$' >"$seed.log" 2>&1 || status=$?
  printf '%s: CTest exit %s\n' "$seed" "$status"
  if [[ $status -eq 0 ]] || ! grep -m 1 -F "$diagnostic" "$seed.log"; then
    tail -35 "$seed.log"
    exit 1
  fi
done
