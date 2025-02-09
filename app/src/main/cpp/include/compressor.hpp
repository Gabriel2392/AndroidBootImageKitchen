#pragma once

#include <filesystem>

#include "fstream"
#include "log.h"
#include "lz4io.h"
#include "zlib.h"
#include "lzma/lzma.h"
#include <thread>

std::error_code errorc;

bool CompressGzipFile(const std::filesystem::path &input,
                      const std::filesystem::path &tmp) {
  constexpr size_t bufferSize = 8192;
  char buffer[bufferSize];

  std::ifstream inFile(input, std::ios::binary);
  if (!inFile.is_open()) {
    LOGE("gzip: Error opening input file: %s", input.string().c_str());
    return false;
  }

  gzFile gzOutput = gzopen(tmp.string().c_str(), "wb");
  if (!gzOutput) {
    LOGE("gzip: Error opening output file: %s", tmp.string().c_str());
    inFile.close();
    return false;
  }

  gzsetparams(gzOutput, Z_BEST_COMPRESSION, Z_DEFAULT_STRATEGY);

  if (std::filesystem::file_size(input, errorc) > 10 * 1024 * 1024) {
      LOG("This might take a while!");
  }

  while (inFile.read(buffer, bufferSize)) {
    int bytesRead = static_cast<int>(inFile.gcount());
    if (gzwrite(gzOutput, buffer, bytesRead) != bytesRead) {
      LOGE("gzip: Error writing to output file: %s", tmp.string().c_str());
      inFile.close();
      gzclose(gzOutput);
      return false;
    }
  }

  if (!inFile.eof()) {
    LOGE("gzip: Error reading from input file: %s", input.string().c_str());
    inFile.close();
    gzclose(gzOutput);
    return false;
  }

  int bytesRead = static_cast<int>(inFile.gcount());
  if (bytesRead > 0) {
    if (gzwrite(gzOutput, buffer, bytesRead) != bytesRead) {
      LOGE("gzip: Error writing final bytes to output file: %s",
           tmp.string().c_str());
      inFile.close();
      gzclose(gzOutput);
      return false;
    }
  }

  inFile.close();
  if (gzclose(gzOutput) != Z_OK) {
    LOGE("gzip: Error closing output file: %s", tmp.string().c_str());
    return false;
  }

  try {
    std::filesystem::rename(tmp, input);
  } catch (const std::filesystem::filesystem_error &e) {
    LOGE("gzip: File replacement failed: %s", e.what());
    std::filesystem::remove(tmp, errorc);
    return false;
  }

  return true;
}

bool CompressLZ4File(const std::filesystem::path &input,
                     const std::filesystem::path &tmp) {
  LZ4IO_prefs_t *prefs = LZ4IO_defaultPreferences();
  if (!prefs) {
    LOGE("LZ4: Error creating preferences");
    return false;
  }
  if (std::filesystem::file_size(input, errorc) > 10 * 1024 * 1024) {
      LOG("This might take a while!");
  }
  LZ4IO_setOverwrite(prefs, 1);
  LZ4IO_setNbWorkers(prefs, static_cast<int>(std::thread::hardware_concurrency()));
  int result = LZ4IO_compressFilename_Legacy(input.string().c_str(),
                                             tmp.string().c_str(), 12, prefs);
  LZ4IO_freePreferences(prefs);
  if (result != 0) {
    LOGE("LZ4: Error compressing");
    return false;
  }

  try {
    std::filesystem::rename(tmp, input);
  } catch (const std::filesystem::filesystem_error &e) {
    LOGE("LZ4: File replacement failed: %s", e.what());
    std::filesystem::remove(tmp, errorc);
    return false;
  }
  return true;
}

bool CompressLZMAFile(const std::filesystem::path &input, const std::filesystem::path &tmp) {
    std::ifstream fin(input, std::ios::binary | std::ios::ate);
    if (!fin) {
        return false;
    }
    fin.seekg(0);

    std::ofstream fout(tmp, std::ios::binary);
    if (!fout) {
        fin.close();
        return false;
    }

    lzma_stream strm = LZMA_STREAM_INIT;
    lzma_options_lzma options;
    lzma_lzma_preset(&options, LZMA_PRESET_EXTREME);
    options.dict_size = 16 * 1024 * 1024;

    lzma_ret ret = lzma_alone_encoder(&strm, &options);
    if (ret != LZMA_OK) {
        fin.close();
        fout.close();
        return false;
    }

    const size_t buffer_size = 65536;
    uint8_t inbuf[buffer_size];
    uint8_t outbuf[buffer_size];
    bool success = true;

    strm.avail_in = 0;
    strm.next_out = outbuf;
    strm.avail_out = buffer_size;

    if (std::filesystem::file_size(input, errorc) > 10 * 1024 * 1024) {
        LOG("This might take a while!");
    }

    while (true) {
        if (strm.avail_in == 0 && !fin.eof()) {
            fin.read(reinterpret_cast<char*>(inbuf), buffer_size);
            if (!fin && !fin.eof()) {
                success = false;
                break;
            }
            strm.avail_in = fin.gcount();
            strm.next_in = inbuf;
        }

        ret = lzma_code(&strm, fin.eof() ? LZMA_FINISH : LZMA_RUN);

        if (strm.avail_out == 0 || ret == LZMA_STREAM_END) {
            size_t write_size = buffer_size - strm.avail_out;
            if (!fout.write(reinterpret_cast<char*>(outbuf), static_cast<std::streamsize>(write_size))) {
                success = false;
                break;
            }
            strm.next_out = outbuf;
            strm.avail_out = buffer_size;
        }

        if (ret == LZMA_STREAM_END) break;
        if (ret != LZMA_OK) {
            success = false;
            break;
        }
    }

    lzma_end(&strm);
    fin.close();
    fout.close();

    if (!success) {
        std::filesystem::remove(tmp);
        return false;
    }

    try {
        std::filesystem::rename(tmp, input);
    } catch (...) {
        std::filesystem::remove(tmp, errorc);
        return false;
    }

    return true;
}