#include "utils.h"

#include <unistd.h>

#include <cstdio>
#include <system_error>

#include "log.h"

namespace utils {

bool CreateDirectory(const std::filesystem::path &dir_path) {
  std::error_code ec;
  std::filesystem::create_directories(dir_path, ec);
  return !ec;
}

bool ExtractImage(int fd, uint64_t offset, uint64_t size,
                  const std::filesystem::path &output_path) {
  if (lseek64(fd, static_cast<int64_t>(offset), SEEK_SET) == -1) {
    LOGE("Error seeking to %lld", offset);
    return false;
  }

  std::vector<char> buffer(size);
  uint64_t bytesRead = read(fd, buffer.data(), size);
  if (bytesRead != size) {
    LOGE("Expected %lld bytes, read %lld", size, bytesRead);
    return false;
  }

  std::ofstream output(output_path, std::ios::binary);
  if (!output) {
    LOGE("Error opening %s", output_path.string().c_str());
    return false;
  }

  if (!output.write(buffer.data(), static_cast<std::streamsize>(size)).good()) {
    LOGE("Error writing to %s", output_path.string().c_str());
    return false;
  }

  return true;
}

std::string CStr(std::string_view s) {
  if (auto pos = s.find('\0'); pos != s.npos)
    return std::string(s.substr(0, pos));
  return std::string(s);
}

std::optional<std::string> FormatOsVersion(uint32_t os_version) {
  if (os_version == 0) return std::nullopt;
  uint32_t a = os_version >> 14;
  uint32_t b = (os_version >> 7) & 0x7F;
  uint32_t c = os_version & 0x7F;
  return std::to_string(a) + "." + std::to_string(b) + "." + std::to_string(c);
}

std::optional<std::string> FormatOsPatchLevel(uint32_t os_patch_level) {
  if (os_patch_level == 0) return std::nullopt;
  uint32_t y = (os_patch_level >> 4) + 2000;
  uint32_t m = os_patch_level & 0x0F;
  char buffer[8];
  std::snprintf(buffer, sizeof(buffer), "%04d-%02d", y, m);
  return std::string(buffer);
}

OsVersionPatchLevel DecodeOsVersionPatchLevel(uint32_t os_version_patch_level) {
  return {FormatOsVersion(os_version_patch_level >> 11),
          FormatOsPatchLevel(os_version_patch_level & 0x7FF)};
}

bool ReadU32(int fd, uint32_t &value) {
  uint8_t bytes[4];
  uint64_t bytesRead = read(fd, reinterpret_cast<char *>(bytes), sizeof(bytes));
  if (bytesRead != sizeof(bytes)) {
    LOGE("Error reading 4 bytes at offset %lld", lseek64(fd, 0, SEEK_CUR));
    return false;
  }

  value = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24);
  return true;
}

bool ReadU64(int fd, uint64_t &value) {
  uint8_t bytes[8];
  uint64_t bytesRead = read(fd, reinterpret_cast<char *>(bytes), sizeof(bytes));
  if (bytesRead != sizeof(bytes)) {
    LOGE("Error reading 8 bytes at offset %lld", lseek64(fd, 0, SEEK_CUR));
    return false;
  }

  value = static_cast<uint64_t>(bytes[0]) |
          (static_cast<uint64_t>(bytes[1]) << 8) |
          (static_cast<uint64_t>(bytes[2]) << 16) |
          (static_cast<uint64_t>(bytes[3]) << 24) |
          (static_cast<uint64_t>(bytes[4]) << 32) |
          (static_cast<uint64_t>(bytes[5]) << 40) |
          (static_cast<uint64_t>(bytes[6]) << 48) |
          (static_cast<uint64_t>(bytes[7]) << 56);
  return true;
}

std::optional<std::string> ReadString(int fd, size_t length) {
  std::string s(length, '\0');
  uint64_t bytesRead = read(fd, s.data(), length);
  if (bytesRead != length) {
    LOGE("Error reading %lld bytes at offset %lld", length,
         lseek64(fd, 0, SEEK_CUR));
    return std::nullopt;
  }
  return CStr(s);
}

bool ReadString(int fd, size_t length, std::string &out) {
  std::string s(length, '\0');
  uint64_t bytesRead = read(fd, s.data(), length);
  if (bytesRead != length) {
    LOGE("Error reading %lld bytes at offset %lld", length,
         lseek64(fd, 0, SEEK_CUR));
    return false;
  }
  out = CStr(s);
  return true;
}

std::vector<uint8_t> ReadNBytesAtOffsetX(int fd, off_t offset,
                                         size_t numBytes) {
  off_t currentPos = lseek(fd, 0, SEEK_CUR);
  if (currentPos == (off_t)-1) {
    return {};
  }

  if (lseek(fd, offset, SEEK_SET) == (off_t)-1) {
    return {};
  }

  std::vector<uint8_t> buffer(numBytes);
  ssize_t bytesRead = read(fd, buffer.data(), numBytes);
  if (bytesRead < 0) {
    lseek(fd, currentPos, SEEK_SET);
    return {};
  }
  buffer.resize(static_cast<size_t>(bytesRead));

  if (lseek(fd, currentPos, SEEK_SET) == (off_t)-1) {
    return {};
  }
  return buffer;
}

}  // namespace utils
