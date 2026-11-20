# Iceberg catalog
Status: planned · ADRs: [ADR-0002](../adrs/0002-architecture-modules/adr.md) (proposed) · Code: `src/catalog/` (not yet created)

## Responsibility

Resolve namespaces and identifiers, manage catalog state, and publish metadata references.

## Design

Implement the table-facing metadata contract. Keep the current-ref cache instance-owned. Use io for storage and HTTP/TLS/request signing and format for payload codecs. Host the future outbound Iceberg REST adapter here.

## Interfaces

Namespace and table lookup, table handles, conditional publication, and authoritative state for reconciliation.

## Invariants

A definitely rejected publication makes no reference change from that attempt. A possibly applied publication returns `CommitStateUnknown`, including when a response is lost after dispatch. After unknown, reconcile against authoritative state before retry and never clean up the written files as failure handling. A cached current ref alone cannot resolve the outcome.

## Planned

Create the target in F02.2. Add the Iceberg REST catalog later.

## Open questions

Catalog persistence, remote integration, and backend-specific reconciliation evidence.
