# Development

Everything tool- and machine-specific for this project: language, toolchain, commands, checks, machines, benchmarks.
The method lives in [WORKFLOW.md](WORKFLOW.md); why each tool was chosen lives in ADRs in [adrs/](adrs/).

Record choices only from accepted ADRs. Describe the current setup only, and update it in the same
commit as the tool change. Every ADR-based entry links the ADR it comes from.

## Language and toolchain
Language and version, compilers or runtimes, build tools, versions, and how to install them.

From [ADR-0001](adrs/0001-core-cpp-toolchain/adr.md):

| Part | Setup |
|---|---|
| Language | ISO C++23 (`CXX_STANDARD 23`, required, no extensions). No C++ modules; `CMAKE_CXX_SCAN_FOR_MODULES OFF`. Exceptions and RTTI on (compiler defaults) until their ADR. |
| C++23 library allowlist | `std::expected`, `std::unexpected`, `std::byteswap`, `std::unreachable`, `std::to_underlying`. Nothing else from C++23's library. |
| Compiler | Upstream LLVM Clang **23.1.5** release tarball, SHA-256-verified, installed in user space (e.g. `~/opt/llvm-23.1.5`). Configure checks the Clang ID, version, upstream banner, driver names, resolved paths and pinned compiler SHA-256. |
| Tools | clang-tidy, clang-format, libFuzzer and sanitizer runtimes from the same LLVM install. |
| C++ library | The pin's libc++ headers; `libc++.a` and `libc++abi.a` linked statically. Linux: static LLVM `libunwind.a`. macOS: system unwinder. |
| Build | CMake ≥ 3.25 with `CMakePresets.json` `"version": 6`; single-config Ninja ≥ 1.12.1. |
| Tests | GoogleTest commit `52eb8108c5bdec04579160ae17225d66034bd723` (v1.17.0) via FetchContent, run with CTest. No package manager. |
| Targets | macOS 26 arm64 (SDK ≥ 26.0 sysroot, deployment target 26.0, availability warnings as errors). Debian 13 x86_64 (glibc 2.41; deploy on Debian 13 or a Debian 13-based container). No `-march=native` in shipped builds. |

LLVM 23.1.5 is installed at `~/opt/llvm-23.1.5` on the Mac and dev01. The task records the
[installation and archive checks](work/T0002-build-test-skeleton/refs/llvm-install.txt). The Mac
tarball SHA-256 was verified and its detached signature remains unverified. The configure gate
checks the resolved LLVM root and pins the SHA-256 of each host's real `bin/clang-23` executable
and static runtime archives. Every C++ compile, including GoogleTest, checks the libc++ header
version through a forced include.

Keep `CPATH`, `C_INCLUDE_PATH`, `CPLUS_INCLUDE_PATH`, `OBJC_INCLUDE_PATH` and `LIBRARY_PATH`
unset when configuring and building. Compiler and linker launchers require an ADR. Keep
`CMAKE_C_COMPILER_LAUNCHER`, `CMAKE_CXX_COMPILER_LAUNCHER`, `CMAKE_C_LINKER_LAUNCHER`
and `CMAKE_CXX_LINKER_LAUNCHER` unset in both CMake and the environment. Shared-library builds
require an exported-symbols ADR. [Architecture](arch/README.md)

To update the pin after approval, verify the upstream release tarball on each supported host.
In [the compiler gate](../cmake/require-llvm.cmake), the platform branches in `doqe_verify_llvm`
hold the `clang_sha256`, `libcxx_sha256`, `libcxxabi_sha256` and Linux `libunwind_sha256` lines.
The comparison with `banner` in the same function holds the exact first line of `clang++ --version`.
Update the version in the gate, the default install path in [the toolchain file](../cmake/llvm.cmake),
the `_LIBCPP_VERSION` assertion in [the forced include](../cmake/check-libcxx.hpp), and the pin
and version expectations in [the check script](work/T0002-build-test-skeleton/refs/skeleton-checks.sh).
The script prints the banner and digests before configuring, so it can report the candidate values
even while the gate still rejects them. Rerun the script on both hosts after updating the pin.

## Commands
Exact commands to build, test, run each check, and reproduce CI locally.

Run the Debug configure, build, and test workflow from the repository root.

```sh
cmake --workflow --preset debug
./build/debug/doqe --version
```

To run the same steps individually:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

After configuring Debug, run both quality checks or run either target by itself.

```sh
cmake --build --preset quality
cmake --build --preset debug --target format
cmake --build --preset debug --target lint
```

Format checks Git-tracked C and C++ sources, headers, and C++ header templates with the pin's
clang-format. Lint runs the pin's clang-tidy on project-owned translation units selected from
the Debug compilation database. It reports header diagnostics under the checkout's source
path and treats warnings as errors. Build directories and fetched GoogleTest sources are
excluded.

Run AddressSanitizer and UndefinedBehaviorSanitizer together in their own build directory.

```sh
cmake --workflow --preset asan-ubsan
```

