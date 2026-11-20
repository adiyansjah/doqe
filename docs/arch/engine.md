# Engine composition and query lifecycle
Status: planned · ADRs: [ADR-0002](../adrs/0002-architecture-modules/adr.md) (proposed) · Code: `src/engine/` (not yet created)

## Responsibility

Own internal Engine, Session, and Query objects, instance configuration, service wiring, cancellation, and resource limits.

## Design

Create configured io and catalog instances, inject the catalog publication implementation into table, and register maintain callables with sql. Set up the exec function registry and observability sinks. Create one pool per query and pass it and the base cancellation token to query components.

## Interfaces

Internal Engine, Session, and Query handles and configuration, statement submission, and cancellation. Query results are returned as an `ArrowArrayStream` using column’s exporter. The engine result header provides the ABI declarations to consumers.

## Invariants

Each query uses one query pool and enforces its own budget. A procedure registered during engine setup is callable through the same submission interface as ordinary SQL. A session uses services and registries from its own engine instance.

## Planned

Create the target in F02.2. Wire the first single-node SQL slice. Enable a dist-backed runner when dist is activated.

## Open questions

Scheduling and cancellation checkpoints under the Engine concurrency model ADR.
