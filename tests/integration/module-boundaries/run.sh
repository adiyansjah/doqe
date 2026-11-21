#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C

cd "$(dirname "$0")/../../.."
root=$PWD
host=$(uname -s)
scratch=$(mktemp -d "${TMPDIR:-/tmp}/doqe-modules.XXXXXX")
trap 'rm -rf -- "$scratch"' EXIT

if [[ $host == Linux && ${1:-} != --ci ]]; then
  exec 9>"$HOME/doqe-quiet.lock"
  flock 9
fi

printf 'Date: %s\nMachine: %s\nOS: ' "$(date -u +%FT%TZ)" "$(uname -n)"
uname -smr
case "$host" in
  Darwin) sw_vers; printf 'SDK: '; xcrun --sdk macosx --show-sdk-version ;;
  Linux) printf 'Debian: '; cat /etc/debian_version; getconf GNU_LIBC_VERSION ;;
esac
printf 'Toolchain: '
"$HOME/opt/llvm-23.1.5/bin/clang++" --version | sed -n '1p'
cmake --version | sed -n '1p'
printf 'Ninja: %s\n' "$(ninja --version)"
dirty=no
if [[ -n $(git status --porcelain) ]]; then
  dirty=yes
fi
printf 'Commit: %s (%s, git dirty: %s)\n' "$(git rev-parse HEAD)" "$(git branch --show-current)" "$dirty"
echo 'Build settings: Debug, C++23, pinned static C++ runtime'

run() {
  local label=$1
  shift
  if ! "$@" >"$scratch/$label.log" 2>&1; then
    tail -n 45 "$scratch/$label.log" >&2
    return 1
  fi
}

expect_failure() {
  local label=$1 diagnostic=$2 status=0
  shift 2
  "$@" >"$scratch/$label.log" 2>&1 || status=$?
  printf '%s exit: %s\n' "$label" "$status"
  if [[ $status -eq 0 ]] || ! grep -m 1 -F -- "$diagnostic" "$scratch/$label.log"; then
    tail -n 45 "$scratch/$label.log" >&2
    return 1
  fi
}

mkdir "$scratch/tree"
cp -R CMakeLists.txt CMakePresets.json cmake src tests "$scratch/tree/"
adr_dir=docs/adrs/0002-architecture-modules
mkdir -p "$scratch/tree/$adr_dir"
cp "$adr_dir/adr.md" "$scratch/tree/$adr_dir/"
cd "$scratch/tree"
configure=(cmake --preset debug -B "$scratch/build" -DBUILD_TESTING=OFF)
build=(cmake --build "$scratch/build")
check=("${build[@]}" --target module-boundaries)

edge_check=(cmake -P cmake/check-module-edges.cmake)
run matching-edge-table "${edge_check[@]}"
cat "$scratch/matching-edge-table.log"
cp "$adr_dir/adr.md" "$scratch/adr.md"
sed 's/6 · composition/6 · composition (e.g. query execution)/' "$scratch/adr.md" > "$adr_dir/adr.md"
run adr-period "${edge_check[@]}"
echo 'ADR row with a period in its layer annotation: passed'
cp "$scratch/adr.md" "$adr_dir/adr.md"
sed 's/| `api` |$/| `api`, engine |/' "$scratch/adr.md" > "$adr_dir/adr.md"
expect_failure adr-unquoted-edge 'Invalid dependency cell in ADR-0002 for cli' "${edge_check[@]}"
cp "$scratch/adr.md" "$adr_dir/adr.md"
cp cmake/modules.cmake "$scratch/modules.cmake"
sed 's/"cli|api"/"cli|api|server"/' "$scratch/modules.cmake" > cmake/modules.cmake
expect_failure adr-edge-drift 'Module edge table differs from ADR-0002' "${edge_check[@]}"
cp "$scratch/modules.cmake" cmake/modules.cmake

