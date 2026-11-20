# ADR-0001: Use C++23, LLVM Clang and CMake for the core toolchain

| Status | Date | Decided by | Confidence | Tags |
|---|---|---|---|---|
| proposed | 2026-11-20 | owner | medium | build, testing, portability |

Supersedes: none · Superseded by: none · Related: none

## Context

M01 needs clean clones to build and run tests on the Mac and dev01. dev01 setup is user-space only. [M01](../../road/M01-walking-skeleton.md).

## Criteria

1. Use the same ISO C++ subset and compiler checks on both hosts.
2. Minimize runtime dependencies and make test dependency selection reproducible.
3. Share local and CI build settings and exercise both native targets with useful correctness checks.

## Options

**Language and compiler**

### A. ISO C++23

- **Good.** `std::expected` provides a value-or-error type.
- **Bad.** Each newly used library feature needs a portability check.

### B. ISO C++20

- **Good.** `std::span` and concepts cover buffer views and constrained templates.
- **Bad.** The engine would need another value-or-error type.

### C. Pin upstream LLVM Clang on both hosts

- **Good.** One compiler release and matching check tools.
- **Bad.** Both hosts need user-space provisioning and pin updates.

### D. Use platform compilers

- **Good.** Uses compilers already inventoried on each host.
- **Bad.** In our judgment, different compilers increase the work of reproducing diagnostics and checking feature availability. [C++ and LLVM](refs/sources.md#c-and-llvm), [inventory](refs/tool-inventory.txt).

**Standard library and unwinding**

### E. Static pinned libc++ and libc++abi on both hosts

- **Good.** One set of headers and ABI libraries, with no separate C++ runtime to provision for deployment.
- **Bad.** Larger binaries and additional sanitizer link and instrumentation work. The archives still need verification.

### F. Use platform C++ libraries

- **Good.** The macOS system libc++ and Debian libstdc++ are supplied by their platforms.
- **Bad.** Two library implementations expand the feature and ABI matrix. Stock libstdc++ is uninstrumented for MSan. [C++ and LLVM](refs/sources.md#c-and-llvm).

**Build and dependencies**

### G. CMake with Ninja

- **Good.** Shared workflow presets, a compilation database, and incremental header dependency tracking.
- **Bad.** Requires a separate Ninja executable.

### H. CMake with Unix Makefiles

- **Good.** Retains CMake presets and the compilation database.
- **Bad.** In our judgment, Ninja's incremental scheduling better fits a growing engine. Build speed is unmeasured.

### I. Meson with Ninja

- **Good.** Out-of-tree builds and a native test command.
- **Bad.** In our judgment, adopting Meson's test workflow would cost us shared workflow presets and built-in CTest discovery for GoogleTest.

### J. Bazel

- **Good.** Supports hermetic builds and remote caching.
- **Bad.** In our judgment, toolchain rules and remote cache operation add work without an M01 need. [Build tools](refs/sources.md#build-tools).

### K. CMake FetchContent

- **Good.** One pinned source dependency builds with the project toolchain.
- **Bad.** Coordinating transitive versions becomes manual as dependencies grow.

### L. vcpkg

- **Good.** Manifests and registries manage versions, with a baseline for minimum versions.
- **Bad.** Bootstrap and port maintenance add work for one test dependency.

### M. Conan

- **Good.** Profiles and lockfiles manage binary configurations and dependency versions.
- **Bad.** Adds a separate client and package configuration.

### N. CPM

- **Good.** Adds dependency caching and source selection to FetchContent.
- **Bad.** Adds a CMake layer to pin and maintain. [Dependencies](refs/sources.md#dependencies).

**Testing and targets**

### O. GoogleTest with CTest

- **Good.** Typed and value-parameterized tests, subprocess death assertions, and CTest discovery.
- **Bad.** Adds a test-only dependency and framework conventions.

### P. Catch2 v3

- **Good.** Provides sections, typed cases, and CTest discovery.
- **Bad.** Subprocess death checks need separate harness code.

### Q. Plain CTest executables

- **Good.** Dependency-free baseline.
- **Bad.** Typed registration, failure diagnostics, and death checks need in-house harness code.

### R. doctest

- **Good.** Lightweight single-header setup.
- **Bad.** Subprocess death checks need separate harness code. [Test frameworks](refs/sources.md#test-frameworks).

### S. Native macOS and Debian

- **Good.** Exercises both M01 hosts and architectures.
- **Bad.** Requires two native CI environments.

### T. Debian only

- **Good.** One CI environment.
- **Bad.** Misses M01's native Mac requirement. [M01](../../road/M01-walking-skeleton.md).

## References

- **Measured.** [Inventory](refs/tool-inventory.txt) records tool versions on macOS arm64 and Debian 13 x86_64, plus header preprocessing with existing toolchains at `16fbf5678c968dbcee58f9932f9a20909ed3f3d3` in a dirty Mac checkout. Reproduce with `sh docs/adrs/0001-core-cpp-toolchain/refs/tool-inventory.sh`. [Script](refs/tool-inventory.sh).
- **Source.** [Primary sources](refs/sources.md) cover release assets and SHA-256 digests, feature support, tooling, the dependency pin, and deployment.
- **Judgment.** A common compiler and library version makes native support easier. One pinned test dependency is sufficient for M01.

## Decision

- **Language (criterion 1).** Use ISO C++23 (`CXX_STANDARD 23`, `CXX_STANDARD_REQUIRED ON`, `CXX_EXTENSIONS OFF`). Allow all C++23 language features supported by the pinned Clang, subject to the modules policy. No C++ modules: no named modules, header units or `import std`. Set `CMAKE_CXX_SCAN_FOR_MODULES OFF` before creating targets. CMake 3.28 and newer scan C++20 and later sources by default with supported Ninja and Clang versions under `CMP0155`'s new behavior. Keep exceptions and RTTI at compiler defaults (both on) until the **Engine error handling and RTTI ADR** (not yet created). [C++ and LLVM](refs/sources.md#c-and-llvm), [build tools](refs/sources.md#build-tools).
- **C++23 library allowlist (criterion 1).** Limit C++23 library use to `std::expected` and `std::unexpected` from `<expected>`, `std::byteswap` from `<bit>`, and `std::unreachable` and `std::to_underlying` from `<utility>`. The allowlist applies only to library features. [C++ and LLVM](refs/sources.md#c-and-llvm).
- **Compiler (criteria 1 and 3).** Pin upstream **LLVM Clang 23.1.5**, the newest 23.1 patch with both required release archives. The 23.1.6 asset list lacks macOS arm64. Verify each selected tarball against its published SHA-256 before installing it in user space (e.g. `~/opt/llvm-23.1.5`). The upstream tarball is the sole source for Clang, clang-tidy, clang-format, libFuzzer, and sanitizer runtimes. Homebrew, Apple Clang, and distro Clang are excluded. CMake configure must fail unless `CMAKE_CXX_COMPILER_ID` is `Clang`, `CMAKE_CXX_COMPILER_VERSION` is exactly `23.1.5`, and the resolved `CMAKE_CXX_COMPILER` is `bin/clang++` inside the checksum-verified installation; a same-version Clang from another source (e.g. Homebrew) is rejected. [Release assets and SHA-256](refs/sources.md#llvm-release-assets).
- **Standard library and unwinder (criteria 1 and 2).** Use only the pin's libc++ headers with `-stdlib=libc++`. Disable default C++ header search so neither the macOS SDK's `c++/v1` nor Debian's libc++ 19 headers are selected. Link the pin's `libc++.a` and `libc++abi.a` explicitly and suppress default C++ runtime linkage. On Linux, link the pin's static LLVM `libunwind.a` so the ABI library and unwinder stay on the same release and unwinding adds no `libgcc_s` deployment dependency. On macOS, use the system unwinder via libSystem so doqe uses the platform's unwind implementation. Tarball contents are not verified for `libc++.a`, `libc++abi.a`, or `libunwind.a` on either platform. [C++ and LLVM](refs/sources.md#c-and-llvm).
- **Build (criterion 3).** Use CMake with single-config Ninja and checked-in configure, build, test, and workflow presets. Set the presets file to `"version": 6`, which introduces workflow presets and sets the CMake floor at **3.25**. Set the Ninja floor at **1.12.1**, the version Debian 13 ships. [Build tools](refs/sources.md#build-tools).
- **Dependencies and tests (criteria 2 and 3).** Use GoogleTest with CTest discovery. Fetch GoogleTest via CMake FetchContent from its upstream Git repository at commit `52eb8108c5bdec04579160ae17225d66034bd723` and build it with the project toolchain. M01 uses no package manager. Defer package-manager choices to the **Iceberg/Arrow dependency ADR** (not yet created). [Dependencies](refs/sources.md#dependencies), [test frameworks](refs/sources.md#test-frameworks).
- **Targets (criteria 1 and 3).** Support macOS 26 arm64 and Debian 13 x86_64. Compile on macOS with SDK 26.0 or later as the sysroot (`CMAKE_OSX_SYSROOT`), set `CMAKE_OSX_DEPLOYMENT_TARGET=26.0` (`-mmacosx-version-min=26.0`), and promote availability warnings to errors. Build Linux deployment binaries against Debian 13 glibc 2.41 and deploy on Debian 13 or in a Debian 13-based container. Shipped builds must avoid `-march=native`. Defer the SIMD baseline and dispatch policy to the **Engine SIMD and ISA ADR** (not yet created). [C++ and LLVM](refs/sources.md#c-and-llvm), [CI and deployment](refs/sources.md#ci-and-deployment).

## Consequences

### Easier

- One pin makes diagnostics and C++23 library behavior easier to reproduce across hosts. Static linkage removes the need to install a matching shared C++ runtime on deployment hosts.

### Harder / cost we accept

- Provision LLVM in user space on dev01 and the Mac. Updating the pin requires checking both release assets and revalidating both hosts.
- Static libc++ increases binary size and requires sanitizer runtime and link validation. The hosted macOS runtime is tested on current 26.x only.

### Follow-up work

- **F01.2.** Obtain owner approval before installing LLVM on either host. Confirm archive contents, run the pin on both hosts, and verify accepted and rejected compiler configurations, including a same-version Clang outside the pinned installation. Compile, link and run allowlisted features, test exception unwinding (on macOS, confirm from the link map or symbol bindings which library provides `_Unwind_RaiseException`, since `libc++abi.a` can embed unwind objects), and inspect final binary dependencies for the selected runtimes. The CMake, Ninja and SDK floors remain unverified with this pin. Record setup in [Development](../../DEVELOPMENT.md) after ADR acceptance.
- **F01.3.** Use standard GitHub Actions runners, free for public repositories. Run Linux on Ubuntu x86_64 in a `debian:13` job container and macOS on the native `macos-26` arm64 runner. Check the CMake, Ninja and SDK floors in CI with the exact LLVM pin. [CI and deployment](refs/sources.md#ci-and-deployment).
- **F01.3.** Run ASan+UBSan on both targets with matching compile and link flags for project code and fetched dependencies. Seed failures to verify reporting and failing job status. Validate ASan interception and link flags with static libc++. For Linux MSan, build a separate instrumented libc++ from the same LLVM pin and instrument dependent code. Keep platform system libraries dynamically linked. [C++ and LLVM](refs/sources.md#c-and-llvm), [M01](../../road/M01-walking-skeleton.md).

## Revisit if

The pin or required archives are unavailable on a target, the engine needs modules or a C++23 library feature outside the allowlist, an approved dependency requires a package manager, or deployment needs another OS baseline.
