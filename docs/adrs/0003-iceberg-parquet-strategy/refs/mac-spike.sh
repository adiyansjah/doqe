#!/bin/sh
# Mac-only source build and fixture smoke test. No doqe build or installation.
# Run from the checkout. Optional arguments: carquet, arrow, iceberg, in any order.
set -eu
if [ "$(uname -s)" != Darwin ] || [ "$(uname -m)" != arm64 ]; then
  echo "This spike requires macOS arm64" >&2
  exit 2
fi
if [ "$#" -eq 0 ]; then set -- carquet arrow iceberg; fi
for mode in "$@"; do
  case "$mode" in
    carquet|arrow|iceberg) ;;
    *) echo "Unknown candidate: $mode" >&2; exit 2 ;;
  esac
done

unset CPATH C_INCLUDE_PATH CPLUS_INCLUDE_PATH OBJC_INCLUDE_PATH LIBRARY_PATH
unset CMAKE_PREFIX_PATH CMAKE_C_COMPILER_LAUNCHER CMAKE_CXX_COMPILER_LAUNCHER
unset CMAKE_C_LINKER_LAUNCHER CMAKE_CXX_LINKER_LAUNCHER
export GIT_TERMINAL_PROMPT=0
llvm=${DOQE_LLVM_ROOT:-"$HOME/opt/llvm-23.1.5"}
sdk=$(xcrun --show-sdk-path)
python=${SPIKE_PYTHON:-"$HOME/.asdf/installs/python/3.12.11/bin/python3"}

# Snapshot of the macOS arm64 pin in cmake/require-llvm.cmake at the spike base.
clang_sha256=a22ab66d53cf70dfee8544eabdeb36f075bf942861cb8d59f98a3fadc7a65c26
libcxx_sha256=bbab7514c4ead42c32e18ff18c1006c9d8696f968fa895a34b6d0a545d5e9726
libcxxabi_sha256=8c7604eda506797adfd63d6556e75990100e017baedb8460fa45b876e68fff1c
printf '%s  %s\n' \
  "$clang_sha256" "$llvm/bin/clang" \
  "$clang_sha256" "$llvm/bin/clang++" \
  "$libcxx_sha256" "$llvm/lib/libc++.a" \
  "$libcxxabi_sha256" "$llvm/lib/libc++abi.a" | shasum -a 256 --check

compiler=$("$llvm/bin/clang++" --version | sed -n '1p')
case "$compiler" in
  'clang version 23.1.5 (https://github.com/llvm/llvm-project 392dfff97d29b59d77e5cac6d4b6ef2585fe7117)') ;;
  *) echo "Expected upstream Clang 23.1.5, got $compiler" >&2; exit 2 ;;
esac
flags="-stdlib=libc++ -nostdinc++ -isystem $llvm/include/c++/v1 -Werror=unguarded-availability"
printf 'Date: %s\nMachine: %s (%s, %s)\n' "$(date '+%F %T %z')" "$(hostname)" \
  "$(sysctl -n hw.model)" "$(sysctl -n machdep.cpu.brand_string)"
sw_vers
printf 'Toolchain: %s\nSDK: %s\n' "$compiler" "$(xcrun --show-sdk-version)"
cmake --version | sed -n '1p'
ninja --version
"$python" --version
printf 'Build: Release arm64, macOS target 26.0, pinned static libc++/libc++abi, jobs=%s\n' "${SPIKE_JOBS:-4}"
printf 'doqe commit: %s\n' "$(git rev-parse HEAD)"

scratch=$(mktemp -d /tmp/doqe-t0006.XXXXXX)
printf 'Scratch and logs: %s\n' "$scratch"
cd "$scratch"
export TMPDIR="$PWD"
export PIP_CACHE_DIR="$PWD/pip-cache" PIP_DISABLE_PIP_VERSION_CHECK=1

fetch() {
  name=$1 repo=$2 revision=$3 digest=$4
  curl --fail --silent --show-error --location --retry 2 --max-time 120 \
    "https://codeload.github.com/$repo/tar.gz/$revision" -o "$name.tgz"
  printf '%s  %s.tgz\n' "$digest" "$name" | shasum -a 256 --check
  mkdir "$name"
  tar -xzf "$name.tgz" -C "$name" --strip-components=1
}

