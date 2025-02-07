#pragma once
#include "../unpackbootimg/bootimg.h"
#include "log.h"

class BootConfig {
 public:
  static bool Write(const BootImageInfo& info, const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
      LOGE("Failed to open config for writing");
      return false;
    }

    WriteString(file, info.boot_magic);
    WriteU32(file, info.header_version);
    WriteU32(file, info.kernel_size);
    WriteU32(file, info.ramdisk_size);
    WriteU8(file, info.ramdisk_compression);
    WriteU32(file, info.page_size);
    WriteString(file, info.os_version);
    WriteString(file, info.os_patch_level);
    WriteString(file, info.cmdline);

    WriteU32(file, info.kernel_load_address);
    WriteU32(file, info.ramdisk_load_address);
    WriteU32(file, info.second_size);
    WriteU32(file, info.second_load_address);
    WriteU32(file, info.tags_load_address);

    WriteString(file, info.product_name);
    WriteString(file, info.extra_cmdline);

    WriteU32(file, info.recovery_dtbo_size);
    WriteU64(file, info.recovery_dtbo_offset);
    WriteU32(file, info.boot_header_size);

    WriteU32(file, info.dtb_size);
    WriteU64(file, info.dtb_load_address);
    WriteU32(file, info.boot_signature_size);

    if (!file.good()) {
      LOGE("Error occurred while writing configuration");
      return false;
    }
    return true;
  }

  static bool Read(BootImageInfo& info, const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
      LOGE("Failed to open configuration for reading");
      return false;
    }

    ReadString(file, info.boot_magic);
    ReadU32(file, info.header_version);
    ReadU32(file, info.kernel_size);
    ReadU32(file, info.ramdisk_size);
    ReadU8(file, info.ramdisk_compression);
    ReadU32(file, info.page_size);
    ReadString(file, info.os_version);
    ReadString(file, info.os_patch_level);
    ReadString(file, info.cmdline);

    ReadU32(file, info.kernel_load_address);
    ReadU32(file, info.ramdisk_load_address);
    ReadU32(file, info.second_size);
    ReadU32(file, info.second_load_address);
    ReadU32(file, info.tags_load_address);

    ReadString(file, info.product_name);
    ReadString(file, info.extra_cmdline);

    ReadU32(file, info.recovery_dtbo_size);
    ReadU64(file, info.recovery_dtbo_offset);
    ReadU32(file, info.boot_header_size);

    ReadU32(file, info.dtb_size);
    ReadU64(file, info.dtb_load_address);
    ReadU32(file, info.boot_signature_size);

    if (!file.good()) {
      LOGE("Error occurred while reading");
      return false;
    }
    return true;
  }

 private:
  static void WriteU8(std::ostream& os, uint8_t value) {
    os.write(reinterpret_cast<const char*>(&value), sizeof(value));
  }

  static void WriteU32(std::ostream& os, uint32_t value) {
    os.write(reinterpret_cast<const char*>(&value), sizeof(value));
  }

  static void WriteU64(std::ostream& os, uint64_t value) {
    os.write(reinterpret_cast<const char*>(&value), sizeof(value));
  }

  static void WriteString(std::ostream& os, const std::string& s) {
    auto size = static_cast<uint32_t>(s.size());
    WriteU32(os, size);
    os.write(s.data(), static_cast<std::streamsize>(size));
  }

  static void ReadU8(std::istream& is, uint8_t& value) {
    is.read(reinterpret_cast<char*>(&value), sizeof(value));
  }

  static void ReadU32(std::istream& is, uint32_t& value) {
    is.read(reinterpret_cast<char*>(&value), sizeof(value));
  }

  static void ReadU64(std::istream& is, uint64_t& value) {
    is.read(reinterpret_cast<char*>(&value), sizeof(value));
  }

  static void ReadString(std::istream& is, std::string& s) {
    uint32_t size;
    ReadU32(is, size);
    s.resize(size);
    is.read(&s[0], static_cast<std::streamsize>(size));
  }
};