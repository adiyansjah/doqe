# ADR-0004: Use compact status, a flat query pool and shared Arrow buffers

| Status | Date | Decided by | Confidence | Tags |
|---|---|---|---|---|
| accepted | 2026-11-21 | owner | medium | errors, memory, column |

Supersedes: none · Superseded by: none · Related: [ADR-0001](../0001-core-cpp-toolchain/adr.md), [ADR-0002](../0002-architecture-modules/adr.md), [ADR-0003](../0003-iceberg-parquet-strategy/adr.md)

## Context

[M03](../../road/M03-first-iceberg-read.md) scans small Iceberg v2 tables on one thread and produces batches of nullable int64 and UTF-8 columns. T0007 needs error reporting that works under memory pressure, a query budget that covers reader allocations, and a buffer lifetime contract for Arrow C Data export.

## Criteria

1. Preserve the failure code under memory pressure and enforce the query budget on every reader allocation.
2. Share and slice immutable columns without copying. Allow consumers to release exports on any thread, including after the Query ends.
3. Preserve Iceberg field identity and keep the M03 implementation small, with no Apache Arrow library dependency.

## Options

### A. Code-only status

- **Good.** Fixed size and allocation-free.
- **Bad.** Loses diagnostics and operation context even when memory is available.

### B. Compact status with optional heap detail

- **Good.** Preserves a small value and a code-only fallback while allowing owned diagnostics.
- **Bad.** Detail construction can allocate and must handle failure.

### C. Flat query pool

- **Good.** One limit and one live byte count cover all query allocations.
- **Bad.** Gives no independent limits to operators or readers.

### D. Hierarchical pools

- **Good.** Can enforce per-operator limits as well as a query-wide limit.
- **Bad.** Adds parent charging, rollback, and lifetime rules before M03 needs them.

### E. Unique buffers with copies for sharing

- **Good.** One owner per allocation and no reference-count bookkeeping.
- **Bad.** Copies consume budget when slices, exports, or batches need independent lifetimes.

### F. Shared Arrow buffers with 32-bit UTF-8 offsets

- **Good.** Slices and exports share storage. Strings use one offsets buffer and one data buffer.
- **Bad.** Shared slices can retain entire allocations. The data span addressed by offsets is bounded by `INT32_MAX`.

### G. Shared Arrow buffers with large UTF-8 or UTF-8 views

- **Good.** Large UTF-8 addresses more data per array. UTF-8 views can reference strings in several backing buffers.
- **Bad.** Large UTF-8 doubles the offset width. Views add another layout and more backing-buffer lifetime cases.

## References

- **Source.** [Sources](refs/sources.md) cover Arrow layouts, release and stream lifetimes, Iceberg field identity, and C++ allocation and object lifetime.
- **Judgment.** One query thread can allocate against a flat pool while consumers free from other threads. One string layout meets M03's scope. Allocation costs and performance are unmeasured.

## Decision

Choose **B, C, and F**. Keep diagnostics optional, charge one budget, and share native Arrow-compatible storage.

### Status

- `base::Status` is a 16-byte error-only value on supported targets, with a code enum of underlying type `std::uint8_t` and a nullable detail pointer. Use the ADR-0002 codes: `Cancelled`, `DeadlineExceeded`, `ResourceExhausted`, `OutOfMemory`, `CommitConflict`, `CommitStateUnknown`, `NotFound`, `AlreadyExists`, `InvalidArgument`, `Unsupported`, `IoError`, and `Internal`. Success is the value branch of `std::expected`. Code-to-name lookup returns a static `std::string_view`, with `"Unknown"` for any unrecognized code.
- All constructors, factories, assignments, and the destructor are `noexcept`. Copies and propagation share one immutable detail using an intrusive atomic reference count. Moves transfer the reference. Copy and move construction and assignment allocate nothing. The last reference frees the detail.
- Detail owns the message and optional operation context. Cap their combined payload at 256 bytes, taking the message first and giving context the remaining bytes. Scan each supplied view without allocating and take the longest valid UTF-8 prefix that fits. Stop before a code point that would cross the byte cap or at the last valid boundary before malformed input. Allocate detail from the global heap with nothrow `::operator new`, outside the query budget, and copy the selected bytes without throwing. `OutOfMemory` is always code-only and allocation-free. Failed detail allocation preserves the original code alone.
- Fallible module boundaries use `[[nodiscard]] std::expected<T, base::Status>`, including `T = void`. Exceptions stay inside their originating module.

