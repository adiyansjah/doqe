# File codecs
Status: planned · ADRs: [ADR-0002](../adrs/0002-architecture-modules/adr.md) (proposed) · Code: `src/format/` (not yet created)

## Responsibility

Encode and decode format-level data and metadata payloads.

## Design

Use io for bytes and column for batches. Accept format-level schemas or callbacks from callers. Decode Iceberg v3 deletion vectors stored in Puffin for exec to apply. Details are deferred.

## Interfaces

Readers and writers, codec schemas and options, and column batch input or output.

## Invariants

The format target has zero links or includes to table, catalog, and exec. Codec failures use the boundary status contract.

## Planned

Create the target in F02.2. Select JSON, Avro, and Parquet implementations in their respective ADRs.

## Open questions

Codec choices and initial file feature coverage.
