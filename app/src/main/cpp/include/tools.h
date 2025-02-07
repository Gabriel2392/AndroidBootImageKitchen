#pragma once

#include <filesystem>
#include <string>

#include "log.h"

constexpr uint8_t FORMAT_NONE = 0;
constexpr uint8_t FORMAT_LZ4 = 1;
constexpr uint8_t FORMAT_GZIP = 2;
constexpr uint8_t FORMAT_LZMA = 3;
constexpr uint8_t FORMAT_OTHER = UINT8_MAX;
constexpr std::string_view CONFIG_FILE = ".parserconfig";

namespace fs = std::filesystem;

inline uint32_t GetNumberOfPages(uint32_t image_size, uint32_t page_size) {
  return (image_size + page_size - 1) / page_size;
}
fs::path get_unique_path(const fs::path& output_dir);
bool isCpioNewcHeader(const uint8_t* data, size_t size);
bool isGzipHeader(const uint8_t* data, size_t size);
bool isLz4LegacyHeader(const uint8_t* data, size_t size);
bool isLzmaHeader(const uint8_t* data, size_t size);
uint8_t getHeaderFormat(const uint8_t* data, size_t size);
