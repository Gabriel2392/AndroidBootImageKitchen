#pragma once

#include "TinySHA1.hpp"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>
#include "tools.h"

namespace utils {

void WriteS32(std::ostream &stream, const std::string &value);
void WriteU32(std::ostream &stream, uint32_t value);
void WriteU64(std::ostream &stream, uint64_t value);

inline std::string UToS(uint32_t value) {
  std::string result;
  result.push_back(static_cast<char>((value >> 24) & 0xFF));
  result.push_back(static_cast<char>((value >> 16) & 0xFF));
  result.push_back(static_cast<char>((value >> 8) & 0xFF));
  result.push_back(static_cast<char>(value & 0xFF));
  return result;
}

struct FileWrapper {
  std::unique_ptr<std::istream> stream;
  size_t size = 0;
  explicit operator bool() const { return stream && stream->good(); }
};

std::optional<FileWrapper> OpenFile(const std::filesystem::path &path);
std::vector<uint8_t> ReadFileContents(FileWrapper &file);
size_t GetFileSize(FileWrapper &file);
size_t GetFileSize(std::optional<FileWrapper> &file);
size_t GetFileSize(const std::filesystem::path &path);
void PadFile(std::ostream &out, size_t padding);

class AsciizString {
  size_t max_length;

public:
  explicit AsciizString(size_t max_len) : max_length(max_len) {}
  std::optional<std::vector<char>> operator()(const std::string &s) const;
};

struct OSVersion {
  uint32_t version = 0;
  uint32_t patch_level = 0;
  std::string version_str = "";
  std::string patch_level_str = "";
  static void Parse(OSVersion &version);
};

} // namespace utils
