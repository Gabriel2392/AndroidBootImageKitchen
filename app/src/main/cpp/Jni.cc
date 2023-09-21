#include <android/log.h>
#include <csetjmp>
#include <csignal>
#include <jni.h>
#include "log.h"
#include <string>
#include <filesystem>
#include "unpackbootimg/bootimg.h"
#include "unpackbootimg/vendorbootimg.h"
#include "mkbootimg/bootimg.h"
#include "mkbootimg/vendorbootimg.h"
#include "bootconfig.h"
#include "vendorbootconfig.h"
#include <random>
#include <unistd.h>
#include "decompressor.hpp"
#include "cpio_extract.hpp"
#include "cpio_build.hpp"
#include "compressor.hpp"
#include <type_traits>
#include <functional>
#include "SHA1FileHelper.hpp"

constexpr std::string s_vendor_boot_magic = "VNDRBOOT";
constexpr std::string s_boot_magic = "ANDROID!";
namespace fs = std::filesystem;
std::error_code ec;

template <typename F, typename... Args>
auto measure(F&& f, Args&&... args) {
    using namespace std::chrono;
    auto start = high_resolution_clock::now();

    if constexpr (std::is_void_v<std::invoke_result_t<F, Args...>>) {
        std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
        auto end = high_resolution_clock::now();
        return duration<double>(end - start);
    } else {
        auto result = std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
        auto end = high_resolution_clock::now();
        return std::make_pair(result, duration<double>(end - start));
    }
}

bool BuildRamdisk(fs::path &ramdisk_in, fs::path &ramdisk_out, uint8_t compression_method) {
    if (fs::is_directory(ramdisk_in)) {
        fs::path ramdisk_tmp = ramdisk_out.string() + ".tmp";
        LOG("Compressing %s using cpio", ramdisk_in.filename().c_str());
        if (!BuildCPIO(ramdisk_in, ramdisk_out)) {
            return false;
        }

        if (compression_method == FORMAT_LZ4) {
            LOG("Compressing %s using LZ4", ramdisk_in.filename().c_str());
            if (!CompressLZ4File(ramdisk_out, ramdisk_tmp)) {
                return false;
            }
        } else if (compression_method == FORMAT_GZIP) {
            LOG("Compressing %s using gzip", ramdisk_in.filename().c_str());
            if (!CompressGzipFile(ramdisk_out, ramdisk_tmp)) {
                return false;
            }
        } else if (compression_method == FORMAT_LZMA) {
            LOG("Compressing %s using lzma", ramdisk_in.filename().c_str());
            if (!CompressLZMAFile(ramdisk_out, ramdisk_tmp)) {
                return false;
            }
        } else {
            LOG("Compression method is unknown!");
            LOG("%s will be kept uncompressed!", ramdisk_in.filename().c_str());
        }
    } else {
        fs::copy_file(ramdisk_in, ramdisk_out, fs::copy_options::overwrite_existing, ec);
    }
    return true;
}


bool UnpackRamdisk(fs::path &ramdisk_in, uint8_t compression_method) {
    fs::path ramdisk_tmp = ramdisk_in.string() + ".tmp";
    if (compression_method == FORMAT_LZ4) {
        LOG("Decompressing %s using LZ4", ramdisk_in.filename().c_str());
        if (!DecompressLZ4File(ramdisk_in, ramdisk_tmp)) {
            return false;
        }
    } else if (compression_method == FORMAT_GZIP) {
        LOG("Decompressing %s using gzip", ramdisk_in.filename().c_str());
        if (!DecompressGzipFile(ramdisk_in, ramdisk_tmp)) {
            return false;
        }
    } else if (compression_method == FORMAT_LZMA) {
        LOG("Decompressing %s using lzma", ramdisk_in.filename().c_str());
        if (!DecompressLZMAFile(ramdisk_in, ramdisk_tmp)) {
            return false;
        }
    } else {
        LOG("Compression method is unknown!");
        LOG("%s will be kept compressed!", ramdisk_in.filename().c_str());
        return true;
    }
    LOG("Decompressing %s using cpio", ramdisk_in.filename().c_str());
    if (!ExtractCPIO(ramdisk_tmp, ramdisk_in)) {
        return false;
    }
    fs::remove_all(ramdisk_tmp, ec);
    return true;
}

std::string GenRandomString(std::size_t length) {
    const std::string charset = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::random_device rd;
    std::mt19937 generator(rd());

    std::uniform_int_distribution<> distribution(0, static_cast<int>(charset.size() - 1));

    std::string randomString;
    randomString.reserve(length);

    for (std::size_t i = 0; i < length; ++i) {
        randomString += charset[distribution(generator)];
    }

    return randomString;
}

