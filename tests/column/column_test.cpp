#include "column_internal.hpp"
#include "doqe/base/memory_pool.hpp"
#include "doqe/base/status.hpp"
#include "doqe/column/arrow/c/abi.h"
#include "doqe/column/column.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <gtest/gtest.h>
#include <latch>
#include <limits>
#include <span>
#include <string_view>
#include <thread>
#include <utility>

namespace doqe::column {
namespace {

using base::Code;

struct Observations {
  std::size_t attempts = 0;
  std::size_t fail_at = 0;
  std::size_t allocated = 0;
  std::atomic<std::size_t> freed{0};
  unsigned destroyed = 0;
  std::size_t live_at_destruction = 0;
  std::thread::id destructor_thread;
};

class ObservedPool final : public base::MemoryPool {
public:
  ObservedPool(base::PoolPtr backing, Observations &observations) noexcept
      : backing_(std::move(backing)), observations_(observations) {}
  ObservedPool(const ObservedPool &) = delete;
  ObservedPool(ObservedPool &&) = delete;
  ObservedPool &operator=(const ObservedPool &) = delete;
  ObservedPool &operator=(ObservedPool &&) = delete;

  base::Result<void *> Allocate(std::size_t charge, std::size_t alignment) noexcept override {
    if (++observations_.attempts == observations_.fail_at) {
      return std::unexpected(base::Status::OutOfMemory());
    }
    auto result = backing_->Allocate(charge, alignment);
    if (result) {
      observations_.allocated += charge;
    }
    return result;
  }

  void Free(void *data, std::size_t charge, std::size_t alignment) noexcept override {
    backing_->Free(data, charge, alignment);
    observations_.freed.fetch_add(charge, std::memory_order_relaxed);
  }

  std::size_t live_bytes() const noexcept override { return backing_->live_bytes(); }
  std::size_t limit() const noexcept override { return backing_->limit(); }

private:
  ~ObservedPool() override {
    observations_.live_at_destruction = live_bytes();
    ++observations_.destroyed;
    observations_.destructor_thread = std::this_thread::get_id();
  }

  base::PoolPtr backing_;
  Observations &observations_;
};

// Concrete fixtures inspect exact Arrow C buffer and child slots and address alignment.
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access,cppcoreguidelines-pro-type-reinterpret-cast)
base::PoolPtr MakeObservedPool(Observations &observations) {
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  return base::PoolPtr::Adopt(new ObservedPool(*base::MakeFlatPool(8192), observations));
}

TEST(BufferTest, SharesAlignedImmutableStorageAndChecksSize) {
  const auto query = *base::MakeFlatPool(1024);
  std::array bytes{std::byte{1}, std::byte{2}, std::byte{3}};
  const auto original = Buffer::Copy(*query, bytes);
  ASSERT_TRUE(original);
  EXPECT_GE(query->live_bytes(), 64U);
  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(original->bytes().data()) % 64, 0U);
  const auto used = query->live_bytes();
  const auto slice = original->Slice(1, 2);
  ASSERT_TRUE(slice);
  bytes[1] = std::byte{9};
  EXPECT_EQ(slice->bytes().data(), original->bytes().subspan(1).data());
  EXPECT_EQ(slice->bytes()[0], std::byte{2});
  EXPECT_EQ(query->live_bytes(), used);
  const auto invalid = original->Slice(2, 2);
  ASSERT_FALSE(invalid);
  EXPECT_EQ(invalid.error().code(), Code::InvalidArgument);

  const auto overflow = Buffer::PaddedSize(std::numeric_limits<std::size_t>::max());
  ASSERT_FALSE(overflow);
  EXPECT_EQ(overflow.error().code(), Code::ResourceExhausted);
  const auto padded = Buffer::PaddedSize(65);
  ASSERT_TRUE(padded);
  EXPECT_EQ(*padded, 128U);
}

