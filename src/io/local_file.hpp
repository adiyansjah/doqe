#ifndef DOQE_IO_LOCAL_FILE_HPP
#define DOQE_IO_LOCAL_FILE_HPP

#include "doqe/io/file_system.hpp"

namespace doqe::io {

// Internal stat-to-read step. The caller owns the descriptor.
[[nodiscard]] base::Result<std::string_view> ReadStatSize(int fd, std::size_t size,
                                                          base::Arena &arena) noexcept;

} // namespace doqe::io

#endif // DOQE_IO_LOCAL_FILE_HPP
