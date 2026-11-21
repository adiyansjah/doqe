#ifndef DOQE_BASE_ARENA_HPP
#define DOQE_BASE_ARENA_HPP

#include "doqe/base/memory_pool.hpp"
#include "doqe/base/status.hpp"

#include <cstddef>
#include <expected>
#include <limits>
#include <new>
#include <span>
#include <type_traits>

namespace doqe::base {

// Allocation belongs to the query thread. The owner may move to another thread for
// exclusive access and destruction. Borrowed storage expires with the owner.
class Arena {
  struct Chunk;

public:
  // Rewind allocations made inside a failed operation. Active savepoints
  // must nest in scope order, and the arena must remain in place.
  class Savepoint {
  public:
    explicit Savepoint(Arena &arena) noexcept;
    Savepoint(const Savepoint &) = delete;
    Savepoint &operator=(const Savepoint &) = delete;
    Savepoint(Savepoint &&) = delete;
    Savepoint &operator=(Savepoint &&) = delete;
    ~Savepoint() noexcept;
    void Commit() noexcept { arena_ = nullptr; }

  private:
    Arena *arena_;
    Chunk *chunks_;
    Chunk *ordinary_;
    std::size_t cursor_;
    std::size_t next_size_;
  };

  explicit Arena(PoolPtr pool) noexcept;
  Arena(const Arena &) = delete;
  Arena &operator=(const Arena &) = delete;
  Arena(Arena &&other) noexcept;
  Arena &operator=(Arena &&other) noexcept;
  ~Arena() noexcept;

  [[nodiscard]] Result<void *> Allocate(std::size_t bytes, std::size_t alignment) noexcept;

  template <class T> [[nodiscard]] Result<std::span<T>> MakeArray(std::size_t count) noexcept {
    static_assert(std::is_trivially_destructible_v<T>);
    static_assert(std::is_nothrow_default_constructible_v<T>);
    if (count == 0) {
      return std::span<T>{};
    }
    if (count > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
      return Fail(Code::ResourceExhausted);
    }
    auto data = Allocate(count * sizeof(T), alignof(T));
    if (!data) {
      return std::unexpected(data.error());
    }
    // Non-allocating array placement new starts the array and element lifetimes.
    return std::span<T>(::new (*data) T[count]{}, count);
  }

private:
  void FreeUntil(Chunk *end) noexcept;

  static constexpr std::size_t first_chunk_size = 4096;
  PoolPtr pool_;
  Chunk *chunks_ = nullptr;
  Chunk *ordinary_ = nullptr;
  std::size_t next_size_ = first_chunk_size;
};

} // namespace doqe::base

#endif // DOQE_BASE_ARENA_HPP
