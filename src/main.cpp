#include "build_version.hpp"
#include "version.hpp"

#include <cstdio>
#include <string_view>

// Use the standard main signature, with argv indexed only after checking argc.
int main(int argc, char **argv) { // NOLINT(misc-const-correctness)
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  if (argc == 2 && std::string_view{argv[1]} == "--version") {
    const auto version =
        doqe::version_string(doqe::build_version, doqe::build_commit, doqe::build_dirty);
    static_cast<void>(std::puts(version.c_str()));
    return 0;
  }

  static_cast<void>(std::fputs("Usage: doqe --version\n", stderr));
  return 1;
}
