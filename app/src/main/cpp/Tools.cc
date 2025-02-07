#include "tools.h"

namespace fs = std::filesystem;

fs::path get_unique_path(const fs::path& output_dir) {
  fs::path candidate = output_dir;
  if (!fs::exists(candidate)) {
    return candidate;
  }

  uint64_t count = 1;
  while (true) {
    std::string new_name = output_dir.string() + "_" + std::to_string(count);
    candidate = output_dir / new_name;
    if (!fs::exists(candidate) || count == UINT64_MAX) {
      return candidate;
    }
    ++count;
  }
}

bool isCpioNewcHeader(const uint8_t* data, size_t size) {
  if (size < 6) {
    return false;
  }

  if (std::memcmp(data, "070701", 6) == 0 ||
      std::memcmp(data, "070702", 6) == 0) {
    return true;
  }
  return false;
}

bool isGzipHeader(const uint8_t* data, size_t size) {
  if (size < 2) {
    return false;
  }
  return (data[0] == 0x1F && data[1] == 0x8B);
}

bool isLz4LegacyHeader(const uint8_t* data, size_t size) {
  if (size < 4) {
    return false;
  }
  uint32_t magic = static_cast<uint32_t>(data[0]) |
                   (static_cast<uint32_t>(data[1]) << 8) |
                   (static_cast<uint32_t>(data[2]) << 16) |
                   (static_cast<uint32_t>(data[3]) << 24);
  return (magic == 0x184C2102);
}

// Based on
// https://sourceforge.net/p/sevenzip/discussion/45797/thread/ec3effac/#0d3a/610e
bool isLzmaHeader(const uint8_t* data, size_t size) {
  if (size < 13) {
    return false;
  }

  if (data[0] >= 225) {
    return false;
  }

  uint32_t dictSize = static_cast<uint32_t>(data[1]) |
                      (static_cast<uint32_t>(data[2]) << 8) |
                      (static_cast<uint32_t>(data[3]) << 16) |
                      (static_cast<uint32_t>(data[4]) << 24);
  if (dictSize == 0) {
    return false;
  }
  if ((dictSize & (dictSize - 1)) != 0) {
    return false;
  }

  uint64_t uncompressedSize = 0;
  for (int i = 0; i < 8; ++i) {
    uncompressedSize |= static_cast<uint64_t>(data[5 + i]) << (8 * i);
  }

  const uint64_t MAX_UNCOMPRESSED_SIZE =
      (1ULL << 32);  // why the hell would a ramdisk be bigger than 4gb.
  if (uncompressedSize != 0xFFFFFFFFFFFFFFFFULL &&
      uncompressedSize > MAX_UNCOMPRESSED_SIZE) {
    return false;
  }

  return true;
}

uint8_t getHeaderFormat(const uint8_t* data, size_t size) {
  if (isCpioNewcHeader(data, size)) {
    return FORMAT_NONE;
  } else if (isLz4LegacyHeader(data, size)) {
    return FORMAT_LZ4;
  } else if (isGzipHeader(data, size)) {
    return FORMAT_GZIP;
  } else if (isLzmaHeader(data, size)) {
    return FORMAT_LZMA;
  }
  return FORMAT_OTHER;
}