# A = cli, B = api, C = engine. The fixture makes the B → C edge public.
sed 's/"api|base|engine"/"api|base|+engine"/' cmake/modules.cmake > "$scratch/public-edge.cmake"
cp "$scratch/public-edge.cmake" cmake/modules.cmake
cp "$root/tests/integration/module-boundaries/api.hpp.in" src/api/include/doqe/api/boundary_fixture.hpp
cp "$root/tests/integration/module-boundaries/engine.hpp.in" src/engine/include/doqe/engine/boundary_fixture.hpp
cp src/cli/version.cpp "$scratch/cli.cpp"
cp src/cli/CMakeLists.txt "$scratch/cli.cmake"
cp src/io/CMakeLists.txt "$scratch/io.cmake"
# Only change server so its link properties cannot mask the type diagnostic.
awk '/^  add_library\(\${target} STATIC/ {
  print "  if(module STREQUAL \"server\")"
  print "    add_library(${target} OBJECT ${ARGN})"
  print "  else()"
  print "  " $0
  print "  endif()"
  next
} { print }' cmake/modules.cmake > "$scratch/server-object.cmake"
cp cmake/modules.cmake "$scratch/fixture-modules.cmake"
cp "$scratch/server-object.cmake" cmake/modules.cmake
expect_failure object-module 'doqe_server: TYPE must be STATIC_LIBRARY' "${configure[@]}"
cp "$scratch/fixture-modules.cmake" cmake/modules.cmake

cp src/api/CMakeLists.txt "$scratch/export-api.cmake"
printf '\ntarget_include_directories(doqe_api PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")\n' >> src/api/CMakeLists.txt
expect_failure private-export 'doqe_api: INTERFACE_INCLUDE_DIRECTORIES must equal' "${configure[@]}"
cp "$scratch/export-api.cmake" src/api/CMakeLists.txt

cp CMakeLists.txt "$scratch/exe-root.cmake"
printf '\nadd_executable(doqe_smoke src/cli/main.cpp)\n' >> CMakeLists.txt
expect_failure unknown-executable 'Unknown executable target: doqe_smoke' "${configure[@]}"
cp "$scratch/exe-root.cmake" CMakeLists.txt

printf '\n#include "doqe/api/boundary_fixture.hpp"\nstatic_assert(doqe::engine::boundary_fixture);\n' >> src/cli/version.cpp

run configure "${configure[@]}"
run positive "${check[@]}"
grep -F 'Module include boundaries passed' "$scratch/positive.log"
echo 'Positive cli -> api -> engine: passed'

for seed in direct-transitive private relative-escape; do
  cp "$scratch/cli.cpp" src/cli/version.cpp
  case "$seed" in
    direct-transitive)
      include='doqe/engine/boundary_fixture.hpp'
      diagnostic='cli has no direct dependency on engine'
      ;;
    private)
      touch src/api/private_fixture.hpp
      include='doqe/api/../../../private_fixture.hpp'
      diagnostic='invalid module-public include'
      ;;
    relative-escape)
      include='../api/private_fixture.hpp'
      diagnostic='include leaves module cli'
      ;;
  esac
  printf '\n#include "%s"\n' "$include" >> src/cli/version.cpp
  # Both the direct transitive include and relative private include compile.
  # Require a boundary diagnostic independently of compile success.
  run "$seed-build" "${build[@]}"
  expect_failure "$seed" "$diagnostic" "${check[@]}"
done
cp "$scratch/cli.cpp" src/cli/version.cpp

# Make a project header reachable under a noncanonical angle include.
printf '\n#include <boundary_fixture.hpp>\n' >> src/cli/version.cpp
printf '\ntarget_include_directories(doqe_cli PRIVATE "${CMAKE_SOURCE_DIR}/src/engine/include/doqe/engine")\n' >> src/cli/CMakeLists.txt
run angle-configure "${configure[@]}"
run angle-build "${build[@]}"
expect_failure angle-include 'project headers require quoted includes' "${check[@]}"
cp "$scratch/cli.cpp" src/cli/version.cpp

