# Byte and object I/O
Status: planned · ADRs: [ADR-0002](../adrs/0002-architecture-modules/adr.md) · Code: `src/io/` (empty skeleton)

The static target is an empty skeleton.

## Responsibility

Provide local and object byte access, listing and deletion, and the outbound HTTP/TLS and request-signing client.

## Design

Own credentials, transport configuration, and byte/footer caches per instance. Cache footer bytes for format to interpret.

## Interfaces

File and object handles, range reads, writes, listing, deletion, and outbound HTTP requests.

## Invariants

Two separately configured instances use their own credentials, transport settings, and cache state. Storage failures use the boundary status contract.

## Planned

Add the local and object-store backends as needed.

## Open questions

Initial object-store backend and I/O scheduling.
