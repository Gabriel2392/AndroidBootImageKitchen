#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>

#include "TinySHA1.hpp"

namespace fs = std::filesystem;
constexpr std::size_t SHA1_DIGEST_SIZE = 20;

bool AppendSHA1(fs::path& filename) noexcept {
  std::ifstream infile(filename, std::ios::binary);
  if (!infile) return false;

  sha1::SHA1 sha;
  constexpr std::size_t buffer_size = 4096;
  std::array<char, buffer_size> buffer{};
  while (infile) {
    infile.read(buffer.data(), buffer.size());
    if (const auto count = infile.gcount(); count > 0) {
      sha.processBytes(buffer.data(), static_cast<std::size_t>(count));
    }
  }
  uint32_t digest[5]{};
  sha.getDigest(digest);

  std::ofstream outfile(filename, std::ios::binary | std::ios::app);
  if (!outfile) return false;
  outfile.write(reinterpret_cast<const char*>(digest), SHA1_DIGEST_SIZE);
  return outfile.good();
}

bool ValidateSHA1(fs::path& filename) noexcept {
  std::ifstream file(filename, std::ios::binary | std::ios::ate);
  if (!file) return false;

  const auto filesize = file.tellg();
  if (filesize < static_cast<std::streamoff>(SHA1_DIGEST_SIZE)) return false;

  const std::streamoff content_size =
      filesize - static_cast<std::streamoff>(SHA1_DIGEST_SIZE);

  file.seekg(content_size, std::ios::beg);
  if (!file) return false;
  uint32_t appended_digest[5]{};
  file.read(reinterpret_cast<char*>(appended_digest), SHA1_DIGEST_SIZE);
  if (file.gcount() != static_cast<std::streamsize>(SHA1_DIGEST_SIZE))
    return false;

  file.seekg(0, std::ios::beg);
  if (!file) return false;
  sha1::SHA1 sha;
  constexpr std::size_t buffer_size = 4096;
  std::array<char, buffer_size> buffer{};
  std::streamoff remaining = content_size;
  while (remaining > 0 && file) {
    const auto to_read = static_cast<std::streamsize>(
        std::min<std::streamoff>(buffer_size, remaining));
    file.read(buffer.data(), to_read);
    const auto count = file.gcount();
    if (count <= 0) break;
    sha.processBytes(buffer.data(), static_cast<std::size_t>(count));
    remaining -= count;
  }
  if (remaining != 0) return false;

  uint32_t computed_digest[5]{};
  sha.getDigest(computed_digest);

  return std::equal(std::begin(appended_digest), std::end(appended_digest),
                    std::begin(computed_digest));
}