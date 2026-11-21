#ifndef DOQE_COLUMN_COLUMN_INTERNAL_HPP
#define DOQE_COLUMN_COLUMN_INTERNAL_HPP

#include "doqe/base/status.hpp"

#include <cstddef>

namespace doqe::column {

[[nodiscard]] base::Result<void> CheckUtf8DataSize(std::size_t size) noexcept;

} // namespace doqe::column

#endif // DOQE_COLUMN_COLUMN_INTERNAL_HPP
