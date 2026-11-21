#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C

cd "$(dirname "$0")/../../../.."
root=$PWD
pin="$HOME/opt/llvm-23.1.5"
scratch=$(mktemp -d "${TMPDIR:-/tmp}/doqe-quality.XXXXXX")
trap 'rm -rf -- "$scratch"' EXIT

host=$(uname -s)
if [[ $host == Linux ]]; then
  exec 9>"$HOME/doqe-quiet.lock"
  flock 9
fi

printf 'Date: %s\nMachine: %s\n' "$(date -u +%FT%TZ)" "$(hostname)"
uname -smr
case "$host" in
  Darwin) sw_vers; printf 'SDK: '; xcrun --sdk macosx --show-sdk-version ;;
  Linux) printf 'Debian: '; cat /etc/debian_version; getconf GNU_LIBC_VERSION ;;
esac
for tool in clang++ clang-format clang-tidy; do
  "$pin/bin/$tool" --version | sed -n '1,2p'
done
cmake --version | sed -n '1p'
printf 'Ninja: %s\n' "$(ninja --version)"
echo 'Build settings: debug and asan-ubsan presets, C++23, pinned static libc++ and libc++abi'
echo 'Sanitizer test preset: ASAN_OPTIONS=detect_stack_use_after_return=1'
dirty=no
if [[ -n $(git status --porcelain) ]]; then
  dirty=yes
fi
printf 'Commit: %s (%s, git dirty: %s)\n' "$(git rev-parse HEAD)" \
  "$(git branch --show-current)" "$dirty"

run() {
  local label=$1
  shift
  printf '+ %s\n' "$*"
  if ! "$@" >"$scratch/$label.log" 2>&1; then
    tail -n 45 "$scratch/$label.log" >&2
    return 1
  fi
}

expect_failure() {
  local label=$1 diagnostic=$2 status=0
  shift 2
  printf '+ %s (%s)\n' "$*" "$label"
  "$@" >"$scratch/$label.log" 2>&1 || status=$?
  printf '%s exit: %s\n' "$label" "$status"
  if [[ $status -eq 0 ]] || ! grep -m 1 -F -- "$diagnostic" "$scratch/$label.log"; then
    tail -n 45 "$scratch/$label.log" >&2
    return 1
  fi
}

# Preserve Git's tracked-file inventory, then overlay the current sources in a
# fresh checkout so positive and seeded runs exercise the same implementation.
git clone --quiet --shared --no-checkout "$root" "$scratch/tree"
cd "$scratch/tree"
git -C "$root" ls-files --stage -z | git update-index -z --index-info
(
  cd "$root"
  git ls-files --cached --others --exclude-standard -z |
    while IFS= read -r -d '' file; do
      if [[ -e $file || -L $file ]]; then
        printf '%s\0' "$file"
      fi
    done | tar --null -T - -cf -
) | tar -xf -

run debug cmake --workflow --preset debug
grep -F '100% tests passed' "$scratch/debug.log"
run quality cmake --build --preset quality
echo 'Format and lint passed'
run new-header-format "$pin/bin/clang-format" --dry-run -Werror tests/integration/throw_for_unwind_test.hpp
run sanitizer cmake --workflow --preset asan-ubsan
grep -F '100% tests passed' "$scratch/sanitizer.log"

# Confirm each instrumented TU, including both GoogleTest TUs, has all flags.
awk '
  /"command":/ {
    ++total
    if (/googletest\/src\/gtest/) ++gtest
    if (!/-fsanitize=address,undefined/ || !/-fno-sanitize-recover=all/ ||
        !/-fno-omit-frame-pointer/) bad = 1
  }
  END {
    if (bad || total <= gtest || gtest != 2) exit 1
    printf "Sanitizer compile flags verified for %d TUs (%d GoogleTest)\n", total, gtest
  }
' build/asan-ubsan/compile_commands.json

