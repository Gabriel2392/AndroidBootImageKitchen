#pragma once

#include <filesystem>
#include "lz4io.h"
#include "log.h"
#include "zlib.h"
#include "fstream"

#define POCKETLZMA_LZMA_C_DEFINE
#include "pocketlzma.hpp"

bool CompressGzipFile(const std::filesystem::path &input, const std::filesystem::path &tmp) {
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
            LOGE("gzip: Error writing final bytes to output file: %s", tmp.string().c_str());
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
        std::error_code ec;
        std::filesystem::remove(tmp, ec);
        return false;
    }

    return true;
}

bool CompressLZ4File(const std::filesystem::path &input, const std::filesystem::path &tmp) {
    LZ4IO_prefs_t* prefs = LZ4IO_defaultPreferences();
    if (!prefs) {
        LOGE("LZ4: Error creating preferences");
        return false;
    }
    LZ4IO_setOverwrite(prefs, 1);
    int result = LZ4IO_compressFilename_Legacy(input.string().c_str(), tmp.string().c_str(), 12, prefs);
    LZ4IO_freePreferences(prefs);
    if (result != 0) {
        LOGE("LZ4: Error compressing");
        return false;
    }

    try {
        std::filesystem::rename(tmp, input);
    } catch (const std::filesystem::filesystem_error &e) {
        LOGE("LZ4: File replacement failed: %s", e.what());
        std::error_code ec;
        std::filesystem::remove(tmp, ec);
        return false;
    }
    return true;
}

bool CompressLZMAFile(const std::filesystem::path& input, const std::filesystem::path& tmp) {
    std::vector<uint8_t> data;
    std::vector<uint8_t> compressedData;
    plz::FileStatus fileStatus = plz::File::FromFile(input.string(), data);

    if (fileStatus.status() != plz::FileStatus::Code::Ok) {
        LOGE("LZMA: Error reading input file: %s", input.string().c_str());
        return false;
    }

    plz::PocketLzma p;
    p.usePreset(plz::Preset::BestCompression);
    plz::StatusCode status = p.compress(data, compressedData);
    if (status != plz::StatusCode::Ok) {
        LOGE("LZMA: Error compressing data");
        return false;
    }

    plz::FileStatus writeStatus = plz::File::ToFile(tmp.string(), compressedData);
    if (writeStatus.status() != plz::FileStatus::Code::Ok) {
        LOGE("LZMA: Error writing compressed data to output file: %s", tmp.string().c_str());
        return false;
    }

    try {
        std::filesystem::rename(tmp, input);
    } catch (const std::filesystem::filesystem_error& e) {
        LOGE("LZMA: File replacement failed: %s", e.what());
        std::error_code ec;
        std::filesystem::remove(tmp, ec);
        return false;
    }
    return true;
}