"$python" -m venv python
python/bin/python -m pip install 'pyarrow==26.0.1' 'pyiceberg[sql-sqlite]==0.12.0' > python.log 2>&1
cat > fixture.py <<'PYTHON'
from pathlib import Path

import pyarrow as pa
import pyarrow.parquet as pq
from pyiceberg.catalog.sql import SqlCatalog
from pyiceberg.schema import Schema
from pyiceberg.types import LongType, NestedField, StringType

root = Path.cwd()
warehouse = root / "warehouse"
warehouse.mkdir(exist_ok=True)
catalog = SqlCatalog("probe", uri=f"sqlite:///{root / 'catalog.db'}", warehouse=warehouse.as_uri())
catalog.create_namespace_if_not_exists("probe")
table = catalog.create_table(
    "probe.rows",
    Schema(NestedField(1, "id", LongType()), NestedField(2, "name", StringType())),
    properties={"format-version": "2", "write.avro.compression-codec": "null"},
)
rows = pa.table({
    "id": pa.array([42, None, -3, 0], type=pa.int64()),
    "name": pa.array(["hello", "", None, "é"], type=pa.string()),
}, schema=pa.schema([
    pa.field("id", pa.int64(), metadata={b"PARQUET:field_id": b"1"}),
    pa.field("name", pa.string(), metadata={b"PARQUET:field_id": b"2"}),
]))
for name, compression, dictionary, page in [
    ("plain", "NONE", False, "1.0"),
    ("snappy", "SNAPPY", True, "1.0"),
    ("zstd", "ZSTD", True, "2.0"),
]:
    pq.write_table(rows, f"{name}.parquet", compression=compression,
                   use_dictionary=dictionary, data_page_version=page)
data = warehouse / "probe" / "rows" / "data"
data.mkdir(parents=True, exist_ok=True)
path = data / "sample.parquet"
path.write_bytes((root / "plain.parquet").read_bytes())
table.add_files([path.as_uri()])
assert table.scan().to_arrow().to_pylist() == rows.to_pylist()
print(table.metadata_location)
assert table.metadata.format_version == 2 and not table.spec().fields
print(table.current_snapshot().manifest_list)
for manifest in table.current_snapshot().manifests(table.io):
    print(manifest.manifest_path)
PYTHON
python/bin/python fixture.py > fixture.log
printf '42\thello\nNULL\t\n-3\tNULL\n0\té\n' > expected.tsv

prepare_carquet() {
  fetch carquet Vitruves/carquet 337a532ecf549d600958b210fe76734deaef8daf \
    24be6e19c5d7da64f61fdb4579f7da6d781376f704d1608e1ff7766026ab96cd
  cat > carquet-probe.cc <<'CPP'
#include <carquet/carquet.h>

#include <cstdint>
#include <cstdio>

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    carquet_error_t error{};
    auto* reader = carquet_reader_open(argv[1], nullptr, &error);
    if (!reader) {
        std::fprintf(stderr, "%s\n", error.message);
        return 1;
    }

    auto* ids = carquet_reader_get_column(reader, 0, 0, &error);
    auto* names = carquet_reader_get_column(reader, 0, 1, &error);
    if (!ids || !names || carquet_reader_num_rows(reader) != 4) return 1;
    for (int row = 0; row < 4; ++row) {
        std::int64_t id{};
        carquet_byte_array_t name{};
        std::int16_t id_level{}, name_level{};
        if (carquet_column_read_batch(ids, &id, 1, &id_level, nullptr) != 1 ||
            carquet_column_read_batch(names, &name, 1, &name_level, nullptr) != 1) return 1;
        if (id_level == 1) std::printf("%lld", static_cast<long long>(id));
        else std::printf("NULL");
        std::printf("\t");
        if (name_level == 1 && name.length != 0)
            std::printf("%.*s", name.length, reinterpret_cast<const char*>(name.data));
        if (name_level == 0) std::printf("NULL");
        std::printf("\n");
    }

    carquet_column_reader_free(ids);
    carquet_column_reader_free(names);
    carquet_reader_close(reader);
}
CPP
  cat >> carquet/CMakeLists.txt <<'CMAKE'

