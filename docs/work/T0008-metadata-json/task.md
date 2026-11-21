# T0008 Local reads and Iceberg metadata JSON
Status: ⬜ todo · Type: feature · Risk: high · Milestone: M03 · Feature: F03.3 · ADRs: [ADR-0002](../../adrs/0002-architecture-modules/adr.md), [ADR-0003](../../adrs/0003-iceberg-parquet-strategy/adr.md), [ADR-0004](../../adrs/0004-status-memory-columns/adr.md), [ADR-0005](../../adrs/0005-metadata-arena-planning/adr.md)

## Goal

Read a local Iceberg v2 metadata file into a pool-backed model with the current schema and optional current snapshot resolved.

## Scope

- In. Arena in `base`, local whole-file and range reads in `io`, a JSON DOM in `format`, and `table::ReadMetadata(location, io, pool)` returning the owning `TableMetadata`. Resolve the current schema and snapshot and keep the selected manifest-list location. Parse each partition spec's transforms and source IDs and retain its identity-source IDs for T0009.
- In. Add a test fault-injection pool in `tests/support/` over the public `MemoryPool` interface. Wrap a flat pool and fail the Nth `Allocate` with the selected `ResourceExhausted` or `OutOfMemory`. Delegate successful allocations, frees and accounting. Keep `src/base/test_probes.hpp` private to `base`.
- In. Add the JSON libFuzzer target and a pinned LLVM libFuzzer configure/build preset to `CMakePresets.json`. Document the exact configure, build and run commands in `docs/DEVELOPMENT.md`, with fixed `-max_total_time`, `-rss_limit_mb` and a fixed per-input query pool limit enforced by the harness.
- Out. T0009 owns Avro, manifest walking, `MetadataSource`, `LocalPathCatalog`, `ScanPlan` and fixture remap. F03.6 owns real PyIceberg fixtures. Compression, other format versions and other projected types are outside M03.

## Edge cases

- Accept omitted, null or `-1` current snapshot IDs. Accept matching `refs.main`, and reject a conflicting ID or a live main with the `-1` sentinel with `InvalidArgument`. Check missing required top-level planner keys, dangling current schema or snapshot IDs, a dangling schema ID on the selected snapshot, and a missing selected manifest-list location. Ignore unknown keys and v2-required top-level keys listed as unused in ADR-0005. An omitted selected snapshot schema ID is valid. Historical snapshots may refer to a schema missing from the metadata. Use a selected snapshot created under a different schema to pin projection of the current schema.
- Reject duplicate schema, snapshot and spec IDs and repeated field IDs within one schema, including nested IDs, with `InvalidArgument`. Accept the same field ID across schema versions and an otherwise well-formed historical schema with unsupported types. For a current unsupported primitive or nested field, return `Unsupported` with its field ID. Preserve field IDs, names and requiredness on rename and reorder. Accept table data field ID 2147483447 and reject 2147483448 with `InvalidArgument`. Cover multiple specs with the same source under different transforms and assert only identity sources are retained per spec.
- Every integer version other than 2 returns `Unsupported`, including 0, 1 and 3. Missing or malformed versions return `InvalidArgument`. Check gzip magic in a file named `metadata.json` and plain JSON with a `.gz` suffix.
- Read exact `INT64_MIN`, `INT64_MAX` and values above `2^53`. Reject overflow, fractions or exponents in integer fields and out-of-range int32 IDs. Check UTF-8, unpaired surrogates, bad escapes, truncation and trailing bytes. Test large objects and schemas with unique keys/IDs and a duplicate near the end, including decoded-equivalent keys, under an explicit CTest timeout. Cover exact JSON size and depth ceilings and `Unsupported` one byte or level over.
- Accept `file:///p`, `FILE:///p`, `file:/p`, and literal bare absolute paths including `%`, `?` and `#`. Use `Unsupported` for both valid and malformed percent escapes in `file:` locations, `file://localhost/p`, other nonempty authorities and non-file schemes. Use `InvalidArgument` for embedded NUL, query or fragment in a file URI, relative bare paths and range arithmetic overflow. Missing files return `NotFound`. Directory targets, short ranges and files that grow or shrink between stat and read return `IoError`. Check exact stat-size reads and the EOF probe, and check the whole-file cap before allocation.
- Inject both fault codes at every allocation and check exact status and restored live bytes. Retain the model across caller pool release, then destroy it on another thread without concurrent access and verify all charges return.

## Done when

- [x] Owner accepts ADR-0005 before implementation.
- [ ] Failing tests recorded before production code. `ctest --preset debug` and `ctest --preset asan-ubsan` pass on the Mac and dev01.
- [ ] The new libFuzzer preset builds the JSON target. Run the exact command recorded in `docs/DEVELOPMENT.md` with the bounded seed corpus and fixed time, RSS and harness pool limits. Record the result and verify the harness returns all pool charges between inputs.
- [ ] Format and lint pass via `cmake --build --preset quality`. Module checks pass via `cmake --build --preset debug --target module-boundaries` and `ctest --preset debug -R '^Modules\.'`. CI is green on `main`.
- [ ] `docs/arch/base.md`, `docs/arch/io.md`, `docs/arch/format.md`, `docs/arch/table.md` and `docs/DEVELOPMENT.md` reflect the implementation and check commands.

## Performance budget

No throughput gate. Enforce the input, nesting and query-memory ceilings in ADR-0005.

## Notes

- 2026-11-21 Round 2 adds case-insensitive file URI scheme coverage with `FILE:///p`.

- 2026-11-21 owner decisions D1–D3. D1 keeps partition values skipped and carries each spec's identity-source IDs from metadata to scan tasks. F03.4 rejects missing identity-source columns and still null-fills ordinary missing optional columns. D2 returns `Unsupported` for v1 manifests and `InvalidArgument` for missing required IDs in v2 manifests. D3 gives T0008 the pinned libFuzzer preset, exact bounded command and JSON target. T0009 reuses the preset for Avro.
- 2026-11-21 owner decisions Q1–Q6. One pool-charged arena per plan owns a JSON DOM and generic Avro tree with borrowed strings. `table` owns `MetadataSource` and `PlanScan(source, io, pool)`, `catalog` supplies `LocalPathCatalog`, `io` supplies whole-file and range reads, and remap lives in test support. Use hand-written JSON and a test-only Avro encoder, with real PyIceberg fixtures in F03.6. T0008 covers local reads, JSON and metadata resolution. T0009 covers Avro and scan planning. Skip partition values by writer schema and plan all live files. Match Avro fields by Iceberg field ID and error when a required field has no matching ID.
- 2026-11-21 design drafted. Next is owner review and acceptance of ADR-0005, then failing tests for T0008.
- 2026-11-21 owner accepted ADR-0005. Next: failing tests first (fault pool, io local reads, JSON DOM, metadata model), then the fuzz preset and JSON fuzzer.

## Result

Commit: — · Proof: —
