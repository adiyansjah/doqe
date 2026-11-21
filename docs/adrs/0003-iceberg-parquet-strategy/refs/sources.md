# Sources for ADR-0003

Read 2026-11-21. Source links describe upstream behavior. [The Mac spike](mac-spike.txt) records
what ran locally.

## Formats

- [Iceberg table specification](https://iceberg.apache.org/spec/) defines metadata JSON, current
  snapshot selection, the v2 Avro manifest list and manifests, entry status, content, schema
  identity and Parquet field IDs. The v3 sections cover defaults, row lineage, nanosecond
  timestamps, variant and deletion vectors. [Puffin specification](https://iceberg.apache.org/puffin-spec/)
  describes the deletion-vector container.
- [Avro 1.12 specification](https://avro.apache.org/docs/1.12.0/specification/#object-container-files)
  defines the embedded writer schema, block and sync framing, codecs (including `null` and
  `deflate`), zigzag integers, unions and schema resolution. The M03 proposal supports a subset.
- [Parquet file layout](https://parquet.apache.org/docs/file-format/),
  [Thrift schema](https://github.com/apache/parquet-format/blob/master/src/main/thrift/parquet.thrift),
  [page encodings](https://parquet.apache.org/docs/file-format/data-pages/encodings/) and
  [compression](https://parquet.apache.org/docs/file-format/data-pages/compression/) define
  footer and page framing, field IDs, null levels, PLAIN and dictionary encodings and codecs.
- [RFC 8259](https://www.rfc-editor.org/rfc/rfc8259) describes JSON syntax and integer
  interoperability limits. Exact reading of Iceberg snapshot IDs must preserve 64-bit
  integer values that exceed binary64's consecutive integer range.

## iceberg-cpp

- [v0.4.0](https://github.com/apache/iceberg-cpp/releases/tag/v0.4.0) at
  `64dc66f159bd62c9817a96d74cbce767268934d0` is the spike pin. The
  [library design](https://github.com/apache/iceberg-cpp/blob/64dc66f159bd62c9817a96d74cbce767268934d0/mkdocs/docs/library.md)
  describes the core, data and bundle division and codec registration. The
  [source list](https://github.com/apache/iceberg-cpp/blob/64dc66f159bd62c9817a96d74cbce767268934d0/src/iceberg/CMakeLists.txt)
  shows the target composition. The [release history](https://github.com/apache/iceberg-cpp/blob/64dc66f159bd62c9817a96d74cbce767268934d0/mkdocs/docs/releases.md)
  records v2 delete-aware planning, scan and write support. The [0.4.0 release](https://github.com/apache/iceberg-cpp/releases/tag/v0.4.0) lists
  default application when reading missing Avro fields, Puffin deletion-vector blob reads and
  writes, and row-lineage metadata reads. The [sort-order implementation](https://github.com/apache/iceberg-cpp/blob/64dc66f159bd62c9817a96d74cbce767268934d0/src/iceberg/sort_order.cc)
  uses `std::views::zip` and `std::ranges::to` in core.
- [Dependencies and pins](https://github.com/apache/iceberg-cpp/blob/64dc66f159bd62c9817a96d74cbce767268934d0/cmake_modules/IcebergThirdpartyToolchain.cmake)
  configures Arrow 25.0.0, nanoarrow 0.9.0, utf8proc 2.10.0, CRoaring 4.4.3,
  nlohmann/json 3.11.3, and Avro C++ at `209a3735ec330679790824da54b7558db55e4e7f`.
  [Avro's CMake](https://github.com/apache/avro/blob/209a3735ec330679790824da54b7558db55e4e7f/lang/c++/CMakeLists.txt)
  requires zlib and discovers optional Snappy and zstd. The
  [Arrow 25 dependencies](https://github.com/apache/arrow/blob/apache-arrow-25.0.0/cpp/thirdparty/versions.txt)
  give the Thrift, Boost, xsimd and RapidJSON pins. Its [Boost setup](https://github.com/apache/arrow/blob/apache-arrow-25.0.0/cpp/cmake_modules/ThirdpartyToolchain.cmake)
  disables Boost.UUID's libatomic link when Arrow threading is disabled. The Mac spike
  uses the Boost option directly with Arrow threading enabled by iceberg-cpp.
- [Iceberg LICENSE and NOTICE](https://github.com/apache/iceberg-cpp/tree/v0.4.0),
  [Avro LICENSE](https://github.com/apache/avro/blob/209a3735ec330679790824da54b7558db55e4e7f/LICENSE.txt)
  and [nanoarrow LICENSE](https://github.com/apache/arrow-nanoarrow/blob/apache-arrow-nanoarrow-0.9.0/LICENSE.txt)
  identify Apache-2.0. [CRoaring LICENSE](https://github.com/RoaringBitmap/CRoaring/blob/v4.4.3/LICENSE)
  offers Apache-2.0 or MIT. [utf8proc terms](https://github.com/JuliaStrings/utf8proc/blob/v2.10.0/LICENSE.md)
  include MIT and Unicode data. [RapidJSON's licence](https://github.com/Tencent/rapidjson/blob/232389d4f1012dddec4ef84861face2d2ba85709/license.txt)
  identifies MIT and separately licensed tree components. The JSON-licensed `bin/jsonchecker`
  code is outside the reader profile. [nlohmann/json 3.11.3 licence](https://github.com/nlohmann/json/blob/v3.11.3/LICENSE.MIT)
  is MIT. Use the pinned LICENSE and NOTICE files for any redistributed files, including
  third-party portions of header distributions.

## Arrow

- [Arrow 26.0.1](https://github.com/apache/arrow/releases/tag/apache-arrow-26.0.1) at
  `9e5d6978e9acf372cc989aaaf94e863fe8582c90` is the standalone Parquet spike pin.
  [Build options](https://github.com/apache/arrow/blob/apache-arrow-26.0.1/cpp/cmake_modules/DefineOptions.cmake)
  make IPC a dependency of Parquet. The
  [Parquet target](https://github.com/apache/arrow/blob/apache-arrow-26.0.1/cpp/src/parquet/CMakeLists.txt)
  uses generated Thrift code and Arrow C++. The
  [schema API](https://github.com/apache/arrow/blob/apache-arrow-26.0.1/cpp/src/parquet/schema.h)
  exposes field IDs.
- [Bundled dependency logic](https://github.com/apache/arrow/blob/apache-arrow-26.0.1/cpp/cmake_modules/ThirdpartyToolchain.cmake)
  includes the explicit CMake 3.26 floor for bundled Thrift and the mandatory uriparser resolver.
  [Versions and hashes](https://github.com/apache/arrow/blob/apache-arrow-26.0.1/cpp/thirdparty/versions.txt)
  identify the source archives for the measured closure.
- [Arrow LICENSE and NOTICE](https://github.com/apache/arrow/tree/apache-arrow-26.0.1) track
  Apache-2.0 and packaged third-party portions. Licences for the source projects include
  [Thrift Apache-2.0](https://github.com/apache/thrift/blob/v0.24.0/LICENSE),
  [Boost BSL-1.0](https://www.boost.org/LICENSE_1_0.txt),
  [simdjson Apache-2.0 or MIT](https://github.com/simdjson/simdjson/tree/v4.6.4),
  [xsimd BSD-3-Clause](https://github.com/xtensor-stack/xsimd/blob/14.2.0/LICENSE) and
  [uriparser BSD-3-Clause](https://github.com/uriparser/uriparser/tree/uriparser-1.0.2)
  for its main library, with separately licensed build/test components.

## Carquet

- [Carquet v0.8.0 source](https://github.com/Vitruves/carquet/tree/337a532ecf549d600958b210fe76734deaef8daf)
  at `337a532ecf549d600958b210fe76734deaef8daf` is the spike pin.
  [Public declarations](https://github.com/Vitruves/carquet/blob/337a532ecf549d600958b210fe76734deaef8daf/include/carquet/carquet.h)
  describe the file, FILE and buffer APIs, column reads and process-global allocator.
  [Thrift decoder](https://github.com/Vitruves/carquet/blob/337a532ecf549d600958b210fe76734deaef8daf/src/thrift/parquet_types.c)
  deserializes the leaf `field_id` in the wire schema. The
  [reader schema builder](https://github.com/Vitruves/carquet/blob/337a532ecf549d600958b210fe76734deaef8daf/src/reader/file_reader.c)
  retains the decoded elements and indexes their leaves. The public header lacks a field-ID
  accessor. [Build](https://github.com/Vitruves/carquet/blob/337a532ecf549d600958b210fe76734deaef8daf/CMakeLists.txt)
  selects C sources, static archives, bundled compression, optional Brotli/OpenMP, and SIMD
  settings, including the Release C flags.
- [Repository metadata](https://api.github.com/repos/Vitruves/carquet) gives the creation date.
  Counting commits by GitHub author account in the [history through the spike pin](https://github.com/Vitruves/carquet/commits/337a532ecf549d600958b210fe76734deaef8daf/)
  yielded 157 of 165 attributed to one account. The [changelog at the pin](https://github.com/Vitruves/carquet/blob/337a532ecf549d600958b210fe76734deaef8daf/CHANGELOG.md)
  records breaking minor releases and the v0.8.0 correctness and memory-safety fixes.
  The [fuzz targets](https://github.com/Vitruves/carquet/tree/337a532ecf549d600958b210fe76734deaef8daf/fuzz)
  include reader targets. The [Arrow export reader](https://github.com/Vitruves/carquet/blob/337a532ecf549d600958b210fe76734deaef8daf/src/reader/arrow_c_read.c)
  and [gzip scratch code](https://github.com/Vitruves/carquet/blob/337a532ecf549d600958b210fe76734deaef8daf/src/compression/gzip.c)
  call raw allocators. The [batch reader](https://github.com/Vitruves/carquet/blob/337a532ecf549d600958b210fe76734deaef8daf/src/reader/batch_reader.c)
  exposes its own worker-pool API.
- [Carquet LICENSE](https://github.com/Vitruves/carquet/blob/337a532ecf549d600958b210fe76734deaef8daf/LICENSE) is MIT. Its
  [built-in Snappy source](https://github.com/Vitruves/carquet/blob/337a532ecf549d600958b210fe76734deaef8daf/src/compression/snappy.c)
  retains Google and contributor notices derived from the
  [Snappy BSD-3-Clause terms](https://github.com/google/snappy/blob/1.2.2/COPYING).
  The source build fetched [zstd BSD-3-Clause or GPL-2.0](https://github.com/facebook/zstd/tree/v1.5.6),
  [zlib licence](https://github.com/madler/zlib/blob/v1.4/LICENSE) and
  [LZ4 library BSD-2-Clause](https://github.com/lz4/lz4/blob/v1.10.0/lib/LICENSE).
  LZ4's CLI and other portions have separate terms. [Brotli MIT](https://github.com/google/brotli/blob/master/LICENSE)
  applies if that optional codec is enabled.

## Other readers

- [DuckDB's Parquet build](https://github.com/duckdb/duckdb/blob/main/extension/parquet/CMakeLists.txt)
  defines a DuckDB extension. The [reader header](https://github.com/duckdb/duckdb/blob/main/extension/parquet/include/parquet_reader.hpp)
  uses DuckDB file-system, vector and table-function types.
- [Apache Arrow Rust `parquet`](https://github.com/apache/arrow-rs/blob/main/parquet/README.md)
  documents the Rust toolchain and optional Arrow integration. [Apache Iceberg Rust](https://github.com/apache/iceberg-rust)
  supplies a Rust Iceberg implementation.
- [Avro C](https://avro.apache.org/docs/1.12.0/api/c/) documents object containers, schema
  resolution and the Jansson dependency. [Avro C++](https://avro.apache.org/docs/1.12.0/api/cpp/html/)
  documents its container reader and resolving decoder. The spike's Avro C++ source and
  zlib requirement are linked under [iceberg-cpp](#iceberg-cpp).
- [Miniparquet](https://github.com/hannes/miniparquet) describes the supported Parquet
  subset and shows the repository was archived on 2020-08-13.

## Fixtures

- [PyIceberg 0.12.0](https://github.com/apache/iceberg-python/releases/tag/pyiceberg-0.12.0),
  [local SQL catalog](https://py.iceberg.apache.org/configuration/#sql-catalog), and
  [add-files API](https://py.iceberg.apache.org/api/#add-files) cover offline table creation
  and importing files. The [pinned table source](https://github.com/apache/iceberg-python/blob/75396614a5f093cf24e93c7d916ee2cda0bdf44e/pyiceberg/table/__init__.py)
  shows that `add_files` adds a default name mapping if absent and that a transaction can
  remove it. The [PyArrow writer](https://arrow.apache.org/docs/python/generated/pyarrow.parquet.write_table.html)
  selects page version, compression, dictionary encoding and schema metadata handling.
- [PyIceberg table properties](https://github.com/apache/iceberg-python/blob/75396614a5f093cf24e93c7d916ee2cda0bdf44e/pyiceberg/table/__init__.py)
  and [Iceberg Java table properties](https://iceberg.apache.org/docs/latest/configuration/#write-properties)
  give the default `zstd` Parquet and `gzip` Avro settings. Iceberg uses the `gzip` setting
  to select the Avro `deflate` codec.
- [Spark/Iceberg getting started](https://iceberg.apache.org/docs/latest/spark-getting-started/)
  and [Spark writes](https://iceberg.apache.org/docs/latest/spark-writes/) cover a second
  offline generator with an independently implemented writer. Pin a compatible Spark and
  Iceberg runtime pair when the fixture is made.
