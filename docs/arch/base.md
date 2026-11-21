# Foundation
Status: implemented for F03.2 · ADRs: [ADR-0002](../adrs/0002-architecture-modules/adr.md), [ADR-0004](../adrs/0004-status-memory-columns/adr.md) · Code: [`src/base/`](../../src/base/)

## Responsibility

Provide domain-neutral primitives, status and cancellation, query-memory accounting, and observability interfaces.

## Interfaces

- [`Status`](../../src/base/include/doqe/base/status.hpp) is a 16-byte, error-only value with a byte-sized `Code`. Module boundaries return `[[nodiscard]] base::Result<T>` (`std::expected<T, Status>`), including `Result<void>`. `Fail` constructs the error branch and `CodeName` returns a static name, with `Unknown` as the fallback.
- [`MemoryPool`](../../src/base/include/doqe/base/memory_pool.hpp) accounts for every query allocation. `MakeFlatPool` creates a pool with a fixed byte limit. `PoolPtr` holds an intrusive reference. An allocating query thread and any freeing threads each hold a live reference during access.

## Invariants

Status detail owns at most 256 bytes of valid UTF-8 across the message and operation context. Detail is shared and allocated with nothrow allocation outside the query budget. An allocation failure preserves the code. `OutOfMemory` always uses the code alone.

The flat pool reserves the requested charge before allocation and refunds the same charge on failure or free. A rejected budget charge returns `ResourceExhausted`. An allocator or registry failure returns `OutOfMemory`. The allocation header records the exact size and alignment, and mismatched frees abort.
Callers pass the original payload pointer from `Allocate` to `Free`, including when the payload
holds a placement-constructed object. The pool uses this pointer to read the header in the
backing byte array. Allocation aborts if the live-byte count already exceeds the pool limit. Debug builds also reject unknown pointers and double frees through a synchronized registry. The final reference destroys the pool on the releasing thread, and destruction with live bytes aborts.

## Tests

`doqe_status_tests` links its allocation probes with GoogleTest and `doqe_base`.
The separate executable keeps the replacement operators out of the Linux ASan
allocation-mismatch checks in `doqe_tests`.

## Planned

Add cancellation, observability sinks, and other primitives with their first callers.