for seed in forbidden-link expression-link raw-link; do
  cp "$scratch/cli.cmake" src/cli/CMakeLists.txt
  case "$seed" in
    forbidden-link) link='doqe_engine'; diagnostic='doqe_cli LINK_LIBRARIES must equal' ;;
    expression-link) link='$<BUILD_INTERFACE:doqe_api>'; diagnostic='non-plain module link' ;;
    raw-link) link='./libdoqe_api.a'; diagnostic='non-plain module link' ;;
  esac
  printf '\ntarget_link_libraries(doqe_cli PRIVATE "%s")\n' "$link" >> src/cli/CMakeLists.txt
  expect_failure "$seed" "$diagnostic" "${configure[@]}"
done

# Forced includes have no source directive for the spelling check to inspect.
cp "$scratch/cli.cmake" src/cli/CMakeLists.txt
printf '\ntarget_compile_options(doqe_cli PRIVATE -include "${CMAKE_SOURCE_DIR}/src/api/private_fixture.hpp")\n' >> src/cli/CMakeLists.txt
run forced-private-configure "${configure[@]}"
run forced-private-build "${build[@]}"
expect_failure forced-private 'compiler dependency on foreign private header' "${check[@]}"

cp "$scratch/cli.cmake" src/cli/CMakeLists.txt
printf '\ntarget_compile_options(doqe_io PRIVATE -include "${CMAKE_SOURCE_DIR}/src/engine/include/doqe/engine/boundary_fixture.hpp")\n' >> src/io/CMakeLists.txt
run forced-public-configure "${configure[@]}"
run forced-public-build "${build[@]}"
expect_failure forced-public 'compiler dependency outside the allowed closure' "${check[@]}"


# Use the normal private api -> engine edge for compiler-dependency seeds.
sed 's/+engine/engine/' "$scratch/public-edge.cmake" > cmake/modules.cmake
printf '#pragma once\n' > src/api/include/doqe/api/boundary_fixture.hpp
api_cmake=src/api/CMakeLists.txt
cp "$api_cmake" "$scratch/api.cmake"
cp "$scratch/io.cmake" src/io/CMakeLists.txt
engine_header='src/engine/include/doqe/engine/boundary_fixture.hpp'


printf '\ntarget_compile_options(doqe_cli PRIVATE -include "${CMAKE_SOURCE_DIR}/%s")\n' "$engine_header" >> src/cli/CMakeLists.txt
run forced-private-chain-build "${build[@]}"
expect_failure forced-private-chain 'compiler dependency outside the allowed closure' "${check[@]}"
cp "$scratch/cli.cmake" src/cli/CMakeLists.txt