TEST(BufferTest, ControlBlockFailureFreesBackingStorage) {
  Observations observations;
  const auto query = MakeObservedPool(observations);
  observations.fail_at = 2;
  const auto buffer = Buffer::Copy(*query, std::array{std::byte{1}});
  ASSERT_FALSE(buffer);
  EXPECT_EQ(buffer.error().code(), Code::OutOfMemory);
  EXPECT_TRUE(buffer.error().message().empty());
  EXPECT_EQ(query->live_bytes(), 0U);
  EXPECT_EQ(observations.attempts, 2U);
  EXPECT_EQ(observations.allocated, 64U);
  EXPECT_EQ(observations.freed.load(), 64U);

  const auto small_pool = *base::MakeFlatPool(64);
  const auto over_budget = Buffer::Copy(*small_pool, std::array{std::byte{1}});
  ASSERT_FALSE(over_budget);
  EXPECT_EQ(over_budget.error().code(), Code::ResourceExhausted);
  EXPECT_EQ(small_pool->live_bytes(), 0U);
}

TEST(ArrayTest, EmptyAndAllNullLayouts) {
  const auto query = *base::MakeFlatPool(4096);
  const auto empty_int = Array::Int64(*query, {});
  const auto empty_utf8 = Array::Utf8(*query, std::array<std::int32_t, 1>{0}, "");
  const auto null_int =
      Array::Int64(*query, std::array<std::int64_t, 2>{}, std::array<std::uint8_t, 1>{0});
  const auto null_utf8 =
      Array::Utf8(*query, std::array<std::int32_t, 3>{0, 0, 0}, "", std::array<std::uint8_t, 1>{0});
  ASSERT_TRUE(empty_int);
  ASSERT_TRUE(empty_utf8);
  ASSERT_TRUE(null_int);
  ASSERT_TRUE(null_utf8);

  for (const auto *array : {&*empty_int, &*empty_utf8, &*null_int, &*null_utf8}) {
    ArrowArray data{};
    ArrowSchema schema{};
    const auto before = query->live_bytes();
    ASSERT_TRUE(array->Export(*query, data, schema));
    EXPECT_EQ(data.length, array->length());
    EXPECT_EQ(data.null_count, data.length);
    EXPECT_EQ(data.buffers[0] == nullptr, data.length == 0);
    if (array->type() == Type::Utf8) {
      EXPECT_STREQ(schema.format, "u");
      EXPECT_EQ(data.n_buffers, 3);
      ASSERT_NE(data.buffers[1], nullptr);
      EXPECT_EQ(static_cast<const std::int32_t *>(data.buffers[1])[0], 0);
    } else {
      EXPECT_STREQ(schema.format, "l");
      EXPECT_EQ(data.n_buffers, 2);
    }
    data.release(&data);
    schema.release(&schema);
    EXPECT_EQ(data.release, nullptr);
    EXPECT_EQ(schema.release, nullptr);
    EXPECT_EQ(query->live_bytes(), before);
  }
}

TEST(ArrayTest, EmptyStringAndPositiveLengthNullAndSlice) {
  const auto query = *base::MakeFlatPool(4096);
  const auto array = Array::Utf8(*query, std::array<std::int32_t, 5>{0, 1, 1, 3, 4}, "axxb",
                                 std::array<std::uint8_t, 1>{0b1011});
  ASSERT_TRUE(array);
  EXPECT_TRUE(array->is_valid(1));
  EXPECT_FALSE(array->is_valid(2));
  const auto slice = array->Slice(1, 2);
  const auto valid_slice = array->Slice(3, 1);
  ASSERT_TRUE(slice);
  ASSERT_TRUE(valid_slice);
  ASSERT_FALSE(array->Slice(3, 2));
  ASSERT_FALSE(array->Slice(-1, 1));
  ArrowArray full{};
  ArrowArray partial{};
  ArrowArray valid{};
  ArrowSchema full_schema{};
  ArrowSchema partial_schema{};
  ArrowSchema valid_schema{};
  ASSERT_TRUE(array->Export(*query, full, full_schema));
  ASSERT_TRUE(slice->Export(*query, partial, partial_schema));
  ASSERT_TRUE(valid_slice->Export(*query, valid, valid_schema));
  EXPECT_EQ(full.null_count, 1);
  EXPECT_EQ(partial.length, 2);
  EXPECT_EQ(partial.offset, 1);
  EXPECT_EQ(partial.null_count, 1);
  EXPECT_EQ(valid.null_count, 0);
  EXPECT_EQ(valid.buffers[0], nullptr);
  for (std::size_t i = 0; i < 3; ++i) {
    EXPECT_EQ(full.buffers[i], partial.buffers[i]);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(full.buffers[i]) % 64, 0U);
  }
  const auto offsets = std::span(static_cast<const std::int32_t *>(partial.buffers[1]), 5);
  EXPECT_EQ(offsets[1], 1);
  EXPECT_EQ(offsets[2], 1);
  EXPECT_EQ(offsets[3], 3);
  EXPECT_EQ(std::string_view(static_cast<const char *>(partial.buffers[2]), 4), "axxb");
  full.release(&full);
  full_schema.release(&full_schema);
  partial.release(&partial);
  partial_schema.release(&partial_schema);
  valid.release(&valid);
  valid_schema.release(&valid_schema);
}

