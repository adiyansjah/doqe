# Table maintenance
Status: planned · ADRs: [ADR-0002](../adrs/0002-architecture-modules/adr.md) · Code: `src/maintain/` (empty skeleton)

The static target is an empty skeleton.

## Responsibility

Orchestrate rewrites such as compaction, snapshot expiration, and orphan cleanup.

## Design

Implement maintenance procedure callables using the signature declared in exec and built only from types below sql. Maintain implements it without including sql headers. Engine registers the callables with sql. Maintain drives commits for procedures and table performs the commit protocol. Reuse exec operators over column batches and table and catalog commit semantics. Use io for object operations.

## Interfaces

Maintenance callables and outcome reports registered by engine, including the planned `CALL system.rewrite_data_files(...)`.

## Invariants

Rewrites use the same conditional publication contract as ordinary writes. After `CommitStateUnknown`, reconcile against authoritative state before retry and never clean up the written files as failure handling. Orphan cleanup preserves files while their publication outcome remains unresolved.

## Planned

Add maintenance operations after their table primitives. Share an authorization point with SQL and server under the Governance and authorization ADR.

## Open questions

First maintenance operation, scheduling, and how unresolved writes are tracked for orphan cleanup.
