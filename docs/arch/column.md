# Column types and buffers
Status: planned · ADRs: [ADR-0002](../adrs/0002-architecture-modules/adr.md) (proposed) · Code: `src/column/` (not yet created)

## Responsibility

Own engine logical types, scalar values, batch schemas, typed vectors, buffer ownership, and Arrow C Data / C Stream export for batches and streams.

## Design

Use native Arrow-compatible buffers, validity bitmaps, and offsets where applicable. Preserve field IDs in batch schemas. The type model must accommodate Iceberg v3 types including `timestamp_ns`, `variant`, and `geometry`. Use the official Apache Arrow `arrow/c/abi.h` verbatim in column when implementing export, as approved by ADR-0002. Charge query buffers to the supplied query pool.

## Interfaces

Engine types and scalar values, field-ID-bearing schemas, buffer owners and views, and column batches.

## Invariants

Renaming or reordering an Iceberg field preserves its batch-schema ID. Published read-only views keep backing buffers valid for their documented lifetime. The module links without Apache Arrow.

## Planned

Create the target in F02.2. Define layouts in the In-memory column format details ADR.

## Open questions

Supported layouts, offset widths, alignment, slicing, and initial v3 type coverage.
