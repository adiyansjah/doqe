# Distributed execution
Status: planned · ADRs: [ADR-0002](../adrs/0002-architecture-modules/adr.md) · Code: `src/dist/` (not yet created)

## Responsibility

Coordinate workers, execute physical fragments, and manage exchange and distributed failures.

## Design

Engine composes the coordinator over the exec runner contract. Expect table scan tasks and transport from io or a future lower-layer net module.

## Interfaces

Internal coordinator, worker, and exchange contracts.

## Invariants

M02 excludes the dist target and all links to it. When dist is activated, engine owns the incoming dependency and dist links only to lower layers.

## Planned

Create the target after the single-node engine under the Distributed execution and transport ADR.

## Open questions

Fragment representation, transport ownership, and failure policy.
