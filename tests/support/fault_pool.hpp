#ifndef DOQE_TESTS_SUPPORT_FAULT_POOL_HPP
#define DOQE_TESTS_SUPPORT_FAULT_POOL_HPP

#include "doqe/base/memory_pool.hpp"

#include <cstddef>
#include <memory>
#include <utility>

namespace doqe::test {

class FaultPool final : public base::MemoryPool {
public:
  explicit FaultPool(base::PoolPtr backing) : backing_(std::move(backing)) {}

  void FailAt(std::size_t call, base::Code code) noexcept {
    fail_at_ = call;
    code_ = code;
    calls_ = 0;
  }

  [[nodiscard]] std::size_t calls() const noexcept { return calls_; }

  [[nodiscard]] base::Result<void *> Allocate(std::size_t charge,
                                              std::size_t alignment) noexcept override {
    if (++calls_ == fail_at_) {
      return base::Fail(code_);
    }
    return backing_->Allocate(charge, alignment);
  }

  void Free(void *data, std::size_t charge, std::size_t alignment) noexcept override {
    backing_->Free(data, charge, alignment);
  }

  [[nodiscard]] std::size_t live_bytes() const noexcept override { return backing_->live_bytes(); }
  [[nodiscard]] std::size_t limit() const noexcept override { return backing_->limit(); }

private:
  base::PoolPtr backing_;
  std::size_t fail_at_ = 0;
  std::size_t calls_ = 0;
  base::Code code_ = base::Code::OutOfMemory;
};

} // namespace doqe::test

#endif
