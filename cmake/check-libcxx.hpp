#ifndef DOQE_CHECK_LIBCXX_HPP
#define DOQE_CHECK_LIBCXX_HPP

#include <version>

#ifndef _LIBCPP_VERSION
#error "doqe requires libc++ headers from LLVM 23.1.5"
#else
namespace doqe {

inline constexpr int pinned_libcxx_version = 230105;
static_assert(_LIBCPP_VERSION == pinned_libcxx_version,
              "doqe requires libc++ headers from LLVM 23.1.5");

} // namespace doqe
#endif

#endif
