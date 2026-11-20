# Foundation
Status: planned · ADRs: [ADR-0002](../adrs/0002-architecture-modules/adr.md) (proposed) · Code: `src/base/` (not yet created)

## Responsibility

Provide domain-neutral primitives, status and cancellation, query-memory accounting, and observability interfaces.

## Design

Define the memory pool and accounting interface in base so I/O and execution can use the same query pool. Logging, metrics, and tracing take injected, instance-owned sinks.

## Interfaces

`Status`, cancellation token, memory pool and accounting, observability sinks, checked arithmetic, and byte views. ADR-0002 lists the required status codes.

## Invariants

Constructing `OutOfMemory` works when allocation fails. A rejected budget charge returns `ResourceExhausted`. Callers have a fallback for unknown future codes. Fallible boundary functions carry `[[nodiscard]]`.

## Planned

Create the target in F02.2. Add contracts with their first callers.

## Open questions

Diagnostic representation, pool implementation, and sink signatures.
