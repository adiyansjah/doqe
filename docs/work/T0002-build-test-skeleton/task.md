# T0002 Build and test skeleton
Status: ✅ done · Type: feature · Risk: high · Milestone: M01 · Feature: F01.2 · ADRs: [ADR-0001](../../adrs/0001-core-cpp-toolchain/adr.md)

## Goal
Install the pinned LLVM 23.1.5 on the Mac and dev01, then add the CMake/Ninja skeleton that builds
a `doqe --version` binary, a version-format test, and an exception-unwinding test.

## Scope
- In: checksum-verified LLVM 23.1.5 user-space install on both hosts; confirm archive contents (static libc++, libc++abi, libunwind);
  CMake presets, compiler gate, FetchContent GoogleTest, `doqe --version`, version and unwind tests; commands in `docs/DEVELOPMENT.md`.
- Out: format, lint, sanitizers, CI (F01.3); any engine code.

## Edge cases
- A compiler other than the pinned install (Apple Clang, Homebrew Clang 23.1.5, distro Clang) is rejected at configure.
- The binary links no shared libc++/libc++abi (checked with `otool -L` / `ldd`), and on macOS the unwinder source is confirmed.
- An exception thrown and caught across the test works with the static runtime.
- A clean clone builds with no network access other than the GoogleTest fetch.

## Done when
- [x] LLVM 23.1.5 installed and SHA-256-verified on the Mac and dev01; archive contents recorded in `refs/`
- [x] Configure with the pin succeeds; configure with any other compiler fails (both hosts)
- [x] `doqe --version` prints the version and commit on both hosts
- [x] `ctest` passes on both hosts, and a seeded failing test makes it fail
- [x] Binary runtime dependencies inspected on both hosts and recorded in `refs/`
- [x] Commands recorded in `docs/DEVELOPMENT.md`

## Performance budget
None.

## Notes
- 2026-11-20 task started; owner approved the LLVM 23.1.5 user-space install on the Mac and dev01.
- 2026-11-20 installed LLVM 23.1.5 on the Mac and dev01 and passed the SHA-256, C++23 smoke, and static-linkage checks on both hosts. dev01's detached signature verified. The Mac signature was not verified because no local OpenPGP verifier was available. See the [commands](refs/llvm-install.sh) and [results](refs/llvm-install.txt).
- 2026-11-20 owner approved new top-level files: CMakeLists.txt, CMakePresets.json, .gitignore, cmake/, src/, tests/.
- 2026-11-20 (UTC) the version test failed against the empty implementation before passing with the formatter. Both hosts then passed the full workflow, compiler rejection, seeded failure, and runtime inspections. The macOS executables keep their symbols private to prevent dyld from coalescing weak C++ symbols with the system runtime. The dev01 verification checkout was removed. The implementation awaits owner review and commit.
- 2026-11-20 review fixes pin the resolved compiler SHA-256 on each host, check the upstream banner and driver names, reject compiler launchers and ambient search paths, and apply pinned runtime flags to the project and GoogleTest independently of the CMake cache. CTest covers CLI behavior, shared runtime linkage and unwinding. The verification script now seeds failures in a source copy and records the rejection tests, GoogleTest flags, and unwinder symbols. The retained Mac log (`/tmp/doqe-t0002-red-runtime.log`) shows the original formatter test failing with an empty result while unwinding passed.
- 2026-11-20 the Mac workflow also passed when reconfigured from the old cache. A generated shared-library link command excluded `-no_exported_symbols`, and the executable link command retained it. A Git wrapper that rejected bare `status` verified `--no-optional-locks`; a scratch checkout stopped reporting clean when it gained an untracked file.
- 2026-11-20 the updated script passed on the Mac and dev01. The dev01 build ran under `~/doqe-quiet.lock` and the verification checkout was removed. The results reflect the uncommitted working tree. A clean rerun is due after commit. CMake 3.25 and SDK 26.0 remain unverified.
- 2026-11-20 (UTC) before the re-check fixes, Mac probes accepted linker launchers from cache and environment, a mixed compiler/runtime root, a substituted libc++ archive, and shared-library mode. A headerless TU built with the SDK injected through build-time CPATH, and the configure trace hashed the same compiler twice.
- 2026-11-20 (UTC) the re-check fixes reject compiler and linker launchers, mixed roots, substituted runtime archives and shared-library builds. The libc++ header version is checked in every C++ TU. The compiler is hashed once when both drivers resolve to the same binary. The [skeleton checks](refs/skeleton-checks.txt) passed on Skyfall and dev01, including the symlinked root and forced header rejection. Both script temp directories and the locked dev01 working copy were removed. Skyfall also confirmed log retention with `DOQE_KEEP_SKELETON_LOGS=1` and no exports from either executable. The branch remains uncommitted.
- 2026-11-20 clean-clone rerun of refs/skeleton-checks.sh at 27d88de: exit 0 on the Mac and dev01, 5/5 tests, `doqe 0.1.0 (27d88de)`, not dirty. Note: the script takes ~/doqe-quiet.lock itself; do not wrap it in another flock (it deadlocks).

## Result
Commit: 27d88de · Proof: [commands](refs/skeleton-checks.sh) and [results](refs/skeleton-checks.txt) for both hosts.
