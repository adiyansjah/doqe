#include "../support/fault_pool.hpp"
#include "../support/temp_file.hpp"
#include "doqe/base/memory_pool.hpp"
#include "doqe/base/status.hpp"
#include "doqe/column/column.hpp"
#include "doqe/io/file_system.hpp"
#include "doqe/table/metadata.hpp"
#include <cstddef>

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace {

// Tests use literal boundary values and index fixture spans after size assertions.
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

using doqe::base::Code;
using doqe::table::ReadMetadata;

constexpr std::string_view minimal =
    R"("format-version":2,"current-schema-id":0,"schemas":[{"type":"struct","schema-id":0,"fields":[]}],"partition-specs":[])";

constexpr std::string_view resolved = R"({
 "refs":{"main":{"snapshot-id":9007199254740993}},"current-snapshot-id":9007199254740993,
 "current-schema-id":1,"format-version":2,"table-uuid":false,"future-extension":{"x":1},
 "snapshots":[{"snapshot-id":-9223372036854775808,"schema-id":999},
 {"snapshot-id":7,"manifest-list":"file:/warehouse/other.avro"},
 {"snapshot-id":9007199254740993,"schema-id":0,"manifest-list":"file:/warehouse/list.avro"}],
 "schemas":[{"schema-id":0,"type":"struct","fields":[{"id":2,"name":"old","required":true,"type":"int"},{"id":2147483447,"name":"old-tail","required":true,"type":"string"}]},
 {"schema-id":1,"type":"struct","fields":[{"id":2147483447,"name":"tail","required":false,"type":"string"},
 {"id":2,"name":"renamed","required":true,"type":"long"}]}],
 "partition-specs":[{"spec-id":0,"fields":[{"source-id":2,"transform":"bucket[16]"},{"source-id":2147483447,"transform":"identity"}]},
 {"spec-id":1,"fields":[{"source-id":2,"transform":"identity"},{"source-id":2147483447,"transform":"truncate[4]"}]}]
})";

class MetadataTest : public testing::Test {
protected:
  void Check(std::string_view json, Code expected) {
    ASSERT_TRUE(file().Write(json));
    const auto pool = doqe::base::MakeFlatPool(std::size_t{16} * 1024U * 1024U);
    ASSERT_TRUE(pool);
    const auto result = ReadMetadata(file().path(), io(), *pool);
    ASSERT_FALSE(result) << json.substr(0, 300);
    EXPECT_EQ(result.error().code(), expected) << json.substr(0, 300);
    EXPECT_EQ((*pool)->live_bytes(), 0U);
  }

  void CheckResolvedEdit(std::string_view before, std::string_view after) {
    const auto pool = doqe::base::MakeFlatPool(65536);
    ASSERT_TRUE(pool);
    ASSERT_TRUE(file().Write(resolved));
    ASSERT_TRUE(ReadMetadata(file().path(), io(), *pool));

    std::string changed(resolved);
    const auto offset = changed.find(before);
    ASSERT_NE(offset, std::string::npos);
    changed.replace(offset, before.size(), after);
    Check(changed, Code::InvalidArgument);
  }

  [[nodiscard]] const doqe::test::TempFile &file() const noexcept { return file_; }
  [[nodiscard]] const doqe::io::LocalFileSystem &io() const noexcept { return io_; }

private:
  const doqe::test::TempFile file_;
  const doqe::io::LocalFileSystem io_;
};

