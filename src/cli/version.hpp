#ifndef DOQE_CLI_VERSION_HPP
#define DOQE_CLI_VERSION_HPP

#include <string>
#include <string_view>

namespace doqe::cli {

std::string version_string(std::string_view version, std::string_view commit, bool dirty);

} // namespace doqe::cli

#endif
