# ADR-0002: Use layered modules with one static library each

| Status | Date | Decided by | Confidence | Tags |
|---|---|---|---|---|
| proposed | 2026-11-21 | owner | medium | architecture, build, execution |

Supersedes: none · Superseded by: none · Related: [ADR-0001](../0001-core-cpp-toolchain/adr.md)

## Context

[M02](../../road/M02-architecture-baseline.md) needs a dependency graph before F02.2 creates the library targets. The first data workload is single-node SQL over Iceberg on macOS arm64 and Debian x86_64, with column batches decoded from local files or object storage. The same table semantics will serve queries, writes, and maintenance. Batch size, data distributions, resource limits, and concurrency targets are unspecified. M02 ships no data features and makes no throughput or memory claim.

## Criteria

1. Give each durable responsibility one owner and keep production dependencies acyclic and enforceable in CMake and source.
2. Support a thin single-node slice with shared table semantics and column buffers across storage, execution, and maintenance.
3. Preserve the owner constraints in [T0004](../../work/T0004-adr-0002-architecture/task.md) and the accepted [C++ toolchain](../0001-core-cpp-toolchain/adr.md).
4. Keep protocol, embedding, and future distributed execution changes localized.

## Options

**Module granularity (Judgment)**

### A. One engine archive with responsibility-based directories

- Good: Simplest baseline to configure and link. Folder boundaries can evolve rapidly.
- Bad: Directory boundaries provide no independent link targets for testing or enforcing module dependencies.

### B. One static library per coarse responsibility

- Good: Makes build dependencies explicit and lets callers link and test a subset. A dedicated column module serves both codecs and execution.
- Bad: Adds targets and headers at module boundaries. Moving a responsibility can require changing the dependency table.

### C. A static library per subsystem, including parser, planner, optimizer, and each codec

- Good: Gives independently reusable subsystems their own build and test targets.
- Bad: Adds interfaces and target wiring before the first slice demonstrates independent consumers.

**Dependency direction (Judgment)**

### D. Permit every downward dependency by layer

- Good: Layer numbers alone define legality. Adding a downward consumer is easy.
- Bad: Front ends could bypass service boundaries and depend directly on codecs or table internals exposed to sibling consumers.

### E. List permitted direct edges and use consumer-owned interfaces for reverse calls

- Good: Makes each dependency reviewable. Lower modules can accept behavior supplied during composition.
- Bad: A new direct edge needs a deliberate graph change. Interfaces or callbacks add a contract to maintain where reverse calls are needed.

**Header layout (Judgment)**

### F. Keep all headers beside their implementation

- Good: Few directories and short local includes.
- Bad: A consumer-visible include root exposes internal headers unless a separate manifest or check distinguishes them.

### G. Give each module its own public include root

- Good: The path identifies the owner and CMake only grants consumers the module's public include root.
- Bad: Public declarations and implementations live in separate subtrees. Embedding still needs a narrower export policy.

**Error policy (Judgment)**

### H. Use exceptions across module boundaries

- Good: Success paths retain simple return types and stack unwinding propagates failures.
- Bad: Fallible APIs obscure the failure contract. Every caller, adapter, and public entry point must account for exceptions from dependencies.

### I. Return `std::expected<T, Status>` at fallible boundaries

- Good: Signatures declare failure and callers can branch on a common error code. Modules can isolate exceptions from library code.
- Bad: Requires explicit propagation, exception translation, and care when constructing failure results under memory pressure.

**Module map (Judgment)**

### J. Keep types with columns and executable expressions with execution

- Good: Codecs and operators share the column contract. SQL binds against the signatures that execution dispatches.
- Bad: Type-only or expression-only consumers still depend on the owning coarse module.

### K. Give types and expressions their own modules

- Good: Type-only and expression-only consumers can link smaller targets.
- Bad: Adds targets and interfaces for scalar values, buffers, kernels, and expression execution before independent consumers need those splits.

**Scan and write boundary (Judgment)**

