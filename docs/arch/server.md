# Network server
Status: planned · ADRs: [ADR-0002](../adrs/0002-architecture-modules/adr.md) (proposed) · Code: `src/server/` (not yet created)

## Responsibility

Serve network query and administrative requests and host future inbound Iceberg REST routes.

## Design

Translate wire requests and results through engine. Server depends directly on base for status mapping and on engine for shared lifecycle, cancellation, resource limits, and SQL procedures. It consumes the `ArrowArrayStream` C ABI through engine’s public result header without including column headers. Future Iceberg REST routes can add a separately approved catalog edge for catalog operations.

## Interfaces

Server configuration and lifecycle, query routes, and future REST catalog routes.

## Invariants

Query and maintenance requests pass through engine. Server maps `base::Status` to wire errors and preserves `CommitConflict`, `CommitStateUnknown`, `Cancelled`, and `DeadlineExceeded` as distinct outcomes. Server has no direct sql or maintain links. The engine builds without server headers or links.

## Planned

Create the target in F02.2. Choose the protocol in the Server wire protocol ADR. Share an authorization point with SQL and maintenance under the Governance and authorization ADR.

## Open questions

Query protocol and future Iceberg REST coverage.
