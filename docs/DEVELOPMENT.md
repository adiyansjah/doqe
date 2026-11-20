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
| Compiler | Upstream LLVM Clang **23.1.5** release tarball, SHA-256-verified, installed in user space (e.g. `~/opt/llvm-23.1.5`). No Homebrew, Apple Clang or distro Clang. Configure fails for any other compiler ID, version or path. |
| Tools | clang-tidy, clang-format, libFuzzer and sanitizer runtimes from the same LLVM install. |
| C++ library | The pin's libc++ headers; `libc++.a` and `libc++abi.a` linked statically. Linux: static LLVM `libunwind.a`. macOS: system unwinder. |
| Build | CMake ≥ 3.25 with `CMakePresets.json` `"version": 6`; single-config Ninja ≥ 1.12.1. |
| Tests | GoogleTest commit `52eb8108c5bdec04579160ae17225d66034bd723` (v1.17.0) via FetchContent, run with CTest. No package manager. |
| Targets | macOS 26 arm64 (SDK ≥ 26.0 sysroot, deployment target 26.0, availability warnings as errors). Debian 13 x86_64 (glibc 2.41; deploy on Debian 13 or a Debian 13-based container). No `-march=native` in shipped builds. |

Install status: LLVM 23.1.5 is not installed on any machine yet (F01.2, owner approval needed).

## Commands
Exact commands to build, test, run each check, and reproduce CI locally.

Not set up yet (F01.2).

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
