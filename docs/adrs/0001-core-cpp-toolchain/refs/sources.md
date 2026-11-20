# Sources for ADR-0001

All sources read 2026-11-20.

## C++ and LLVM

- Clang tracks ISO C++ language feature support. The LLVM 23 libc++ table documents `expected` (including `unexpected`), `byteswap`, `unreachable` and `to_underlying`. GCC tracks libstdc++ separately. [Clang C++ status](https://clang.llvm.org/cxx_status.html), [libc++ C++23 status](https://releases.llvm.org/23.1.0/projects/libcxx/docs/Status/Cxx23.html), [libstdc++ status](https://gcc.gnu.org/onlinedocs/libstdc%2B%2B/manual/status.html).
- The libc++ supported-compiler policy covers the two latest stable upstream Clang release series and the development version. The LLVM 22.x series finished with 22.1.8, the last planned 22.1.x release. [libc++ compiler policy](https://libcxx.llvm.org/#platform-and-compiler-support), [LLVM 22.1.8 announcement](https://discourse.llvm.org/t/llvm-22-1-8-released/91084).
- Apple documents SDK and OS availability. LLVM documents how deployment availability controls libc++ declarations and feature macros. CMake exposes the SDK sysroot and the minimum macOS version as separate settings. [Apple C++ support](https://developer.apple.com/xcode/cpp/), [libc++ availability](https://libcxx.llvm.org/VendorDocumentation.html), [CMake sysroot](https://cmake.org/cmake/help/v3.25/variable/CMAKE_OSX_SYSROOT.html), [CMake deployment target](https://cmake.org/cmake/help/v3.25/variable/CMAKE_OSX_DEPLOYMENT_TARGET.html).
- Clang documents standard-library, ABI library, and unwinder selection. [Clang toolchain](https://clang.llvm.org/docs/Toolchain.html).
- Clang documents ASan and UBSan link requirements. MSan supports Linux and documents a self-built instrumented libc++ and the need to instrument dependent code. libFuzzer is provided by LLVM's compiler-rt. [ASan](https://clang.llvm.org/docs/AddressSanitizer.html), [UBSan](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html), [MSan](https://clang.llvm.org/docs/MemorySanitizer.html), [libFuzzer](https://llvm.org/docs/LibFuzzer.html).

### LLVM release assets

- The 23.1.6 asset list shows Linux x64 archives with SHA-256 digests and no macOS arm64 archive. LLVM 23.1.5 is the newest 23.1 patch with both target archives, detached signatures, and published SHA-256 digests. [23.1.6 assets](https://github.com/llvm/llvm-project/releases/expanded_assets/llvmorg-23.1.6), [23.1.5 assets and digests](https://github.com/llvm/llvm-project/releases/expanded_assets/llvmorg-23.1.5).

| Target | LLVM 23.1.5 archive | Detached signature | Published archive SHA-256 |
|---|---|---|---|
| macOS arm64 | `LLVM-23.1.5-macOS-ARM64.tar.xz` | `LLVM-23.1.5-macOS-ARM64.tar.xz.sig` | `aba592b4a15e8f90c634c5367b51ec8418b8bcd2829865db331a17dc9418dcff` |
| Linux x86_64 | `LLVM-23.1.5-Linux-X64.tar.xz` | `LLVM-23.1.5-Linux-X64.tar.xz.sig` | `f72edb0b36580ece7f52e23858f00c18fad4e87395ea7ad05d7e55c39f8fc78a` |

The release asset lists show archive metadata. Archive bytes, signatures and contents are unverified.

## Build tools

- CMake 3.25 added schema 6 and workflow presets. C++23 is supported as a standard level from 3.20 and CMake can require the requested level and disable compiler extensions. [CMake 3.25 changes](https://cmake.org/cmake/help/v3.25/release/3.25.html), [CMake presets](https://cmake.org/cmake/help/v3.25/manual/cmake-presets.7.html), [CXX_STANDARD](https://cmake.org/cmake/help/v3.25/prop_tgt/CXX_STANDARD.html).
- CMake 3.28 added `CMAKE_CXX_SCAN_FOR_MODULES`. Under `CMP0155`'s new behavior CMake scans C++20 and newer sources by default when the generator and compiler support it, including Ninja with recent Clang. [Module scanning variable](https://cmake.org/cmake/help/latest/variable/CMAKE_CXX_SCAN_FOR_MODULES.html), [CMP0155](https://cmake.org/cmake/help/latest/policy/CMP0155.html), [CMake modules manual](https://cmake.org/cmake/help/latest/manual/cmake-cxxmodules.7.html).
- Ninja and Unix Makefiles support compilation databases with CMake. Ninja tracks header dependencies. Debian 13 ships Ninja 1.12.1. [CMake compilation database](https://cmake.org/cmake/help/v3.25/variable/CMAKE_EXPORT_COMPILE_COMMANDS.html), [Ninja manual](https://ninja-build.org/manual.html), [Debian ninja-build](https://packages.debian.org/trixie/ninja-build).
- Meson defaults to Ninja and supports out-of-tree builds and a test subcommand. Bazel documents hermeticity and remote caching. [Running Meson](https://mesonbuild.com/Running-Meson.html), [Bazel hermeticity](https://bazel.build/basics/hermeticity), [Bazel remote caching](https://bazel.build/remote/caching).

## Dependencies

- FetchContent makes sources available during configuration and recommends a Git commit hash as a pin. GoogleTest's v1.17.0 release commit is the proposed source pin. [CMake FetchContent](https://cmake.org/cmake/help/v3.25/module/FetchContent.html), [GoogleTest commit](https://github.com/google/googletest/commit/52eb8108c5bdec04579160ae17225d66034bd723).
- vcpkg uses registry baselines as minimum versions. Conan offers profiles and lockfiles. CPM adds dependency caching and source selection around FetchContent. [vcpkg versioning](https://learn.microsoft.com/en-us/vcpkg/users/versioning), [Conan profiles](https://docs.conan.io/2/reference/config_files/profiles.html), [Conan lockfiles](https://docs.conan.io/2/tutorial/versioning/lockfiles.html), [CPM](https://github.com/cpm-cmake/CPM.cmake).

## Test frameworks

- GoogleTest supports typed cases, value parameters, death assertions and a companion mocking framework. CMake provides discovery. [GoogleTest advanced guide](https://google.github.io/googletest/advanced.html), [gMock](https://google.github.io/googletest/gmock_for_dummies.html), [CMake GoogleTest module](https://cmake.org/cmake/help/v3.25/module/GoogleTest.html).
- Catch2 documents typed cases, sections, CTest discovery and ways to reduce assertion compile overhead. [Catch2 tests and sections](https://github.com/catchorg/Catch2/blob/devel/docs/test-cases-and-sections.md), [Catch2 CMake integration](https://github.com/catchorg/Catch2/blob/devel/docs/cmake-integration.md), [Catch2 compile-time configuration](https://github.com/catchorg/Catch2/blob/devel/docs/configuration.md), [Catch2 mocking discussion](https://github.com/catchorg/Catch2/issues/1659).
- CTest can register executables directly. doctest provides a lightweight single-header framework. [CMake add_test](https://cmake.org/cmake/help/v3.25/command/add_test.html), [doctest](https://github.com/doctest/doctest).

## CI and deployment

- GitHub lists Ubuntu x86_64 and macOS 26 arm64 images, supports container jobs on Linux, and provides free standard runners for public repositories. [Runner images](https://github.com/actions/runner-images), [Container jobs](https://docs.github.com/en/actions/how-tos/write-workflows/choose-where-workflows-run/run-jobs-in-a-container), [Actions billing](https://docs.github.com/en/billing/concepts/product-billing/github-actions).
- The November 17 `macos-26` arm64 image build is version `20261117.0516.1` and runs macOS 26.7.2 (25G332). [GitHub runner image release](https://github.com/actions/runner-images/releases/tag/macos-26-arm64%2F20261117.0516).
- Debian 13 (trixie) ships glibc 2.41. [Debian libc6](https://packages.debian.org/trixie/libc6).
