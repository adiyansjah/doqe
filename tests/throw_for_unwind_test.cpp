#include <stdexcept>

void throw_for_unwind_test() {
  throw std::runtime_error{"unwind test"};
}
