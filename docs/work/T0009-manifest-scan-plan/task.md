# T0009 Manifest reads and Iceberg scan planning
Status: ⬜ todo · Type: feature · Risk: high · Milestone: M03 · Feature: F03.3 · ADRs: [ADR-0002](../../adrs/0002-architecture-modules/adr.md), [ADR-0003](../../adrs/0003-iceberg-parquet-strategy/adr.md), [ADR-0004](../../adrs/0004-status-memory-columns/adr.md), [ADR-0005](../../adrs/0005-metadata-arena-planning/adr.md)

## Goal

Turn the current Iceberg v2 snapshot into a `ScanPlan` with the current schema, field-ID projection and all live Parquet files.

## Scope

- In. Add the null-codec Avro container reader and projected writer-schema record tree in `format`. In `table`, bind the v2 manifest-list, entry and data-file fields by the ID/type contract in ADR-0005. Walk the current snapshot's list and manifests, validate headers and inheritance, and skip partition values by writer schema. Add `MetadataSource`, `PlanScan(source, io, pool)`, `ScanPlan` and `FileScanTask`. `catalog::LocalPathCatalog` implements the location contract using the explicit metadata JSON location.
- In. Test with hand-written metadata JSON, Avro bytes from a small encoder in `tests/support/`, a test-only raw-prefix decorator for both read methods and an Avro reader libFuzzer target using T0008's preset and fault pool.
- Out. F03.4 owns Parquet decoding, F03.5 owns CLI wiring and F03.6 owns real PyIceberg fixtures. Compression, delete application and partition pruning are outside M03.

## Edge cases

- Return an empty task list for no current snapshot, a valid header-only manifest list, a valid header-only data manifest or a valid DELETED-only data manifest. Follow only the current snapshot's list. Test mixed statuses, explicit old sequences and snapshot IDs, and null or missing optional IDs on ADDED. Assert the inherited snapshot ID and data sequence on a task when a later snapshot reuses its manifest. Check explicit file sequence handling and ADDED file sequence inheritance in decoded entries. For each of EXISTING and DELETED independently, null or missing data sequence and null or missing file sequence each return `InvalidArgument`. Reject negative sequences. Validate both fields before skipping DELETED.
- Use v2 metadata whose current snapshot references a v1 list with a v1 header and a v1 list without the header. Both return `Unsupported`. Accept a valid unversioned v2 list. Return `Unsupported` for other non-2 integer versions and `InvalidArgument` for a malformed header or a declared-v2 list missing ID 515.
- Use a v2 table and list referencing a legacy v1 manifest to assert `Unsupported`, including a variant without the optional v1 version header. Use a separate v2 manifest lacking ID 134 to assert `InvalidArgument`. In a two-known-spec fixture, test both a valid match and a header spec ID that is known but differs from the list entry. Test a v2 header content mismatch independently. Assert no plan on each error.
- Test a delete manifest from the list and a live non-Parquet file as `Unsupported`. File content 1 or 2 inside a data manifest, unknown content or status, unknown spec IDs, negative file size or record count, and list `manifest_length` differing from the actual bytes each return `InvalidArgument`. Exercise an actual read failure separately. Use hand-authored lowercase `parquet` and assert canonical `PARQUET` in the task.
- Rename and reorder same-typed fields and add an unknown field. Check missing required IDs despite matching names, duplicate IDs, and ill-typed required or optional bindings, including a non-record partition field. Accept a skipped Avro field with an ID in the range reserved for table metadata columns. Exercise nullable longs in both branch orders and omission of optional IDs. In a nonempty partition record before retained values, skip each supported Avro type from ADR-0005, including namespace-relative and fully qualified named references, and assert the later values still decode.
- Accept absent or explicit `null` codec. Return `Unsupported` for deflate, Snappy, an unknown codec, a valid non-record root or a recursive named schema. Accept a zero-count, zero-size block with matching sync. Check truncation, bad sync, zero-count blocks with nonzero size, negative container counts, illegal lengths, varint overflow, block size or count/payload mismatch, invalid UTF-8, and out-of-range enum or union indexes as `InvalidArgument`. Cover sized array/map blocks with negative counts. Confirm exact container, nesting, cumulative item and work ceilings and `Unsupported` over each. Exercise repeated named references with skipped zero-width fields, where traversal reaches its budget before bytes, stack depth or collection items reach theirs. Assert no plan and restored live pool bytes. Include an acyclic shared schema to exercise memoized minimum sizing. Use a schema whose minimum encoded size exceeds uint64 to test saturation at the container cap plus one. Accept its header-only container and return `InvalidArgument` for a positive-count block with too-short payload.
- Read through both decorator methods. Check raw exact and component-boundary matches, a partial or nonmatching prefix, and alternate spellings such as `file:/p` versus `file:///p`. Assert that the task stores the original URI and remains usable after the source, decorator and caller pool reference go away.
- Plan files with two different known specs. Assert the identity-source IDs for each file's own spec and distinguish the projected identity-source field from an ordinary optional field. At the plan boundary, check the IDs needed for both missing-column cases. F03.4 owns the reader assertions and F03.6 owns the corresponding checked-in fixtures.
- Inject both fault codes at every allocation, including after the first manifest supplies live tasks. Assert exact status, no partial plan and restored live bytes. Destroy a successful plan on another thread with no concurrent access and verify all charges return.

