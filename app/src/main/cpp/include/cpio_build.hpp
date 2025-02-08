#pragma once

#include <sys/stat.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "log.h"
#include "tools.h"

namespace fs = std::filesystem;

bool BuildCPIO(const std::filesystem::path &input,
               const std::filesystem::path &output) noexcept {
  fs::path config_path = input / CONFIG_FILE;
  std::ifstream config(config_path);
  if (!config) {
    LOGE("Error opening config file: %s", config_path.c_str());
    return false;
  }

  std::ofstream cpio_out(output.string(), std::ios::binary);
  if (!cpio_out) {
    LOGE("Error creating output file: %s", output.c_str());
    return false;
  }

  std::error_code ec;
  errno = 0;
  std::string line;
  while (std::getline(config, line)) {
    std::map<std::string, std::string> entry;
    size_t pos = 0;
    while (pos < line.size()) {
      while (pos < line.size() && std::isspace(line[pos])) {
        pos++;
      }
      if (pos >= line.size()) break;

      size_t eq_pos = line.find('=', pos);
      if (eq_pos == std::string::npos) break;
      std::string key = line.substr(pos, eq_pos - pos);
      pos = eq_pos + 1;

      if (pos < line.size() && line[pos] == '"') {
        pos++;
        size_t end_quote = line.find('"', pos);
        if (end_quote == std::string::npos) {
          LOGE("Error: Unterminated quote in config line.");
          return false;
        }
        std::string value = line.substr(pos, end_quote - pos);
        entry[key] = value;
        pos = end_quote + 1;
      } else {
        size_t value_end = line.find_first_of(" \t", pos);
        if (value_end == std::string::npos) {
          value_end = line.size();
        }
        std::string value = line.substr(pos, value_end - pos);
        entry[key] = value;
        pos = value_end;
      }
    }

    std::string path = entry["path"];
    std::string type = entry["type"];
    std::string mode_str = entry["mode"];
    unsigned long uid = std::strtoul(entry["uid"].c_str(), nullptr, 10);
    unsigned long gid = std::strtoul(entry["gid"].c_str(), nullptr, 10);

    auto permissions = static_cast<mode_t>(std::strtoul(mode_str.c_str(), nullptr, 8));
    mode_t file_type = 0;
    unsigned long filesize = 0;
    unsigned long nlink = 1;
    std::string target;

    if (type == "dir") {
      file_type = S_IFDIR;
      nlink = 2;
      if (permissions == 0000) permissions = 0755;
    } else if (type == "file") {
      file_type = S_IFREG;
      fs::path file_path = input / path;
      if (!fs::exists(file_path, ec)) {
        LOGE("File not found: ", file_path.c_str());
        return false;
      }
      filesize = fs::file_size(file_path, ec);
      if (permissions == 0000) permissions = 0754;
    } else if (type == "symlink") {
      file_type = S_IFLNK;
      target = entry["target"];
      filesize = target.size();
      if (permissions == 0000) permissions = 0754;
    } else {
      LOGE("Unsupported entry type: ", type.c_str());
      return false;
    }

    mode_t mode = file_type | (permissions & 07777);

    char header[110] = {};
    std::memcpy(header, "070701", 6);  // Magic

    unsigned long namesize = path.size() + 1;  // +1 for null terminator

    auto write_field = [&](int offset, unsigned long value) {
      char temp[9];
      std::snprintf(temp, sizeof(temp), "%08lX", value);
      std::memcpy(header + offset, temp, 8);
    };

    write_field(6, 0);       // ino
    write_field(14, mode);   // mode
    write_field(22, uid);    // uid
    write_field(30, gid);    // gid
    write_field(38, nlink);  // nlink
    write_field(46, 0);      // mtime (not stored)
    write_field(54, filesize);
    write_field(62, 0);  // devmajor
    write_field(70, 0);  // devminor
    write_field(78, 0);  // rdevmajor
    write_field(86, 0);  // rdevminor
    write_field(94, namesize);
    write_field(102, 0);  // check

    cpio_out.write(header, 110);

    cpio_out.write(path.c_str(), static_cast<std::streamsize>(path.size()));
    cpio_out.put('\0');

    size_t name_pad = (4 - ((110 + namesize) % 4)) % 4;
    cpio_out.write("\0\0\0", static_cast<std::streamsize>(name_pad));

    if (type == "file") {
      std::ifstream file(input / path, std::ios::binary);
      std::vector<char> data(filesize);
      file.read(data.data(), static_cast<std::streamsize>(filesize));
      cpio_out.write(data.data(), static_cast<std::streamsize>(filesize));
    } else if (type == "symlink") {
      cpio_out.write(target.c_str(), static_cast<std::streamsize>(target.size()));
    }

    size_t data_pad = (4 - (filesize % 4)) % 4;
    cpio_out.write("\0\0\0", static_cast<std::streamsize>(data_pad));
  }

  char trailer_header[110] = {};
  std::memcpy(trailer_header, "070701", 6);
  std::string trailer_name = "TRAILER!!!";
  unsigned long trailer_namesize = trailer_name.size() + 1;

  auto write_field = [&](int offset, unsigned long value) {
    char temp[9];
    std::snprintf(temp, sizeof(temp), "%08lX", value);
    std::memcpy(trailer_header + offset, temp, 8);
  };

  write_field(94, trailer_namesize);  // namesize
  cpio_out.write(trailer_header, 110);
  cpio_out.write(trailer_name.c_str(), static_cast<std::streamsize>(trailer_name.size()));
  cpio_out.put('\0');

  size_t trailer_pad = (4 - ((110 + trailer_namesize) % 4)) % 4;
  cpio_out.write("\0\0\0", static_cast<std::streamsize>(trailer_pad));

  return true;
}