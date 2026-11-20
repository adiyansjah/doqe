#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../../../.."
if [[ -e build/debug ]]; then
  echo "Run from a checkout without build/debug to verify a fresh build." >&2
  exit 1
fi

root=$PWD
pin="$HOME/opt/llvm-23.1.5"
logs=$(mktemp -d "${TMPDIR:-/tmp}/doqe-skeleton.XXXXXX")
if [[ ${DOQE_KEEP_SKELETON_LOGS:-0} != 1 ]]; then
  trap 'rm -rf -- "$logs"' EXIT
fi
host=$(uname -s)
if [[ $host == Linux ]]; then
  exec 9>"$HOME/doqe-quiet.lock"
  flock 9
fi

run_logged() {
  local log=$1
  shift
  if ! "$@" >"$logs/$log" 2>&1; then
    tail -n 45 "$logs/$log" >&2
    return 1
  fi
}

expect_failure() {
  local label=$1 diagnostic=$2 status=0
  shift 2
  "$@" >"$logs/$label.log" 2>&1 || status=$?
  printf '%s exit: %s\n' "$label" "$status"
  [[ $status -ne 0 ]]
  grep -m 1 -F -- "$diagnostic" "$logs/$label.log"
}

printf 'Date: %s\nMachine: %s\n' "$(date -u +%FT%TZ)" "$(hostname)"
case "$host" in
  Darwin) sw_vers; printf 'SDK: '; xcrun --sdk macosx --show-sdk-version ;;
  Linux) printf 'Debian: '; cat /etc/debian_version; getconf GNU_LIBC_VERSION ;;
esac
uname -smr
"$pin/bin/clang++" --version
archives=(libc++.a libc++abi.a)
libdir=lib
case "$host" in
  Darwin) hash_command=(shasum -a 256) ;;
  Linux)
    hash_command=(sha256sum)
    libdir=lib/x86_64-unknown-linux-gnu
    archives+=(libunwind.a)
    ;;
esac
"${hash_command[@]}" "$pin/bin/clang-23"
for archive in "${archives[@]}"; do
  "${hash_command[@]}" "$pin/$libdir/$archive"
done
cmake --version | sed -n '1p'
printf 'Ninja: %s\n' "$(ninja --version)"
printf 'Build settings: debug preset, C++23, pinned static libc++ and libc++abi\n'
commit=$(git rev-parse HEAD)
short_commit=$(git rev-parse --short HEAD)
dirty=no
dirty_suffix=
if [[ -n $(git --no-optional-locks status --porcelain --untracked-files=normal) ]]; then
  dirty=yes
  dirty_suffix=-dirty
fi
printf 'Commit: %s (%s, git dirty: %s)\n' "$commit" "$(git branch --show-current)" "$dirty"
printf 'Logs: %s\n' "$logs"

run_logged workflow.log cmake --workflow --preset debug
tail -n 14 "$logs/workflow.log"
version=$(./build/debug/doqe --version)
[[ $version == "doqe 0.1.0 ($short_commit$dirty_suffix)" ]]
printf '%s\n' "$version"
if [[ $dirty == yes ]]; then
  echo 'A clean rerun is due after commit.'
fi
gtest_source="$root/build/debug/_deps/googletest-src"
actual_gtest=$(git -C "$gtest_source" rev-parse HEAD)
[[ $actual_gtest == 52eb8108c5bdec04579160ae17225d66034bd723 ]]
printf 'GoogleTest: %s\n' "$actual_gtest"

