#include "doqe/base/status.hpp"
#include "doqe/base/utf8.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <memory>
#include <new>
#include <span>
#include <string_view>
#include <utility>

namespace doqe::base {
namespace {

constexpr std::size_t kDetailLimit = 256;

} // namespace

// RFC 3629 permits only scalar values encoded in their shortest form.
// The loop checks each index and code point width against the prefix limit.
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
std::size_t Utf8Prefix(std::string_view text, std::size_t limit) noexcept {
  const auto end = std::min(text.size(), limit);
  std::size_t index = 0;
  while (index < end) {
    const auto lead = static_cast<unsigned char>(text[index]);
    if (lead <= 0x7f) {
      ++index;
      continue;
    }
    if (lead < 0xc2 || lead > 0xf4) {
      break;
    }
    std::size_t width = 2;
    if (lead >= 0xf0) {
      width = 4;
    } else if (lead >= 0xe0) {
      width = 3;
    }
    if (width > end - index) {
      break;
    }
    for (std::size_t i = 1; i < width; ++i) {
      const auto byte = static_cast<unsigned char>(text[index + i]);
      if (byte < 0x80 || byte > 0xbf) {
        return index;
      }
      if (i == 1 && ((lead == 0xe0 && byte < 0xa0) || (lead == 0xed && byte > 0x9f) ||
                     (lead == 0xf0 && byte < 0x90) || (lead == 0xf4 && byte > 0x8f))) {
        return index;
      }
    }
    index += width;
  }
  return index;
}
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

struct Status::Detail {
  std::atomic_size_t references{1};
  std::size_t message_size = 0;
  std::size_t context_size = 0;
  std::array<char, kDetailLimit> bytes{};
};

std::string_view CodeName(Code code) noexcept {
  switch (code) {
  case Code::Cancelled:
    return "Cancelled";
  case Code::DeadlineExceeded:
    return "DeadlineExceeded";
  case Code::ResourceExhausted:
    return "ResourceExhausted";
  case Code::OutOfMemory:
    return "OutOfMemory";
  case Code::CommitConflict:
    return "CommitConflict";
  case Code::CommitStateUnknown:
    return "CommitStateUnknown";
  case Code::NotFound:
    return "NotFound";
  case Code::AlreadyExists:
    return "AlreadyExists";
  case Code::InvalidArgument:
    return "InvalidArgument";
  case Code::Unsupported:
    return "Unsupported";
  case Code::IoError:
    return "IoError";
  case Code::Internal:
    return "Internal";
  }
  return "Unknown";
}

Status::Status(Code code, std::string_view message, std::string_view context) noexcept
    : code_(code) {
  if (code == Code::OutOfMemory) {
    return;
  }
  const auto message_size = Utf8Prefix(message, kDetailLimit);
  const auto context_size = Utf8Prefix(context, kDetailLimit - message_size);
  if (message_size + context_size == 0) {
    return;
  }
  auto *memory = ::operator new(sizeof(Detail), std::nothrow);
  if (memory == nullptr) {
    return;
  }
  detail_ = std::construct_at(static_cast<Detail *>(memory));
  detail_->message_size = message_size;
  detail_->context_size = context_size;
  if (message_size != 0) {
    std::memcpy(detail_->bytes.data(), message.data(), message_size);
  }
  if (context_size != 0) {
    std::memcpy(std::span(detail_->bytes).subspan(message_size).data(), context.data(),
                context_size);
  }
}

Status::Status(const Status &other) noexcept : detail_(other.detail_), code_(other.code_) {
  if (detail_ != nullptr) {
    detail_->references.fetch_add(1, std::memory_order_relaxed);
  }
}

Status::Status(Status &&other) noexcept
    : detail_(std::exchange(other.detail_, nullptr)), code_(other.code_) {}

Status &Status::operator=(const Status &other) noexcept { return *this = Status(other); }

Status &Status::operator=(Status &&other) noexcept {
  Status incoming(std::move(other));
  std::swap(detail_, incoming.detail_);
  std::swap(code_, incoming.code_);
  return *this;
}

Status::~Status() noexcept {
  if (detail_ != nullptr && detail_->references.fetch_sub(1, std::memory_order_acq_rel) == 1) {
    std::destroy_at(detail_);
    ::operator delete(detail_);
  }
}

Status Status::OutOfMemory() noexcept { return Status(Code::OutOfMemory); }

std::string_view Status::message() const noexcept {
  if (detail_ == nullptr) {
    return {};
  }
  return {detail_->bytes.data(), detail_->message_size};
}

std::string_view Status::context() const noexcept {
  if (detail_ == nullptr) {
    return {};
  }
  return {std::span<const char>(detail_->bytes).subspan(detail_->message_size).data(),
          detail_->context_size};
}

} // namespace doqe::base
