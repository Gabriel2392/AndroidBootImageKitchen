#pragma once

#include <filesystem>
#include "lz4io.h"
#include "log.h"
#include "zlib.h"
#include "fstream"

#define POCKETLZMA_LZMA_C_DEFINE
#include "pocketlzma.hpp"

bool DecompressGzipFile(const std::filesystem::path &input, const std::filesystem::path &output) {
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

bool DecompressLZ4File(const std::filesystem::path &input, const std::filesystem::path &output) {
    LZ4IO_prefs_t* prefs = LZ4IO_defaultPreferences();
    if (!prefs) {
        LOGE("LZ4: Error creating preferences");
        return false;
    }
    LZ4IO_setOverwrite(prefs, 1);
    int result = LZ4IO_decompressFilename(input.string().c_str(), output.string().c_str(), prefs);
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

bool DecompressLZMAFile(const std::filesystem::path& input, const std::filesystem::path& output) {
    std::vector<uint8_t> data;
    std::vector<uint8_t> decompressedData;
    plz::FileStatus fileStatus = plz::File::FromFile(input.string(), data);

    if (fileStatus.status() == plz::FileStatus::Code::Ok) {
        plz::PocketLzma p;
        plz::StatusCode status = p.decompress(data, decompressedData);
        if (status == plz::StatusCode::Ok) {
            plz::FileStatus writeStatus = plz::File::ToFile(output.string(), decompressedData);
            if (writeStatus.status() == plz::FileStatus::Code::Ok) {
                try {
                    std::filesystem::remove_all(input);
                } catch (const std::filesystem::filesystem_error &e) {
                    LOGE("LZMA: File removal failed: %s", e.what());
                    return false;
                }
                return true;
            }
        }
    }

    LOGE("LZMA: Error decompressing!");
    return false;
}