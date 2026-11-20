# M01 Walking skeleton
Status: 🟨 doing · ADRs: [ADR-0001](../adrs/0001-core-cpp-toolchain/adr.md) · Arch: none

## Goal
An empty doqe that builds, tests and checks itself the same way on every target machine,
so every later task starts from a working pipeline. No data features.

## Features
- F01.1 Toolchain decision: ADR-0001 (C++ standard, build system, packages, compilers, test framework, platforms) → [T0001](../work/T0001-adr-0001-toolchain/task.md) ✅
- F01.2 Build and test skeleton: a `doqe --version` binary and one unit test → [T0002](../work/T0002-build-test-skeleton/task.md) ✅
- F01.3 Quality checks: format, lint, sanitizers and CI on every push → [T0003](../work/T0003-quality-checks-ci/task.md)

## Exit criteria
- [x] ADR-0001 is accepted and its setup is recorded in `docs/DEVELOPMENT.md`
- [x] A clean clone builds and passes all tests on the Mac and on dev01 with the commands in `docs/DEVELOPMENT.md`
- [x] `doqe --version` prints the version and commit
- [x] Format and lint checks pass, and a seeded violation makes them fail
- [x] Tests pass under ASan+UBSan, and a seeded memory bug makes them fail
- [ ] CI runs the build, tests and checks on every push and is green on `main`
