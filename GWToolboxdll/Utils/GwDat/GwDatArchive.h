#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// Native reader for the Guild Wars .dat archive.
//
// Reads and decompresses files straight off disk (Gw.dat next to the running
// executable) - no game functions and no memory scans, so it is immune to the
// byte shifts that break Scanner-based access on client updates.
//
// The MFT/decompression logic is derived from GuildWarsMapBrowser
// (https://github.com/Jonathan-Greve/GuildWarsMapBrowser) - see GwDat/CREDITS.txt.
class GwDatArchive {
public:
    static GwDatArchive& Instance();

    // Opens and indexes Gw.dat on first use. Thread-safe and idempotent.
    bool EnsureLoaded();

    // Reads the decompressed bytes for a GW file id (the dat "file number", the
    // same value ArenaNetFileParser::FileHashToFileId produces). stream_id picks
    // among multiple dat entries that share an id (0 = first). Returns false if
    // the archive can't be opened or the id isn't present.
    bool ReadFile(uint32_t file_id, std::vector<uint8_t>& out, uint32_t stream_id = 0);

private:
    GwDatArchive() = default;

#pragma pack(push, 1)
    struct MainHeader {
        uint8_t id[4];
        int32_t header_size;
        int32_t sector_size;
        int32_t crc1;
        int64_t mft_offset;
        int32_t mft_size;
        int32_t flags;
    };
    struct MftHeader {
        uint8_t id[4];
        int32_t unk1, unk2, entry_count, unk4, unk5;
    };
    struct MftExpansion {
        int32_t file_number;
        int32_t file_offset;
    };
    // The first 24 bytes map directly onto the on-disk MFT entry.
    struct MftEntry {
        int64_t offset;
        int32_t size; // compressed size on disk
        uint16_t a;   // non-zero => the payload is GWDat-compressed
        uint8_t b;
        uint8_t c;
        int32_t id;
        int32_t crc;
        uint32_t hash = 0; // GW file number (from the MFT expansion table)
    };
#pragma pack(pop)
    static_assert(offsetof(MftEntry, hash) == 24, "on-disk MFT entry must be 24 bytes");

    bool ParseIndex(); // reads header + MFT once, driven by EnsureLoaded's call_once

    std::once_flag m_load_once;
    bool m_loaded = false;
    std::wstring m_dat_path;
    std::vector<MftEntry> m_mft;
    std::unordered_map<uint32_t, std::vector<int>> m_hash_index; // file id -> MFT indices
};
