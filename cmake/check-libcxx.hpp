#pragma once

#include <version>

#ifndef _LIBCPP_VERSION
#error "doqe requires libc++ headers from LLVM 23.1.5"
#else
static_assert(_LIBCPP_VERSION == 230105, "doqe requires libc++ headers from LLVM 23.1.5");
#endif
