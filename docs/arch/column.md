# Column types and buffers
Status: implemented for F03.2 · ADRs: [ADR-0002](../adrs/0002-architecture-modules/adr.md), [ADR-0004](../adrs/0004-status-memory-columns/adr.md) · Code: [`src/column/`](../../src/column/)

## Responsibility

Own engine logical types, scalar values, batch schemas, typed vectors, buffer ownership, and Arrow C Data and C Stream export for batches and streams.

## Interfaces

- [`Buffer`](../../src/column/include/doqe/column/column.hpp) publishes immutable, reference-counted storage. Backing allocations are aligned and padded to 64 bytes. Slices share storage and expose only their logical range.
- `Array` builds int64 and UTF-8 arrays from borrowed values and an optional Arrow validity bitmap. Slices preserve the backing pointers and record a row offset. The bitmap distinguishes nulls from empty strings. Exports report the exact slice null count and omit the bitmap when that count is zero.
- `Batch` owns an ordered set of uniquely keyed Iceberg fields. `Find` looks up the field ID independently of its name or position. Batch fields and names use separate pool-charged allocations. Names must be valid UTF-8 without embedded NUL bytes, and each field array must match the batch length.
- Array and batch `Export` methods populate caller-owned C Data structs. The array and schema callbacks each release their producer state independently. A child moved out of a parent has independent producer state.

## Invariants

UTF-8 offsets are nonnegative, nondecreasing int32 values with one more entry than the row count. Unsliced arrays start at zero, null slots may span bytes, and the final offset equals the data size. Readers validate UTF-8 and call `CheckUtf8Append` before accepting a row. A value over `INT32_MAX` bytes returns `Unsupported`. A cumulative size over the limit signals a new batch so the reader can retry the entire row.

Every buffer control block and export state retains its pool and the original payload pointer used to free it. Exports charge their state, pointer tables, child structs and names to the supplied pool and roll back partial state on failure. Consumers serialize access to each C struct and may release independent exports concurrently after query teardown.

Pool payloads are raw storage. Construct arrays with non-allocating placement array new and use the returned first-element pointer for indexing, spans and pointer arithmetic. Buffer copies use a placement byte array for byte views and the pointer returned by `memcpy` for the implicit-lifetime objects read by Arrow consumers. Native validity reads use that same `uint8_t` view, so doqe and Arrow consumers index the same bitmap elements. Destroy nontrivial elements before freeing the original allocation pointer.

## Arrow C ABI provenance

[`arrow/c/abi.h`](../../src/column/include/doqe/column/arrow/c/abi.h) is the verbatim Apache-2.0 header from release tag `apache-arrow-26.0.1`, at `cpp/src/arrow/c/abi.h`. Its SHA-256 is `1828f1ce60368cd590d19595910fa19db09b30728f8a43b5f89933623f7a7276`. [Upstream header](https://github.com/apache/arrow/blob/apache-arrow-26.0.1/cpp/src/arrow/c/abi.h)

The release's accompanying files are also vendored verbatim.

| File | SHA-256 |
|---|---|
| [LICENSE.txt](../../src/column/include/doqe/column/arrow/LICENSE.txt) | `cd03925a27219d326d622609fbcae6567279af8b8885c33d5969a9e4ef5d7525` |
| [NOTICE.txt](../../src/column/include/doqe/column/arrow/NOTICE.txt) | `afd895d61a00101f6d5a41cf518c83865d815e8109e956f512a0ce4ea9738c06` |

[Upstream license and notice](https://github.com/apache/arrow/tree/apache-arrow-26.0.1)

## Planned

C Stream wiring in F03.5 will preserve independent export lifetimes. The type model will expand to Iceberg v3 types including `timestamp_ns`, `variant`, and `geometry` as their readers arrive.
