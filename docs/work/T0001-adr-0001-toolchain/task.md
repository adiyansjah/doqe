# T0001 ADR-0001: core C++ toolchain
Status: ✅ done · Type: feature · Risk: high · Milestone: M01 · Feature: F01.1 · ADRs: ADR-0001

## Goal
Draft ADR-0001, choosing the C++ standard, build system, package manager, compilers,
test framework and target platforms, with references; the owner accepts it.

## Scope
- In: the ADR with options and references; tool versions checked on the Mac and dev01; `refs/` for that evidence.
- Out: build files and code (F01.2); format, lint, sanitizers and CI setup (F01.3); the Iceberg/Arrow dependency strategy (its own ADR later).

## Edge cases
- A chosen tool version is missing or too old on the Mac or dev01 (dev01 is user space only, no sudo).
- A C++ standard feature the ADR relies on is unsupported by one of the chosen compilers.

## Done when
- [x] `docs/adrs/0001-*/adr.md` exists with options, references and a decision, and is listed in the ADR index
- [x] Every factual claim links to `refs/` or a source; judgment calls are marked as judgment
- [x] Two independent reviews were done and their findings resolved
- [x] The owner accepts ADR-0001, and `docs/DEVELOPMENT.md` records the setup

## Performance budget
None.

## Notes
- 2026-11-20 task started; drafting ADR-0001.
- 2026-11-20 drafted and indexed [ADR-0001](../../adrs/0001-core-cpp-toolchain/adr.md) as proposed and recorded version inventories for both hosts. Independent reviews and owner acceptance are pending.
- 2026-11-20 revised the proposal after two reviews. The owner chose ISO C++23 with headers and a library allowlist, pinned upstream Clang on both hosts, CMake/Ninja presets and commit-pinned GoogleTest via FetchContent with CTest. The targets are macOS 26 arm64 and Debian 13 x86_64. The proposal selects LLVM 22.1.8 and libc++ on both. dev01 provisioning needs owner approval, and the ADR remains proposed.
- 2026-11-20 revised the proposed ADR after the owner decisions and re-check. Pin checksum-verified upstream LLVM 23.1.5 on both hosts with an exact Clang configure gate because 23.1.6 lacks a macOS arm64 asset. Use the pin's libc++ headers and static libc++/libc++abi, with static LLVM libunwind on Linux and system unwinding on macOS. Allow supported C++23 language features under the no-modules policy, limit C++23 library use to the allowlist, and keep exceptions and RTTI on. Set CMake 3.25 (presets schema 6), Ninja 1.12.1, and the macOS SDK sysroot with deployment target 26.0 and availability warnings as errors. Hosted macOS runtime coverage is current 26.x only. Installation on both hosts awaits owner approval. Archive contents and floor builds are unverified. F01.3 covers sanitizer linkage and a separate instrumented libc++ for Linux MSan.
- 2026-11-20 final check found that a same-version non-pinned Clang could pass configure; the gate now also requires the compiler path inside the verified install. Owner accepted ADR-0001.

## Result
Commit: see the T0001 acceptance commit on main · Proof: ADR-0001 accepted by the owner on 2026-11-20 after a draft, two independent reviews (Codex gpt-6.2-astra, Claude claude-opus-5-5), two revisions, a re-check and a final check; evidence in [ADR-0001 refs](../../adrs/0001-core-cpp-toolchain/refs/).
