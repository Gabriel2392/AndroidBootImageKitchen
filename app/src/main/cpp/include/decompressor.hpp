#pragma once

#include <filesystem>

#include "fstream"
#include "log.h"
#include "lz4io.h"
#include "zlib.h"

bool DecompressGzipFile(const std::filesystem::path &input,
                        const std::filesystem::path &output) {
  constexpr size_t bufferSize = 8192;
  char buffer[bufferSize];

  gzFile gzInput = gzopen(input.string().c_str(), "rb");
  if (!gzInput) {
    LOGE("gzip: Error opening input file: %s", input.string().c_str());
    return false;
  }

  std::ofstream outFile(output, std::ios::binary);
  if (!outFile.is_open()) {
    LOGE("gzip: Error opening output file: %s", output.string().c_str());
    gzclose(gzInput);
    return false;
  }

  int bytesRead = 0;
  while ((bytesRead = gzread(gzInput, buffer, bufferSize)) > 0) {
    outFile.write(buffer, bytesRead);
    if (!outFile) {
      LOGE("gzip: Error writing to output file: %s", output.string().c_str());
      outFile.close();
      gzclose(gzInput);
      return false;
    }
  }

  if (bytesRead < 0) {
    int errnum = 0;
    const char *errorMsg = gzerror(gzInput, &errnum);
    LOGE("gzip: Error during decompression: %s", errorMsg);
    outFile.close();
    gzclose(gzInput);
    return false;
  }

  outFile.close();
  gzclose(gzInput);

  try {
    std::filesystem::remove_all(input);
  } catch (const std::filesystem::filesystem_error &e) {
    LOGE("GZIP: File removal failed: %s", e.what());
    return false;
  }

  return true;
}

bool DecompressLZ4File(const std::filesystem::path &input,
                       const std::filesystem::path &output) {
  LZ4IO_prefs_t *prefs = LZ4IO_defaultPreferences();
  if (!prefs) {
    LOGE("LZ4: Error creating preferences");
    return false;
  }
  LZ4IO_setOverwrite(prefs, 1);
  int result = LZ4IO_decompressFilename(input.string().c_str(),
                                        output.string().c_str(), prefs);
  LZ4IO_freePreferences(prefs);
  if (result != 0) {
    LOGE("LZ4: Error decompressing");
    return false;
  }

  try {
    std::filesystem::remove_all(input);
  } catch (const std::filesystem::filesystem_error &e) {
    LOGE("LZ4: File removal failed: %s", e.what());
    return false;
  }
  return true;
}

bool DecompressLZMAFile(const std::filesystem::path &input,
                        const std::filesystem::path &output) {
    std::ifstream inFile(input, std::ios::binary);
    if (!inFile) {
        LOGE("LZMA: Error opening input file");
        return false;
    }

    lzma_stream strm = LZMA_STREAM_INIT;
    lzma_ret ret = lzma_alone_decoder(&strm, UINT64_MAX);
    if (ret != LZMA_OK) {
        LOGE("LZMA: Decoder initialization failed: %d", ret);
        return false;
    }

    std::ofstream outFile(output, std::ios::binary);
    if (!outFile) {
        LOGE("LZMA: Error opening output file");
        lzma_end(&strm);
        return false;
    }

    constexpr size_t IN_BUFSIZE = 65536;
    uint8_t inBuf[IN_BUFSIZE];
    bool input_eof = false;

    constexpr size_t OUT_BUFSIZE = 65536;
    uint8_t outBuf[OUT_BUFSIZE];

    do {
        if (strm.avail_in == 0 && !input_eof) {
            inFile.read(reinterpret_cast<char*>(inBuf), IN_BUFSIZE);
            std::streamsize bytesRead = inFile.gcount();
            if (bytesRead > 0) {
                strm.next_in = inBuf;
                strm.avail_in = bytesRead;
            } else {
                input_eof = true;
            }
        }

        strm.next_out = outBuf;
        strm.avail_out = OUT_BUFSIZE;

        ret = lzma_code(&strm, LZMA_RUN);

        if (ret != LZMA_OK && ret != LZMA_STREAM_END) {
            LOGE("LZMA: Decompression error: %d", ret);
            break;
        }

        size_t decompressedBytes = OUT_BUFSIZE - strm.avail_out;
        if (decompressedBytes > 0) {
            if (!outFile.write(reinterpret_cast<const char*>(outBuf), static_cast<std::streamsize>(decompressedBytes))) {
                LOGE("LZMA: Error writing to output file");
                ret = LZMA_DATA_ERROR;
                break;
            }
        }
    } while (ret != LZMA_STREAM_END);

    lzma_end(&strm);

    if (ret != LZMA_STREAM_END) {
        outFile.close();
        std::error_code ec;
        std::filesystem::remove(output, ec);
        return false;
    }

    outFile.close();

    try {
        std::filesystem::remove(input);
    } catch (const std::filesystem::filesystem_error &e) {
        LOGE("LZMA: File removal failed: %s", e.what());
        return false;
    }

    return true;
}