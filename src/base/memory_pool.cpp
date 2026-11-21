#include "doqe/base/memory_pool.hpp"
#include "doqe/base/status.hpp"

#ifdef DOQE_TEST_HOOKS
#include "test_probes.hpp"
#endif

#include <algorithm>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <expected>
#include <limits>
#include <new>
#include <utility>
#ifndef NDEBUG
#include <mutex>
#include <unordered_set>
#endif

namespace doqe::base {
namespace {

constexpr std::size_t kMaxAlignment = 64;

bool ValidAlignment(std::size_t alignment) noexcept {
  return alignment <= kMaxAlignment && std::has_single_bit(alignment);
}

[[noreturn]] void AbortPool(const char *message) noexcept {
  static_cast<void>(std::fputs(message, stderr));
  std::abort();
}

struct AllocationHeader {
  std::size_t charge = 0;
  std::size_t alignment = 0;
};

std::size_t BackingAlignment(std::size_t requested) noexcept {
  return std::max(requested, alignof(AllocationHeader));
}

std::size_t HeaderSpace(std::size_t backing_alignment) noexcept {
  return (sizeof(AllocationHeader) + backing_alignment - 1) & ~(backing_alignment - 1);
}

class FlatPool final : public MemoryPool {
public:
#ifndef DOQE_TEST_HOOKS
  explicit FlatPool(std::size_t limit) noexcept : limit_(limit) {}
#endif
  FlatPool(const FlatPool &) = delete;
  FlatPool(FlatPool &&) = delete;
  FlatPool &operator=(const FlatPool &) = delete;
  FlatPool &operator=(FlatPool &&) = delete;

#ifdef DOQE_TEST_HOOKS
  FlatPool(std::size_t limit, testing::PoolProbe probe) noexcept : limit_(limit), probe_(probe) {}
  void InjectLiveBytes(std::size_t live) noexcept { live_.store(live, std::memory_order_relaxed); }
#endif

  [[nodiscard]] Result<void *> Allocate(std::size_t charge,
                                        std::size_t alignment) noexcept override;
  void Free(void *data, std::size_t charge, std::size_t alignment) noexcept override;

  [[nodiscard]] std::size_t live_bytes() const noexcept override {
    return live_.load(std::memory_order_relaxed);
  }

  [[nodiscard]] std::size_t limit() const noexcept override { return limit_; }

private:
  ~FlatPool() override {
    if (live_bytes() != 0) {
      AbortPool("MemoryPool destroyed with live bytes\n");
    }
#ifdef DOQE_TEST_HOOKS
    if (probe_.destroyed != nullptr) {
      probe_.destroyed(probe_.context);
    }
#endif
  }

  [[nodiscard]] bool Record(void *data) noexcept;
  void FreeBacking(void *memory, std::size_t alignment) noexcept;