std::string ReadString(JNIEnv* env, jstring jStr) {
    if (!jStr) {
        return "";
    }

    const char* chars = env->GetStringUTFChars(jStr, nullptr);
    if (!chars) {
        return "";
    }

    std::string ret(chars);

    env->ReleaseStringUTFChars(jStr, chars);

    return ret;
}

void try_clean(const fs::path& directory, const std::string& extension) {
    try {
        for (const auto& entry : fs::directory_iterator(directory)) {
            if (entry.is_regular_file() && entry.path().extension() == extension) {
                fs::remove(entry.path(), ec);
            }
        }
    } catch (...) {
    }
}

bool mkbootimg_wrapper(const std::string &workdir) {
    fs::path config_file = fs::path(workdir) / CONFIG_FILE;
    std::ifstream config(config_file.string(), std::ios::binary);
    if (!config) {
        LOGE("Configuration file does not exist.");
        return false;
    }

    if (!ValidateSHA1(config_file)) {
        LOGE("Configuration file is invalid.");
        return false;
    }

    uint32_t str_size = 8;
    config.read(reinterpret_cast<char*>(&str_size), sizeof(str_size));
    std::vector<uint8_t> magic(str_size);
    config.read(reinterpret_cast<char *>(magic.data()), static_cast<std::streamsize>(str_size));
    if (config.gcount() != str_size) {
        LOGE("Failed to read boot magic");
        return false;
    }

    config.close();
    bool ret = false;

    if (std::string_view(reinterpret_cast<const char*>(magic.data()), str_size) == s_boot_magic) {
        LOG("boot magic: %s", s_boot_magic.c_str());
        BootImageInfo info;
        BootConfig::Read(info, config_file.string());
        auto ramdisk = fs::path(workdir) / fs::path("ramdisk");
        auto ramdisk_build = fs::path(ramdisk.string() + ".build");
        if (!BuildRamdisk(ramdisk, ramdisk_build, info.ramdisk_compression)) {
            return false;
        }
        BootImageArgs args;
        if (info.kernel_size > 0) {
            args.kernel = fs::path(fs::path(workdir) / "kernel");
            args.kernel_offset = info.kernel_load_address;
        }
        if (info.ramdisk_size > 0) {
            args.ramdisk = ramdisk_build;
            args.ramdisk_offset = info.ramdisk_load_address;
        }
        if (info.second_size > 0) {
            args.second = fs::path(fs::path(workdir) / "second");
            args.second_offset = info.second_load_address;
        }
        if (info.dtb_size > 0) {
            args.dtb = fs::path(fs::path(workdir) / "dtb");
            args.dtb_offset = info.dtb_load_address;
        }
        if (info.recovery_dtbo_size > 0) {
            args.recovery_dtbo = fs::path(fs::path(workdir) / "recovery_dtbo");
        }
        args.tags_offset = info.tags_load_address;
        args.os_version.patch_level_str = info.os_patch_level;
        args.os_version.version_str = info.os_version;
        args.header_version = info.header_version;
        args.board = info.product_name;
        args.base = 0x0; // unpackbootimg returns offsets with base already being included
        args.page_size = info.page_size;
        args.cmdline = info.cmdline;
        if (!info.extra_cmdline.empty()) args.cmdline += " " + info.extra_cmdline;
        args.output = fs::path(workdir) / fs::path("image-new");
        fs::remove_all(args.output, ec);
        ret = WriteBootImage(args);
        try_clean(workdir, ".build");
    } else if (std::string_view(reinterpret_cast<const char*>(magic.data()), str_size) == s_vendor_boot_magic) {
        LOG("boot magic: %s", s_vendor_boot_magic.c_str());
        VendorBootImageInfo info;
        VendorBootConfig::Read(info, config_file.string());
        for (const auto &i : info.vendor_ramdisk_table) {
            auto ramdisk = fs::path(workdir) / fs::path(i.output_name);
            auto ramdisk_build = fs::path(ramdisk.string() + ".build");
            if (!BuildRamdisk(ramdisk, ramdisk_build, i.ramdisk_compression)) {
                return false;
            }
        }
        VendorBootArgs args;
        if (info.dtb_size > 0) {
            args.dtb = fs::path(fs::path(workdir) / "dtb");
            args.dtb_offset = info.dtb_load_address;
        }
        args.tags_offset = info.tags_load_address;
        args.page_size = info.page_size;
        args.header_version = info.header_version;
        args.kernel_offset = info.kernel_load_address;
        args.ramdisk_offset = info.ramdisk_load_address;
        args.board = info.product_name;
        args.base = 0x0; // unpackbootimg returns offsets with base already being included
        if (info.vendor_bootconfig_size > 0) {
            args.bootconfig = fs::path(fs::path(workdir) / "bootconfig");
        }
        args.vendor_cmdline = info.cmdline;
        std::vector<VendorRamdiskEntry> rds;
        for (const auto &i : info.vendor_ramdisk_table) {
            VendorRamdiskEntry entry;
            auto ramdisk_build = fs::path(fs::path(workdir) / fs::path(i.output_name + ".build"));
            entry.path = fs::is_regular_file(ramdisk_build) ? ramdisk_build : fs::path(workdir) / i.output_name;
            entry.type = i.type;
            entry.name = i.name;
            rds.push_back(entry);
        }
        args.ramdisks = rds;
        args.output = fs::path(workdir) / fs::path("vendor_boot-new");
        fs::remove_all(args.output, ec);
        VendorBootBuilder builder(std::move(args));
        ret = builder.Build();
        try_clean(workdir, ".build");
    } else {
        LOGE("Invalid boot magic: %s", utils::toHexString(std::string_view(reinterpret_cast<const char*>(magic.data()), str_size)).c_str());
    }

    return ret;
}