check_gtest_flags() {
  local database=$1 command count=0
  grep -E '"command": .* -c .*googletest/src/gtest(_main|-all)\.cc"' "$database" \
    >"$logs/gtest-commands.log"
  while IFS= read -r command; do
    [[ $command == *"\"command\": \"$pin/bin/clang++ "* ]]
    [[ $command == *" -nostdinc++ "* && $command == *" -stdlib=libc++ "* ]]
    [[ $command == *" -std=c++23 "* && $command == *" -isystem $pin/include/c++/v1 "* ]]
    [[ $command == *" -include "*"/cmake/check-libcxx.hpp"* ]]
    if [[ $host == Darwin ]]; then
      [[ $command == *" -mmacosx-version-min=26.0 "* ]]
      [[ $command == *" -Werror=unguarded-availability "* ]]
    else
      [[ $command == *" -isystem $pin/include/x86_64-unknown-linux-gnu/c++/v1 "* ]]
    fi
    count=$((count + 1))
  done <"$logs/gtest-commands.log"
  [[ $count -eq 2 ]]
  printf 'GoogleTest compile commands (%s): pinned driver, C++23, forced libc++ version check, pinned headers and platform flags\n' "$count"
}

inspect_runtime() {
  local log=$2
  run_logged "$log" cmake "-DHOST_SYSTEM=$host" \
    "-DDOQE=$1/doqe" "-DDOQE_TESTS=$1/doqe_tests" -P tests/check-runtime-dependencies.cmake
  cat "$logs/$log"
}

check_gtest_flags build/debug/compile_commands.json
inspect_runtime build/debug dependencies.log
for binary in build/debug/doqe build/debug/doqe_tests; do
  printf 'Pinned llvm-nm unwinder symbol for %s\n' "$binary"
  if [[ $host == Darwin ]]; then
    "$pin/bin/llvm-nm" -m "$binary" >"$logs/${binary##*/}.nm"
    grep -E 'undefined.*__Unwind_RaiseException \(from libSystem\)' "$logs/${binary##*/}.nm"
    xcrun dyld_info -fixups "$binary" >"$logs/${binary##*/}.fixups"
    if grep -E 'weak-def-coalesce.*(__ZNSt|__Znw|__Zdl|___cxa_)' "$logs/${binary##*/}.fixups"; then
      echo "Unexpected C++ runtime weak binding" >&2
      exit 1
    fi
    echo 'No weak coalescing for the checked C++ runtime symbols'
  else
    "$pin/bin/llvm-nm" --defined-only "$binary" >"$logs/${binary##*/}.nm"
    grep -E ' [Tt] _Unwind_RaiseException$' "$logs/${binary##*/}.nm"
  fi
done

if [[ $host == Darwin ]]; then
  sdk=$(xcrun --sdk macosx --show-sdk-path)
  grep -o '/usr/lib/system/libunwind.dylib' "$sdk/usr/lib/libSystem.tbd" | sort -u
  grep -o '__Unwind_RaiseException' "$sdk/usr/lib/system/libunwind.tbd" | sort -u
fi

# Seed a failing test in an independent source copy.
seeded="$logs/seeded"
mkdir "$seeded"
cp -R CMakeLists.txt CMakePresets.json cmake src tests "$seeded/"
printf '\nadd_library(header_probe OBJECT header_probe.cpp)\n' >>"$seeded/CMakeLists.txt"
printf 'static_assert(true);\n' >"$seeded/header_probe.cpp"
cat >> "$seeded/tests/version_test.cpp" <<'CPP'

TEST(SkeletonCheck, SeededFailure) {
  FAIL() << "intentional skeleton check";
}
CPP
(
  cd "$seeded"
  run_logged seed-configure.log cmake --preset debug "-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=$gtest_source"
  run_logged seed-build.log cmake --build --preset debug
  expect_failure seeded-ctest 'intentional skeleton check' ctest --preset debug
  grep -E 'SkeletonCheck.SeededFailure.*Failed' "$logs/seeded-ctest.log"

  if [[ $host == Darwin ]]; then
    leaked_headers="$sdk/usr/include/c++/v1"
  else
    leaked_headers="$logs/leaked-headers"
    mkdir "$leaked_headers"
    cat >"$leaked_headers/version" <<'CPP'
#include_next <version>
#undef _LIBCPP_VERSION
#define _LIBCPP_VERSION 190000
CPP
  fi
  touch header_probe.cpp
  expect_failure build-time-header-leak 'doqe requires libc++ headers from LLVM 23.1.5' \
    env "CPATH=$leaked_headers" cmake --build --preset debug --target header_probe
)

