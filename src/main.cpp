#include "build_version.hpp"
#include "version.hpp"

#include <cstdio>
#include <string_view>

int main(int argc, char **argv) {
  if (argc == 2 && std::string_view{argv[1]} == "--version") {
    const auto version =
        doqe::version_string(doqe::build_version, doqe::build_commit, doqe::build_dirty);
    std::puts(version.c_str());
    return 0;
  }

  std::fputs("Usage: doqe --version\n", stderr);
  return 1;
}