TEST_F(MetadataTest, ResolvesCurrentSchemaSnapshotAndPerSpecIdentitySources) {
  ASSERT_TRUE(file().Write(resolved));
  const auto pool = doqe::base::MakeFlatPool(65536);
  ASSERT_TRUE(pool);
  const auto result = ReadMetadata(file().path(), io(), *pool);
  ASSERT_TRUE(result);
  ASSERT_EQ(result->fields().size(), 2U);
  EXPECT_EQ(result->current_schema_id(), 1);
  EXPECT_EQ(result->fields()[0].id, 2147483447);
  EXPECT_EQ(result->fields()[0].name, "tail");
  EXPECT_FALSE(result->fields()[0].required);
  EXPECT_EQ(result->fields()[0].type, doqe::column::Type::Utf8);
  EXPECT_EQ(result->fields()[1].id, 2);
  EXPECT_EQ(result->fields()[1].name, "renamed");
  EXPECT_TRUE(result->fields()[1].required);
  EXPECT_EQ(result->fields()[1].type, doqe::column::Type::Int64);
  const auto snapshot = result->current_snapshot();
  ASSERT_TRUE(snapshot);
  EXPECT_EQ(snapshot->id, 9007199254740993LL);
  EXPECT_EQ(snapshot->manifest_list, "file:/warehouse/list.avro");
  ASSERT_EQ(result->partition_specs().size(), 2U);
  EXPECT_EQ(result->partition_specs()[0].id, 0);
  EXPECT_EQ(result->partition_specs()[1].id, 1);
  ASSERT_EQ(result->partition_specs()[0].identity_source_ids.size(), 1U);
  EXPECT_EQ(result->partition_specs()[0].identity_source_ids[0], 2147483447);
  ASSERT_EQ(result->partition_specs()[1].identity_source_ids.size(), 1U);
  EXPECT_EQ(result->partition_specs()[1].identity_source_ids[0], 2);
}

TEST_F(MetadataTest, SnapshotSentinelsAndSelectedSnapshotRequirements) {
  const auto pool = doqe::base::MakeFlatPool(65536);
  ASSERT_TRUE(pool);
  const std::string prefix = "{" + std::string(minimal);
  for (const auto suffix :
       {"}", ",\"current-snapshot-id\":null}", ",\"current-snapshot-id\":-1}"}) {
    ASSERT_TRUE(file().Write(prefix + suffix));
    const auto result = ReadMetadata(file().path(), io(), *pool);
    ASSERT_TRUE(result);
    EXPECT_FALSE(result->current_snapshot());
  }
  ASSERT_TRUE(file().Write(
      prefix +
      R"(,"current-snapshot-id":9223372036854775807,"snapshots":[{"snapshot-id":9223372036854775807,"manifest-list":"file:/list"}]})"));
  const auto selected = ReadMetadata(file().path(), io(), *pool);
  ASSERT_TRUE(selected);
  ASSERT_TRUE(selected->current_snapshot());
  for (
      const auto suffix : {
          R"(,"current-snapshot-id":-1,"refs":{"main":{"snapshot-id":7}}})",
          R"(,"current-snapshot-id":-1,"refs":{"main":{"snapshot-id":-1}}})",
          R"(,"refs":{"main":{"snapshot-id":-1}}})",
          R"(,"current-snapshot-id":4})",
          R"(,"current-snapshot-id":4,"snapshots":[{"snapshot-id":4}]})",
          R"(,"current-snapshot-id":4,"snapshots":[{"snapshot-id":4,"schema-id":8,"manifest-list":"file:/list"}]})",
          R"(,"snapshots":[{"snapshot-id":5},{"snapshot-id":5}]})",
      }) {
    Check(prefix + suffix, Code::InvalidArgument);
  }
}

TEST_F(MetadataTest, RejectsOneInvalidFieldInResolvableMetadata) {
  CheckResolvedEdit(R"("refs":{"main":{"snapshot-id":9007199254740993}})",
                    R"("refs":{"main":{"snapshot-id":7}})");
  CheckResolvedEdit(R"("current-schema-id":1)", R"("current-schema-id":2147483648)");
  const auto specs = resolved.rfind(",\n \"partition-specs\"");
  ASSERT_NE(specs, std::string_view::npos);
  CheckResolvedEdit(resolved.substr(specs), "}");
}

TEST_F(MetadataTest, VersionsRequiredKeysAndCompression) {
  for (const auto version :
       {"0", "1", "3", "-9223372036854775808", "9223372036854775807", "9223372036854775808",
        "-9223372036854775809", "99999999999999999999"}) {
    Check(std::string("{\"format-version\":") + version + "}", Code::Unsupported);
  }
  for (const auto input :
       {"{}", R"({"format-version":1e0})", R"({"format-version":"2"})", R"({"format-version":2.0})",
        R"({"format-version":9223372036854775808.0})",
        R"({"format-version":99999999999999999999E+0})", R"({"format-version":2})",
        R"({"format-version":2,"schemas":[],"partition-specs":[]})",
        R"({"format-version":2,"current-schema-id":0,"partition-specs":[]})",
        R"({"format-version":2,"schemas":[],"current-schema-id":0,"partition-specs":[]})"}) {
    Check(input, Code::InvalidArgument);
  }
  Check("\x1f\x8b", Code::Unsupported);
  const doqe::test::TempFile gz("metadata.json.gz");
  ASSERT_TRUE(gz.Write("{" + std::string(minimal) + "}"));
  const auto pool = doqe::base::MakeFlatPool(65536);
  ASSERT_TRUE(pool);
  EXPECT_TRUE(ReadMetadata(gz.path(), io(), *pool));
}

