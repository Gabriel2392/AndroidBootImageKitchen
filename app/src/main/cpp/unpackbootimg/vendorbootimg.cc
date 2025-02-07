#include "vendorbootimg.h"

#include <unistd.h>

#include <array>
#include <sstream>

#include "log.h"
#include "tools.h"
#include "utils.h"
#include "vendorbootconfig.h"

namespace {
constexpr uint32_t VENDOR_RAMDISK_NAME_SIZE = 32;
}

std::optional<VendorBootImageInfo> UnpackVendorBootImage(
    int fd, const std::filesystem::path &output_dir, bool dec_ramdisk) {
  VendorBootImageInfo info;

  LOG("Working at: %s", output_dir.c_str());

  // Read header fields
  if (!(utils::ReadString(fd, 8, info.boot_magic) &&
        utils::ReadU32(fd, info.header_version) &&
        utils::ReadU32(fd, info.page_size) &&
        utils::ReadU32(fd, info.kernel_load_address) &&
        utils::ReadU32(fd, info.ramdisk_load_address) &&
        utils::ReadU32(fd, info.vendor_ramdisk_size) &&
        utils::ReadString(fd, 2048, info.cmdline) &&
        utils::ReadU32(fd, info.tags_load_address) &&
        utils::ReadString(fd, 16, info.product_name) &&
        utils::ReadU32(fd, info.header_size) &&
        utils::ReadU32(fd, info.dtb_size) &&
        utils::ReadU64(fd, info.dtb_load_address))) {
    return std::nullopt;
  }

  LOG("Header version: %d", info.header_version);
  LOG("Page size: %d", info.page_size);
  LOG("Ramdisk(s) total size: %.2fMB",
      static_cast<float>(info.vendor_ramdisk_size) / 1024 / 1024);
  LOG("Board: %s", info.product_name.c_str());
  LOG("Cmdline length: %d", info.cmdline.length());
  LOG("DTB size: %.2fMB", static_cast<float>(info.dtb_size) / 1024 / 1024);

  info.cmdline = utils::CStr(info.cmdline);
  info.product_name = utils::CStr(info.product_name);

  // Handle version >3 fields
  if (info.header_version > 3) {
    if (!(utils::ReadU32(fd, info.vendor_ramdisk_table_size) &&
          utils::ReadU32(fd, info.vendor_ramdisk_table_entry_num) &&
          utils::ReadU32(fd, info.vendor_ramdisk_table_entry_size) &&
          utils::ReadU32(fd, info.vendor_bootconfig_size))) {
      return std::nullopt;
    }
    LOG("Bootconfig size: %d", info.vendor_bootconfig_size);
  }

  // Calculate offsets
  const uint32_t page_size = info.page_size;
  const uint32_t num_header_pages =
      GetNumberOfPages(info.header_size, page_size);
  const uint32_t ramdisk_offset_base = page_size * num_header_pages;

  std::vector<utils::ImageEntry> image_entries;
  std::vector<std::pair<std::string, std::string>> vendor_ramdisk_symlinks;

  // Handle vendor ramdisk table
  if (info.header_version > 3) {
    const uint32_t table_pages =
        GetNumberOfPages(info.vendor_ramdisk_table_size, page_size);
    const uint64_t table_offset =
        page_size * (num_header_pages +
                     GetNumberOfPages(info.vendor_ramdisk_size, page_size) +
                     GetNumberOfPages(info.dtb_size, page_size));

    for (uint32_t i = 0; i < info.vendor_ramdisk_table_entry_num; ++i) {
      const uint64_t entry_offset =
          table_offset + (info.vendor_ramdisk_table_entry_size * i);
      if (lseek64(fd, static_cast<int64_t>(entry_offset), SEEK_SET) == -1) {
        LOGE("Error seeking in file");
        return std::nullopt;
      }

      VendorRamdiskTableEntry entry;
      if (!(utils::ReadU32(fd, entry.size) &&
            utils::ReadU32(fd, entry.offset) &&
            utils::ReadU32(fd, entry.type) &&
            utils::ReadString(fd, VENDOR_RAMDISK_NAME_SIZE, entry.name) &&
            utils::ReadU32Array(fd, entry.board_id))) {
        return std::nullopt;
      }

      entry.output_name = std::format("vendor_ramdisk{:02}", i);
      entry.name = utils::CStr(entry.name);

      entry.ramdisk_compression = FORMAT_OTHER;

      if (dec_ramdisk) {
        auto buf = utils::ReadNBytesAtOffsetX(
            fd, ramdisk_offset_base + entry.offset, 16);
        if (buf.empty()) {
          LOGE("Could not read %s", entry.output_name.c_str());
          return std::nullopt;
        }
        entry.ramdisk_compression = getHeaderFormat(buf.data(), buf.size());
      }

      image_entries.emplace_back(ramdisk_offset_base + entry.offset, entry.size,
                                 entry.output_name);

      vendor_ramdisk_symlinks.emplace_back(entry.output_name, entry.name);
      info.vendor_ramdisk_table.push_back(std::move(entry));
    }

    // Handle bootconfig
    const uint64_t bootconfig_offset =
        page_size * (num_header_pages +
                     GetNumberOfPages(info.vendor_ramdisk_size, page_size) +
                     GetNumberOfPages(info.dtb_size, page_size) + table_pages);
    image_entries.emplace_back(bootconfig_offset, info.vendor_bootconfig_size,
                               "bootconfig");
  } else {
    image_entries.emplace_back(ramdisk_offset_base, info.vendor_ramdisk_size,
                               "vendor_ramdisk");
  }

  // Handle DTB
  if (info.dtb_size > 0) {
    const uint64_t dtb_offset =
        page_size * (num_header_pages +
                     GetNumberOfPages(info.vendor_ramdisk_size, page_size));
    image_entries.emplace_back(dtb_offset, info.dtb_size, "dtb");
  }

  // Extract images
  for (const auto &entry : image_entries) {
    const auto output_path = output_dir / entry.name;
    LOG("Extracting %s", entry.name.c_str());
    if (!utils::ExtractImage(fd, entry.offset, entry.size, output_path)) {
      return std::nullopt;
    }
  }

  auto config_dir = output_dir / CONFIG_FILE;
  VendorBootConfig::Write(info, config_dir.string());
  return info;
}
