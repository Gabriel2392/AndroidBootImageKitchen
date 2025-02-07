#include "bootimg.h"

#include <unistd.h>

#include <array>
#include <sstream>
#include <vector>

#include "bootconfig.h"
#include "log.h"
#include "tools.h"
#include "utils.h"

namespace {
constexpr uint32_t BOOT_IMAGE_HEADER_V3_PAGESIZE = 4096;
constexpr uint32_t BOOT_MAGIC_SIZE = 8;
constexpr uint32_t SHA_LENGTH = 32;

struct ImageEntry {
  uint64_t offset;
  uint32_t size;
  std::string name;
};
}  // namespace

std::optional<BootImageInfo> UnpackBootImage(
    int fd, const std::filesystem::path &output_dir, bool dec_ramdisk) {
  BootImageInfo info;

  LOG("Working at: %s", output_dir.filename().c_str());

  // Read boot magic
  if (auto magic = utils::ReadString(fd, BOOT_MAGIC_SIZE)) {
    info.boot_magic = *magic;
  } else {
    return std::nullopt;
  }

  // Read kernel/ramdisk/second info (9 uint32_t)
  std::array<uint32_t, 9> kernel_ramdisk_second_info{};
  for (auto &val : kernel_ramdisk_second_info) {
    if (!utils::ReadU32(fd, val)) return std::nullopt;
  }

  info.header_version = kernel_ramdisk_second_info[8];

  // Legacy:
  // union {
  //     uint32_t header_version;
  //     uint32_t dt_size;
  // };
  if (info.header_version > 1024) {
    LOGE("Legacy boot images are not supported!!!");
    return std::nullopt;
  }
  LOG("Header version: %d", info.header_version);

  info.page_size = (info.header_version < 3) ? kernel_ramdisk_second_info[7]
                                             : BOOT_IMAGE_HEADER_V3_PAGESIZE;

  LOG("Page size: %d", info.page_size);

  // Handle version-specific fields
  uint32_t os_version_patch_level;
  if (info.header_version < 3) {
    info.kernel_size = kernel_ramdisk_second_info[0];
    info.kernel_load_address = kernel_ramdisk_second_info[1];
    info.ramdisk_size = kernel_ramdisk_second_info[2];
    info.ramdisk_load_address = kernel_ramdisk_second_info[3];
    info.second_size = kernel_ramdisk_second_info[4];
    LOG("Secondary bootloader size: %.2fMB",
        static_cast<float>(info.second_size) / 1024 / 1024);
    info.second_load_address = kernel_ramdisk_second_info[5];
    info.tags_load_address = kernel_ramdisk_second_info[6];

    if (!utils::ReadU32(fd, os_version_patch_level)) return std::nullopt;
  } else {
    info.kernel_size = kernel_ramdisk_second_info[0];
    info.ramdisk_size = kernel_ramdisk_second_info[1];
    os_version_patch_level = kernel_ramdisk_second_info[2];
  }

  LOG("Kernel size: %.2fMB",
      static_cast<float>(info.kernel_size) / 1024 / 1024);
  LOG("Ramdisk size: %.2fMB",
      static_cast<float>(info.ramdisk_size) / 1024 / 1024);

  auto [os_ver, os_patch] =
      utils::DecodeOsVersionPatchLevel(os_version_patch_level);
  info.os_version = os_ver.value_or("");
  info.os_patch_level = os_patch.value_or("");

  // Handle command line fields
  if (info.header_version < 3) {
    if (auto product = utils::ReadString(fd, 16)) {
      info.product_name = utils::CStr(*product);
      LOG("Board: %s", info.product_name.c_str());
    }
    if (auto cmdline = utils::ReadString(fd, 512)) {
      info.cmdline = utils::CStr(*cmdline);
    }

    off_t result = lseek64(fd, SHA_LENGTH, SEEK_CUR);
    if (result == -1) {
      LOGE("Error reading SHA1 checksum");
      return std::nullopt;
    }

    if (auto extra = utils::ReadString(fd, 1024)) {
      info.extra_cmdline = utils::CStr(*extra);
      LOG("Extra cmdline length: %d", info.extra_cmdline.length());
    }
  } else {
    if (auto cmdline = utils::ReadString(fd, 1536))
      info.cmdline = utils::CStr(*cmdline);
  }
  LOG("Cmdline length: %d", info.cmdline.length());
  std::string full_cmdline = info.cmdline;
  if (!info.extra_cmdline.empty()) {
    full_cmdline += " " + info.extra_cmdline;
  }

  // Handle version-specific extensions
  if (info.header_version == 1 || info.header_version == 2) {
    if (!utils::ReadU32(fd, info.recovery_dtbo_size)) return std::nullopt;
    LOG("Recovery DTBO size: %.2fMB",
        static_cast<float>(info.recovery_dtbo_size) / 1024 / 1024);
    if (!utils::ReadU64(fd, info.recovery_dtbo_offset)) return std::nullopt;
    if (!utils::ReadU32(fd, info.boot_header_size)) return std::nullopt;
  }

  if (info.header_version == 2) {
    if (!utils::ReadU32(fd, info.dtb_size)) return std::nullopt;
    LOG("DTB size: %.2fMB", static_cast<float>(info.dtb_size) / 1024 / 1024);
    if (!utils::ReadU64(fd, info.dtb_load_address)) return std::nullopt;
  }

  if (info.header_version >= 4) {
    if (!utils::ReadU32(fd, info.boot_signature_size)) return std::nullopt;
  }

  // Calculate image offsets
  std::vector<ImageEntry> image_entries;
  const uint32_t page_size = info.page_size;
  const uint32_t num_header_pages = 1;

  // Kernel
  uint32_t num_kernel_pages = GetNumberOfPages(info.kernel_size, page_size);
  if (info.kernel_size > 0) {  // Patch1: Only unpack kernel if it exists
    image_entries.emplace_back(page_size * num_header_pages, info.kernel_size,
                               "kernel");
  }

  // Ramdisk
  uint32_t num_ramdisk_pages = GetNumberOfPages(info.ramdisk_size, page_size);
  if (info.ramdisk_size > 0) {  // Patch2: Only unpack ramdisk if it exists
    off_t ramdisk_offset = page_size * (num_header_pages + num_kernel_pages);
    info.ramdisk_compression = FORMAT_OTHER;

    if (dec_ramdisk) {
      auto buf = utils::ReadNBytesAtOffsetX(fd, ramdisk_offset, 16);
      if (buf.empty()) {
        LOGE("Could not read ramdisk");
        return std::nullopt;
      }
      info.ramdisk_compression = getHeaderFormat(buf.data(), buf.size());
    }
    image_entries.emplace_back(ramdisk_offset, info.ramdisk_size, "ramdisk");
  }

  // Second
  if (info.second_size > 0) {
    image_entries.emplace_back(
        page_size * (num_header_pages + num_kernel_pages + num_ramdisk_pages),
        info.second_size, "second");
  }

  // Recovery DTBO
  if (info.recovery_dtbo_size > 0) {
    image_entries.emplace_back(info.recovery_dtbo_offset,
                               info.recovery_dtbo_size, "recovery_dtbo");
  }

  // DTB
  if (info.dtb_size > 0) {
    image_entries.emplace_back(
        page_size * (num_header_pages + num_kernel_pages + num_ramdisk_pages +
                     GetNumberOfPages(info.second_size, page_size) +
                     GetNumberOfPages(info.recovery_dtbo_size, page_size)),
        info.dtb_size, "dtb");
  }

  // Boot signature
  if (info.boot_signature_size > 0) {
    image_entries.emplace_back(
        page_size * (num_header_pages + num_kernel_pages + num_ramdisk_pages),
        info.boot_signature_size, "boot_signature");
  }

  // Extract images
  for (const auto &entry : image_entries) {
    const auto output_path = output_dir / entry.name;
    LOG("Extracting %s", entry.name.c_str());
    if (!utils::ExtractImage(fd, entry.offset, entry.size, output_path)) {
      return std::nullopt;
    }
  }

  // info.image_dir = output_dir;
  auto config_dir = output_dir / CONFIG_FILE;
  BootConfig::Write(info, config_dir.string());
  return info;
}