TEST_F(MetadataTest, RejectsBareNestedTypeNamesInEverySchema) {
  for (const auto type : {"list", "map", "struct"}) {
    const std::string prefix =
        R"({"format-version":2,"partition-specs":[],"schemas":[{"schema-id":0,"type":"struct","fields":[)";
    const std::string schemas = prefix + R"({"id":1,"name":"x","required":true,"type":")" + type +
                                R"("}]},{"schema-id":1,"type":"struct","fields":[]}],)";
    Check(schemas + R"("current-schema-id":0})", Code::InvalidArgument);
    Check(schemas + R"("current-schema-id":1})", Code::InvalidArgument);
  }
}

TEST_F(MetadataTest, RejectsDuplicateAndReservedNestedFieldIds) {
  const std::string prefix =
      R"({"format-version":2,"partition-specs":[],"schemas":[{"schema-id":0,"type":"struct","fields":[{"id":1,"name":"top","required":true,"type":"long"},{"id":2,"name":"nested","required":true,"type":)";
  constexpr std::string_view suffix =
      R"(}]},{"schema-id":1,"type":"struct","fields":[]}],"current-schema-id":)";
  for (
      const auto type : {
          R"({"type":"struct","fields":[{"id":1,"name":"child","required":true,"type":"long"}]})",
          R"({"type":"list","element-id":1,"element-required":true,"element":"long"})",
          R"({"type":"list","element-id":2147483448,"element-required":true,"element":"long"})",
          R"({"type":"map","key-id":2147483448,"key":"long","value-id":3,"value-required":true,"value":"string"})",
          R"({"type":"map","key-id":3,"key":"long","value-id":2147483448,"value-required":true,"value":"string"})",
      }) {
    const std::string schemas = prefix + type + std::string(suffix);
    Check(schemas + "0}", Code::InvalidArgument);
    Check(schemas + "1}", Code::InvalidArgument);
  }
}

TEST_F(MetadataTest, EnforcesInputSizeCapBeforeAllocation) {
  constexpr std::size_t cap = std::size_t{16} * 1024U * 1024U;
  std::string json = "{" + std::string(minimal) + "}";
  json.resize(cap, ' ');
  const auto backing = doqe::base::MakeFlatPool(2 * cap);
  ASSERT_TRUE(backing);
  auto *fault = new doqe::test::FaultPool(*backing);
  const auto pool = doqe::base::PoolPtr::Adopt(fault);

  ASSERT_TRUE(file().Write(json));
  ASSERT_TRUE(ReadMetadata(file().path(), io(), pool));
  EXPECT_GT(fault->calls(), 0U);
  EXPECT_EQ(pool->live_bytes(), 0U);

  json += ' ';
  ASSERT_TRUE(file().Write(json));
  fault->FailAt(1, Code::OutOfMemory);
  const auto capped = ReadMetadata(file().path(), io(), pool);
  ASSERT_FALSE(capped);
  EXPECT_EQ(capped.error().code(), Code::Unsupported);
  EXPECT_EQ(fault->calls(), 0U);
  EXPECT_EQ(pool->live_bytes(), 0U);
}