for seed in link-options interface-link-options direct-interface static-options link-directory final-link; do
  case "$seed" in
    link-options)
      where=src/cli/CMakeLists.txt
      statement='target_link_options(doqe_cli PRIVATE "$<TARGET_FILE:doqe_engine>")'
      diagnostic='doqe_cli: LINK_OPTIONS'
      ;;
    interface-link-options)
      where=$api_cmake
      statement='target_link_options(doqe_api INTERFACE "$<TARGET_FILE:doqe_server>")'
      diagnostic='doqe_api: INTERFACE_LINK_OPTIONS'
      ;;
    direct-interface)
      where=$api_cmake
      statement='set_property(TARGET doqe_api PROPERTY INTERFACE_LINK_LIBRARIES_DIRECT doqe_server)'
      diagnostic='doqe_api: INTERFACE_LINK_LIBRARIES_DIRECT'
      ;;
    static-options)
      where=src/cli/CMakeLists.txt
      statement='set_property(TARGET doqe_cli PROPERTY STATIC_LIBRARY_OPTIONS "$<TARGET_FILE:doqe_engine>")'
      diagnostic='doqe_cli: STATIC_LIBRARY_OPTIONS'
      ;;
    link-directory)
      where=src/cli/CMakeLists.txt
      statement='target_link_directories(doqe_cli PRIVATE "${CMAKE_BINARY_DIR}/src/engine")'
      diagnostic='doqe_cli: LINK_DIRECTORIES'
      ;;
    final-link)
      # Global linker flags bypass target properties. The unused archive is
      # built explicitly before linking doqe.
      where=cmake/pinned-runtime.cmake
      statement='string(APPEND CMAKE_EXE_LINKER_FLAGS " \"${CMAKE_BINARY_DIR}/src/server/libdoqe_server.a\"")'
      diagnostic='doqe link command uses archive outside the allowed closure'
      ;;
  esac
  cp "$where" "$scratch/seed.cmake"
  printf '\n%s\n' "$statement" >> "$where"
  if [[ $seed == final-link ]]; then
    run final-link-configure "${configure[@]}"
    run final-link-server "${build[@]}" --target doqe_server
    expect_failure "$seed" "$diagnostic" "${check[@]}"
  else
    expect_failure "$seed" "$diagnostic" "${configure[@]}"
  fi
  cp "$scratch/seed.cmake" "$where"
done

cp src/base/CMakeLists.txt "$scratch/base.cmake"
touch src/engine/private_fixture.hpp
printf '#include "../src/engine/private_fixture.hpp"\n' > tests/injected.cpp
printf '\ntarget_sources(doqe_base PRIVATE "${CMAKE_SOURCE_DIR}/tests/injected.cpp")\n' >> src/base/CMakeLists.txt
expect_failure foreign-test-source 'doqe_base: source outside' "${configure[@]}"
cp "$scratch/base.cmake" src/base/CMakeLists.txt
printf '\ntarget_sources(doqe_cli PRIVATE "${CMAKE_SOURCE_DIR}/src/engine/placeholder.cpp")\n' >> src/cli/CMakeLists.txt
expect_failure foreign-module-source 'doqe_cli: source outside' "${configure[@]}"
cp "$scratch/cli.cmake" src/cli/CMakeLists.txt
cat >> src/cli/CMakeLists.txt <<'CASE'
add_library(boundary_objects OBJECT ../engine/placeholder.cpp)
target_sources(doqe_cli PRIVATE $<TARGET_OBJECTS:boundary_objects>)
CASE
expect_failure target-objects 'doqe_cli: unsupported source expression' "${configure[@]}"
cp "$scratch/cli.cmake" src/cli/CMakeLists.txt

for seed in idirafter iquote framework include-directory cxx-isystem; do
  cp "$scratch/cli.cpp" src/cli/version.cpp
  search='${CMAKE_SOURCE_DIR}/src/engine/include/doqe/engine'
  header='<boundary_fixture.hpp>'
  diagnostic='project headers require quoted includes'
  case "$seed" in
    idirafter) option="-idirafter \"$search\"" ;;
    iquote)
      option="-iquote \"$search\""
      header='"boundary_fixture.hpp"'
      diagnostic='project header probe'
      ;;
    framework)
      mkdir -p src/engine/Boundary.framework/Headers
      cp "$engine_header" src/engine/Boundary.framework/Headers/boundary_fixture.hpp
      option='-F "${CMAKE_SOURCE_DIR}/src/engine"'
      header='<Boundary/boundary_fixture.hpp>'
      ;;
    include-directory) option="\"--include-directory=$search\"" ;;
    cxx-isystem) option="-cxx-isystem \"$search\"" ;;
  esac
  # Inspect every directive even when no compiler dependency is emitted.
  if [[ $seed == idirafter ]]; then
    printf '\n#include %s\n' "$header" >> src/cli/version.cpp
  elif [[ $seed == iquote ]]; then
    printf '\n#if __has_include(%s)\n#endif\n' "$header" >> src/cli/version.cpp
  else
    printf '\n#if 0\n#include %s\n#endif\n' "$header" >> src/cli/version.cpp
  fi
  printf '\ntarget_compile_options(doqe_cli PRIVATE %s)\n' "$option" >> src/cli/CMakeLists.txt
  run "$seed-build" "${build[@]}"
  expect_failure "$seed" "$diagnostic" "${check[@]}"
  cp "$scratch/cli.cmake" src/cli/CMakeLists.txt
