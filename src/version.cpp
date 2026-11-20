#include "version.hpp"

namespace doqe {

std::string version_string(std::string_view version, std::string_view commit,
                           bool dirty) {
  std::string result = "doqe ";
  result += version;
  result += " (";
  result += commit;
  if (dirty) {
    result += "-dirty";
  }
  result += ')';
  return result;
}

}  // namespace doqe