### Pool and ownership

- `base` provides the pool interface and a flat pool with a fixed per-query byte limit and atomic live-byte count. Route all reader allocations through it, including input, scratch, dictionaries, and output. Allocation is confined to the single query thread in M03. Free, live-byte and limit reads, and pool retain/release are thread-safe while the caller holds a live reference. They may overlap allocation and run concurrently on different threads.
- The Query, each buffer control block, and each export state hold their own pool reference through an intrusive atomic counter. Each owner returns its allocations before dropping its reference. The final reference decrement is the sole production path to pool destruction and runs the destructor on the releasing thread. The destructor prints a diagnostic and calls `std::abort` if live bytes are nonzero in any build. All reference-count decrements for Status detail, buffers, and the pool use `acq_rel` (or a release decrement with an acquire fence before destruction) so prior owners' accesses happen before destruction.
- The pool charges the requested size. Buffer backing storage requests `round_up(n, 64)` bytes at 64-byte alignment and excludes padding from its logical extent. Control blocks charge `sizeof` the control block. Allocator overhead is excluded from the charge. Reject round-up overflow or `charge > limit - live` with `ResourceExhausted` before allocation. Equality succeeds when the allocator succeeds. Reserve the charge before allocating with an atomic compare-and-exchange loop that rechecks the limit. Refunds and rollbacks use atomic `fetch_sub`. Relaxed ordering suffices for byte accounting alone. On allocator failure, undo the reservation and return code-only `OutOfMemory`. If allocating the control block fails after allocating the data, free the data and propagate the control block's error.
- Accept power-of-two alignments from 1 to 64. Return `InvalidArgument` for zero, non-powers of two, or requests above 64. A valid zero-byte request succeeds with a null pointer and no charge or allocator call. Freeing a null pointer with zero size and a valid alignment is a no-op. For each nonzero allocation, the pool stores the original charge (including any requested padding) and alignment in an allocation header. If recording fails, free the backing allocation, undo the reservation, and return code-only `OutOfMemory`. Free takes the pointer, exact charge, and requested alignment. In every build, abort on a size or alignment mismatch before deallocating or subtracting the charge. In debug builds, validate and retire the registry entry atomically with respect to other registry operations, aborting on unknown pointers or double frees. On a matching free, return the recorded charge.
- `column` publishes immutable buffers with intrusive atomic reference counts. Slices and batches can share backing storage, including dictionary storage. Use implicit object creation in the allocated storage for implicit-lifetime types such as `int64_t` and `int32_t`. Use placement construction and explicit destruction for control blocks and export state. ADR-0001's C++23 library allowlist applies.

### Columns and export

