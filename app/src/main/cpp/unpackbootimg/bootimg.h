#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "tools.h"
#include "utils.h"

struct BootImageInfo {
  std::string boot_magic;
  uint32_t header_version = 0;

  // Common fields
  uint32_t kernel_size = 0;
  uint32_t ramdisk_size = 0;
  uint8_t ramdisk_compression = FORMAT_OTHER;
  uint32_t page_size = 4096;
  std::string os_version;
  std::string os_patch_level;
  std::string cmdline;

  // Version <3 fields
  uint32_t kernel_load_address = 0;
  uint32_t ramdisk_load_address = 0;
  uint32_t second_size = 0;
  uint32_t second_load_address = 0;
  uint32_t tags_load_address = 0;
  std::string product_name;
  std::string extra_cmdline;

  // Version 1-2 fields
  uint32_t recovery_dtbo_size = 0;
  uint64_t recovery_dtbo_offset = 0;
  uint32_t boot_header_size = 0;

  // Version 2 fields
  uint32_t dtb_size = 0;
  uint64_t dtb_load_address = 0;

  // Version 4+ fields
  uint32_t boot_signature_size = 0;

  // std::filesystem::path image_dir;
};

std::optional<BootImageInfo> UnpackBootImage(
    int fd, const std::filesystem::path &output_dir, bool dec_ramdisk);