TEST(ArrayTest, Utf8SizeCheckSeparatesSingleValueAndBatchBoundary) {
  constexpr auto limit = static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max());
  const auto exact_value = CheckUtf8Append(0, limit);
  const auto large_value = CheckUtf8Append(0, limit + 1);
  const auto exact_total = CheckUtf8Append(limit - 1, 1);
  const auto too_much = CheckUtf8Append(limit - 1, 2);
  const auto empty_at_limit = CheckUtf8Append(limit, 0);
  ASSERT_TRUE(exact_value);
  ASSERT_TRUE(exact_total);
  ASSERT_TRUE(too_much);
  ASSERT_TRUE(empty_at_limit);
  ASSERT_FALSE(large_value);
  const auto invalid_used = CheckUtf8Append(limit + 1, 0);
  ASSERT_FALSE(invalid_used);
  EXPECT_EQ(invalid_used.error().code(), Code::InvalidArgument);
  EXPECT_EQ(*exact_value, Utf8Append::Fits);
  EXPECT_EQ(*exact_total, Utf8Append::Fits);
  EXPECT_EQ(*too_much, Utf8Append::NewBatch);
  EXPECT_EQ(*empty_at_limit, Utf8Append::Fits);
  EXPECT_EQ(large_value.error().code(), Code::Unsupported);

  const auto query = *base::MakeFlatPool(4096);
  for (const auto type : {Type::LargeUtf8, Type::Utf8View}) {
    const auto unsupported = Array::Utf8(*query, std::array<std::int32_t, 1>{0}, "", {}, type);
    ASSERT_FALSE(unsupported);
    EXPECT_EQ(unsupported.error().code(), Code::Unsupported);
  }
  const auto wrong_type = Array::Utf8(*query, std::array<std::int32_t, 1>{0}, "", {}, Type::Int64);
  ASSERT_FALSE(wrong_type);
  EXPECT_EQ(wrong_type.error().code(), Code::InvalidArgument);
  for (const auto offsets :
       {std::array<std::int32_t, 3>{1, 1, 1}, std::array<std::int32_t, 3>{0, -1, 1},
        std::array<std::int32_t, 3>{0, 2, 1}, std::array<std::int32_t, 3>{0, 1, 2}}) {
    const auto invalid = Array::Utf8(*query, offsets, "x");
    ASSERT_FALSE(invalid);
    EXPECT_EQ(invalid.error().code(), Code::InvalidArgument);
  }
}

TEST(ArrayTest, ChecksUtf8DataExtentBeforeInspectingOffsets) {
  constexpr auto limit = static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max());
  EXPECT_TRUE(CheckUtf8DataSize(0));
  EXPECT_TRUE(CheckUtf8DataSize(limit));
  const auto oversized = CheckUtf8DataSize(limit + 1);
  ASSERT_FALSE(oversized);
  EXPECT_EQ(oversized.error().code(), Code::Unsupported);
}

TEST(ArrayTest, RejectsWrongValidityBitmapSize) {
  const auto query = *base::MakeFlatPool(4096);
  const auto too_short =
      Array::Int64(*query, std::array<std::int64_t, 9>{}, std::array<std::uint8_t, 1>{0xff});
  ASSERT_FALSE(too_short);
  EXPECT_EQ(too_short.error().code(), Code::InvalidArgument);
  const auto too_long =
      Array::Utf8(*query, std::array<std::int32_t, 2>{0, 0}, "", std::array<std::uint8_t, 2>{});
  ASSERT_FALSE(too_long);
  EXPECT_EQ(too_long.error().code(), Code::InvalidArgument);
  EXPECT_EQ(query->live_bytes(), 0U);
}

