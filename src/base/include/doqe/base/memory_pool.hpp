#ifndef DOQE_BASE_MEMORY_POOL_HPP
#define DOQE_BASE_MEMORY_POOL_HPP

#include "doqe/base/status.hpp"

#include <atomic>
#include <cstddef>

namespace doqe::base {

class PoolPtr;

// Only the last PoolPtr can destroy a pool.
// NOLINTNEXTLINE(cppcoreguidelines-virtual-class-destructor)
class MemoryPool {
public:
  MemoryPool(const MemoryPool &) = delete;
  MemoryPool &operator=(const MemoryPool &) = delete;
  MemoryPool(MemoryPool &&) = delete;
  MemoryPool &operator=(MemoryPool &&) = delete;

  // The allocating query thread and any freeing threads must hold a live pool reference.
  // A nonzero allocation returns raw storage. Create objects explicitly in the payload
  // and use their pointers for access, including the first-element pointer for arrays.
  [[nodiscard]] virtual Result<void *> Allocate(std::size_t charge,
                                                std::size_t alignment) noexcept = 0;
  // Pass back the original allocation pointer, including after destroying objects
  // constructed in the payload. Free uses it to address the backing byte array.
  virtual void Free(void *data, std::size_t charge, std::size_t alignment) noexcept = 0;
  [[nodiscard]] virtual std::size_t live_bytes() const noexcept = 0;
  [[nodiscard]] virtual std::size_t limit() const noexcept = 0;

protected:
  MemoryPool() noexcept = default;
  virtual ~MemoryPool() = default;

private:
  friend class PoolPtr;
  std::atomic_size_t references_{1};
};

class PoolPtr {
public:
  PoolPtr() noexcept = default;
  PoolPtr(const PoolPtr &other) noexcept;
  PoolPtr(PoolPtr &&other) noexcept;
  PoolPtr &operator=(const PoolPtr &other) noexcept;
  PoolPtr &operator=(PoolPtr &&other) noexcept;
  ~PoolPtr() noexcept;

  // Adopt transfers the initial reference of a freshly constructed pool.
  [[nodiscard]] static PoolPtr Adopt(MemoryPool *pool) noexcept;
  // The caller must already hold a live reference.
  [[nodiscard]] static PoolPtr Retain(MemoryPool &pool) noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return pool_ != nullptr; }
  [[nodiscard]] MemoryPool &operator*() const noexcept { return *pool_; }
  [[nodiscard]] MemoryPool *operator->() const noexcept { return pool_; }

private:
  explicit PoolPtr(MemoryPool *pool) noexcept : pool_(pool) {}
  MemoryPool *pool_ = nullptr;
};

[[nodiscard]] Result<PoolPtr> MakeFlatPool(std::size_t limit) noexcept;

} // namespace doqe::base

#endif // DOQE_BASE_MEMORY_POOL_HPP
