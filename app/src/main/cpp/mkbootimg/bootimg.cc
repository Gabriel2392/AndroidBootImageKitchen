#include "bootimg.h"
#include "TinySHA1.hpp"
#include "utils.h"
#include <array>
#include <format>
#include <fstream>
#include <regex>

namespace {
constexpr uint32_t BOOT_MAGIC_SIZE = 8;
constexpr std::string_view BOOT_MAGIC = "ANDROID!";
constexpr uint32_t BOOT_IMAGE_HEADER_V1_SIZE = 1648;
constexpr uint32_t BOOT_IMAGE_HEADER_V2_SIZE = 1660;
constexpr uint32_t BOOT_IMAGE_HEADER_V3_SIZE = 1580;
constexpr uint32_t BOOT_IMAGE_HEADER_V4_SIZE = 1584;
constexpr uint32_t BOOT_NAME_SIZE = 16;
constexpr uint32_t BOOT_ARGS_SIZE = 512;
constexpr uint32_t BOOT_EXTRA_ARGS_SIZE = 1024;
constexpr uint32_t BOOT_IMAGE_HEADER_V3_PAGESIZE = 4096;

bool WriteHeaderV3Plus(std::ostream &out, const BootImageArgs &args) {
  const uint32_t header_size = args.header_version > 3
                                   ? BOOT_IMAGE_HEADER_V4_SIZE
                                   : BOOT_IMAGE_HEADER_V3_SIZE;

  out.write(BOOT_MAGIC.data(), BOOT_MAGIC_SIZE);
  auto kernel = utils::OpenFile(args.kernel);
  auto ramdisk = utils::OpenFile(args.ramdisk);

  utils::WriteU32(out, utils::GetFileSize(kernel));
  utils::WriteU32(out, utils::GetFileSize(ramdisk));

  utils::OSVersion os_version = args.os_version;
  utils::OSVersion::Parse(os_version);

  utils::WriteU32(out, (os_version.version << 11) | os_version.patch_level);
  utils::WriteU32(out, header_size);
  utils::WriteU32(out, 0); // reserved
  utils::WriteU32(out, 0);
  utils::WriteU32(out, 0);
  utils::WriteU32(out, 0);
  utils::WriteU32(out, args.header_version);

  std::vector<char> cmdline(args.cmdline.begin(), args.cmdline.end());
  cmdline.resize(BOOT_ARGS_SIZE + BOOT_EXTRA_ARGS_SIZE, 0);
  out.write(cmdline.data(), cmdline.size());

  if (args.header_version >= 4) {
    utils::WriteU32(out, 0); // boot_signature_size
  }

  utils::PadFile(out, BOOT_IMAGE_HEADER_V3_PAGESIZE);
  return true;
}

bool WriteLegacyHeader(std::ostream &out, const BootImageArgs &args) {
  const uint32_t ramdisk_load =
      !args.ramdisk.empty() ? args.base + args.ramdisk_offset : 0;
  const uint32_t second_load =
      !args.second.empty() ? args.base + args.second_offset : 0;

  out.write(BOOT_MAGIC.data(), BOOT_MAGIC_SIZE);

  auto kernel = utils::OpenFile(args.kernel);
  auto ramdisk = utils::OpenFile(args.ramdisk);
  auto second = utils::OpenFile(args.second);
  auto recovery_dtbo = utils::OpenFile(args.recovery_dtbo);
  auto dtb = utils::OpenFile(args.dtb);

  utils::WriteU32(out, utils::GetFileSize(kernel));

  utils::WriteU32(out, args.base + args.kernel_offset);
  utils::WriteU32(out, utils::GetFileSize(ramdisk));
  utils::WriteU32(out, ramdisk_load);
  utils::WriteU32(out, utils::GetFileSize(second));
  utils::WriteU32(out, second_load);
  utils::WriteU32(out, args.base + args.tags_offset);
  utils::WriteU32(out, args.page_size);
  utils::WriteU32(out, args.header_version);

  utils::OSVersion os_version = args.os_version;
  utils::OSVersion::Parse(os_version);

  utils::WriteU32(out, (os_version.version << 11) |
                           os_version.patch_level);

  std::vector<char> board(BOOT_NAME_SIZE, 0);
  std::copy_n(
      args.board.begin(),
      std::min(args.board.size(), static_cast<size_t>(BOOT_NAME_SIZE - 1)),
      board.begin());
  out.write(board.data(), board.size());

  std::vector<char> cmdline(BOOT_ARGS_SIZE, 0);
  std::copy_n(
      args.cmdline.begin(),
      std::min(args.cmdline.size(), static_cast<size_t>(BOOT_ARGS_SIZE - 1)),
      cmdline.begin());
  out.write(cmdline.data(), cmdline.size());

  sha1::SHA1 sha;
  constexpr std::array<uint8_t, 4> zero{0, 0, 0, 0};
  auto update_sha = [&](const std::filesystem::path &path) {
    if (!path.empty()) {
      if (auto file = utils::OpenFile(path)) {
        auto data = utils::ReadFileContents(*file);
        sha.processBytes(data.data(), data.size());

        uint32_t size = static_cast<uint32_t>(data.size());
        std::array<uint8_t, 4> size_bytes{
            static_cast<uint8_t>(size & 0xFF),
            static_cast<uint8_t>((size >> 8) & 0xFF),
            static_cast<uint8_t>((size >> 16) & 0xFF),
            static_cast<uint8_t>((size >> 24) & 0xFF)};
        sha.processBytes(size_bytes.data(), size_bytes.size());
      } else {
        sha.processBytes(zero.data(), zero.size());
      }
    } else {
      sha.processBytes(zero.data(), zero.size());
    }
  };

  update_sha(args.kernel);
  update_sha(args.ramdisk);
  update_sha(args.second);
  if (args.header_version > 0)
    update_sha(args.recovery_dtbo);
  if (args.header_version > 1)
    update_sha(args.dtb);

  uint32_t digest[5];
  sha.getDigest(digest);
  std::string digestStr;
  for (size_t i = 0; i < 5; ++i) {
    digestStr.append(utils::UToS(digest[i]));
  }
  utils::WriteS32(out, digestStr);

  std::vector<char> extra_cmdline(BOOT_EXTRA_ARGS_SIZE, 0);
  if (args.cmdline.size() > BOOT_ARGS_SIZE) {
    auto start = args.cmdline.begin() + BOOT_ARGS_SIZE;
    std::copy(start, args.cmdline.end(), extra_cmdline.begin());
  }
  out.write(extra_cmdline.data(), extra_cmdline.size());

  if (args.header_version > 0) {
    utils::WriteU32(out, utils::GetFileSize(recovery_dtbo));
    if (recovery_dtbo) {
      uint32_t num_header_pages = 1;
      uint32_t num_kernel_pages =
          GetNumberOfPages(utils::GetFileSize(kernel), args.page_size);
      uint32_t num_ramdisk_pages =
          GetNumberOfPages(utils::GetFileSize(ramdisk), args.page_size);
      uint32_t num_second_pages =
          GetNumberOfPages(utils::GetFileSize(second), args.page_size);
      uint64_t dtbo_offset =
          args.page_size * (num_header_pages + num_kernel_pages +
                            num_ramdisk_pages + num_second_pages);
      utils::WriteU64(out, dtbo_offset);
    } else {
      utils::WriteU64(out, 0);
    }
  }

  if (args.header_version == 1) {
    utils::WriteU32(out, BOOT_IMAGE_HEADER_V1_SIZE);
  } else if (args.header_version == 2) {
    utils::WriteU32(out, BOOT_IMAGE_HEADER_V2_SIZE);
  }

  if (args.header_version > 1) {
    if (utils::GetFileSize(dtb) == 0) {
      return false;
    }
    utils::WriteU32(out, utils::GetFileSize(dtb));
    utils::WriteU32(out, args.base + args.dtb_offset);
  }

  utils::PadFile(out, args.page_size);
  return true;
}
} // namespace

bool WriteBootImage(const BootImageArgs &args) {
  std::ofstream out(args.output, std::ios::binary);
  if (!out) {
      LOGE("Failed to open %s for writing", fs::path(args.output).filename().c_str());
      return false;
  }

  LOG("Building to: %s", fs::path(args.output).filename().c_str());

  if (args.header_version >= 3) {
    if (!WriteHeaderV3Plus(out, args))
      return false;
  } else {
    if (!WriteLegacyHeader(out, args))
      return false;
  }

  // Write kernel/ramdisk/second data
  auto write_section = [&](const auto &path) {
    if (!path.empty()) {
      auto file = utils::OpenFile(path);
      if (!file) return false;
      auto data = utils::ReadFileContents(*file);
      out.write(reinterpret_cast<const char *>(data.data()), data.size());
      utils::PadFile(out, args.page_size);
    }
    return true;
  };

  if (!write_section(args.kernel))
    return false;
  if (!write_section(args.ramdisk))
    return false;
  if (!write_section(args.second))
    return false;

  if (args.header_version > 0 && args.header_version < 3) {
    if (!write_section(args.recovery_dtbo))
      return false;
  }

  if (args.header_version == 2) {
    if (!write_section(args.dtb))
      return false;
  }

  return true;
}
