# Iceberg table semantics
Status: metadata reader implemented in T0008 · ADRs: [ADR-0002](../adrs/0002-architecture-modules/adr.md), [ADR-0003](../adrs/0003-iceberg-parquet-strategy/adr.md), [ADR-0005](../adrs/0005-metadata-arena-planning/adr.md) · Code: [`src/table/`](../../src/table/)

## Responsibility

Own Iceberg schemas, field IDs, snapshots, manifests, partitioning, predicate rules, delete applicability, and commit rules.

## Design

Keep the snapshot metadata cache instance-owned. Use Iceberg manifest, metrics, and residual rules to plan each task with a data file, applicable position- and equality-delete files, residual filter, and field-ID projection map including hidden read fields. Prepare commits from writer-produced data-file metadata and call the injected publication implementation.

## Interfaces

[`ReadMetadata`](../../src/table/include/doqe/table/metadata.hpp) reads local
Iceberg v2 JSON through an injected file system and returns a move-only
`TableMetadata`. The model owns its arena and exposes the current schema in
schema order, the optional selected snapshot and manifest-list location, and
each partition spec's identity-source IDs. Model views remain valid while the
owner holds their arena, including after the caller releases its pool reference.
The owner may be destroyed on another thread with exclusive access.

## Invariants

- Require the version, schema list, current schema ID and partition spec list.
  Detect gzip by magic and return `Unsupported` for gzip or non-v2 integer versions.
  Classify integer versions from their JSON spelling, including integers beyond
  int64. Fraction and exponent spellings return `InvalidArgument`.
- Validate unique schema, spec and snapshot IDs and recursively validate field IDs
  within each schema. Table data field IDs may be at most 2147483447. Nested
  `struct`, `list` and `map` types require an object in every schema. Project
  from the current schema, mapping `long` to int64
  and `string` to UTF-8 while preserving ID, name and requiredness. Other
  current field types return `Unsupported` with the field ID.
- Omitted, null or `-1` current snapshot IDs denote an empty table. Check
  `refs.main` against the current ID and reject a main ref with ID `-1`.
  A live selected snapshot must exist,
  have a manifest-list location and resolve its schema ID when supplied.

- A task projects by field ID and includes the inputs needed for deletes and residual evaluation.
- A position delete file applies only when its data sequence number is greater than or equal to the data file’s and Iceberg’s path and partition scope rules hold.
- An equality delete file requires a strictly greater data sequence number and matching partition spec ID and values. An equality delete file with an unpartitioned spec applies globally, with the same strict sequence comparison.
- Commit returns committed, definitely rejected, or outcome unknown. Every recognised commit error other than `CommitStateUnknown` means the attempt published nothing and its written files may be cleaned up. Treat an unrecognised future commit error as `CommitStateUnknown`. After `CommitStateUnknown`, reconcile against authoritative state before retry and never clean up the written files as failure handling. An inconclusive reconciliation keeps the outcome unknown and the files protected.

## Planned

T0009 adds manifest walking and scan planning.

## Open questions

Initial Iceberg spec coverage, library integration, and backend reconciliation evidence.