bool unpackbootimg_wrapper(int fd, const std::string &workdir, bool dec_ramdisk) {
    if (!utils::CreateDirectory(workdir)) {
        LOGE("Could not create output directory");
        return false;
    }

    char magic[8];
    ssize_t bytes_read = read(fd, magic, sizeof(magic));
    if (bytes_read != sizeof(magic)) {
        LOGE("Failed to read boot magic");
        return false;
    }

    if (lseek(fd, 0, SEEK_SET) == (off_t)-1) {
        LOGE("Failed to seek back to beginning of boot image");
        return false;
    }

    std::optional<BootImageInfo> boot_info;
    std::optional<VendorBootImageInfo> vendor_boot_info;

    if (std::string_view(magic, 8) == s_boot_magic) {
        LOG("boot magic: %s", s_boot_magic.c_str());
        boot_info = UnpackBootImage(fd, workdir, dec_ramdisk);
    } else if (std::string_view(magic, 8) == s_vendor_boot_magic) {
        LOG("boot magic: %s", s_vendor_boot_magic.c_str());
        vendor_boot_info = UnpackVendorBootImage(fd, workdir, dec_ramdisk);
    } else {
        LOGE("Invalid boot magic: %s", utils::toHexString(std::string_view(magic, 8)).c_str());
        return false;
    }

    auto config = fs::path(workdir) / fs::path(CONFIG_FILE);
    if (!AppendSHA1(config)) {
        LOGE("Error calculating SHA1");
        return false;
    }

    if (!boot_info && !vendor_boot_info) {
        LOGE("Failed to unpack boot image");
        return false;
    }

    if (boot_info && dec_ramdisk && boot_info->ramdisk_compression != FORMAT_OTHER) {
        fs::path ramdisk_in = fs::path(workdir) / "ramdisk";
        if (!UnpackRamdisk(ramdisk_in, boot_info->ramdisk_compression)) {
            return false;
        }
    } else if (vendor_boot_info && dec_ramdisk) {
        for (const auto& i : vendor_boot_info->vendor_ramdisk_table) {
            if (i.ramdisk_compression == FORMAT_OTHER) continue;
            fs::path ramdisk_in = fs::path(workdir) / i.output_name;
            if (!UnpackRamdisk(ramdisk_in, i.ramdisk_compression)) {
                return false;
            }
        }
    }

    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_oops_abik_ABIKBridge_jniExtract(JNIEnv *env, jobject, jint input_fd, jstring input_name, jstring dir, jboolean extract_ramdisk) {
    initializeJNIReferences(env);

    std::string directory = ReadString(env, dir);
    std::string input = ReadString(env, input_name);

    if (input_fd < 0) {
        LOGE("Input file descriptor is invalid");
        return false;
    }

    if (input.empty()) input = GenRandomString(16);
    std::string workdir = directory + "/" + input;

    try {
        std::filesystem::create_directories(directory);
    } catch (...) {
        LOGE("Failed to create: %s", workdir.c_str());
        return false;
    }

    auto unique_work_dir = get_unique_path(workdir);

    auto [ret, elapsed] = measure(unpackbootimg_wrapper, input_fd, unique_work_dir, extract_ramdisk);

    if (!ret) fs::remove_all(workdir, ec);

    LOG(ret ? "Done in %.1fs!" : "Failed in %.1fs!", elapsed.count());
    LOG("");

    releaseJNIReferences();
    return ret;
}


extern "C" JNIEXPORT jboolean JNICALL
Java_com_oops_abik_ABIKBridge_jniBuild(JNIEnv *env, jobject, jstring input_dir) {
    initializeJNIReferences(env);

    std::string input = ReadString(env, input_dir);
    if (input.empty()) {
        LOGE("Error reading jstring");
        return false;
    }

    auto [ret, elapsed] = measure(mkbootimg_wrapper, input);

    LOG(ret ? "Done in %.1fs!" : "Failed in %.1fs!", elapsed.count());
    LOG("");

    releaseJNIReferences();
    return ret;
}