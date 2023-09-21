#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>
#include "tools.h"

namespace utils {

bool CreateDirectory(const std::filesystem::path &dir_path);
bool ExtractImage(int fd, uint64_t offset, uint64_t size,
                  const std::filesystem::path &output_path);

inline std::string toHexString(std::string_view data) {
  std::string result = "0x";
  for (unsigned char c : data) {
    result += std::format("{:02X}", c);
  }
  return result;
}

std::string CStr(std::string_view s);
std::optional<std::string> FormatOsVersion(uint32_t os_version);
std::optional<std::string> FormatOsPatchLevel(uint32_t os_patch_level);

struct OsVersionPatchLevel {
  std::optional<std::string> os_version;
  std::optional<std::string> os_patch_level;
};
OsVersionPatchLevel DecodeOsVersionPatchLevel(uint32_t os_version_patch_level);

bool ReadU32(int fd, uint32_t &value);
bool ReadU64(int fd, uint64_t &value);
std::optional<std::string> ReadString(int fd, size_t length);
bool ReadString(int fd, size_t length, std::string &out);
std::vector<uint8_t> ReadNBytesAtOffsetX(int fd, off_t offset, size_t numBytes);

template <size_t N>
bool ReadU32Array(int fd, std::array<uint32_t, N> &arr) {
  for (auto &val : arr) {
    if (!ReadU32(fd, val))
      return false;
  }
  return true;
}

struct ImageEntry {
  uint64_t offset;
  uint32_t size;
  std::string name;

  ImageEntry(uint64_t o, uint32_t s, std::string n)
      : offset(o), size(s), name(std::move(n)) {}
};

} // namespace utils