enable_language(CXX)
add_executable(carquet-probe ../carquet-probe.cc)
target_link_libraries(carquet-probe PRIVATE carquet)
CMAKE
}

prepare_arrow() {
  fetch arrow apache/arrow 9e5d6978e9acf372cc989aaaf94e863fe8582c90 \
    75a2611d5cbf711adf408e3edc4050e1387a45c3a78443771de098599bd24743
  cat > arrow-probe.cc <<'CPP'
#include <parquet/column_reader.h>
#include <parquet/file_reader.h>

#include <cstdint>
#include <cstdio>

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    auto file = parquet::ParquetFileReader::OpenFile(argv[1]);
    auto metadata = file->metadata();
    if (metadata->num_rows() != 4 || metadata->num_row_groups() != 1 ||
        metadata->schema()->Column(0)->schema_node()->field_id() != 1 ||
        metadata->schema()->Column(1)->schema_node()->field_id() != 2) return 1;
    auto group = file->RowGroup(0);
    auto id_column = group->Column(0);
    auto name_column = group->Column(1);
    auto* ids = static_cast<parquet::Int64Reader*>(id_column.get());
    auto* names = static_cast<parquet::ByteArrayReader*>(name_column.get());
    for (int row = 0; row < 4; ++row) {
        std::int64_t id{}, id_count{}, name_count{};
        parquet::ByteArray name{};
        std::int16_t id_level{}, name_level{};
        if (ids->ReadBatch(1, &id_level, nullptr, &id, &id_count) != 1 ||
            names->ReadBatch(1, &name_level, nullptr, &name, &name_count) != 1 ||
            id_level != id_count || name_level != name_count) return 1;
        if (id_count == 1) std::printf("%lld", static_cast<long long>(id));
        else std::printf("NULL");
        std::printf("\t");
        if (name_count == 1 && name.len != 0)
            std::printf("%.*s", static_cast<int>(name.len), reinterpret_cast<const char*>(name.ptr));
        if (name_count == 0) std::printf("NULL");
        std::printf("\n");
    }
}
CPP
  cat >> arrow/cpp/CMakeLists.txt <<'CMAKE'

add_executable(arrow-probe ../../arrow-probe.cc)
target_include_directories(arrow-probe PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_BINARY_DIR}/src")
target_link_libraries(arrow-probe PRIVATE parquet_static arrow_static)
CMAKE
}

