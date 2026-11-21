#include "doqe/base/arena.hpp"

#include "doqe/base/memory_pool.hpp"
#include "doqe/base/status.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <expected>
#include <limits>
#include <memory>
#include <span>
#include <utility>

namespace doqe::base {

struct Arena::Chunk {
  Chunk *next;
  void *allocation;
  std::size_t charge;
  std::size_t cursor;
};

namespace {

constexpr std::size_t chunk_alignment = 64;
constexpr std::size_t largest_chunk = std::size_t{64} * 1024U;

constexpr std::size_t RoundUp(std::size_t bytes, std::size_t alignment) noexcept {
  return (bytes + alignment - 1) & ~(alignment - 1);
}

} // namespace

Arena::Savepoint::Savepoint(Arena &arena) noexcept
    : arena_(&arena), chunks_(arena.chunks_), ordinary_(arena.ordinary_),
      cursor_(ordinary_ == nullptr ? 0 : ordinary_->cursor), next_size_(arena.next_size_) {}

Arena::Savepoint::~Savepoint() noexcept {
  if (arena_ != nullptr) {
    arena_->FreeUntil(chunks_);
    arena_->ordinary_ = ordinary_;
    arena_->next_size_ = next_size_;
    if (ordinary_ != nullptr) {
      ordinary_->cursor = cursor_;
    }
  }
}

Arena::Arena(PoolPtr pool) noexcept : pool_(std::move(pool)) {}

Arena::Arena(Arena &&other) noexcept
    : pool_(std::move(other.pool_)), chunks_(std::exchange(other.chunks_, nullptr)),
      ordinary_(std::exchange(other.ordinary_, nullptr)),
      next_size_(std::exchange(other.next_size_, first_chunk_size)) {}

Arena &Arena::operator=(Arena &&other) noexcept {
  if (this != &other) {
    FreeUntil(nullptr);
    pool_ = std::move(other.pool_);
    chunks_ = std::exchange(other.chunks_, nullptr);
    ordinary_ = std::exchange(other.ordinary_, nullptr);
    next_size_ = std::exchange(other.next_size_, first_chunk_size);
  }
  return *this;
}

Arena::~Arena() noexcept { FreeUntil(nullptr); }

void Arena::FreeUntil(Chunk *end) noexcept {
  while (chunks_ != end) {
    auto *chunk = chunks_;
    chunks_ = chunk->next;
    auto *allocation = chunk->allocation;
    const auto charge = chunk->charge;
    std::destroy_at(chunk);
    pool_->Free(allocation, charge, chunk_alignment);
  }
}

// The size/alignment pair follows the MemoryPool allocation contract.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
Result<void *> Arena::Allocate(std::size_t bytes, std::size_t alignment) noexcept {
  if (!std::has_single_bit(alignment) || alignment > chunk_alignment) {
    return Fail(Code::InvalidArgument, "invalid arena alignment");
  }
  if (bytes == 0) {
    return nullptr;
  }
  if (!pool_) {
    return Fail(Code::InvalidArgument, "arena requires a memory pool");
  }
  // An ordinary cursor is bounded by largest_chunk, so rounding cannot overflow.
  if (ordinary_ != nullptr) {
    const auto start = RoundUp(ordinary_->cursor, alignment);
    if (start <= ordinary_->charge && bytes <= ordinary_->charge - start) {
      ordinary_->cursor = start + bytes;
      return std::span(static_cast<std::byte *>(ordinary_->allocation), ordinary_->charge)
          .subspan(start)
          .data();
    }
  }
  const auto start = RoundUp(sizeof(Chunk), alignment);
  if (bytes > std::numeric_limits<std::size_t>::max() - start) {
    return Fail(Code::ResourceExhausted);
  }
  const auto needed = start + bytes;
  auto charge = needed;
  if (needed <= largest_chunk) {
    charge = next_size_;
    while (charge < needed) {
      charge *= 2;
    }
  }
  auto data = pool_->Allocate(charge, chunk_alignment);
  if (!data) {
    return std::unexpected(data.error());
  }
  // Placement construction uses the original allocation owned by the pool.
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  chunks_ =
      ::new (*data) Chunk{.next = chunks_, .allocation = *data, .charge = charge, .cursor = needed};
  if (needed <= largest_chunk) {
    ordinary_ = chunks_;
    next_size_ = std::min(charge * 2, largest_chunk);
  }
  return std::span(static_cast<std::byte *>(*data), charge).subspan(start).data();
}

} // namespace doqe::base
