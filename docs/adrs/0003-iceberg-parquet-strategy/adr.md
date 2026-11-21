# ADR-0003: Use in-house readers for the first Iceberg scans

| Status | Date | Decided by | Confidence | Tags |
|---|---|---|---|---|
| accepted | 2026-11-21 | owner | medium | Iceberg, Parquet, dependencies |

Supersedes: none · Superseded by: none · Related: [ADR-0001](../0001-core-cpp-toolchain/adr.md), [ADR-0002](../0002-architecture-modules/adr.md)

## Context

[M03](../../road/M03-first-iceberg-read.md) needs to scan a local Iceberg v2 table with nullable int64
and string columns. It reads the current snapshot, its manifest list and manifests, and the live
Parquet data files. The scan is single-threaded and does no partition pruning. Writes, delete-file
application, REST catalogs and additional codecs are later work. Neither the Iceberg version nor the
two supported types determine the Parquet page version, encoding, or compression. M03 needs an
explicit fixture and reader coverage contract.

## Criteria

1. Preserve doqe-owned columns, field-ID projection, byte I/O, and table planning within the
   accepted module graph. Build and run without an Apache Arrow library dependency.
2. Build pinned source with upstream Clang 23.1.5 and its static libc++ and libc++abi, under the
   C++23 library allowlist, with CMake/Ninja and no production package manager. Support macOS 26
   arm64 and Debian 13 x86_64 with their existing build and sanitizer workflow.
3. Reach M03 with externally generated fixtures and explicit errors for unsupported input. Prefer
   the option with the smallest implementation and ongoing dependency burden that meets 1 and 2.
4. Assess maturity and maintenance ownership, including the contributor base, release
   compatibility, correctness and memory-safety history, fuzz coverage, and patch upkeep.
5. Keep later Iceberg v3 adoption and Parquet coverage independently testable. Track
   provenance, notices and upgrades for third-party code.

## Options

### A. Use Apache iceberg-cpp with its Avro and Parquet bundle

