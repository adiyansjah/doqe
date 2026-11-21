# T0007 Status, query memory budget and minimal columns
Status: ✅ done · Type: feature · Risk: high · Milestone: M03 · Feature: F03.2 · ADRs: [ADR-0002](../../adrs/0002-architecture-modules/adr.md), [ADR-0003](../../adrs/0003-iceberg-parquet-strategy/adr.md), [ADR-0004](../../adrs/0004-status-memory-columns/adr.md)

## Goal
Add `base::Status` with the ADR-0002 codes, a query memory budget that every reader allocation
goes through (ADR-0003), and the minimal `column` layout for int64 and UTF-8 string with nulls,
exported through the Arrow C Data interface.

## Scope
- In: `base::Status` (codes per ADR-0002, plus `Unsupported` if missing) and `std::expected` helpers;
  a query memory budget and allocator in `base` (reserve and release, `ResourceExhausted` over budget,
  `OutOfMemory` on allocator failure); `column` buffers, validity bitmap, int64 and string (offsets + data)
  columns and a batch keyed by Iceberg field ID; Arrow C Data export (verbatim `abi.h` per ADR-0002);
  unit tests in `tests/base/` and `tests/column/`.
- Out: other types, nested columns, Arrow C Stream (with `engine`), SIMD, concurrency, readers.
- First step: ask the owner the design questions (Status payload, budget ownership and reservation,
  owned vs shared buffers). If they are real trade-offs, write ADR-0004 before code.

## Edge cases
- Zero-row and all-null columns; empty strings vs nulls; offsets at the int32 limit.
- Budget exactly reached, exceeded by one byte, release after failure, allocator returning null.
- Arrow export: release callback frees exactly once; consumer moves the struct; budget released after release.
- Under ASan+UBSan: no leaks, no misaligned buffers (64-byte alignment).

## Done when
- [x] Owner answered the design questions (ADR-0004 accepted if needed)
- [x] Failing tests first, then code; `ctest --preset debug` and `ctest --preset asan-ubsan` pass on the Mac and dev01
- [x] Format, lint and module-boundary checks pass; CI green on `main`
- [x] `docs/arch/base.md`, `docs/arch/column.md` and `docs/DEVELOPMENT.md` updated

## Performance budget
None yet; record allocation counts per batch in tests as a baseline.

## Notes
- 2026-11-21 task created. Start in a new thread; ask the owner the design questions first.
- 2026-11-21 revised proposed [ADR-0004](../../adrs/0004-status-memory-columns/adr.md) after review.
  The 16-byte error-only `Status` shares immutable detail through an atomic count and copies or
  moves without allocating. Diagnostic payload is capped at 256 bytes outside the query budget.
  `OutOfMemory` and failed detail allocation use code-only status.
- 2026-11-21 the Query, buffer control blocks, and Arrow export states each retain the pool.
  Allocation stays on the query thread. Consumers can release exports concurrently after Query
  teardown, with atomic accounting and pool destruction on the last reference. The ADR specifies
  sized frees, export rollback, moved-child lifetime, and UTF-8 batch rollover at the 32-bit limit.
  The Scope exclusion for concurrency applies to query execution. Cross-thread release is in scope.
  C Stream wiring is F03.5 and must preserve export ownership. ADR-0004 awaits owner acceptance
  before implementation.
- 2026-11-21 applied the ADR-0004 re-check findings. The proposed contract now specifies
  final-reference destruction, atomic byte accounting, sized-free checks in every build,
  UTF-8 truncation, and export rollback. The ADR records the tests to write after owner acceptance.

- 2026-11-21 owner accepted ADR-0004. Next: failing tests first, then base Status, MemoryPool, column buffers, utf8/int64 arrays and Arrow C Data export.

- 2026-11-21 added the GoogleTest cases before production code. The initial Debug build failed
  on the three absent module headers. Implemented the pool, columns and C Data export against
  those tests. Lifecycle tests model Query teardown by dropping the Query's owning pool
  reference. The reader remains responsible for UTF-8 validation and whole-row retry.
- 2026-11-21 prepared the implementation for owner review. The checked tree, platform results
  and raw-log locations are recorded in the proof. TSan has no existing preset.

- 2026-11-21 addressed both implementation reviews. The pool and column owners preserve
  original allocation pointers. Batch names use a separate charged allocation and strict
  UTF-8 validation. Only the test binary links the hooked pool copy and replaces the global
  allocation operators. Added rollback, accounting, lifetime, and boundary regressions.
  The [verification record](refs/verification.txt) covers both hosts and the seeded mutations.

- 2026-11-21 addressed the pool payload array lifetime review. Batch fields, child records,
  child links, names, and buffer byte views now use placement array construction. Cleanup
  uses the original allocation pointers. The [verification record](refs/verification.txt)
  records the source audit and fresh checks on the Mac and dev01.

- 2026-11-21 addressed the validity bitmap element type review. Native validity reads use
  the same `uint8_t` view as Arrow consumers. Fresh checks pass on the Mac and dev01
  ([verification](refs/verification.txt)). Next: owner review, then commit, push and CI.
- 2026-11-21 owner approved. Branch CI run 44886565035 and `main` CI run 44887575552 passed on
  macOS 26 arm64, Debian 13 x86_64 and the CMake 3.25 floor. Task closed.

## Result
Commit: be62ac7 · Proof: [verification](refs/verification.txt) — Debug and ASan+UBSan 41/41 on the Mac and dev01; format, lint, boundaries and Release death tests pass; CI run 44887575552 green on `main`. Not verified: TSan (no preset).
