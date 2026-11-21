# File codecs
Status: JSON DOM implemented in T0008 · ADRs: [ADR-0002](../adrs/0002-architecture-modules/adr.md), [ADR-0003](../adrs/0003-iceberg-parquet-strategy/adr.md), [ADR-0005](../adrs/0005-metadata-arena-planning/adr.md) · Code: [`src/format/`](../../src/format/)

## Responsibility

Encode and decode format-level data and metadata payloads. Use io for byte access
and column for batches. The module has zero links or includes to table, catalog, and exec.

## Interfaces

[`ParseJson`](../../src/format/include/doqe/format/json.hpp) reads a whole JSON
document. Nodes, decoded strings and original number lexemes borrow the caller's
arena. Array order is preserved. Object members are sorted by decoded key for
duplicate detection and binary lookup. `AsInt64` checks an integer lexeme
directly for syntax and range.

## Invariants

The DOM allows at most 16 MiB per document and 64 nested objects or arrays,
including a container at the root. Over-limit input returns `Unsupported`.
Invalid UTF-8, invalid escapes or surrogate pairs, decoded duplicate keys,
truncation and trailing bytes return `InvalidArgument`.

## Planned

T0009 adds Avro decoding. F03.4 adds Parquet reading. Readers and writers can
accept format-level schemas or callbacks from callers. Future Puffin decoding
will expose Iceberg v3 deletion vectors for exec to apply.
