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
```

From a checkout without a `build/debug` directory, the
[skeleton check script](work/T0002-build-test-skeleton/refs/skeleton-checks.sh) reruns validation
and takes the machine lock on Linux.

```sh
bash docs/work/T0002-build-test-skeleton/refs/skeleton-checks.sh
```

The script removes its temporary logs and fixtures on exit. Set `DOQE_KEEP_SKELETON_LOGS=1` to keep them.

`doqe --version` appends `-dirty` when Git reports worktree changes, including untracked files.

## Checks
The tool that covers each check kind in WORKFLOW.md.

| Check kind | Tool | Where it runs |
|---|---|---|
| memory safety / undefined behavior | ASan + UBSan (planned, F01.3) | |
| races and leaks | | |
| fuzz / property tests | libFuzzer from the pin (planned) | |
| static checks and formatting | clang-tidy, clang-format from the pin (planned, F01.3) | |
| target platforms | macOS 26 arm64, Debian 13 x86_64 | GitHub Actions (planned, F01.3) |

## Machines
Each machine or CI runner and what it is trusted for (correctness, performance, or both).

| Machine | Platform | Use for | Not for |
|---|---|---|---|
| Mac (owner) | macOS 26 arm64, Apple M4 | development, correctness | |
| dev01 | Debian 13 x86_64, Intel i3-1220P, user space only | development, correctness | |

## Benchmarks
Harness, which machine to run on, and where results are recorded.

Not set up yet.