check_rejected() {
  local label=$1 diagnostic=$2
  shift 2
  expect_failure "$label" "$diagnostic" cmake --preset debug -B "$logs/reject-$label" \
    -DBUILD_TESTING=OFF "$@"
}

compiler_diagnostic='doqe requires upstream LLVM Clang 23.1.5'
if [[ $host == Darwin ]]; then
  alternate_cxx=/usr/bin/clang++
  alternate_c=/usr/bin/clang
  check_rejected apple-clang "$compiler_diagnostic" "-DCMAKE_CXX_COMPILER=$alternate_cxx"
else
  alternate_cxx=$(command -v g++)
  alternate_c=$(command -v gcc)
  check_rejected gcc "$compiler_diagnostic" "-DCMAKE_CXX_COMPILER=$alternate_cxx"
  if distro_clang=$(command -v clang++-19); then
    check_rejected distro-clang "$compiler_diagnostic" "-DCMAKE_CXX_COMPILER=$distro_clang"
  else
    echo 'Distro Clang 19 rejection: not verified (clang++-19 unavailable)'
  fi
fi
CXX="$alternate_cxx" check_rejected cxx-env "$compiler_diagnostic"
CC="$alternate_c" check_rejected cc-env "$compiler_diagnostic"
for driver in clang clang++; do
  # The actual pinned binary under a different driver name must also fail.
  ln -s "$pin/bin/$driver" "$logs/wrong-$driver"
done
check_rejected wrong-cxx-name "$compiler_diagnostic" "-DCMAKE_CXX_COMPILER=$logs/wrong-clang++"
check_rejected wrong-c-name "$compiler_diagnostic" "-DCMAKE_C_COMPILER=$logs/wrong-clang"

# Both paths in this simulated foreign root report the exact upstream version.
mkdir -p "$logs/foreign/bin"
cat > "$logs/foreign/bin/clang" <<'SH'
#!/bin/sh
exec "$HOME/opt/llvm-23.1.5/bin/${0##*/}" "$@"
SH
chmod +x "$logs/foreign/bin/clang"
ln -s clang "$logs/foreign/bin/clang++"
[[ $("$logs/foreign/bin/clang++" --version) == "$("$pin/bin/clang++" --version)" ]]
check_rejected foreign-root 'SHA-256 mismatch' "-DDOQE_LLVM_ROOT=$logs/foreign"

# The real compilers and headers cannot vouch for a separate library tree.
mkdir -p "$logs/mixed/lib"
ln -s "$pin/bin" "$logs/mixed/bin"
ln -s "$pin/include" "$logs/mixed/include"
printf 'foreign archive\n' >"$logs/foreign-libc++.a"
ln -s "$logs/foreign-libc++.a" "$logs/mixed/lib/libc++.a"
check_rejected mixed-root 'must resolve to the installation containing' "-DDOQE_LLVM_ROOT=$logs/mixed"

# Give the pinned driver its own real path to exercise the archive hashes
# separately from the root check.
modified="$logs/modified"
mkdir -p "$modified/bin" "$modified/$libdir"
ln "$pin/bin/clang-23" "$modified/bin/clang-23" 2>/dev/null ||
  cp -p "$pin/bin/clang-23" "$modified/bin/clang-23"
ln -s clang-23 "$modified/bin/clang"
ln -s clang "$modified/bin/clang++"
ln -s "$pin/include" "$modified/include"
ln -s "$pin/lib/clang" "$modified/lib/clang"
for archive in "${archives[@]}"; do
  ln -s "$pin/$libdir/$archive" "$modified/$libdir/$archive"
done
for archive in "${archives[@]}"; do
  printf 'foreign archive\n' >"$logs/foreign-$archive"
  ln -sfn "$logs/foreign-$archive" "$modified/$libdir/$archive"
  check_rejected "modified-$archive" 'SHA-256 mismatch for' "-DDOQE_LLVM_ROOT=$modified"
  grep -m 1 -F -- "/foreign-$archive" "$logs/modified-$archive.log"
  ln -sfn "$pin/$libdir/$archive" "$modified/$libdir/$archive"
