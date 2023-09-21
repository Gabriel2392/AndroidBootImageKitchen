#pragma once
#include "log.h"
#include "../unpackbootimg/vendorbootimg.h"

class VendorBootConfig {
public:
    static bool Write(const VendorBootImageInfo& info, const std::string& filename) {
        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            LOGE("Failed to open config for writing");
            return false;
        }

        WriteString(file, info.boot_magic);
        WriteU32(file, info.header_version);
        WriteU32(file, info.page_size);
        WriteU32(file, info.kernel_load_address);
        WriteU32(file, info.ramdisk_load_address);
        WriteU32(file, info.vendor_ramdisk_size);
        WriteString(file, info.cmdline);
        WriteU32(file, info.tags_load_address);
        WriteString(file, info.product_name);
        WriteU32(file, info.header_size);
        WriteU32(file, info.dtb_size);
        WriteU64(file, info.dtb_load_address);

        // Version >3 fields
        WriteU32(file, info.vendor_ramdisk_table_size);
        WriteU32(file, info.vendor_ramdisk_table_entry_num);
        WriteU32(file, info.vendor_ramdisk_table_entry_size);
        WriteU32(file, info.vendor_bootconfig_size);

        // Validate entry count matches vector size
        if (info.vendor_ramdisk_table.size() != info.vendor_ramdisk_table_entry_num) {
            LOGE("Mismatch between vendor_ramdisk_table size and entry_num");
            return false;
        }

        // Write each ramdisk table entry
        for (const auto& entry : info.vendor_ramdisk_table) {
            WriteString(file, entry.output_name);
            WriteU32(file, entry.size);
            WriteU32(file, entry.offset);
            WriteU32(file, entry.type);
            WriteString(file, entry.name);
            for (uint32_t id : entry.board_id) {
                WriteU32(file, id);
            }
            WriteU8(file, entry.ramdisk_compression);
        }

        if (!file.good()) {
            LOGE("Error occurred while writing configuration");
            return false;
        }
        return true;
    }

    static bool Read(VendorBootImageInfo& info, const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            LOGE("Failed to open configuration for reading");
            return false;
        }

        ReadString(file, info.boot_magic);
        ReadU32(file, info.header_version);
        ReadU32(file, info.page_size);
        ReadU32(file, info.kernel_load_address);
        ReadU32(file, info.ramdisk_load_address);
        ReadU32(file, info.vendor_ramdisk_size);
        ReadString(file, info.cmdline);
        ReadU32(file, info.tags_load_address);
        ReadString(file, info.product_name);
        ReadU32(file, info.header_size);
        ReadU32(file, info.dtb_size);
        ReadU64(file, info.dtb_load_address);

        // Version >3 fields
        ReadU32(file, info.vendor_ramdisk_table_size);
        ReadU32(file, info.vendor_ramdisk_table_entry_num);
        ReadU32(file, info.vendor_ramdisk_table_entry_size);
        ReadU32(file, info.vendor_bootconfig_size);

        // Read ramdisk table entries
        info.vendor_ramdisk_table.clear();
        info.vendor_ramdisk_table.reserve(info.vendor_ramdisk_table_entry_num);
        for (uint32_t i = 0; i < info.vendor_ramdisk_table_entry_num; ++i) {
            VendorRamdiskTableEntry entry;
            ReadString(file, entry.output_name);
            ReadU32(file, entry.size);
            ReadU32(file, entry.offset);
            ReadU32(file, entry.type);
            ReadString(file, entry.name);
            for (auto& id : entry.board_id) {
                ReadU32(file, id);
            }
            ReadU8(file, entry.ramdisk_compression);
            info.vendor_ramdisk_table.push_back(entry);
        }

        if (!file.good()) {
            LOGE("Error occurred while reading");
            return false;
        }
        return true;
    }

private:
    static void WriteU8(std::ostream& os, uint8_t value) {
        os.write(reinterpret_cast<const char*>(&value), sizeof(value));
    }

    static void WriteU32(std::ostream& os, uint32_t value) {
        os.write(reinterpret_cast<const char*>(&value), sizeof(value));
    }

    static void WriteU64(std::ostream& os, uint64_t value) {
        os.write(reinterpret_cast<const char*>(&value), sizeof(value));
    }

    static void WriteString(std::ostream& os, const std::string& s) {
        auto size = static_cast<uint32_t>(s.size());
        WriteU32(os, size);
        os.write(s.data(), static_cast<std::streamsize>(size));
    }

    static void ReadU8(std::istream& is, uint8_t& value) {
        is.read(reinterpret_cast<char*>(&value), sizeof(value));
    }

    static void ReadU32(std::istream& is, uint32_t& value) {
        is.read(reinterpret_cast<char*>(&value), sizeof(value));
    }

    static void ReadU64(std::istream& is, uint64_t& value) {
        is.read(reinterpret_cast<char*>(&value), sizeof(value));
    }

    static void ReadString(std::istream& is, std::string& s) {
        uint32_t size;
        ReadU32(is, size);
        s.resize(size);
        is.read(&s[0], static_cast<std::streamsize>(size));
    }
};