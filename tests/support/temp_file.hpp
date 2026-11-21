#ifndef DOQE_TESTS_SUPPORT_TEMP_FILE_HPP
#define DOQE_TESTS_SUPPORT_TEMP_FILE_HPP

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>

namespace doqe::test {

class TempFile {
public:
  explicit TempFile(std::string_view name = "metadata.json") {
    auto pattern = (std::filesystem::temp_directory_path() / "doqe-t0008-XXXXXX").string();
    if (mkdtemp(pattern.data()) == nullptr) {
      std::abort();
    }
    directory_ = pattern;
    path_ = directory_ / name;
  }

  TempFile(const TempFile &) = delete;
  TempFile &operator=(const TempFile &) = delete;
  TempFile(TempFile &&) = delete;
  TempFile &operator=(TempFile &&) = delete;
  ~TempFile() {
    std::error_code ignored;
    std::filesystem::remove(path_, ignored);
    std::filesystem::remove(directory_, ignored);
  }

  [[nodiscard]] bool Write(std::string_view contents) const {
    std::ofstream file(path_, std::ios::binary | std::ios::trunc);
    file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    file.close();
    return static_cast<bool>(file);
  }

  [[nodiscard]] const std::string &path() const { return path_.native(); }
  [[nodiscard]] const std::string &directory() const { return directory_.native(); }

private:
  std::filesystem::path directory_;
  std::filesystem::path path_;
};

} // namespace doqe::test

#endif
