# ADR-0005: Use a pool-charged arena and injected file reads for scan planning

| Status | Date | Decided by | Confidence | Tags |
|---|---|---|---|---|
| accepted | 2026-11-21 | owner | medium | Iceberg, memory, planning |

Supersedes: none · Superseded by: none · Related: [ADR-0002](../0002-architecture-modules/adr.md), [ADR-0003](../0003-iceberg-parquet-strategy/adr.md) (refines “Entry point and paths” by placing remap in the test decorator and narrows the missing optional Parquet field rule below), [ADR-0004](../0004-status-memory-columns/adr.md)

## Context

[F03.3](../../road/M03-first-iceberg-read.md#features) plans local scans for small Iceberg v2 tables. A single query thread reads one metadata JSON document, one Avro manifest list and the manifests it references. Paths and schema names must stay valid through execution. All reader storage shares the query budget. Checked-in fixtures need relocation while preserving their original bytes.

## Criteria

1. Bound allocations and parser work on malformed input. Return every charge on failure and plan destruction.
2. Keep Iceberg interpretation in `table`, format decoding in `format`, and byte reads in `io`.
3. Keep metadata lookup and fixture relocation injectable. Preserve field identity and avoid production relocation settings.

## Options

### A. Pool-charged arena per plan

- **Good.** Amortizes pool allocations and gives decoded values and the plan one lifetime.
- **Bad.** Chunk slack and temporary data remain charged until the plan is dropped.

### B. Individual pool allocations for nodes and strings

- **Good.** Allows early release of each temporary value and limits retained slack.
- **Bad.** Requires many allocations, ownership records and rollback paths.

### C. Standard containers on the default allocator

- **Good.** Straightforward container APIs and cleanup.
- **Bad.** Reader storage escapes the query budget and allocation failures require exception translation.

### D. JSON DOM and schema-driven Avro record tree

- **Good.** `table` can resolve cross-references in any JSON member order and consume Avro fields by ID.
- **Bad.** Input and decoded nodes share the budget for the lifetime of the plan.

### E. Streaming JSON events and Avro callbacks

- **Good.** Can discard input and ignored values earlier.
- **Bad.** JSON cross-references and arbitrary member order require retained state or extra passes. Callbacks complicate ownership of projected values.

### F. Abstract file system with a fixture decorator

- **Good.** The same planning path works with local reads and relocated fixture reads. The metadata source stays independent of byte transport.
- **Bad.** Tests must explicitly pass the decorated file system to planning and data reads.

### G. Warehouse prefix option in production `io`

- **Good.** Centralizes rewriting for every location read.
- **Bad.** Adds a production path interpretation mode and a configuration and verification surface for fixture use.

## References

- **Source.** [Sources](refs/sources.md) cover Iceberg metadata and projection, Avro container and binary encoding, JSON, and file URIs.
- **Judgment.** One arena and whole-file metadata reads fit the small M03 workload. Chunk sizes and parser ceilings below are proposed engineering limits. Allocation costs, fixture headroom and throughput are not verified.

## Decision

Choose **A, D and F**. Use pool-backed storage throughout the metadata read and keep relocation in test support.

### Arena and ownership

- `base` provides the domain-neutral arena over `MemoryPool`. Allocate lazily, starting with a 4 KiB chunk and doubling ordinary chunk size up to 64 KiB. Sizes include the chunk header and padding. Use a dedicated chunk sized to the checked request, header and padding when the request cannot fit in a 64 KiB chunk. Dedicated chunks do not advance ordinary growth. Charge the complete requested chunk size to the pool. Honor alignments through 64 bytes and check size, alignment and cursor arithmetic before allocation.
- Use the arena for whole-file input, read and parser scratch, JSON nodes and decoded strings, Avro writer schemas and retained record values, and scan plan storage. Strings and byte spans borrow arena storage. Reader-owned dynamic storage never uses the default allocator. ADR-0004's independent, bounded `Status` detail still applies. Use the pool's original pointer, exact charge and alignment when freeing each chunk.
- The arena object holds its `PoolPtr` and chunk list outside the chunks. Typed storage requires trivially destructible types, enforced by `static_assert`. `TableMetadata` owns the arena after a successful metadata read. `PlanScan` extends that arena and transfers it to `ScanPlan` on success. Both owners are move-only. Views borrow only arena storage and expire on owner destruction. Allocate on the query thread. Either owner may be destroyed on any thread with exclusive access, using ADR-0004's thread-safe `Free`.
- A failed request leaves the chunk list intact. A failed read or plan returns all its charges without allocating or publishing a partial result. Reserve `ResourceExhausted` for pool budget rejection and checked allocation-size arithmetic overflow against that budget. Propagate `OutOfMemory` for allocator failure. Implementation ceilings return `Unsupported`.

### Local reads

- `io` exposes capped whole-file reads into arena storage and exact range reads into caller storage. Charge scratch to the same pool. Check file size against the cap before allocation. Read the stat size and require EOF. A short read or unexpected extra byte returns `IoError`. Range arithmetic overflow returns `InvalidArgument`. Missing files return `NotFound`. Directory targets and other I/O failures return `IoError`.
- Accept `file:///p`, `file:/p` and bare absolute paths. Match URI schemes case-insensitively, including `FILE:///p`. Treat locations as opaque strings and reject embedded NUL with `InvalidArgument` before classifying the location. A `%` in a `file:` location, a nonempty file authority (including `localhost`), or another scheme returns `Unsupported`. A query or fragment in a file URI and relative bare paths return `InvalidArgument`. Bare absolute paths treat `%`, `?` and `#` literally.
- The test decorator matches the raw location prefix at a path-component boundary and rewrites it for both read methods. Forward other locations unchanged. Use the same decorator for metadata, manifest and data reads and preserve fixture bytes.

### JSON and table metadata

- `format` provides a whole-document JSON DOM. Cap each JSON document at 16 MiB and 64 nested objects or arrays, counting the root. Apply the same DOM and limits to each Avro header schema. Its bytes count toward the enclosing 64 MiB container cap, separately from the metadata.json cap. For inline records, JSON depth binds before Avro nesting because each record level uses about three JSON levels. Invalid UTF-8, unpaired surrogates, duplicate decoded keys, bad syntax, truncation and trailing bytes return `InvalidArgument`. Preserve number lexemes for exact int64 integer access and check field-specific ranges before narrowing. Detect duplicate decoded keys and field IDs with arena-backed O(n log n) or better work per object or schema.
- `[[nodiscard]] table::ReadMetadata(location, io, pool) -> base::Result<TableMetadata>` is the T0008 entry point. Detect gzip by magic bytes `1f 8b`. Gzip and every integer format version other than 2 return `Unsupported`. A missing or malformed version returns `InvalidArgument`.
- Require `format-version`, `schemas`, `current-schema-id` and `partition-specs`. The planner ignores the other v2-required top-level keys `table-uuid`, `location`, `last-sequence-number`, `last-updated-ms`, `last-column-id`, `default-spec-id`, `last-partition-id`, `sort-orders` and `default-sort-order-id`. Of the optional keys, read `current-snapshot-id`, `snapshots` and `refs.main`. Ignore other optional and unknown keys.
- Validate shape and field-ID uniqueness within each schema, including nested struct, list and map IDs. IDs may recur across schema versions. Table data field IDs above 2147483447 return `InvalidArgument`. The reserved range check applies only to table data fields. Type-check only the current schema for M03 support. Preserve ID, name and requiredness, mapping `long` to `int64` and `string` to `utf8`. Other current-schema types return `Unsupported`. Require unique schema, snapshot and partition spec IDs.
- Omitted, null or `-1` current snapshot ID means no current snapshot. A present `refs.main` must agree with the current snapshot ID. A live ID must resolve to a snapshot with a manifest-list location. Resolve an explicit snapshot `schema-id` only for the selected snapshot and project the table's current schema. Unresolved required references and inconsistent `refs.main` return `InvalidArgument`. Parse each spec field’s string transform and int32 `source-id`. Retain source IDs for `identity` only.

### Avro decoding

- `format` decodes a projected record tree from the embedded writer schema. Keep annotations for field selection and skip unused values by writer schema without materializing them. Support null, boolean, int, long, float, double, bytes, string, record, enum, array, map, union, fixed, and named references. Resolve fullnames and relative names under Avro's namespace and definition-before-use rules. Recursive named types, non-record roots and record roots with zero minimum encoded size return `Unsupported`. Malformed schemas return `InvalidArgument`.
- Cap containers at 64 MiB and value nesting at 64 record, collection or union levels, including skipped values. Bound cumulative collection items by container bytes, including zero-width items. Compute schema minimum sizes with cycle detection and memoization per schema node. Saturate minimum sizes at the container cap plus one, checking before arithmetic to prevent uint64 overflow. A saturated minimum permits header-only containers. Set a cumulative traversal budget of 16 steps per container byte. Charge schema-node and reference visits during minimum-size computation, and value, record field and named-reference visits during decoding, including skips. Exhaustion returns `Unsupported` and releases the read's charges.
- Accept an absent codec or the name `null`. All other codecs return `Unsupported`. Accept header-only containers for manifest lists and manifests as an M03 extension. Accept zero-count blocks with zero byte size and matching sync. Negative counts or zero counts with nonzero size return `InvalidArgument`. Each block uses at least 18 framing bytes, bounding block count by file length. Bound block size by bytes before sync, and object count by payload bytes divided by positive root minimum size. Require exact payload consumption and matching sync. Invalid framing, lengths, counts, varints, enum or union indexes, and malformed UTF-8 in strings and map keys return `InvalidArgument`, including for skipped values. Negative array and map counts encode sized blocks whose byte extents must match.

### Manifest planning

- `table::PlanScan(source, io, pool) -> base::Result<ScanPlan>` gets a metadata location from the table-owned `MetadataSource` and calls `ReadMetadata` through the injected `io`. `catalog::LocalPathCatalog` supplies an explicit metadata file location valid for the call. Fallible module boundaries use `[[nodiscard]] base::Result<T>`. Exceptions stay inside their originating module.
- Read only the current snapshot's manifest list. Before binding IDs 515 and 517, read any string `format-version` header. Non-2 integers return `Unsupported` and malformed values return `InvalidArgument`. For an unversioned list, absence of either ID signals a legacy list and returns `Unsupported`. A declared-v2 list missing either ID returns `InvalidArgument`. Bind these M03 fields by integer `field-id` within their writer record. Required fields use the indicated non-null type. Missing required, duplicate or ill-typed bindings in the v2 profile return `InvalidArgument`. Skip unprojected fields by writer schema, including Iceberg-required list statistics outside the planner projection.

| Writer record | Required IDs and Avro types | Optional projected IDs and Avro types |
| --- | --- | --- |
| Manifest list | 500 path `string`, 501 length `long`, 502 spec ID `int`, 503 added snapshot ID `long`, 515 sequence `long`, 517 content `int` | — |
| Manifest entry | 0 status `int`, 2 data file `record` | 1 snapshot ID, 3 data sequence, 4 file sequence, each a union of `null` and `long` |
| Data file | 100 path `string`, 101 format `string`, 102 partition `record`, 103 record count `long`, 104 size `long`, 134 content `int` | — |

- Missing optional projected IDs read as null. Accept either union branch order and use writer branch indexes. Null snapshot IDs inherit the list entry's added snapshot ID. Null ADDED data and file sequences inherit the list entry's sequence. Require both sequences to be explicit and non-null on EXISTING and DELETED before dropping DELETED. Reject negative sequence numbers. Preserve explicit values and use the list entry's recorded values even when a later snapshot reuses the manifest.
- Read the manifest's string `format-version` header before binding its required IDs. Explicit v1 and unversioned manifests return `Unsupported` because v1 may omit the header. Other integer versions besides 2 also return `Unsupported`. A present malformed version returns `InvalidArgument`. For v2 manifests, require `partition-spec-id` and `content` headers and compare them to the list entry. Missing, malformed or mismatched values return `InvalidArgument`. Unknown spec, invalid status or content, negative file sizes or record counts, or a mismatch between `manifest_length` and the successfully read byte count return `InvalidArgument`. A delete manifest identified in the list returns `Unsupported`. File content 1 or 2 in a data manifest returns `InvalidArgument`.
- Plan live ADDED and EXISTING Parquet files. Compare `file_format` case-insensitively and store canonical `PARQUET`. A live non-Parquet file returns `Unsupported`. Each `FileScanTask` carries its original unremapped URI for ADR-0002's exact position-delete path matching, file size, record count, partition spec ID, identity-source field IDs, snapshot ID and data sequence number. Project current schema fields by ID in schema order. With no current snapshot or live files, the task list is empty.
- Skip partition values. When a projected field is missing from Parquet, F03.4 returns `Unsupported` if the field ID is an identity source of that file's spec. A missing ordinary optional field still reads as null. This narrows ADR-0003's optional-field rule for M03 because Iceberg calls for an identity partition value fallback.

## Consequences

- A retained owner keeps input, intermediate trees and chunk slack charged. [T0008](../../work/T0008-metadata-json/task.md) supplies the fault pool and libFuzzer preset for JSON. [T0009](../../work/T0009-manifest-scan-plan/task.md) reuses both for Avro. F03.6 supplies real PyIceberg fixtures and byte-identical relocation.
- M03 covers the v2 manifest profile and null Avro codec. Iceberg's v1 compatibility defaults and Avro's general-purpose deflate support are outside this profile. [Sources](refs/sources.md)

## Revisit if

Supported fixtures hit the proposed limits, measured retained metadata prevents otherwise viable scans, or a required table depends on compressed Avro, v1 manifest compatibility or partition value projection.