done

for seed in bom digit-separator has-include digraph; do
  cp "$scratch/cli.cpp" src/cli/version.cpp
  diagnostic='cli has no direct dependency on engine'
  case "$seed" in
    bom)
      { printf '\357\273\277#include "doqe/engine/boundary_fixture.hpp"\n'; cat "$scratch/cli.cpp"; } > src/cli/version.cpp
      cp "$scratch/public-edge.cmake" cmake/modules.cmake
      ;;
    digit-separator)
      # A regex that mistakes the separator for a quote can also mistake
      # the line-comment contents for the start of a block comment.
      printf "\nconstexpr auto boundary_number = 1'000; // ' /*\n" >> src/cli/version.cpp
      printf '#if 0\n#include "doqe/engine/boundary_fixture.hpp"\n#endif\n// */\n' >> src/cli/version.cpp
      ;;
    has-include)
      printf '\n#if __has_include("doqe/engine/boundary_fixture.hpp")\n#endif\n' >> src/cli/version.cpp
      diagnostic='project header probe'
      ;;
    digraph)
      printf '\n#if 0\n/* prefix */ %%:include "doqe/engine/boundary_fixture.hpp"\n#endif\n' >> src/cli/version.cpp
      ;;
  esac
  run "$seed-build" "${build[@]}"
  expect_failure "$seed" "$diagnostic" "${check[@]}"
  sed 's/+engine/engine/' "$scratch/public-edge.cmake" > cmake/modules.cmake
done
cp "$scratch/cli.cpp" src/cli/version.cpp

cp cmake/pinned-runtime.cmake "$scratch/runtime.cmake"
cp src/server/CMakeLists.txt "$scratch/server.cmake"
for seed in final-object final-duplicate renamed-archive final-raw-archive; do
  case "$seed" in
    final-object)
      input='src/server/CMakeFiles/doqe_server.dir/placeholder.cpp.o'
      diagnostic='doqe link command uses foreign object'
      ;;
    final-duplicate)
      input='src/engine/libdoqe_engine.a'
      diagnostic='doqe link command repeats input'
      ;;
    renamed-archive)
      input='src/server/libsrv.a'
      printf '\nset_target_properties(doqe_server PROPERTIES OUTPUT_NAME srv)\n' >> src/server/CMakeLists.txt
      printf '\nstring(APPEND CMAKE_EXE_LINKER_FLAGS " \\"${CMAKE_BINARY_DIR}/%s\\"")\n' "$input" >> cmake/pinned-runtime.cmake
      expect_failure "$seed" 'doqe_server: OUTPUT_NAME is forbidden' "${configure[@]}"
      cp "$scratch/runtime.cmake" cmake/pinned-runtime.cmake
      cp "$scratch/server.cmake" src/server/CMakeLists.txt
      continue
      ;;
    final-raw-archive)
      input='src/server/libsrv.a'
      diagnostic='doqe link command uses archive outside the allowed closure'
      # Use the existing server archive under a non-module filename.
      run "$seed-input" "${build[@]}" --target doqe_server
      cp "$scratch/build/src/server/libdoqe_server.a" "$scratch/build/$input"
      ;;
  esac
  printf '\nstring(APPEND CMAKE_EXE_LINKER_FLAGS " \\"${CMAKE_BINARY_DIR}/%s\\"")\n' "$input" >> cmake/pinned-runtime.cmake
  run "$seed-configure" "${configure[@]}"
  run "$seed-input" "${build[@]}" --target doqe_server doqe_engine
  run "$seed-build" "${build[@]}"
  expect_failure "$seed" "$diagnostic" "${check[@]}"
  cp "$scratch/runtime.cmake" cmake/pinned-runtime.cmake