prepare_iceberg() {
  fetch iceberg apache/iceberg-cpp 64dc66f159bd62c9817a96d74cbce767268934d0 \
    3337d36edab645f9a036cf8f614bd112037e5c868082cdf8b8ae537ba3dd8f5e
  cat > iceberg-probe.cc <<'CPP'
#include "iceberg/arrow/arrow_register.h"
#include "iceberg/avro/avro_register.h"
#include "iceberg/data/file_scan_task_reader.h"
#include "iceberg/file_io.h"
#include "iceberg/file_io_registry.h"
#include "iceberg/parquet/parquet_register.h"
#include "iceberg/schema.h"
#include "iceberg/table.h"
#include "iceberg/table_metadata.h"
#include "iceberg/table_scan.h"

#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <memory>

template <typename T>
T take(iceberg::Result<T> result) {
    if (!result) {
        std::fprintf(stderr, "%s\n", result.error().message.c_str());
        std::exit(1);
    }
    return std::move(*result);
}

bool present(const ArrowArray& array, std::int64_t row) {
    auto* validity = static_cast<const std::uint8_t*>(array.buffers[0]);
    return !validity || ((validity[row / 8] >> (row % 8)) & 1) != 0;
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    iceberg::arrow::RegisterAll();
    iceberg::avro::RegisterAll();
    iceberg::parquet::RegisterAll();
    using Registry = iceberg::FileIORegistry;
    std::shared_ptr<iceberg::FileIO> io = take(Registry::Load(Registry::kArrowLocalFileIO, {}));
    auto metadata = take(iceberg::TableMetadataUtil::Read(*io, argv[1]));
    auto table = take(iceberg::StaticTable::Make({.name = "probe"}, std::move(metadata), argv[1], io));
    auto builder = take(table->NewScan());
    auto scan = take(builder->Build());
    auto tasks = take(scan->PlanFiles());
    if (tasks.size() != 1) return 1;

    auto schema = take(table->schema());
    auto reader = take(iceberg::FileScanTaskReader::Make({
        .io = io, .table_schema = schema, .projected_schema = schema,
    }));
    auto stream = take(reader->Open(*tasks.front()));
    std::int64_t rows = 0;
    for (;;) {
        ArrowArray batch{};
        if (stream.get_next(&stream, &batch) != 0) {
            std::fprintf(stderr, "%s\n", stream.get_last_error(&stream));
            return 1;
        }
        if (!batch.release) break;
        auto& ids = *batch.children[0];
        auto& names = *batch.children[1];
        auto* numbers = static_cast<const std::int64_t*>(ids.buffers[1]);
        auto* offsets = static_cast<const std::int32_t*>(names.buffers[1]);
        auto* chars = static_cast<const char*>(names.buffers[2]);
        for (std::int64_t row = 0; row < batch.length; ++row) {
            auto id = row + ids.offset;
            auto name = row + names.offset;
            if (present(ids, id)) std::printf("%lld", static_cast<long long>(numbers[id]));
            else std::printf("NULL");
            std::printf("\t");
            if (present(names, name)) {
                auto len = offsets[name + 1] - offsets[name];
                if (len != 0) std::printf("%.*s", len, chars + offsets[name]);
            } else {
                std::printf("NULL");
            }
            std::printf("\n");
        }
        rows += batch.length;
        batch.release(&batch);
    }
    stream.release(&stream);
    return rows == 4 ? 0 : 1;
}
CPP
  cat >> iceberg/CMakeLists.txt <<'CMAKE'

add_executable(iceberg-probe ../iceberg-probe.cc)
target_link_libraries(iceberg-probe PRIVATE iceberg_bundle_static)
CMAKE
}

build() {
  mode=$1
  unset ARROW_THRIFT_URL
  case "$mode" in
    arrow)
      src=arrow/cpp
      target=arrow-probe
      ARROW_THRIFT_URL=https://archive.apache.org/dist/thrift/0.24.0/thrift-0.24.0.tar.gz
      export ARROW_THRIFT_URL
      set -- -DARROW_PARQUET=ON -DARROW_BUILD_STATIC=ON -DARROW_BUILD_SHARED=OFF \
        -DARROW_DEPENDENCY_SOURCE=BUNDLED -DARROW_MIMALLOC=OFF -DARROW_WITH_BACKTRACE=OFF \
        -DARROW_SIMD_LEVEL=NONE -DARROW_RUNTIME_SIMD_LEVEL=NONE
      ;;
    iceberg)
      src=iceberg
      target=iceberg-probe
      # Arrow 25 otherwise asks Boost.UUID to link a libatomic absent on macOS.
      set -- -DICEBERG_BUILD_BUNDLE=ON -DICEBERG_BUILD_TESTS=OFF -DICEBERG_BUILD_REST=OFF \
        -DICEBERG_SPDLOG=OFF -DARROW_MIMALLOC=OFF -DARROW_WITH_BACKTRACE=OFF \
        -DBOOST_UUID_LINK_LIBATOMIC=OFF \
        -DCMAKE_DISABLE_FIND_PACKAGE_Snappy=ON
      ;;
    carquet)
      src=carquet
      target=carquet-probe
      set -- -DCARQUET_BUILD_CLI=OFF -DCARQUET_BUNDLE_DEPS=ON -DCARQUET_ENABLE_BROTLI=OFF \
        -DCARQUET_NATIVE_ARCH=OFF -DCMAKE_DISABLE_FIND_PACKAGE_OpenMP=ON
      ;;
  esac
  /usr/bin/time -p cmake -S "$src" -B "$mode-build" -G Ninja \
    -DCMAKE_INSTALL_PREFIX="$PWD/$mode-install" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_SYSROOT="$sdk" -DCMAKE_OSX_DEPLOYMENT_TARGET=26.0 -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_C_COMPILER="$llvm/bin/clang" -DCMAKE_CXX_COMPILER="$llvm/bin/clang++" \
    -DCMAKE_AR="$llvm/bin/llvm-ar" -DCMAKE_RANLIB="$llvm/bin/llvm-ranlib" \
    -DCMAKE_CXX_FLAGS="$flags" -DCMAKE_C_FLAGS=-Werror=unguarded-availability \
    -DCMAKE_CXX_STANDARD=23 -DCMAKE_CXX_EXTENSIONS=OFF -DCMAKE_CXX_SCAN_FOR_MODULES=OFF \
    -DCMAKE_CXX_STANDARD_LIBRARIES="$llvm/lib/libc++.a $llvm/lib/libc++abi.a" \
    -DCMAKE_EXE_LINKER_FLAGS="-nostdlib++ -Wl,-no_exported_symbols" \
    -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF \
    -DCMAKE_IGNORE_PREFIX_PATH="/opt/homebrew;/usr/local" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBUILD_SHARED_LIBS=OFF "$@" > "$mode-configure.log" 2>&1
  /usr/bin/time -p cmake --build "$mode-build" --parallel "${SPIKE_JOBS:-4}" --target "$target" > "$mode-build.log" 2>&1
}