- Use Arrow's int64 values layout and `utf8` layout. Export the exact, nonnegative `null_count` for the logical range, including slices. Omit the validity buffer only when that count is zero. The bitmap distinguishes a null from an empty string.
- UTF-8 offsets are signed 32-bit, nonnegative, and nondecreasing even for nulls. A null slot may span bytes. There are `length + 1` entries in an unsliced array, including one offset for an empty array. An unsliced builder starts offsets at zero. Export a slice with unchanged backing pointers and the logical `ArrowArray.offset`, so its first logical offset may be nonzero. Check size and offset arithmetic before narrowing.
- A single value larger than `INT32_MAX` bytes returns `Unsupported`. If adding a value would put its column's data buffer past `INT32_MAX`, end the batch before that row and have the reader retry the whole row in a fresh batch. Exactly `INT32_MAX` bytes fits. Readers check the value size and validate UTF-8 before passing it to builders, returning `InvalidArgument` for invalid UTF-8 within the size limit. Builders assume valid UTF-8. `large_utf8` and `utf8_view` return `Unsupported`.
- Key batch schemas by Iceberg field ID, preserving IDs across rename and reorder. Duplicate IDs return `InvalidArgument`.
- Use the verbatim upstream `arrow/c/abi.h` in `column` as approved by ADR-0002. Export into caller-owned C Data structs with `[[nodiscard]] std::expected<void, base::Status>`. Charge all producer-owned export allocations to the pool, including `private_data`, buffer pointer arrays, and child structs and pointer arrays. On failure, free every partial allocation and release every reference acquired by the export without allocating. Leave each output's `release` null. On success, export state owns its buffer and pool references without depending on the addresses of the caller's structs.
- Consumers may release exports on any thread, including after Query teardown. Independent exports may be released concurrently. A consumer serializes access, moves, and release of the same live C struct. The `noexcept` callback releases still-owned children, frees export allocations, drops retained references, and clears `release`. A consumer may move a parent and then move one or more children out of it. It clears the moved-from release pointers and releases the parent immediately after moving the children. Moved children remain usable and releasable independently. Each charge returns exactly once when its last owner releases it. See [Arrow C Data and C Stream sources](refs/sources.md#arrow-and-iceberg).

Other column types, nested data columns, dictionary-encoded export, SIMD, concurrent query execution, hierarchical pools, and reader implementation are out of T0007 scope. C Stream wiring is F03.5 and must preserve the pool ownership above. Consumers serialize `get_next` calls, and `get_next` and `get_schema` results can outlive the stream.

## Consequences

Small slices can retain whole backing buffers and their charges. Cross-thread release adds atomic reference-count and accounting operations. After owner acceptance, write failing tests first and update the architecture and development docs with the implementation.

- **Status.** Use `static_assert` for size 16 and the applicable `std::is_nothrow_*` construction, assignment, and destruction traits. Check factories with `noexcept` assertions. Split a multibyte code point at the byte cap in separate message and context tests. Check that malformed input ends at the last valid boundary. Test detail sharing and code-only fallback. With a failing allocator, construct `std::unexpected(Status::OutOfMemory())` and an error-valued `std::expected` from it with zero allocation attempts. Also copy and move an existing detailed status with zero attempts.
- **Pool.** Test exact-limit success, a charge one byte over, round-up overflow, requested alignment, invalid alignment, zero-byte requests, allocator failure and control block rollback. Inject a recording failure and check that the backing allocation is freed, the reservation is refunded, and the error is code-only `OutOfMemory`. Exercise both successful allocation and failed-allocation rollback while a free overlaps on another thread, and assert the exact final live-byte count in each case. Tear down the Query while another owner holds a reference to an empty pool, then release that reference on another thread and verify it destroys the pool exactly once there. In debug and release builds, death-test size and alignment mismatches and use test-only fault injection to death-test the destructor invariant on final release. Death-test unknown pointers and double frees in debug builds.
- **Columns and export.** Test empty and all-null arrays, empty versus null strings, exact slice null counts, a positive-length null slot, and duplicate IDs. Test both single-value and cumulative-size cases at `INT32_MAX` and one byte beyond. Boundary tests may simulate logical sizes without allocating gigabytes. At each injected export allocation failure, verify all output `release` callbacks are null and live bytes equal the pre-export count. Then drop the original batch and Query and verify that the pool is destroyed exactly once. Move a parent and a child in a successful export and verify independent release. Across Query teardown and concurrent releases, verify retained bytes remain readable, charges return exactly once, and the pool dies on the last reference. Run lifetime and alignment cases under ASan+UBSan.

## Revisit if

Operators need independent budgets, queries need multiple allocating threads, or workloads need larger string arrays or enough string-buffer reuse to warrant `utf8_view`.