### L. Plan in table and run scan and write operators in exec

- Good: Table owns Iceberg rules while execution shares batch scheduling, codecs, and hash joins across queries and maintenance.
- Bad: Execution knows the Iceberg task and data-file metadata contracts and must honor delete semantics selected by table.

### M. Keep exec free of Iceberg through source and sink interfaces

- Good: Execution consumes and produces batches through generic interfaces. Iceberg adapters can evolve independently.
- Bad: Iceberg adapters need buffering and scheduling contracts and a seam to reuse the execution hash join for equality deletes. This adds indirection to the first Iceberg slice.

**Service composition (Judgment)**

### N. Put service wiring in an internal engine module

- Good: Server and embedding share lifecycle, configuration, limits, and procedure registration.
- Bad: Adds a target and requires translation between internal handles and the public façade.

### O. Put service wiring in api

- Good: Removes one target and lets embedding construct services directly.
- Bad: Server lifecycle would depend on the embedding layer, or server would need duplicate wiring.

## References

- **Source.** Primary sources cover component boundaries, CMake usage, Iceberg predicate and commit semantics, and Arrow layouts and interchange. [Sources](refs/sources.md)
- **Judgment.** The ownership and dependency choices are design judgments informed by these examples and the owner constraints. Build and runtime performance are unmeasured.

## Decision

Choose **B, E, G, I, J, L, and N**. Use coarse static libraries, explicit downward edges, module-public include roots, value-or-error boundaries, and a shared internal engine.

### Module ownership (criteria 1 and 2)

- **`base`** owns `Status`, the cancellation token, the memory pool and accounting interface, logging, metrics, and tracing interfaces with injected sinks, and small domain-neutral primitives. A base pool lets I/O and execution charge the same query budget without a dependency on column types.
- **`column`** owns engine logical types, scalar values, field-ID-bearing batch schemas, typed vectors, and buffer ownership and views. Codecs and execution share these contracts. The column module exports batches and streams through the Arrow C Data and C Stream interfaces.
- **`io`** owns local and object byte access, listing and deletion, the outbound HTTP/TLS and request-signing client, and the byte/footer cache. `format` interprets the cached footer bytes.
- **`format`** owns format-level codecs, including JSON, Avro, and Parquet when selected. It exchanges bytes or streams and format-level schemas or options, and uses engine batches for tabular data.
- **`table`** owns Iceberg schemas and type mapping, snapshots, manifests, partition and predicate rules, delete applicability, scan-task planning, and commit rules. It owns the snapshot metadata cache and defines the metadata load and conditional publication contract.
- **`catalog`** owns identifier and namespace resolution, the current-ref cache, and atomic publication of metadata references. It implements table's metadata contract, using `io` for transport and storage and `format` for payload codecs. The future outbound Iceberg REST adapter lives here.
- **`exec`** owns physical operators, bound expressions, the local runner, and the function registry for signatures and kernels. Scan and write operators use table contracts and call `format` and `io`.
- **`sql`** owns parsing, SQL type and coercion rules, binding, logical planning, optimization, and physical lowering. It binds against the `exec` function registry and binds procedure calls to registered callables.
- **`maintain`** implements maintenance procedures and owns workflows such as compaction, snapshot expiration, and orphan cleanup. It shares column batches, operators, and commit contracts with ordinary SQL.
- **`engine`** owns internal Engine, Session, and Query objects, configuration, cancellation, resource limits, and service wiring. It creates I/O and catalog instances, connects catalog publication to table, registers procedures, and passes one pool per query to components performing query-accounted allocations. It uses column’s exporter to return query results as an `ArrowArrayStream`.
- **`api`** owns the thin C++ embedding façade over `engine` and the external error type. It consumes the result C ABI through `engine` without including `column` headers.
- **`server`** owns network listeners, client sessions, routing, and wire translation. Its active edges are to `base` for wire error mapping and `engine` for shared lifecycle, SQL submission, and results. It consumes the result C ABI through `engine` without including `column` headers. Future inbound Iceberg REST routes would require a separately approved direct edge to `catalog` for catalog operations.
- **`cli`** owns commands, flags, interactive input, output formatting, and exit codes. It uses `api` for SQL and administrative commands.
- **`dist` (deferred)** owns coordinator and worker orchestration, physical plan fragments, exchange transport, and distributed failure handling.

