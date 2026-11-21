#ifndef DOQE_BASE_TEST_PROBES_HPP
#define DOQE_BASE_TEST_PROBES_HPP

#include "doqe/base/memory_pool.hpp"

#include <cstddef>

namespace doqe::base::testing {

// The probe outlives the pool. Callbacks can run on a freeing thread.
struct PoolProbe {
  void *context = nullptr;
  void (*before_reserve)(void *) noexcept = nullptr;
  bool (*fail_allocation)(void *) noexcept = nullptr;
  bool (*fail_recording)(void *) noexcept = nullptr;
  void (*backing_freed)(void *) noexcept = nullptr;
  void (*destroyed)(void *) noexcept = nullptr;
};

[[nodiscard]] Result<PoolPtr> MakeTestPool(std::size_t limit, PoolProbe probe) noexcept;
void InjectLiveBytes(MemoryPool &pool, std::size_t live) noexcept;

} // namespace doqe::base::testing

#endif // DOQE_BASE_TEST_PROBES_HPP