TEST(ArrayTest, NativeAndArrowReadTheSameValidityBitmap) {
  const auto query = *base::MakeFlatPool(4096);
  const auto array =
      Array::Int64(*query, std::array<std::int64_t, 9>{}, std::array<std::uint8_t, 2>{0xff, 0x00});
  ASSERT_TRUE(array);
  EXPECT_TRUE(array->is_valid(7));
  EXPECT_FALSE(array->is_valid(8));
  ArrowArray data{};
  ArrowSchema schema{};
  ASSERT_TRUE(array->Export(*query, data, schema));
  ASSERT_NE(data.buffers[0], nullptr);
  const auto *bits = static_cast<const std::uint8_t *>(data.buffers[0]);
  EXPECT_EQ(bits[0], 0xffU);
  EXPECT_EQ(bits[1] & 0x01U, 0U);
  EXPECT_EQ(data.null_count, 1);
  data.release(&data);
  schema.release(&schema);
}

TEST(BatchTest, RejectsInvalidNamesAndLengthMismatch) {
  Observations observations;
  const auto query = MakeObservedPool(observations);
  for (const auto name :
       {std::string_view("a\0b", 3), std::string_view("\xff"), std::string_view("\xe0\x80\xaf")}) {
    SCOPED_TRACE(name);
    const auto batch =
        Batch::Make(*query, 0, std::array{Field{.id = 1, .name = name, .array = {}}});
    EXPECT_FALSE(batch);
    if (!batch) {
      EXPECT_EQ(batch.error().code(), Code::InvalidArgument);
    }
  }
  const auto mismatch =
      Batch::Make(*query, 1, std::array{Field{.id = 1, .name = "field", .array = {}}});
  ASSERT_FALSE(mismatch);
  EXPECT_EQ(mismatch.error().code(), Code::InvalidArgument);
  EXPECT_EQ(observations.attempts, 0U);
  EXPECT_EQ(query->live_bytes(), 0U);
}

TEST(BatchTest, RollsBackFieldsWhenNameAllocationFails) {
  Observations observations;
  auto query = MakeObservedPool(observations);
  observations.fail_at = 2;
  const std::array fields{Field{.id = 1, .name = "é", .array = {}}};
  const auto batch = Batch::Make(*query, 0, fields);
  ASSERT_FALSE(batch);
  EXPECT_EQ(batch.error().code(), Code::OutOfMemory);
  EXPECT_EQ(query->live_bytes(), 0U);
  query = {};
  EXPECT_EQ(observations.destroyed, 1U);
  EXPECT_EQ(observations.allocated, observations.freed.load());
}

TEST(BatchTest, FieldIdentitySurvivesRenameAndReorder) {
  const auto query = *base::MakeFlatPool(4096);
  const auto integers = Array::Int64(*query, std::array<std::int64_t, 1>{42});
  const auto strings = Array::Utf8(*query, std::array<std::int32_t, 2>{0, 1}, "x");
  ASSERT_TRUE(integers);
  ASSERT_TRUE(strings);
  const std::array initial{Field{.id = 2, .name = "old", .array = *integers},
                           Field{.id = 9, .name = "other", .array = *strings}};
  const std::array reordered{Field{.id = 9, .name = "other", .array = *strings},
                             Field{.id = 2, .name = "new", .array = *integers}};
  const auto first = Batch::Make(*query, 1, initial);
  const auto second = Batch::Make(*query, 1, reordered);
  ASSERT_TRUE(first);
  ASSERT_TRUE(second);
  EXPECT_EQ(first->fields()[0].id, 2);
  EXPECT_EQ(second->fields()[0].id, 9);
  EXPECT_EQ(first->Find(2)->name, "old");
  EXPECT_EQ(second->Find(2)->name, "new");
  EXPECT_EQ(second->Find(2)->array.type(), Type::Int64);
  EXPECT_EQ(second->Find(99), nullptr);
  const auto before = query->live_bytes();
  const std::array duplicates{Field{.id = 2, .name = "a", .array = *integers},
                              Field{.id = 2, .name = "b", .array = *strings}};
  const auto invalid = Batch::Make(*query, 1, duplicates);
  ASSERT_FALSE(invalid);
  EXPECT_EQ(invalid.error().code(), Code::InvalidArgument);
  EXPECT_EQ(query->live_bytes(), before);
}

