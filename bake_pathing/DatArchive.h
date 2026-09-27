#pragma once

#include <cstdint>
#include <filesystem>
#include <unordered_map>
#include <vector>

#include <Windows.h>

class DatArchive {
public:
    explicit DatArchive(const std::filesystem::path& path);
    ~DatArchive();
    DatArchive(const DatArchive&) = delete;
    DatArchive& operator=(const DatArchive&) = delete;

    bool Valid() const { return valid_; }
    bool ReadFile(uint32_t file_id, uint8_t stream_id, std::vector<uint8_t>& out) const;

private:
#pragma pack(push, 1)
    struct MainHeader {
        uint8_t id[4];
        int32_t header_size;
        int32_t sector_size;
        int32_t crc;
        int64_t mft_offset;
        int32_t mft_size;
        int32_t flags;
    };
    struct MftHeader {
        uint8_t id[4];
        int32_t unknown[2];
        int32_t entry_count;
        int32_t more_unknown[2];
    };
    struct MftEntry {
        int64_t offset;
        int32_t size;
        uint16_t compressed;
        uint8_t payload;
        uint8_t stream;
        int32_t next;
        int32_t crc;
    };
    struct HashEntry {
        int32_t file_id;
        int32_t slot;
    };
#pragma pack(pop)
    static_assert(sizeof(MainHeader) == 32);
    static_assert(sizeof(MftHeader) == 24);
    static_assert(sizeof(MftEntry) == 24);

    bool ReadAt(int64_t offset, void* buffer, size_t size) const;
    HANDLE file_ = INVALID_HANDLE_VALUE;
    HANDLE mapping_ = nullptr;
    bool valid_ = false;
    int64_t size_ = 0;
    std::vector<MftEntry> slots_;
    std::unordered_map<uint32_t, int32_t> by_file_id_;
};