### Layers and direct dependencies (criteria 1 and 4)

Every edge points to a smaller layer number. Declare an edge for direct use, including direct inclusion of a module-public header. Transitive CMake visibility grants no direct access.

| Layer | Module | Allowed direct dependencies |
|---|---|---|
| 0 · foundation | `base` | — |
| 1 · primitives | `column` | `base` |
| 1 · primitives | `io` | `base` |
| 2 · codecs | `format` | `base`, `column`, `io` |
| 3 · tables | `table` | `base`, `column`, `io`, `format` |
| 4 · services | `catalog` | `base`, `io`, `format`, `table` |
| 4 · services | `exec` | `base`, `column`, `io`, `format`, `table` |
| 5 · workflows | `sql` | `base`, `column`, `table`, `catalog`, `exec` |
| 5 · workflows | `maintain` | `base`, `column`, `io`, `table`, `catalog`, `exec` |
| 5 · workflows | `dist` (deferred) | `base`, `column`, `io`, `table`, `exec` (expected) |
| 6 · composition | `engine` | `base`, `column`, `io`, `format`, `table`, `catalog`, `exec`, `sql`, `maintain`. Add `dist` when activated |
| 7 · embedding | `api` | `base`, `engine` |
| 8 · delivery | `server` | `base`, `engine` |
| 8 · delivery | `cli` | `api` |

A lower consumer defines the interface it needs using types it can see. `engine` injects the `catalog` implementation of the `table` publication contract. `exec` declares the procedure contract as a callable signature built only from types below `sql`. `maintain` implements it without including `sql` headers. `engine` registers the callables with `sql`, which binds procedure calls to them. This keeps `sql` and `maintain` at layer 5 with no edge between them. SQL, CLI, and embedder calls such as `CALL system.rewrite_data_files(...)` use this registration.

### Iceberg scan, write, and commit (criteria 1 and 2)

`table` plans tasks with a data file, applicable position- and equality-delete files, a residual filter, and a field-ID projection map including hidden read fields. It determines applicability from Iceberg schema, partition, and sequence-number rules and specifies delete-key comparison semantics. `exec` reads bytes through `io` and decodes them through `format`. It applies position deletes by exact match to the recorded data-file path (`file_path`) and by original row position, and applies equality deletes using its hash join as an anti-join with Iceberg key semantics, including null matching null. It evaluates the residual and projects the output. For writes, `exec` encodes and writes batches through `format` and `io`, then returns data-file metadata. The `engine`/`sql` DML path and `maintain` procedures drive their commits. `table` prepares the update and performs the commit protocol through the injected catalog contract.

For `SELECT city FROM customers WHERE spend > 100` on an Iceberg v2 table, `sql` binds the query. `table` plans a task mapping field 7 (`city`) to output and fields 9 (`spend`) and 2 (`customer_id`) to temporary inputs. It supplies `spend > 100` as the residual and selects an applicable position-delete file naming row 0 of the data file and an applicable equality-delete file with field 2 equal to 42. The files share a partition, and their data sequence numbers are 10 for the data file and 11 for both delete files. `exec` reads the three files through `io` and `format`. Given original rows `(Oslo, 150, 11)`, `(Rome, 200, 42)`, `(Lima, 90, 8)`, and `(Accra, 120, 5)`, position filtering removes Oslo, the hash anti-join removes Rome, and the residual removes Lima. The batch projects only `city#7` and returns Accra.