Debug and Release link the pinned static C++ runtime and run without ASan. The ASan+UBSan
preset instruments project code and GoogleTest, keeps frame pointers, and makes sanitizer
findings fatal. CTest runs with `ASAN_OPTIONS=detect_stack_use_after_return=1`.
On Linux the preset links static compiler-rt and the pinned static C++ runtime,
including libunwind. On macOS it links the pinned static C++ runtime and the pinned
compiler-rt ASan dylib, which also supplies UBSan handlers. The Mac executables keep empty
export tables. Seeded checks on both hosts cover C++ heap use-after-free, heap-buffer-overflow,
and signed overflow. On macOS, ASan sees `malloc` and `free` beneath the static C++ allocation
operators. The pinned shared libc++ trial loaded both the pinned and system C++ runtimes and
aborted at GoogleTest registration. Allocation-family mismatch detection runs on Linux, where
compiler-rt's `asan_cxx` supplies `operator new` and `operator delete`. The proof script checks
`new` with `free`, array `new` with scalar `delete`, and a wrong-sized `delete` on Linux.
[Prototype evidence](work/T0003-quality-checks-ci/refs/asan-prototype.txt)

`Runtime.SharedDependencies` inspects direct Mach-O load commands on macOS and resolved
dependencies on Linux. It rejects shared C++ runtimes and requires the macOS ASan dylib
or Linux static `__asan_init` in both executables when sanitizers are enabled.
The Mac compiler-rt dylib loads system libc++ and libc++abi for its own use.

To use the same verified LLVM installation at another path, configure a fresh build directory
and point `DOQE_LLVM_ROOT` at its absolute path.

```sh
cmake --preset debug -B build/debug-alt -DDOQE_LLVM_ROOT="/absolute/path/to/llvm-23.1.5"
cmake --build build/debug-alt
ctest --test-dir build/debug-alt --output-on-failure
./build/debug-alt/doqe --version
```

On dev01, prefix build and workflow commands with `flock "$HOME/doqe-quiet.lock"`.

```sh
flock "$HOME/doqe-quiet.lock" cmake --workflow --preset debug
flock "$HOME/doqe-quiet.lock" cmake --build --preset quality
flock "$HOME/doqe-quiet.lock" cmake --workflow --preset asan-ubsan
```

From a checkout without a `build/debug` directory, the
[skeleton check script](work/T0002-build-test-skeleton/refs/skeleton-checks.sh) reruns validation
and takes the machine lock on Linux.

```sh
bash docs/work/T0002-build-test-skeleton/refs/skeleton-checks.sh
```

The script removes its temporary logs and fixtures on exit. Set `DOQE_KEEP_SKELETON_LOGS=1` to keep them.

Run the [quality check proof script](work/T0003-quality-checks-ci/refs/quality-checks.sh) to
repeat the positive and seeded-failure checks in a disposable copy. The script takes the
machine lock on Linux. Invoke it directly on dev01.

```sh
bash docs/work/T0003-quality-checks-ci/refs/quality-checks.sh
```

`doqe --version` appends `-dirty` when Git reports worktree changes, including untracked files.

## Checks
The tool that covers each check kind in WORKFLOW.md.

| Check kind | Tool | Where it runs |
|---|---|---|
| memory safety / undefined behavior | ASan + UBSan from the pin, `asan-ubsan` workflow | Mac, dev01, both CI platforms |
| races and leaks | | |
| fuzz / property tests | libFuzzer from the pin (planned) | |
| static checks and formatting | clang-tidy and clang-format from the pin, `quality` build preset | Mac, dev01, both CI platforms |
| target platforms | Debug workflow on macOS 26 arm64 and Debian 13 x86_64 | Mac, dev01, both CI platforms |
| toolchain floors | Debug with CMake 3.25.3 and Ninja 1.12.1 on Debian 13, Debug with macOS SDK 26.0 | CI |

## Machines
Each machine or CI runner and what it is trusted for (correctness, performance, or both).

| Machine | Platform | Use for | Not for |
|---|---|---|---|
| Mac (owner) | macOS 26 arm64, Apple M4 | development, correctness | |
| dev01 | Debian 13 x86_64, Intel i3-1220P, user space only | development, correctness | |
| GitHub Actions `macos-26` | macOS 26 arm64, Xcode 26.0.1 for the SDK floor | build and correctness checks | performance |
| GitHub Actions `ubuntu-latest` with `debian:13` | Debian 13 x86_64, distribution Ninja, separate CMake 3.25.3 job | build and correctness checks | performance |

The [CI workflow](../.github/workflows/ci.yml) runs on pushes and pull requests. It caches the
LLVM release archive by version, asset, and SHA-256. Every job verifies the entire archive,
including cache hits. Debian extracts the compiler and check tools, libc++ headers, static C++
runtime archives, and Clang resource directory to `$HOME/opt/llvm-23.1.5`. Both Debian jobs assert
Ninja 1.12.1. The CMake floor job asserts 3.25.3, and the SDK floor step checks the sysroot in
the compilation database for SDK 26.0. First hosted run: all jobs green (run 44823432158).

## Benchmarks
Harness, which machine to run on, and where results are recorded.

Not set up yet.