- **Good.** The upstream bundle has concrete Avro and Parquet codecs, local file I/O, snapshot
  planning and an Arrow C stream over a scan task. It offers the shortest route to a standalone
  M03-shaped scan and upstream maintenance of much of the Iceberg and Parquet feature surface.
  Release 0.4.0 builds on v2 delete-aware planning, scan and write support. Upstream it
  adds applying defaults when reading missing Avro fields, Puffin deletion-vector blob reads
  and writes, and row-lineage metadata reads (upstream features; the spike exercised only a v2
  scan). Full v3 semantics need
  feature-by-feature verification. [iceberg-cpp](refs/sources.md#iceberg-cpp)
- **Bad.** The production bundle links Arrow C++ and crosses doqe's `table`, `catalog`, `format`
  and `io` responsibilities if used end to end. A wrapper could keep doqe column types, translate
  scan tasks at `table`, and adapt `io` and decode callbacks, but the Arrow link still requires a
  successor to ADR-0002. The core uses `std::views::zip` and `std::ranges::to`, both outside
  ADR-0001's allowlist. An upstream source build is possible with CMake FetchContent, but approval
  would also need to address that allowlist and the bundled Thrift build's CMake floor.
- **Maturity and maintenance.** The Apache project has a multi-contributor release history.
  The bundle shifts much format maintenance upstream and leaves doqe responsible for the
  adapter and dependency upgrades.
- **Dependencies.** Iceberg, Arrow/Parquet, nanoarrow, Avro C++, and Thrift are Apache-2.0.
  The local profile also brings Boost (BSL-1.0), CRoaring (Apache-2.0 or MIT), utf8proc (MIT
  with Unicode data terms), nlohmann/json and RapidJSON (MIT), and SDK zlib (zlib licence). Arrow's own tree
  contains separately noticed code. Avro discovers optional Snappy (BSD-3-Clause) and zstd
  (BSD-3-Clause or GPL-2.0); support depends on the build. REST/Hive, cloud SDKs and their
  dependency trees are unnecessary for M03. The Mac profile leaves Snappy and zstd disabled.
  Debian provenance, link and sanitizer checks would still be needed. [Dependency sources](refs/sources.md)

### B. Implement Iceberg metadata and Parquet in-house

- **Good.** Use the existing toolchain, library subset and module graph directly on both targets.
  For a fixture profile using Avro `null` blocks and uncompressed Parquet, no runtime Thrift,
  Avro, zstd, Snappy or JSON library is required. Doqe can implement the required Thrift Compact
  Protocol subset without a Thrift compiler and limit its JSON and Avro readers to Iceberg
  metadata and manifests. [Format specifications](refs/sources.md#formats)
- **Bad.** Highest M03 implementation burden. In addition to metadata planning and Avro object
  container handling, `format` must validate and decode Parquet metadata and page headers, map
  field IDs, handle definition levels, bound lengths and offsets, and distinguish empty strings from
  null. Doqe then owns each new page version, encoding, logical type and nesting case as well as the
  Iceberg v3 changes. Snappy or zstd would need pinned source dependencies and license/notice review
  when their input profiles are enabled. Copied spec schemas or code retain their upstream terms.
- **Maturity and maintenance.** The reader starts without a production track record. Doqe
  owns conformance, fixes and fuzz coverage from the first release and has no third-party
  Parquet build or patch queue to maintain.

### C. Own Iceberg metadata and use a third-party Parquet reader

**C1. Arrow's parquet-cpp**

- **Good.** The low-level typed column API exposes definition levels and Parquet field IDs.
  Doqe can own the scan plan, feed bytes from `io`, materialize into its own batches in `format`,
  and translate library failures at that boundary. Arrow maintains Parquet encodings and page
  support while doqe tracks Iceberg v3. The Mac static build uses pinned source and C++20-capable
  Arrow APIs. Neither a subprocess nor a production package manager is required.
- **Bad.** Even the typed reader links Arrow C++ and enables IPC under this pin, so it needs a
  successor to ADR-0002. Bundled Thrift checks for CMake 3.26 or later. This exceeds the accepted
  CMake 3.25 floor even though the Mac spike uses a newer installation. Debian 13 fit and a
  complete dependency conformance check remain open.
- **Maturity and maintenance.** Apache Arrow maintains broad Parquet coverage and a regular
  release history. Doqe would maintain the adapter, validate upgrades and track bundled
  dependencies.
- **Dependencies.** Arrow/Parquet and Thrift (Apache-2.0) plus Boost (BSL-1.0), simdjson
  (Apache-2.0 or MIT), xsimd and uriparser (BSD-3-Clause), and zlib. Snappy and zstd are optional
  source-built Parquet codecs. Arrow contains its own separately noticed
  vendor code. Any JSON and Avro codecs for Iceberg metadata are independent of Arrow.
  [Arrow build](refs/sources.md#arrow)

**C2. Carquet**

- **Good.** A C reader with an internal Thrift Compact Protocol decoder, a built-in Snappy codec,
  and a direct buffer input. `format` can consume bytes obtained by `io` and fill doqe-owned
  columns via the C API. The C sources have no C++23 library requirement. The spike reads nullable
  int64 and UTF-8 strings with PLAIN and dictionary values, V1 and V2 pages, and three compression
  profiles. Pinned static source builds through its CMake targets without Arrow or a package
  manager. [Carquet](refs/sources.md#carquet), [Mac spike](refs/mac-spike.txt)
- **Bad.** This pin decodes Parquet field IDs internally and exposes column names and types in the
  public API. It provides no public leaf field-ID accessor. An upstreamable accessor over the
  existing decoded schema and rename/reorder tests would be needed. The allocator setter
  is process-global, and library paths including Arrow export and gzip scratch use raw
  `malloc`/`calloc` outside that allocator. Carquet also has its own thread pool. Integration
  would need to establish query memory accounting and a single-threaded path. The buffer
  entry point accepts a whole file. Range-based integration would need more work.
- **Maturity and maintenance.** The repository was created on 2025-12-29. At the spike pin,
  one author accounted for roughly 160 of 170 commits (157 of 165 by GitHub account). The
  project is on 0.x releases with breaking changes in minor versions. The v0.8.0 changelog
  reports fixes for 11 bugs that silently returned wrong results or wrote corrupt files and
  seven memory-safety bugs. It ships fuzz targets, including instrumented reader targets.
  Doqe would review local patches and check applicable fixes and compatibility on upgrades.
  [Carquet history and source](refs/sources.md#carquet)
- **Dependencies.** Carquet and its internal Thrift decoder (MIT), built-in Snappy (BSD-3-Clause
  provenance), zstd (select BSD-3-Clause), zlib (zlib licence), LZ4 (BSD-2-Clause for the library).
  Brotli (MIT) and OpenMP are disabled in the spike. Standalone JSON and Avro codecs would be
  separate `format` dependencies. [Carquet and codec licences](refs/sources.md#carquet)

### D. Use iceberg-cpp core for planning and supply independent codecs

- **Good.** The upstream core separates metadata and planning from the Arrow C++ bundle. It can
  reuse upstream Iceberg rules while `table` adapts plan results, `catalog` supplies the local
  metadata reference, and `format` and `io` supply reader and byte adapters. Carquet or a
  doqe-owned reader could then serve `exec`. This has value if broad Iceberg semantics become
  more urgent than the first scan. [Library boundaries](refs/sources.md#iceberg-cpp)
- **Bad.** Core still uses Apache nanoarrow and non-allowlisted C++23 library facilities. Production
  would need an explicit change to the accepted dependency and allowlist decisions, or a maintained
  patch removing those dependencies and usages. It supplies registration interfaces for Avro
  manifests, leaving an adapter to write and test. This adds an upstream table model and I/O
  contract to bridge while retaining both codec decisions. Updating upstream still requires
  testing the translated semantics and each Parquet feature independently.
- **Maturity and maintenance.** The same upstream contributors maintain core and bundle.
  Doqe benefits from upstream Iceberg fixes and owns the adapters, independent codecs and
  any toolchain patches.
- **Dependencies.** Core source uses nanoarrow and CRoaring (Apache-2.0 or MIT), nlohmann/json and
  utf8proc (MIT, plus Unicode data terms) and zlib. Its independent Avro and Parquet choices
  determine Thrift, Snappy, and zstd requirements. The bundle build exercises the core on the Mac.
  A standalone core plus doqe adapters and a Debian static build were outside the spike.

### E. Copy and maintain a trimmed Carquet reader

- Reuse the MIT-licensed field and page decoders, retain notices, and remove unused codecs and
  paths. Expose field IDs, route allocations through the query pool, and retain a single-threaded
  read path.
- **Maturity and maintenance.** Start with C2's upstream history and take ownership of the
  fork's correctness, fuzzing and backports. Trimming and reviewing upstream fixes against
  the fork add work to the M03 subset. [Carquet](refs/sources.md#carquet)

### Other alternatives

- **DuckDB Parquet reader.** Actively maintained as a DuckDB extension that depends on
  DuckDB internals. Extracting it leaves doqe maintaining a port and tracking those
  internals. [Other reader sources](refs/sources.md#other-readers)
- **Rust `parquet` or `iceberg-rust` via a C ABI.** The Apache projects offer maintained
  implementations. A C ABI still needs a Rust/Cargo build outside ADR-0001's toolchain,
  plus an adapter for allocation, lifetimes and statuses. [Other reader sources](refs/sources.md#other-readers)
- **Avro C or Avro C++ for manifests.** Apache Avro maintains container readers and schema
  resolution in both libraries. Avro C adds Jansson and the spike's Avro C++ uses zlib.
  Either adds source and build maintenance while leaving Iceberg planning and Parquet
  reading to doqe. [Other reader sources](refs/sources.md#other-readers)
- **Miniparquet.** A C++ reader for a subset of Parquet, archived since 2020. Doqe would
  own compatibility, fixes and fuzzing. [Other reader sources](refs/sources.md#other-readers)

## References

- **Measured.** The [Mac spike summary](refs/mac-spike.txt) records Release build observations,
  sizes and smoke checks on macOS 26 arm64 at `a54308f1a5dd0459efd30146291918c907d5fe96`.
  The candidates use different codec closures, workloads and optimization settings.
  Reproduce with `sh docs/adrs/0003-iceberg-parquet-strategy/refs/mac-spike.sh`.
  [Script](refs/mac-spike.sh)
- **Source.** The [primary sources](refs/sources.md) cover wire formats, licences, source
  versions, library boundaries, and fixture generation.
- **Judgment.** Estimate two to four engineer-weeks for B's M03 Parquet subset, including
  Compact Protocol and page decoding, field-ID projection, query-memory integration,
  differential tests and fuzz targets. The estimate assumes one engineer and excludes
  JSON, Avro, Iceberg scan planning, and compression. The spike measured builds of
  existing libraries.

## Decision

Choose **B** for M03 and implement the Iceberg metadata and Parquet readers in-house. This
preserves the module boundaries and lets doqe enforce field-ID projection and query memory
accounting at the decoder. Carquet remains a spike candidate only. ADR-0002's no-Arrow rule
excludes A and C1 independently of the C++23 library allowlist and bundled Thrift's CMake
3.26 requirement. The ADR remains proposed pending owner acceptance.

- **Entry point and paths.** `doqe scan <metadata.json>` takes the metadata file. The local
  catalog implements the table-owned metadata contract. Tests configure `io` to map the
  original warehouse prefix to the checked-in fixture folder across all metadata and data
  reads. Preserve the fixture files byte for byte.
- **Iceberg planning.** `table` owns v2 format checks, current snapshot and schema resolution,
  manifest content and entry status, and field-ID projection. Ignore deleted data-file
  entries and plan live ADDED and EXISTING entries. Return zero rows for a table without
  a current snapshot. Reject delete manifests/files, unsupported formats and malformed input
  through the status contract.
- **JSON and Avro.** In `format`, write small in-house readers for the JSON fields and Avro
  container and schema operations needed by Iceberg `metadata.json`, manifest lists, and
  manifests. Preserve exact 64-bit JSON integers. Read the embedded Avro writer schema and
  skip unused fields by their schema. Use no third-party JSON or Avro code. This narrows
  ADR-0002's deferred JSON and Avro implementation choices for M03. Open separate ADRs
  only if implementing these subsets proves too costly.
- **Parquet.** In `format`, decode the footer and page headers with a Thrift Compact Protocol
  decoder. Read flat schemas with INT64 and UTF-8 strings using V1 and V2 data pages,
  RLE/bit-packed definition levels, and PLAIN and dictionary-encoded values. Match columns
  solely by Iceberg field ID. A Parquet column without a `field_id` returns `Unsupported`;
  name mapping is outside the M03 reader contract. When every column carries an ID and an
  optional schema field has no matching column (for example, the file predates adding that
  column), project the field as all nulls, per Iceberg column projection. A missing required
  field returns `Unsupported`, since initial defaults (v3) are out of M03. `exec` emits doqe-owned batches single-threaded.
- **Memory and correctness.** Route all read allocations through the query memory budget,
  including input, scratch, dictionary and output buffers. Return `ResourceExhausted` when
  the budget is exceeded and `OutOfMemory` when the allocator fails. Report reader errors
  as `base::Status`. Run offline differential checks against PyArrow output and use libFuzzer
  from the pinned LLVM toolchain on the Thrift and page decoders.
- **Compression.** M03 supports uncompressed Parquet and uncompressed Avro (`null` codec).
  PyIceberg and Iceberg Java default to zstd Parquet and gzip Avro, so real-world tables
  written with those defaults need the compression follow-up. Introduce zlib, zstd and
  Snappy via a dedicated compression ADR and build them from pinned upstream source.
  [Writer defaults](refs/sources.md#fixtures)

### F03.6 fixtures

Generate Iceberg v2 fixtures offline with PyIceberg and PyArrow, using a local SQLite catalog
and a hash-locked requirements file that pins all Python dependencies. Record the generator
commands and independently specified expected rows. Check in the generated bytes for direct
use by CTest. Cover uncompressed PLAIN and dictionary values across V1 and V2 pages, nulls,
empty strings, non-ASCII UTF-8, and multiple data files. Include negative cases for a Parquet column
without a field ID (expect `Unsupported`), delete metadata and unsupported compression, and a
positive case for a fully ID-tagged file written before an optional column was added (expect
that column as nulls).

Write a field-ID fixture with two same-typed columns, distinct values, and explicit Parquet
field IDs. Swap their names relative to their Iceberg field IDs and reverse the physical
field-ID order relative to the table schema so both name and ordinal matching return wrong
rows. Disable column metrics for this fixture so the import does not infer them from the
swapped names. PyIceberg's `add_files` sets `schema.name-mapping.default` automatically.
Remove it through a PyIceberg metadata transaction after import, assert its absence in the
generated current metadata, and verify PyIceberg still scans the expected rows. Run the relocation
test from a second directory with the original warehouse unavailable, using the `io` prefix
mapping against the byte-identical checked-in fixture. After the compression follow-up,
add a positive fixture made by PyIceberg's own append with default writer properties.
[Fixture sources](refs/sources.md#fixtures)

## Consequences

- Normal builds use the existing toolchain and avoid third-party format readers. PyIceberg
  and PyArrow serve as offline fixture writers and correctness oracles.
- Doqe owns decoder conformance, allocation and input validation, and coverage for each
  additional encoding, logical type or nesting case. Iceberg v3 features such as row lineage,
  defaults and Puffin deletion vectors remain later work.
- F03.2–F03.5 implement the bounded read path. F03.6 supplies the fixtures and relocation
  checks. Debug and ASan+UBSan verification for M03 remains due on both supported hosts.

## Revisit if

The bounded readers exceed the estimated effort, fail differential or sanitizer checks, or
an interoperability target needs a broader Parquet or Iceberg profile.
