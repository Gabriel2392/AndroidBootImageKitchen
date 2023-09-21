#pragma once

#include "utils.h"
#include <cstdint>
#include <filesystem>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

struct BootImageArgs {
  std::filesystem::path kernel;
  std::filesystem::path ramdisk;
  std::filesystem::path second;
  std::filesystem::path dtb;
  std::filesystem::path recovery_dtbo;
  std::string cmdline;
  uint32_t base = 0x10000000;
  uint32_t kernel_offset = 0x00008000;
  uint32_t ramdisk_offset = 0x01000000;
  uint32_t second_offset = 0x00f00000;
  uint32_t dtb_offset = 0x01f00000;
  utils::OSVersion os_version;
  uint32_t tags_offset = 0x00000100;
  std::string board;
  uint32_t page_size = 2048;
  uint32_t header_version = 4;
  std::filesystem::path output;
  //bool print_id = false;
};

// std::optional<BootImageArgs> ParseArguments(int argc, char* argv[]);
bool WriteBootImage(const BootImageArgs &args);
