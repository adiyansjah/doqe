# Iceberg table semantics
Status: planned · ADRs: [ADR-0002](../adrs/0002-architecture-modules/adr.md) · Code: `src/table/` (empty skeleton)

The static target is an empty skeleton.

## Responsibility

Own Iceberg schemas, field IDs, snapshots, manifests, partitioning, predicate rules, delete applicability, and commit rules.

## Design

Keep the snapshot metadata cache instance-owned. Use Iceberg manifest, metrics, and residual rules to plan each task with a data file, applicable position- and equality-delete files, residual filter, and field-ID projection map including hidden read fields. Prepare commits from writer-produced data-file metadata and call the injected publication implementation.

## Interfaces

Iceberg schemas, scan tasks, data-file metadata, table updates, and the metadata load and conditional publication contract.

## Invariants

- A task projects by field ID and includes the inputs needed for deletes and residual evaluation.
- A position delete file applies only when its data sequence number is greater than or equal to the data file’s and Iceberg’s path and partition scope rules hold.
- An equality delete file requires a strictly greater data sequence number and matching partition spec ID and values. An equality delete file with an unpartitioned spec applies globally, with the same strict sequence comparison.
- Commit returns committed, definitely rejected, or outcome unknown. Every recognised commit error other than `CommitStateUnknown` means the attempt published nothing and its written files may be cleaned up. Treat an unrecognised future commit error as `CommitStateUnknown`. After `CommitStateUnknown`, reconcile against authoritative state before retry and never clean up the written files as failure handling. An inconclusive reconciliation keeps the outcome unknown and the files protected.

## Planned

Decide external library use in the Iceberg library strategy ADR.

## Open questions

Initial Iceberg spec coverage, library integration, and backend reconciliation evidence.