done
check_rejected shared-libraries 'requires an exported-symbols ADR' -DBUILD_SHARED_LIBS=ON

for variable in CPATH C_INCLUDE_PATH CPLUS_INCLUDE_PATH OBJC_INCLUDE_PATH LIBRARY_PATH; do
  export "$variable=$logs"
  check_rejected "$variable" "doqe requires the $variable environment variable"
  unset "$variable"
done
for variable in CMAKE_C_COMPILER_LAUNCHER CMAKE_CXX_COMPILER_LAUNCHER \
    CMAKE_C_LINKER_LAUNCHER CMAKE_CXX_LINKER_LAUNCHER; do
  check_rejected "$variable-cache" "doqe requires $variable to be unset" "-D$variable=/usr/bin/env"
  export "$variable=/usr/bin/env"
  check_rejected "$variable-env" "doqe requires the $variable environment variable"
  unset "$variable"
done
printf 'set(CMAKE_C_COMPILER "%s/missing-compiler")\n' "$logs" >"$logs/foreign-toolchain.cmake"
expect_failure foreign-toolchain 'DOQE_LLVM_ROOT is empty' cmake -G Ninja -S . \
  -B "$logs/reject-foreign-toolchain" "-DCMAKE_TOOLCHAIN_FILE=$logs/foreign-toolchain.cmake"

flags_build="$logs/empty-flags"
run_logged empty-flags-configure.log cmake --preset debug -B "$flags_build" \
  -DCMAKE_C_FLAGS= -DCMAKE_CXX_FLAGS= -DCMAKE_CXX_STANDARD_LIBRARIES= \
  "-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=$gtest_source"
run_logged empty-flags-build.log cmake --build "$flags_build"
run_logged empty-flags-test.log ctest --test-dir "$flags_build" --output-on-failure
echo 'Empty C/C++ flags and C++ standard libraries cache values:'
tail -n 14 "$logs/empty-flags-test.log"
check_gtest_flags "$flags_build/compile_commands.json"
inspect_runtime "$flags_build" empty-flags-dependencies.log

# Check the exported-source fallback and all five approved C++23 library names.
exported="$logs/exported"
mkdir "$exported"
cp -R CMakeLists.txt CMakePresets.json cmake src "$exported/"
ln -s "$pin" "$logs/alternate LLVM"
cat >> "$exported/CMakeLists.txt" <<'CMAKE'

add_executable(allowlist allowlist.cpp)
CMAKE
cat > "$exported/allowlist.cpp" <<'CPP'
#include <bit>
#include <expected>
#include <utility>

enum class Code : unsigned { bytes = 0x12345678u };

int main() {
  std::expected<unsigned, int> value = std::to_underlying(Code::bytes);
  if (!value || std::byteswap(*value) != 0x78563412u) {
    return 1;
  }
  value = std::unexpected{2};
  switch (value.error()) {
    case 1: return 1;
    case 2: return 0;
  }
  std::unreachable();
}
CPP
(
  cd "$exported"
  run_logged export-configure.log cmake --preset debug -DBUILD_TESTING=OFF \
    "-DDOQE_LLVM_ROOT=$logs/alternate LLVM" --trace-expand \
    "--trace-source=$exported/cmake/require-llvm.cmake"
  compiler_hashes=$(grep -c 'file(SHA256 .*/bin/clang-23 ' "$logs/export-configure.log")
  [[ $compiler_hashes -eq 1 ]]
  printf 'Compiler SHA-256 calls for common C and C++ binary: %s\n' "$compiler_hashes"
  run_logged export-build.log cmake --build --preset debug
  version=$(./build/debug/doqe --version)
  [[ $version == 'doqe 0.1.0 (unknown)' ]]
  printf 'Exported source and alternate LLVM root: %s\n' "$version"
  ./build/debug/allowlist
  echo 'C++23 library allowlist smoke: passed'
)