## Done when

- [ ] Owner accepts ADR-0005 and T0008 is complete before implementation.
- [ ] Failing tests recorded before production code. `ctest --preset debug` and `ctest --preset asan-ubsan` pass on the Mac and dev01.
- [ ] The Avro libFuzzer target runs under T0008's preset and the fixed time, RSS and per-input pool limits. Record the exact command and bounded corpus in `docs/DEVELOPMENT.md` and record the result.
- [ ] Verify each task carries the per-spec identity-source IDs needed by both D1 cases. Record the F03.4 and F03.6 handoff below.
- [ ] Format and lint pass via `cmake --build --preset quality`. Module checks pass via `cmake --build --preset debug --target module-boundaries` and `ctest --preset debug -R '^Modules\.'`. CI is green on `main`.
- [ ] `docs/arch/io.md`, `docs/arch/format.md`, `docs/arch/table.md`, `docs/arch/catalog.md` and `docs/DEVELOPMENT.md` reflect the implementation and check commands.

## Performance budget

No throughput gate. Enforce the input, traversal and query-memory ceilings in ADR-0005.

## Notes

- 2026-11-21 Round 2 adds legacy list detection, saturated Avro minimum sizing, empty-block checks and per-schema JSON cap clarification.

- 2026-11-21 owner decisions D1–D3. D1 keeps partition values skipped and carries each spec's identity-source IDs from metadata to scan tasks. F03.4 rejects missing identity-source columns and still null-fills ordinary missing optional columns. D2 returns `Unsupported` for v1 manifests and `InvalidArgument` for missing required IDs in v2 manifests. D3 gives T0008 the pinned libFuzzer preset, exact bounded command and JSON target. T0009 reuses the preset for Avro.
- 2026-11-21 owner decisions Q1–Q6. One pool-charged arena per plan owns a JSON DOM and generic Avro tree with borrowed strings. `table` owns `MetadataSource` and `PlanScan(source, io, pool)`, `catalog` supplies `LocalPathCatalog`, `io` supplies whole-file and range reads, and remap lives in test support. Use hand-written JSON and a test-only Avro encoder, with real PyIceberg fixtures in F03.6. T0008 covers local reads, JSON and metadata resolution. T0009 covers Avro and scan planning. Skip partition values by writer schema and plan all live files. Match Avro fields by Iceberg field ID and error when a required field has no matching ID.
- 2026-11-21 design drafted. Start after ADR-0005 acceptance and T0008 completion.
- 2026-11-21 F03.4 and F03.6 handoff for D1. Use ID-tagged Parquet files to test a projected identity-source ID absent from the file returning `Unsupported`, and an ordinary missing optional field reading as null.
- 2026-11-21 owner accepted ADR-0005. Start after T0008 is complete.

## Result

Commit: — · Proof: —
