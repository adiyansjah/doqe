# T0005 Module skeleton and boundary checks
Status: ✅ done · Type: feature · Risk: high · Milestone: M02 · Feature: F02.2 · ADRs: [ADR-0001](../../adrs/0001-core-cpp-toolchain/adr.md), [ADR-0002](../../adrs/0002-architecture-modules/adr.md)

## Goal
Create the 13 active ADR-0002 modules as empty `doqe_<module>` static libraries under `src/<module>/`,
linked only along allowed edges, and add build checks that reject forbidden links and includes, in CI.

## Scope
- In: `src/<module>/` with `include/doqe/<module>/` and a placeholder source; CMake targets and allowed links;
  a link check and an include check (path rule plus real compiler dependencies as backstop); positive and negative
  fixtures; move the existing version code into the right module (e.g. `base` or `cli`) with `doqe` built from `cli`;
  `tests/<module>/` layout; update `docs/arch/` Status and Code lines, `docs/DEVELOPMENT.md`.
- Out: any engine functionality; `dist` (deferred); `benchmarks/`, `tools/` (need owner approval); `Status` implementation beyond what checks need.

## Edge cases
- An allowed A → B → C chain: A including B's public header that includes C builds; A including C directly fails.
- A forbidden direct link, a generator-expression link, a private-header include across modules, a relative `..` include leaving the module.
- Empty static libraries on macOS (ranlib "no symbols" warnings) do not fail the build.
- Existing checks (format, lint, ASan+UBSan, runtime dependencies, compiler gate) still pass.

## Done when
- [x] All 13 active modules build as `doqe_<module>` with exactly the ADR-0002 edges
- [x] Seeded forbidden link and include cases each fail the check; the positive A → B → C fixture passes
- [x] `doqe --version` and all existing tests and checks pass on the Mac and dev01
- [x] The boundary checks run in CI and CI is green on `main`
- [x] `docs/arch/` Status/Code lines and `docs/DEVELOPMENT.md` are updated

## Performance budget
None.

## Notes
- 2026-11-21 task started.
- 2026-11-21 created the 13 active targets and implemented configure-time link validation
  and CTest include validation. The CLI owns version formatting and build provenance.
- 2026-11-21 verified Debug, ASan+UBSan, format, lint, runtime dependencies, the compiler
  gate, and the boundary fixtures on the Mac and dev01. CI wiring is in place. The
  hosted jobs and CI on main are pending.
- 2026-11-21 addressed the two boundary reviews. The CMake checker uses pinned Clang
  for directives and include search paths and checks Ninja's dependencies and final
  CLI link. The integration fixture runner covers the review cases, stale CTest
  dependencies and ADR edge drift. See the proof for verification of this revision.
- 2026-11-21 added final-link input allowlists for both executables, test source and
  link ownership checks, a compile-flag allowlist, and token-aware resource scanning.
  CTest now checks the ADR edge table. The development guide records the owner-approved
  limits on build-script and symbol-level enforcement. See the proof for regression
  results and the Mac and dev01 verification commands.
- 2026-11-21 added regression fixtures and tightened the checker for module types and
  public exports, unknown executables, ADR cells, and GoogleTest headers in production.
  Shared test fixtures and the optional `doqe_test_support` target use the ADR-0002
  public-interface rules. The checker accepts feature guards and diagnostic flags and
  reuses Clang search paths for identical invocations. The skeleton script compiles its
  C++23 library probe in the CLI target. See the proof for this revision.
- 2026-11-21 reverted an edit to accepted ADR-0002 and made the edge parser read the original
  text. Hosted CI green on the branch (run 44863625280) and on main at cd15af3 (run
  44865094173): macOS 26 arm64, Debian 13, Debian 13 with the CMake 3.25 floor. Task done.

## Result
Commit: cd15af3 · CI: runs 44863625280 (branch) and 44865094173 (main), all three jobs green · Proof: [Mac and dev01 check results](refs/module-checks.txt)
