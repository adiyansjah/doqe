# T0003 Quality checks and CI
Status: ✅ done · Type: feature · Risk: high · Milestone: M01 · Feature: F01.3 · ADRs: [ADR-0001](../../adrs/0001-core-cpp-toolchain/adr.md)

## Goal
Add format and lint checks, ASan+UBSan test builds, and GitHub Actions CI that runs the build, tests
and checks on every push, on macOS 26 arm64 and Debian 13 x86_64, all with the pinned LLVM 23.1.5.

## Scope
- In: clang-format and clang-tidy configs and check commands; an ASan+UBSan preset; CI workflow on
  `macos-26` and a `debian:13` container, installing and checksum-verifying the LLVM pin;
  CI checks of the CMake 3.25 and SDK 26.0 floors where possible; commands in `docs/DEVELOPMENT.md`.
- Out: MSan with an instrumented libc++ (later task); fuzzing (no parsers yet); engine code.

## Edge cases
- A seeded format violation and a seeded lint violation each fail their check.
- A seeded memory bug (ASan) and a seeded UB bug (UBSan) each fail the sanitizer test run.
- Static libc++ with ASan on macOS and Linux. Both hosts check heap accesses. Linux also checks
  allocation-family mismatches. The macOS executables export no symbols.
- CI must fail, not skip, when the LLVM download or checksum fails.

## Done when
- [x] Format and lint checks pass on the tree; seeded violations make them fail (both hosts)
- [x] ASan+UBSan tests pass; seeded memory and UB bugs make them fail (both hosts)
- [x] CI runs build, tests, format, lint and sanitizers on every push and is green on `main`
- [x] The CMake 3.25 and macOS SDK 26.0 floors are checked in CI, or recorded as not verified with the reason
- [x] Commands and checks recorded in `docs/DEVELOPMENT.md`

## Performance budget
None.

## Notes
- 2026-11-20 task started.
- 2026-11-20 Debug and ASan+UBSan passed 5/5 tests on the Mac and dev01. Format and lint
  passed. Seeded format, lint, C++ heap-use-after-free, heap-buffer-overflow and signed
  overflow violations failed their respective checks on both hosts. On the Mac, ASan reported
  errors in C++ heap allocations with empty executable export tables.
- 2026-11-20 Checksum-verified CMake 3.25.3 and Ninja 1.12.1 passed Debug on dev01.
  The hosted workflow also selects Xcode 26.0.1 for SDK 26.0. The local Mac has SDK 26.5.
  Workflow YAML parsed and its shell blocks passed syntax checks. GitHub Actions execution
  and the SDK 26.0 build are not verified.
- 2026-11-21 The macOS shared-libc++ prototype loaded both the pinned and system C++ runtimes
  and aborted during GoogleTest discovery with `alloc-dealloc-mismatch (malloc vs operator delete)`.
  Part A stopped at the prototype. dev01 passed 5/5 and caught all three allocation mismatch seeds
  with static libc++. The link map identified compiler-rt's ASan C++ operators.
  [Prototype evidence](refs/asan-prototype.txt)
- 2026-11-21 The owner retained static libc++ in all builds under ADR-0001. macOS ASan sees
  `malloc` and `free` beneath the static C++ operators. Allocation-family mismatch detection
  runs on Linux, where compiler-rt's `asan_cxx` supplies `operator new` and `operator delete`.
- 2026-11-21 Both hosts passed Debug and ASan+UBSan 5/5 with stack-use-after-return detection
  enabled in sanitizer CTest runs. The disposable proof runs rejected the format, new
  static-library lint, heap-use-after-free, heap-buffer-overflow and signed-overflow seeds.
  dev01 also rejected unsanitized executables in `Runtime.SharedDependencies` and reported
  the three allocation mismatches seeded by the proof script. The dev01 transfer was removed.
- 2026-11-21 The `debian:13` Docker dry run on `linux/amd64` used the workflow's package set
  and checksum-verified selective LLVM extraction (632 MiB). Debug and ASan+UBSan passed 5/5
  each, and format and lint passed. Debug passed 5/5 with the verified CMake 3.25.3 and
  Ninja 1.12.1 floors. The local SDK 26.5 compile database matched the CI assertion form.
  Hosted GitHub Actions and the SDK 26.0 build remain not verified.
- 2026-11-21 first GitHub Actions run 44823432158 on 1b24365 (branch push): all three jobs green: macOS 26 arm64 (Debug 5/5, ASan+UBSan 5/5, format, lint, SDK 26.0 build 5/5), Debian 13 x86_64 (Debug 5/5, ASan+UBSan 5/5, format, lint), CMake 3.25.3 floor (Debug 5/5). Green on `main` pending merge.
- 2026-11-21 CI on `main` green at 1b52fef (run 44824437148, all three jobs). Task closed.

## Result
Commit: 1b24365, 1b52fef · Proof: [check script](refs/quality-checks.sh) and [host results](refs/quality-checks.txt)