done

for seed in probe-alias embed-alias has-embed embed extensionless trigraphs vfsoverlay; do
  cp "$scratch/cli.cpp" src/cli/version.cpp
  case "$seed" in
    probe-alias|embed-alias)
      builtin=__has_include
      if [[ $seed == embed-alias ]]; then builtin=__has_embed; fi
      printf '\n#define HEADER_EXISTS %s\n#if 0\n#if HEADER_EXISTS("../server/placeholder.cpp")\n#endif\n#endif\n' "$builtin" >> src/cli/version.cpp
      diagnostic="macro aliases $builtin"
      ;;
    has-embed)
      printf '\n#if 0\n#if __has_embed("../server/placeholder.cpp" limit(1))\n#endif\n#endif\n' >> src/cli/version.cpp
      diagnostic='project header probe'
      ;;
    embed)
      printf '\n#if 0\n#embed "../server/placeholder.cpp" limit(1)\n#endif\n' >> src/cli/version.cpp
      diagnostic='include leaves module cli'
      ;;
    extensionless)
      printf '#include "doqe/engine/boundary_fixture.hpp"\n' > src/cli/extensionless
      diagnostic='extensionless: cli has no direct dependency on engine'
      ;;
    trigraphs)
      printf '\n#if 0\n??=include "../server/placeholder.cpp"\n#endif\n' >> src/cli/version.cpp
      printf '\ntarget_compile_options(doqe_cli PRIVATE -trigraphs)\n' >> src/cli/CMakeLists.txt
      diagnostic='unsupported compiler argument: -trigraphs'
      ;;
    vfsoverlay)
      touch src/cli/decoy.hpp
      printf '#define DOQE_BOUNDARY_OVERLAY 1\n' > src/api/private_fixture.hpp
      cat >"$scratch/overlay.yaml" <<CASE
{"version":0,"use-external-names":false,"roots":[
  {"type":"file","name":"$PWD/src/cli/decoy.hpp","external-contents":"$PWD/src/api/private_fixture.hpp"}
]}
CASE
      printf '\n#include "decoy.hpp"\nstatic_assert(DOQE_BOUNDARY_OVERLAY == 1);\n' >> src/cli/version.cpp
      printf '\ntarget_compile_options(doqe_cli PRIVATE -ivfsoverlay "%s/overlay.yaml")\n' "$scratch" >> src/cli/CMakeLists.txt
      diagnostic='unsupported compiler argument: -ivfsoverlay'
      ;;
  esac
  run "$seed-build" "${build[@]}"
  expect_failure "$seed" "$diagnostic" "${check[@]}"
  cp "$scratch/cli.cmake" src/cli/CMakeLists.txt
  rm -f src/cli/extensionless src/cli/decoy.hpp
done
cp "$scratch/cli.cpp" src/cli/version.cpp
printf '\n#define BOUNDARY_NOTE R"note(__has_include(<doqe/engine/x.hpp>))note"\n' >> src/cli/version.cpp
run probe-string "${check[@]}"
echo 'Probe text in a raw string literal: passed'
cp "$scratch/cli.cpp" src/cli/version.cpp

printf '\n#if defined /* feature */ (__has_include) && defined __has_include\n#if __has_include(<vector>)\n#endif\n#endif\n' >> src/cli/version.cpp
run defined-probe "${check[@]}"
echo 'Builtin feature guards and external probe: passed'
cp "$scratch/cli.cpp" src/cli/version.cpp

run diagnostic-flags-configure "${configure[@]}" '-DCMAKE_CXX_FLAGS=-Wall -Werror=unused-variable -Wno-error=unused-variable -fcolor-diagnostics -fno-color-diagnostics -fdiagnostics-color -fdiagnostics-color=never'
run diagnostic-flags "${check[@]}"
echo 'Warning and color flags on compile and link commands: passed'
run reset-diagnostic-flags "${configure[@]}" -DCMAKE_CXX_FLAGS=