base::Result<Batch> MakeBatch(base::MemoryPool &pool) {
  auto ints = Array::Int64(pool, std::array<std::int64_t, 2>{12, 34});
  if (!ints) {
    return std::unexpected(ints.error());
  }
  auto strings = Array::Utf8(pool, std::array<std::int32_t, 3>{0, 1, 2}, "ab");
  if (!strings) {
    return std::unexpected(strings.error());
  }
  return Batch::Make(pool, 2,
                     std::array{Field{.id = 2, .name = "numbers", .array = *ints},
                                Field{.id = 3, .name = "letters", .array = *strings}});
}

TEST(ArrowExportTest, RollsBackEveryAllocationFailure) {
  std::size_t export_allocations = 0;
  {
    Observations observations;
    const auto query = MakeObservedPool(observations);
    const auto batch = MakeBatch(*query);
    ASSERT_TRUE(batch);
    const auto native_allocations = observations.attempts;
    ArrowArray data{};
    ArrowSchema schema{};
    ASSERT_TRUE(batch->Export(data, schema));
    export_allocations = observations.attempts - native_allocations;
    data.release(&data);
    schema.release(&schema);
    RecordProperty("native_allocations_per_batch", native_allocations);
    RecordProperty("export_allocations_per_batch", export_allocations);
    EXPECT_GT(export_allocations, 0U);
  }
  for (std::size_t step = 1; step <= export_allocations; ++step) {
    SCOPED_TRACE(step);
    Observations observations;
    auto query = MakeObservedPool(observations);
    {
      const auto batch = MakeBatch(*query);
      ASSERT_TRUE(batch);
      const auto live = query->live_bytes();
      observations.fail_at = observations.attempts + step;
      ArrowArray data{};
      ArrowSchema schema{};
      const auto failure = batch->Export(data, schema);
      ASSERT_FALSE(failure);
      EXPECT_EQ(failure.error().code(), Code::OutOfMemory);
      EXPECT_EQ(data.release, nullptr);
      EXPECT_EQ(schema.release, nullptr);
      EXPECT_EQ(query->live_bytes(), live);
    }
    EXPECT_EQ(query->live_bytes(), 0U);
    query = {};
    EXPECT_EQ(observations.destroyed, 1U);
    EXPECT_EQ(observations.allocated, observations.freed.load());
  }
}

TEST(ArrowExportTest, SingleArrayRollsBackEveryAllocationFailure) {
  std::size_t export_allocations = 0;
  {
    Observations observations;
    const auto query = MakeObservedPool(observations);
    const auto array = Array::Int64(*query, std::array<std::int64_t, 1>{42});
    ASSERT_TRUE(array);
    const auto before = observations.attempts;
    ArrowArray data{};
    ArrowSchema schema{};
    ASSERT_TRUE(array->Export(*query, data, schema));
    export_allocations = observations.attempts - before;
    data.release(&data);
    schema.release(&schema);
    ASSERT_GE(export_allocations, 2U);
  }
  for (std::size_t step = 1; step <= export_allocations; ++step) {
    SCOPED_TRACE(step);
    Observations observations;
    auto query = MakeObservedPool(observations);
    {
      const auto array = Array::Int64(*query, std::array<std::int64_t, 1>{42});
      ASSERT_TRUE(array);
      const auto baseline = query->live_bytes();
      observations.fail_at = observations.attempts + step;
      ArrowArray data{};
      ArrowSchema schema{};
      const auto failure = array->Export(*query, data, schema);
      ASSERT_FALSE(failure);
      EXPECT_EQ(failure.error().code(), Code::OutOfMemory);
      EXPECT_EQ(data.release, nullptr);
      EXPECT_EQ(schema.release, nullptr);
      EXPECT_EQ(query->live_bytes(), baseline);
      // Clean up any surviving state after recording a rollback failure.
      if (data.release != nullptr) {
        data.release(&data);
      }
      if (schema.release != nullptr) {
        schema.release(&schema);
      }
    }
    EXPECT_EQ(query->live_bytes(), 0U);
    query = {};
    EXPECT_EQ(observations.destroyed, 1U);
    EXPECT_EQ(observations.live_at_destruction, 0U);
    EXPECT_EQ(observations.allocated, observations.freed.load());
  }
}