summarize() {
  mode=$1 exe=$2
  printf '\n%s\n' "$mode"
  awk '/^real / { print FILENAME, $2, "seconds" }' "$mode-configure.log" "$mode-build.log"
  stat -f '%z bytes %N' "$exe"
  otool -L "$exe"
  printf 'Static link inputs (includes libc++ and libc++abi):\n'
  awk -v exe="$exe_name" '
    index($0, "build " exe ":") == 1 { link = 1; next }
    link && /^  LINK_LIBRARIES = / {
      for (i = 3; i <= NF; ++i) if ($i ~ /\.a$/) print $i
      exit
    }
  ' "$mode-build/build.ninja" | while IFS= read -r archive; do
    (cd "$mode-build" && stat -f '%z bytes %N' "$archive")
  done
  printf 'Fetched Git dependency revisions:\n'
  find "$mode-build" -type d -name .git -prune -exec sh -ec '
    for dot_git do
      source_dir=${dot_git%/.git}
      revision=$(git -C "$source_dir" rev-parse HEAD)
      printf "%s  %s\n" "$revision" "$source_dir"
    done
  ' sh {} +
  printf 'Downloaded dependency archives and SHA-256 digests:\n'
  find "$mode-build" -type f \( -name '*.tar.gz' -o -name '*.tar.xz' -o -name '*.tar.bz2' \) \
    -exec shasum -a 256 {} +
}

for candidate in "$@"; do
  "prepare_$candidate"
  build "$candidate"
  case "$candidate" in
    carquet)
      exe_name=carquet-probe
      exe=carquet-build/$exe_name
      for profile in plain snappy zstd; do
        "$exe" "$profile.parquet" > "$profile-carquet.tsv"
        diff -u expected.tsv "$profile-carquet.tsv"
      done
      ;;
    arrow)
      exe_name=release/arrow-probe
      exe=arrow-build/$exe_name
      "$exe" plain.parquet > plain-arrow.tsv
      diff -u expected.tsv plain-arrow.tsv
      ;;
    iceberg)
      exe_name=iceberg-probe
      exe=iceberg-build/$exe_name
      metadata=$(find "$PWD/warehouse" -name '00001-*.metadata.json')
      "$exe" "$metadata" > iceberg.tsv
      diff -u expected.tsv iceberg.tsv
      ;;
  esac
  summarize "$candidate" "$exe"
  printf 'Value checks passed for %s\n' "$candidate"
done
