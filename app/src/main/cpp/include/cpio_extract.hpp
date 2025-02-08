#pragma once

#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>
#include <system_error>
#include <vector>

#include "log.h"
#include "tools.h"

namespace fs = std::filesystem;

bool ExtractCPIO(const std::filesystem::path &input,
                 const std::filesystem::path &output) noexcept {
  std::ifstream in(input.string(), std::ios::binary);
  if (!in) {
    LOGE("Error opening input file: %s", input.string().c_str());
    return false;
  }

  std::error_code ec;
  if (!fs::create_directory(output, ec)) {
    return false;
  }

  fs::path config_path = fs::path(output.string()) / CONFIG_FILE;
  std::ofstream config(config_path);
  if (!config) {
    LOGE("Error creating config file");
    return false;
  }

  while (true) {
    char header[110];
    if (!in.read(header, 110)) break;

    std::string magic(header, 6);
    if (magic != "070701") {
      LOGE("Unsupported format");
      return false;
    }

    auto read_field = [&](int offset) -> unsigned long {
      std::string field(header + offset, 8);
      return std::strtoul(field.c_str(), nullptr, 16);
    };

    unsigned long ino = read_field(6);
    unsigned long mode = read_field(14);
    unsigned long uid = read_field(22);
    unsigned long gid = read_field(30);
    unsigned long nlink = read_field(38);
    unsigned long mtime = read_field(46);
    unsigned long filesize = read_field(54);
    unsigned long devmajor = read_field(62);
    unsigned long devminor = read_field(70);
    unsigned long rdevmajor = read_field(78);
    unsigned long rdevminor = read_field(86);
    unsigned long namesize = read_field(94);
    unsigned long check = read_field(102);

    std::vector<char> namebuf(namesize);
    in.read(namebuf.data(), static_cast<std::streamsize>(namesize));
    std::string filename(namebuf.data(), namesize - 1);

    int header_and_name = static_cast<int>(110 + namesize);
    int pad = (4 - (header_and_name % 4)) % 4;
    in.ignore(pad);

    if (filename == "TRAILER!!!") break;

    fs::path outpath = output / filename;

    fs::create_directories(outpath.parent_path(), ec);

    mode_t file_mode = mode & 07777;
    mode_t file_type = mode & S_IFMT;

    char mode_str[6];
    std::snprintf(mode_str, sizeof(mode_str), "0%03o",
                  static_cast<unsigned int>(file_mode));

    if (file_type == S_IFDIR) {
      fs::create_directory(outpath, ec);
      config << "path=\"" << filename << "\" type=dir mode=" << mode_str
             << " uid=" << uid << " gid=" << gid << "\n";
    } else if (file_type == S_IFREG) {
      std::ofstream outfile(outpath, std::ios::binary);
      if (!outfile) {
        LOGE("Error creating file: %s", outpath.string().c_str());
        return false;
      }
      std::vector<char> filedata(filesize);
      in.read(filedata.data(), static_cast<std::streamsize>(filesize));
      outfile.write(filedata.data(), static_cast<std::streamsize>(filesize));
      outfile.close();
      config << "path=\"" << filename << "\" type=file mode=" << mode_str
             << " uid=" << uid << " gid=" << gid << "\n";
    } else if (file_type == S_IFLNK) {
      std::vector<char> linkdata(filesize);
      in.read(linkdata.data(), static_cast<std::streamsize>(filesize));
      std::string target(linkdata.data(), filesize);
      config << "path=\"" << filename << "\" type=symlink mode=" << mode_str
             << " uid=" << uid << " gid=" << gid << " target=\"" << target
             << "\"\n";
    } else {
      LOGE("Unsupported file type");
      in.ignore(static_cast<std::streamsize>(filesize));
    }

    int pad2 = static_cast<std::streamsize>((4 - (filesize % 4)) % 4);
    in.ignore(pad2);
  }
  return true;
}