TEST(ArrowExportTest, MovedChildrenOutliveMovedParentAndQuery) {
  Observations observations;
  auto query = MakeObservedPool(observations);
  ArrowArray original{};
  ArrowSchema original_schema{};
  {
    const auto batch = MakeBatch(*query);
    ASSERT_TRUE(batch);
    ASSERT_TRUE(batch->Export(original, original_schema));
  }
  ArrowArray parent = original;
  original.release = nullptr;
  ArrowSchema parent_schema = original_schema;
  original_schema.release = nullptr;
  ASSERT_EQ(parent.n_children, 2);
  ASSERT_EQ(parent_schema.n_children, 2);
  EXPECT_EQ(parent_schema.children[0]->flags, ARROW_FLAG_NULLABLE);
  ArrowArray child = *parent.children[1];
  parent.children[1]->release = nullptr;
  ArrowSchema child_schema = *parent_schema.children[1];
  parent_schema.children[1]->release = nullptr;
  parent.release(&parent);
  parent_schema.release(&parent_schema);
  EXPECT_EQ(parent.release, nullptr);
  EXPECT_EQ(parent_schema.release, nullptr);
  query = {};
  EXPECT_EQ(observations.destroyed, 0U);
  EXPECT_EQ(std::string_view(static_cast<const char *>(child.buffers[2]), 2), "ab");
  EXPECT_EQ(child_schema.flags, ARROW_FLAG_NULLABLE);
  EXPECT_STREQ(child_schema.name, "letters");
  EXPECT_STREQ(child_schema.format, "u");
  child.release(&child);
  EXPECT_EQ(observations.destroyed, 0U);
  child_schema.release(&child_schema);
  EXPECT_EQ(observations.destroyed, 1U);
  EXPECT_EQ(observations.live_at_destruction, 0U);
  EXPECT_EQ(observations.allocated, observations.freed.load());
}

TEST(ArrowExportTest, IndependentExportsReleaseConcurrentlyAfterQueryTeardown) {
  Observations observations;
  auto query = MakeObservedPool(observations);
  ArrowArray first{};
  ArrowSchema first_schema{};
  ArrowArray second{};
  ArrowSchema second_schema{};
  {
    const auto batch = MakeBatch(*query);
    ASSERT_TRUE(batch);
    ASSERT_TRUE(batch->Export(first, first_schema));
    ASSERT_TRUE(batch->Export(second, second_schema));
  }
  query = {};
  EXPECT_EQ(observations.destroyed, 0U);
  EXPECT_GT(observations.allocated, observations.freed.load());
  std::latch ready{2};
  const auto release = [&ready](ArrowArray &data, ArrowSchema &schema) {
    EXPECT_EQ(static_cast<const std::int64_t *>(data.children[0]->buffers[1])[1], 34);
    EXPECT_STREQ(schema.children[0]->name, "numbers");
    ready.arrive_and_wait();
    data.release(&data);
    schema.release(&schema);
  };
  std::thread one([&] { release(first, first_schema); });
  std::thread two([&] { release(second, second_schema); });
  const auto one_id = one.get_id();
  const auto two_id = two.get_id();
  one.join();
  two.join();
  EXPECT_EQ(observations.destroyed, 1U);
  EXPECT_TRUE(observations.destructor_thread == one_id || observations.destructor_thread == two_id);
  EXPECT_EQ(observations.live_at_destruction, 0U);
  EXPECT_EQ(observations.allocated, observations.freed.load());
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access,cppcoreguidelines-pro-type-reinterpret-cast)

} // namespace
} // namespace doqe::column