cp cmake/build_version.hpp.in "$scratch/build_version.hpp.in"
printf '\n#if 0\n#include "doqe/engine/boundary_fixture.hpp"\n#endif\n' >> cmake/build_version.hpp.in
run generated-build "${build[@]}"
expect_failure generated 'build_version.hpp: cli has no direct dependency on engine' "${check[@]}"
cp "$scratch/build_version.hpp.in" cmake/build_version.hpp.in

test_configure=(cmake --preset debug -B "$scratch/build" -DBUILD_TESTING=ON)
ctest_modules=(ctest --test-dir "$scratch/build" --output-on-failure -R '^Modules.IncludeBoundaries$')
run test-configure "${test_configure[@]}"
run test-build "${build[@]}"
run clean-ctest "${ctest_modules[@]}"
ctest_edges=(ctest --test-dir "$scratch/build" --output-on-failure -R '^Modules.AdrEdges$' --no-tests=error)
run clean-adr-ctest "${ctest_edges[@]}"
echo 'ADR edge check registered in CTest: passed'
cp tests/integration/unwind_test.cpp "$scratch/unwind.cpp"
sed 's/<gtest\/gtest.h>/"gtest\/gtest.h"/' "$scratch/unwind.cpp" > tests/integration/unwind_test.cpp
run quoted-external "${ctest_modules[@]}"
echo 'Quoted GoogleTest header: passed'
cp "$scratch/unwind.cpp" tests/integration/unwind_test.cpp

printf '\ntarget_include_directories(doqe_cli PRIVATE "${CMAKE_BINARY_DIR}/_deps/googletest-src/googletest/include")\n' >> src/cli/CMakeLists.txt
printf '\n#include <gtest/gtest.h>\n' >> src/cli/version.cpp
run production-gtest-build "${build[@]}"
expect_failure production-gtest 'GoogleTest headers are test-only' "${ctest_modules[@]}"
cp "$scratch/cli.cpp" src/cli/version.cpp
printf '\ntarget_compile_options(doqe_cli PRIVATE -include "${CMAKE_BINARY_DIR}/_deps/googletest-src/googletest/include/gtest/gtest.h")\n' >> src/cli/CMakeLists.txt
run production-forced-gtest-build "${build[@]}"
expect_failure production-forced-gtest 'compiler dependency on test-only GoogleTest header' "${ctest_modules[@]}"
cp "$scratch/cli.cmake" src/cli/CMakeLists.txt

cp CMakeLists.txt "$scratch/root.cmake"
# CMake 3.25 can emit the engine archive twice for these declared links.
sed 's/doqe_tests PRIVATE doqe_cli GTest/doqe_tests PRIVATE doqe_engine doqe_cli GTest/' \
  "$scratch/root.cmake" > CMakeLists.txt
printf '\n#include "doqe/engine/boundary_fixture.hpp"\n' >> tests/integration/unwind_test.cpp
run overlapping-test-links-configure "${test_configure[@]}"
run overlapping-test-links "${ctest_modules[@]}"
echo 'Overlapping direct and transitive test links: passed'
cp "$scratch/root.cmake" CMakeLists.txt
cp "$scratch/unwind.cpp" tests/integration/unwind_test.cpp

cp src/cli/CMakeLists.txt "$scratch/test-link-cli.cmake"
printf '\nset(CMAKE_EXE_LINKER_FLAGS "")\n' >> src/cli/CMakeLists.txt
printf '\nstring(APPEND CMAKE_EXE_LINKER_FLAGS " \\"${CMAKE_BINARY_DIR}/src/server/CMakeFiles/doqe_server.dir/placeholder.cpp.o\\"")\n' >> cmake/pinned-runtime.cmake
run final-test-link-configure "${test_configure[@]}"
run final-test-link-input "${build[@]}" --target doqe_server
run final-test-link-build "${build[@]}"
expect_failure final-test-link 'doqe_tests link command uses foreign object' "${ctest_modules[@]}"
cp "$scratch/runtime.cmake" cmake/pinned-runtime.cmake
cp "$scratch/test-link-cli.cmake" src/cli/CMakeLists.txt