TEST_F(MetadataTest, SchemaShapeDuplicateIdsAndUnsupportedCurrentTypes) {
  const std::string prefix =
      R"({"format-version":2,"current-schema-id":0,"partition-specs":[],"schemas":[{"schema-id":0,"type":"struct","fields":[)";
  constexpr std::string_view suffix = "]}]}";
  Check(prefix + R"({"id":2147483448,"name":"high","required":true,"type":"long"})" +
            std::string(suffix),
        Code::InvalidArgument);
  const auto pool = doqe::base::MakeFlatPool(65536);
  ASSERT_TRUE(pool);
  for (const auto type :
       {R"("int")",
        R"({"type":"list","element-id":23,"element-required":true,"element":"long"})"}) {
    ASSERT_TRUE(file().Write(prefix + R"({"id":42,"name":"x","required":true,"type":)" + type +
                             "}" + std::string(suffix)));
    const auto result = ReadMetadata(file().path(), io(), *pool);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code(), Code::Unsupported);
    EXPECT_NE(result.error().message().find("42"), std::string_view::npos);
  }
  Check(
      prefix +
          R"({"id":1,"name":"x","required":true,"type":{"type":"map","key-id":2,"key":"long","value-id":1,"value-required":false,"value":"string"}})" +
          std::string(suffix),
      Code::InvalidArgument);
  Check(
      R"({"format-version":2,"current-schema-id":0,"partition-specs":[],"schemas":[{"schema-id":0,"type":"struct","fields":[]},{"schema-id":0,"type":"struct","fields":[]}]})",
      Code::InvalidArgument);
  Check(
      R"({"format-version":2,"current-schema-id":0,"schemas":[{"schema-id":0,"type":"struct","fields":[]}],"partition-specs":[{"spec-id":0,"fields":[]},{"spec-id":0,"fields":[]}]})",
      Code::InvalidArgument);
}

TEST_F(MetadataTest, LargeSchemaAndDuplicateNearEnd) {
  std::string json =
      R"({"format-version":2,"current-schema-id":0,"partition-specs":[],"schemas":[{"schema-id":0,"type":"struct","fields":[)";
  for (int id = 1; id <= 10000; ++id) {
    if (id > 1) {
      json += ',';
    }
    json += R"({"id":)" + std::to_string(id) + R"(,"name":"f)" + std::to_string(id) +
            R"(","required":true,"type":"long"})";
  }
  ASSERT_TRUE(file().Write(json + "]}]}"));
  const auto pool = doqe::base::MakeFlatPool(std::size_t{32} * 1024U * 1024U);
  ASSERT_TRUE(pool);
  const auto result = ReadMetadata(file().path(), io(), *pool);
  ASSERT_TRUE(result);
  ASSERT_EQ(result->fields().size(), 10000U);
  EXPECT_EQ(result->fields().back().id, 10000);
  Check(json + R"(,{"id":10000,"name":"dup","required":true,"type":"long"}]}]})",
        Code::InvalidArgument);
}

TEST_F(MetadataTest, EveryPoolFailureAndCrossThreadRelease) {
  ASSERT_TRUE(file().Write(std::string(70000, ' ') + std::string(resolved)));
  const auto backing = doqe::base::MakeFlatPool(std::size_t{256} * 1024U);
  ASSERT_TRUE(backing);
  auto *fault = new doqe::test::FaultPool(*backing);
  auto pool = doqe::base::PoolPtr::Adopt(fault);
  {
    const auto success = ReadMetadata(file().path(), io(), pool);
    ASSERT_TRUE(success);
  }
  const auto allocations = fault->calls();
  ASSERT_GT(allocations, 1U);
  EXPECT_EQ((*backing)->live_bytes(), 0U);
  for (const auto code : {Code::ResourceExhausted, Code::OutOfMemory}) {
    for (std::size_t step = 1; step <= allocations; ++step) {
      fault->FailAt(step, code);
      const auto result = ReadMetadata(file().path(), io(), pool);
      ASSERT_FALSE(result) << step;
      EXPECT_EQ(result.error().code(), code) << step;
      EXPECT_EQ((*backing)->live_bytes(), 0U) << step;
    }
  }
  fault->FailAt(0, Code::OutOfMemory);
  auto result = ReadMetadata(file().path(), io(), pool);
  ASSERT_TRUE(result);
  pool = {};
  std::thread release([model = std::move(*result)] {
    EXPECT_EQ(model.fields()[1].name, "renamed");
    const auto snapshot = model.current_snapshot();
    ASSERT_TRUE(snapshot);
    EXPECT_EQ(snapshot->manifest_list, "file:/warehouse/list.avro");
  });
  release.join();
  EXPECT_EQ((*backing)->live_bytes(), 0U);
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

} // namespace
