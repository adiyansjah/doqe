# T0007 Status, query memory budget and minimal columns
Status: 🟨 doing · Type: feature · Risk: high · Milestone: M03 · Feature: F03.2 · ADRs: [ADR-0002](../../adrs/0002-architecture-modules/adr.md), [ADR-0003](../../adrs/0003-iceberg-parquet-strategy/adr.md)

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
- [ ] Owner answered the design questions (ADR-0004 accepted if needed)
- [ ] Failing tests first, then code; `ctest --preset debug` and `ctest --preset asan-ubsan` pass on the Mac and dev01
- [ ] Format, lint and module-boundary checks pass; CI green on `main`
- [ ] `docs/arch/base.md`, `docs/arch/column.md` and `docs/DEVELOPMENT.md` updated

## Performance budget
None yet; record allocation counts per batch in tests as a baseline.

## Notes
- 2026-11-21 task created. Start in a new thread; ask the owner the design questions first.

## Result
Commit: — · Proof: —