mkdir -p tests/support
cat >> CMakeLists.txt <<'CASE'
add_library(boundary_helper OBJECT tests/support/boundary_helper.cpp)
target_include_directories(boundary_helper PRIVATE src/cli)
target_link_libraries(doqe_tests PRIVATE boundary_helper)
CASE
printf '#include "version.hpp"\n' > tests/support/boundary_helper.cpp
expect_failure test-object-helper 'doqe_tests: non-plain test link' "${test_configure[@]}"
cp "$scratch/root.cmake" CMakeLists.txt
rm tests/support/boundary_helper.cpp

printf '#pragma once\ninline constexpr bool boundary_support = true;\n' > tests/support/fixture.hpp
printf '\n#include "../support/fixture.hpp"\nstatic_assert(boundary_support);\n' >> tests/integration/unwind_test.cpp
run support-header-build "${build[@]}"
run support-header "${ctest_modules[@]}"
echo 'Header-only shared test support: passed'

sed '/^  include(GoogleTest)/i\
  add_library(doqe_test_support STATIC tests/support/fixture.cpp)\
  target_link_libraries(doqe_test_support PUBLIC doqe_engine)\
  target_link_libraries(doqe_tests PRIVATE doqe_test_support)
' "$scratch/root.cmake" > CMakeLists.txt
printf '#pragma once\n#include "doqe/engine/boundary_fixture.hpp"\ninline constexpr bool boundary_support = doqe::engine::boundary_fixture;\n' > tests/support/fixture.hpp
printf '#include "fixture.hpp"\nstatic_assert(boundary_support);\n' > tests/support/fixture.cpp
run test-support-build "${build[@]}"
run test-support "${ctest_modules[@]}"
echo 'Shared test header and compiled test support: passed'

printf '\n#if 0\n#include "../../src/cli/version.hpp"\n#endif\n' >> tests/support/fixture.cpp
expect_failure support-private 'include leaves test directory support' "${ctest_modules[@]}"
printf '#include "fixture.hpp"\n' > tests/support/fixture.cpp
printf '\ntarget_compile_options(doqe_test_support PRIVATE -include "${CMAKE_SOURCE_DIR}/src/cli/version.hpp")\n' >> CMakeLists.txt
run support-forced-private-build "${build[@]}"
expect_failure support-forced-private 'compiler dependency outside the allowed closure' "${ctest_modules[@]}"
printf '\ntarget_link_libraries(doqe_cli PRIVATE doqe_test_support)\n' >> src/cli/CMakeLists.txt
expect_failure production-support 'non-plain module link' "${test_configure[@]}"
cp "$scratch/cli.cmake" src/cli/CMakeLists.txt
cp "$scratch/root.cmake" CMakeLists.txt
cp "$scratch/unwind.cpp" tests/integration/unwind_test.cpp
rm tests/support/fixture.{cpp,hpp}

printf '\n#if 0\n#include "doqe/engine/boundary_fixture.hpp"\n#endif\n' >> cmake/build_version.hpp.in
expect_failure stale-ctest 'build_version.hpp: cli has no direct dependency on engine' "${ctest_modules[@]}"
cp "$scratch/build_version.hpp.in" cmake/build_version.hpp.in

printf '\n#include "../../src/cli/version.hpp"\n' >> tests/integration/unwind_test.cpp
run integration-private-build "${build[@]}"
expect_failure integration-private 'include leaves test directory integration' "${ctest_modules[@]}"

echo 'All module boundary fixtures passed'
