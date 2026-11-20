# T0004 ADR-0002: architecture, modules and components
Status: 🟨 doing · Type: feature · Risk: high · Milestone: M02 · Feature: F02.1 · ADRs: ADR-0002

## Goal
Draft ADR-0002 defining doqe's modules, layers, dependency rules, code layout and cross-module
conventions, plus a planned `docs/arch/<module>.md` per module; the owner accepts it.

## Scope
- In: module map and responsibilities; layer and dependency rules; code layout; error-handling and RTTI policy;
  in-memory column format principles; public surfaces; where future parts (catalog REST, distributed) fit.
- Out: build changes (F02.2); concurrency model (later ADR); server wire protocol (later ADR);
  per-format choices (JSON, Avro, Parquet: later ADRs); Iceberg library strategy (later ADR).

## Edge cases
- A module that needs something from a higher layer (e.g. `format` wanting table schemas): the rule must say how.
- Code shared by `maintain` and `exec`, or `catalog` and `table`, without cycles.
- Test-only and benchmark code: where it lives and what it may depend on.

## Done when
- [ ] ADR-0002 exists with options, references and a decision, listed in the ADR index
- [ ] Each module has a `docs/arch/<module>.md` (Status: planned) listed in `docs/arch/README.md`
- [ ] Two independent reviews done and findings resolved
- [ ] The owner accepts ADR-0002; AGENTS.md gets the resulting hard rules

## Performance budget
None.

## Notes
- 2026-11-21 task started. Owner decisions: one static library per module with explicit allowed dependencies;
  own in-memory column format with an Arrow-compatible layout and no Arrow dependency; concurrency model decided later;
  surfaces are server (network protocol), CLI and an embeddable C++ library; Iceberg REST catalog planned for later;
  server protocol decided later; `std::expected` at module boundaries, exceptions never cross a module boundary, RTTI on;
  starting module list: base, io, format, table, catalog, exec, sql, server, cli, maintain, dist (later).
- 2026-11-21 drafted [ADR-0002](../../adrs/0002-architecture-modules/adr.md) and the planned module parts. Owner acceptance and independent reviews are pending.
- 2026-11-21 applied owner decisions on table-planned scans and exec scan/write, layer-6 engine composition,
  SQL procedures, three-way commit outcomes and reconciliation, and instance-owned services and shared contracts.
  Added field-ID batches, Iceberg v3 type principles, pinned-toolchain Arrow C embedding, F02.2 active-target
  and dependency checks, module-map alternatives, and corrected sources. Recorded deferred governance and
  distribution and the required approval for new top-level folders. Owner acceptance and independent reviews remain pending.
- 2026-11-21 applied the design review fixes for Arrow result ownership, server status mapping, Iceberg delete and commit semantics, and procedure contracts. ADR-0002 remains proposed.

## Result
Commit: — · Proof: —