resource=$("$pin/bin/clang++" -print-resource-dir)
case "$host" in
  Darwin)
    run dependencies otool -L build/asan-ubsan/doqe build/asan-ubsan/doqe_tests
    grep -F 'libclang_rt.asan_osx_dynamic.dylib' "$scratch/dependencies.log"
    test -f "$resource/lib/darwin/libclang_rt.asan_osx_dynamic.dylib"
    test -f "$resource/lib/darwin/libclang_rt.ubsan_osx_dynamic.dylib"
    run asan-dependencies otool -L "$resource/lib/darwin/libclang_rt.asan_osx_dynamic.dylib"
    grep -E '/usr/lib/libc\+\+(abi)?\.' "$scratch/asan-dependencies.log"
    run exports xcrun dyld_info -exports build/asan-ubsan/doqe build/asan-ubsan/doqe_tests
    awk '
      /^build\// || !NF || /^ +(-exports:|offset +symbol)$/ { next }
      { bad = 1 }
      END { exit bad }
    ' "$scratch/exports.log"
    echo 'Both sanitized executables have empty export tables'
    ;;
  Linux)
    run dependencies ldd build/asan-ubsan/doqe build/asan-ubsan/doqe_tests
    runtime="$resource/lib/x86_64-unknown-linux-gnu"
    test -f "$runtime/libclang_rt.asan.a"
    test -f "$runtime/libclang_rt.asan_cxx.a"
    test -f "$runtime/libclang_rt.ubsan_standalone.a"
    for binary in build/asan-ubsan/doqe build/asan-ubsan/doqe_tests; do
      run symbols "$pin/bin/llvm-nm" --defined-only "$binary"
      grep -m 1 -E '[[:space:]]T __ubsan_handle_' "$scratch/symbols.log"
    done
    ;;
esac
run dependency-test ctest --preset asan-ubsan -V -R '^Runtime.SharedDependencies$'
grep -F '100% tests passed' "$scratch/dependency-test.log"
if [[ $host == Linux ]]; then
  grep -F 'Static ASan verified in' "$scratch/dependency-test.log"
  expect_failure missing-static-asan 'Missing statically linked __asan_init' \
    cmake -DHOST_SYSTEM=Linux -DSANITIZED=ON "-DLLVM_NM=$pin/bin/llvm-nm" \
    "-DDOQE=$PWD/build/debug/doqe" "-DDOQE_TESTS=$PWD/build/debug/doqe_tests" \
    -P tests/integration/check-runtime-dependencies.cmake
fi

printf '\nint  format_seed( ){return  0;}\n' >> src/cli/version.cpp
expect_failure format-seed 'code should be clang-formatted' cmake --build --preset debug --target format
cp "$root/src/cli/version.cpp" src/cli/version.cpp
printf 'static_assert(sizeof(int) == sizeof(int));\n' > src/lint_seed.cpp
printf '\nadd_library(lint_seed STATIC src/lint_seed.cpp)\n' >> CMakeLists.txt
run lint-seed-configure cmake --preset debug
expect_failure lint-seed 'misc-redundant-expression' cmake --build --preset debug --target lint
grep -F 'lint_seed.cpp:1:' "$scratch/lint-seed.log"
cp "$root/CMakeLists.txt" CMakeLists.txt
rm src/lint_seed.cpp

# Build successfully with each seed, then require its own runtime diagnostic.
# Using a C++ allocation exercises the static libc++/ASan interposition path.
seeds=(use-after-free heap-overflow signed-overflow)
if [[ $host == Linux ]]; then
  seeds+=(new-free array-delete sized-delete)
else
  echo 'Allocation-family mismatch seeds run on Linux only. macOS ASan sees malloc/free beneath the static C++ operators.'
fi
for seed in "${seeds[@]}"; do
  case "$seed" in
    use-after-free)
      diagnostic='AddressSanitizer: heap-use-after-free'
      body='volatile int *value = new int{42}; delete value; EXPECT_EQ(*value, 42);'
      ;;
    heap-overflow)
      diagnostic='AddressSanitizer: heap-buffer-overflow'
      body='volatile int *value = new int[1]{}; EXPECT_EQ(value[1], 0); delete[] value;'
      ;;
    signed-overflow)
      diagnostic='runtime error: signed integer overflow'
      body='volatile int value = INT_MAX; EXPECT_LT(value + 1, 0);'
      ;;
    new-free)
      diagnostic='AddressSanitizer: alloc-dealloc-mismatch (operator new vs free)'
      body='std::free(new int{42});'
      ;;
    array-delete)
      diagnostic='AddressSanitizer: alloc-dealloc-mismatch (operator new [] vs operator delete)'
      body='::operator delete(static_cast<void *>(new int[2]{}));'
      ;;
    sized-delete)
      diagnostic='AddressSanitizer: new-delete-type-mismatch'
      body='::operator delete(::operator new(32), std::size_t{16});'
      # Request the sized overload for this scratch build only.
      run sized-delete-configure cmake --preset asan-ubsan -DCMAKE_CXX_FLAGS=-fsized-deallocation
      ;;
  esac
  cp "$root/tests/cli/version_test.cpp" tests/cli/version_test.cpp
  printf '\n#include <climits>\n#include <cstdlib>\n#include <new>\nTEST(Seed, Violation) { %s }\n' "$body" >> tests/cli/version_test.cpp
  run "$seed-build" cmake --build --preset asan-ubsan
  expect_failure "$seed" "$diagnostic" ctest --preset asan-ubsan -R '^Seed.Violation$'
done