  std::size_t limit_;
  std::atomic_size_t live_{0};
#ifndef NDEBUG
  std::mutex registry_mutex_;
  std::unordered_set<void *> registry_;
#endif
#ifdef DOQE_TEST_HOOKS
  testing::PoolProbe probe_{};
#endif
};

Result<void *> FlatPool::Allocate(std::size_t charge, std::size_t alignment) noexcept {
  if (!ValidAlignment(alignment)) {
    return Fail(Code::InvalidArgument);
  }
  if (charge == 0) {
    return nullptr;
  }
  const auto actual_alignment = BackingAlignment(alignment);
  const auto prefix = HeaderSpace(actual_alignment);
  if (charge > std::numeric_limits<std::size_t>::max() - prefix) {
    return Fail(Code::ResourceExhausted);
  }
  auto live = live_bytes();
#ifdef DOQE_TEST_HOOKS
  if (probe_.before_reserve != nullptr) {
    probe_.before_reserve(probe_.context);
  }
#endif
  for (;;) {
    if (live > limit_) {
      AbortPool("MemoryPool live bytes exceed limit\n");
    }
    if (charge > limit_ - live) {
      return Fail(Code::ResourceExhausted);
    }
    if (live_.compare_exchange_weak(live, live + charge, std::memory_order_relaxed)) {
      break;
    }
  }

  void *memory = nullptr;
#ifdef DOQE_TEST_HOOKS
  if (probe_.fail_allocation == nullptr || !probe_.fail_allocation(probe_.context))
#endif
  {
    memory = ::operator new(prefix + charge, std::align_val_t{actual_alignment}, std::nothrow);
  }
  if (memory == nullptr) {
    live_.fetch_sub(charge, std::memory_order_relaxed);
    return std::unexpected(Status::OutOfMemory());
  }
  // Keep the header and the caller's raw storage in a single backing byte array.
  // Placement construction borrows the existing allocation.
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  auto *backing = ::new (memory) std::byte[prefix + charge];
  const AllocationHeader header{.charge = charge, .alignment = alignment};
  // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  auto *data = backing + prefix;
  std::memcpy(data - sizeof(header), &header, sizeof(header));
  // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  if (!Record(data)) {
    FreeBacking(memory, actual_alignment);
    live_.fetch_sub(charge, std::memory_order_relaxed);
    return std::unexpected(Status::OutOfMemory());
  }
  return data;
}

bool FlatPool::Record([[maybe_unused]] void *data) noexcept {
#ifdef DOQE_TEST_HOOKS
  if (probe_.fail_recording != nullptr && probe_.fail_recording(probe_.context)) {
    return false;
  }
#endif
#ifndef NDEBUG
  try {
    const std::scoped_lock guard(registry_mutex_);
    if (!registry_.insert(data).second) {
      AbortPool("MemoryPool recorded a live pointer twice\n");
    }
  } catch (...) {
    return false;
  }
#endif
  return true;
}

void FlatPool::FreeBacking(void *memory, std::size_t alignment) noexcept {
  ::operator delete(memory, std::align_val_t{alignment});
#ifdef DOQE_TEST_HOOKS
  if (probe_.backing_freed != nullptr) {
    probe_.backing_freed(probe_.context);
  }
#endif
}

void FlatPool::Free(void *data, std::size_t charge, std::size_t alignment) noexcept {
  if (data == nullptr) {
    if (charge != 0 || !ValidAlignment(alignment)) {
      AbortPool("MemoryPool size or alignment mismatch\n");
    }
    return;
  }

  auto *bytes = static_cast<std::byte *>(data);
  AllocationHeader recorded;
  {
#ifndef NDEBUG
    const std::scoped_lock guard(registry_mutex_);
    const auto entry = registry_.find(data);
    if (entry == registry_.end()) {
      AbortPool("MemoryPool unknown or already freed pointer\n");
    }
#endif
    // Free receives the original byte-array pointer returned by Allocate.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    std::memcpy(&recorded, bytes - sizeof(recorded), sizeof(recorded));
    if (recorded.charge != charge || recorded.alignment != alignment) {
      AbortPool("MemoryPool size or alignment mismatch\n");
    }
#ifndef NDEBUG
    registry_.erase(entry);
#endif
  }
  const auto actual_alignment = BackingAlignment(recorded.alignment);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  FreeBacking(bytes - HeaderSpace(actual_alignment), actual_alignment);
  live_.fetch_sub(recorded.charge, std::memory_order_relaxed);
}

} // namespace

#ifndef DOQE_TEST_HOOKS
PoolPtr::PoolPtr(const PoolPtr &other) noexcept : pool_(other.pool_) {
  if (pool_ != nullptr) {
    pool_->references_.fetch_add(1, std::memory_order_relaxed);
  }
}

PoolPtr::PoolPtr(PoolPtr &&other) noexcept : pool_(std::exchange(other.pool_, nullptr)) {}

PoolPtr &PoolPtr::operator=(const PoolPtr &other) noexcept { return *this = PoolPtr(other); }

PoolPtr &PoolPtr::operator=(PoolPtr &&other) noexcept {
  PoolPtr incoming(std::move(other));
  std::swap(pool_, incoming.pool_);
  return *this;
}

PoolPtr::~PoolPtr() noexcept {
  if (pool_ != nullptr && pool_->references_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
    delete pool_;
  }
}

PoolPtr PoolPtr::Adopt(MemoryPool *pool) noexcept { return PoolPtr(pool); }

PoolPtr PoolPtr::Retain(MemoryPool &pool) noexcept {
  pool.references_.fetch_add(1, std::memory_order_relaxed);
  return PoolPtr(&pool);
}

Result<PoolPtr> MakeFlatPool(std::size_t limit) noexcept {
  // Transfer the initial intrusive reference to PoolPtr.
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  auto *pool = new (std::nothrow) FlatPool(limit);
  if (pool == nullptr) {
    return std::unexpected(Status::OutOfMemory());
  }
  return PoolPtr::Adopt(pool);
}

#else
Result<PoolPtr> testing::MakeTestPool(std::size_t limit, PoolProbe probe) noexcept {
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  auto *pool = new (std::nothrow) FlatPool(limit, probe);
  if (pool == nullptr) {
    return std::unexpected(Status::OutOfMemory());
  }
  return PoolPtr::Adopt(pool);
}

void testing::InjectLiveBytes(MemoryPool &pool, std::size_t live) noexcept {
  dynamic_cast<FlatPool *>(&pool)->InjectLiveBytes(live);
}
#endif

} // namespace doqe::base