The commit contract reports **committed**, **definitely rejected**, or **outcome unknown**. Any recognised commit error other than `CommitStateUnknown` means the attempt published nothing and its written files may be cleaned up. An unrecognised future commit error is treated as `CommitStateUnknown`. A confirmed retryable conflict is `CommitConflict` and requires refreshing and revalidating before retry. If publication may have taken effect, return `CommitStateUnknown`, including after a timeout or cancellation with indeterminate publication. Reconcile against authoritative catalog state before any retry. Never clean up written files as failure handling for an unknown outcome. If reconciliation is inconclusive, the outcome remains unknown and the files stay protected. [Iceberg sources](refs/sources.md#iceberg)

### Layout and F02.2 enforcement (criteria 1 and 3)

Each module uses namespace `doqe::<module>` and one CMake `STATIC` target named `doqe_<module>`. Sources and private headers live under `src/<module>/`. Module-public headers live under `src/<module>/include/doqe/<module>/` and are included as `#include "doqe/<module>/x.hpp"`. Include a private header by its local path within its owning module. Keep the CMake source include root private and propagate only the module-public include root. Module-public headers may use the owning module's public headers and those of its declared public dependencies. Module-public visibility is for repository consumers. The embedding section defines the external contract.

F02.2 creates each active archive with a placeholder source until real sources land. Check direct CMake production links against the allowed edges using target properties or the File API. Cross-module link declarations use plain target names in `target_link_libraries`, with `PUBLIC` for public-header requirements and `PRIVATE` for implementation dependencies. Reject links hidden in expressions, aliases, or raw archive paths. Count declared public links as direct edges and allow legitimate propagation.

Check each include directive at the file that declares it. The spelling `"doqe/<m>/..."` is legal only for the owning module or a declared direct dependency and must resolve to a public header. Cross-module includes must use that spelling. Relative includes stay within the owning module. Backstop the source check with real compiler dependencies, such as `ninja -t deps`, to catch foreign private headers, unexpected owners, and project headers outside the allowed transitive closure. The backstop permits legitimate public transitive dependencies. Seed illegal link and include cases, including a direct include made reachable by a transitive include path. In a positive A → B → C fixture, A can include B's public header when that header includes C. A's direct include of C must fail.

Keep module tests under `tests/<module>/` and cross-module tests under `tests/integration/`. A module test may use its own module's private headers through a test-only include path. Integration tests use only module-public headers and explicitly link the modules under test. Put benchmarks under `benchmarks/<module>/` or `benchmarks/integration/`. Use module-public headers for end-to-end measurements and allow a module microbenchmark to use its own private headers. Repo tools live under `tools/` when needed and use module-public headers and explicit target links. Shared fixtures and fakes belong in `tests/support/`, with a test-only `doqe_test_support` library if there is compiled code to share. Support code uses module-public interfaces and may also serve benchmarks. Production targets cannot depend on tests, test support, benchmarks, or tools.

### Cross-module conventions (criteria 1 and 3)

- Mark fallible boundary functions `[[nodiscard]]`. Return `std::expected<T, base::Status>`, with `T = void` for operations with no success value. `Status` owns its diagnostic and optional operation context. Required codes are `Cancelled`, `DeadlineExceeded`, `ResourceExhausted` (budget exceeded), `OutOfMemory` (constructible without allocating), `CommitConflict` (retryable), `CommitStateUnknown` (no cleanup), `NotFound`, `AlreadyExists`, `InvalidArgument`, `Unsupported`, `IoError`, and `Internal`. Callers must handle unknown future codes conservatively, without assuming retry or cleanup is safe. For a commit, treat an unrecognised code as `CommitStateUnknown`. Every other recognised commit error permits cleanup because nothing was published.
- Exceptions and RTTI stay enabled under ADR-0001. Exceptions stop at the module where they originate, including in callbacks and virtual implementations invoked by another module. Translate internal or third-party exceptions into `Status` when the module can return safely. Use an internal error code for unexpected failures that can be contained. Treat return construction and callback adapters as part of the boundary and keep destruction non-throwing. Every callback supplied across a boundary follows the same error contract. F02.2 may use `noexcept` and clang-tidy’s `bugprone-exception-escape` where the boundary can honor that guarantee.
- Pass small values by value, use RAII owners for transfers, and use borrowed references or views with a documented lifetime and invalidation rule. A callee may retain a borrow only with an explicit lifetime contract. Prefer unique ownership, and use shared ownership where consumers genuinely share a lifetime. A result published for read-only consumption keeps its backing buffers alive for the promised lifetime.
- Keep mutable state in explicitly owned objects, such as engine, catalog, session, and query instances. Pass services and configuration at construction or invocation. Registries, caches, sinks, and configuration are instance-owned. Pass instances explicitly, including the query pool and cancellation token.

### In-memory column format (criteria 2 and 3)

`column` owns native Arrow-compatible buffers, validity bitmaps, and offsets where the corresponding layout requires them. Batch schemas carry field IDs and preserve Iceberg identity across rename and reordering. The engine type model must be able to represent Iceberg v3 types such as `timestamp_ns`, `variant`, and `geometry`. For Iceberg v3 deletion vectors stored in Puffin, `format` decodes the vectors and `exec` applies them. Deletion-vector details, type coverage, layouts, encodings, alignment, and slicing belong to later work. Build and run without an Apache Arrow library dependency. [Arrow specifications](refs/sources.md#column-layout-and-build-rules), [Iceberg specification](refs/sources.md#iceberg)

### Embedding, delivery, and future composition (criteria 3 and 4)

- **No API stability before 1.0.** Embedders build with the same pinned toolchain and static libc++ linkage as doqe under ADR-0001. Designated `api` headers expose façade configuration and handles and an `api`-owned error type mapped from `base::Status`. Internal error codes can evolve independently while the mapping preserves distinctions such as commit conflict and uncertain publication. The direct `base` edge supports that mapping.
- `engine` exposes `ArrowArrayStream` to `api` and `server` through its public result header, which includes the ABI header under the public transitive include rule above. Release callbacks preserve buffer lifetimes. This ADR approves placing a verbatim copy of the official Apache Arrow `arrow/c/abi.h` (Apache-2.0) in `column` when implementing export. The specification supports copying the ABI declarations. [Arrow sources](refs/sources.md#column-layout-and-build-rules) Plan one CMake package for consumers, with an imported façade target carrying the required static transitive links.
- `cli` owns the command-line contract and `server` owns the wire contract. Both use the same engine service. Future Iceberg REST uses `server` for inbound HTTP translation and `catalog` for catalog semantics and the outbound adapter.
- When distribution is in scope, `engine` selects a coordinator through the execution runner contract. Expected `dist` dependencies include `table` for scan tasks and `io` for transport, or a lower-layer `net` module subject to a separate ADR. `exec` owns the runner contract and physical plan used by SQL and workers. Fragment and runner seams land with distributed work.
- Defer access policy, audit, and lineage to the **Governance and authorization ADR**. Its authorization point will be shared by SQL, server, and maintenance.

## Consequences

- Easier: CMake and include checks can reject forbidden edges. Query execution and maintenance reuse the same table and column contracts.
- Harder / cost we accept: Public module headers and error translation need care at each boundary. Embedding requires a deliberate façade. Static linkage and the checks need verification on both target hosts during F02.2.
- Follow-up work: F02.2 creates the active module targets and checks. The distribution target and its incoming edge wait for distributed work. Obtain owner approval before creating the planned new top-level folders `benchmarks/` and `tools/`. The following ADRs are all **not yet created**:
  - Engine concurrency model
  - Server wire protocol
  - JSON implementation choice
  - Avro implementation choice
  - Parquet implementation choice
  - Iceberg library strategy
  - In-memory column format details
  - Governance and authorization
  - Distributed execution and transport

## Revisit if

A real single-node feature repeatedly needs forbidden edges, a measured hot path requires copying at a module boundary, or the distributed runner or supported embedding API cannot use these contracts without exposing lower-module implementation types.
