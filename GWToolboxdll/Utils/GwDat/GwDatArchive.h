#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// Native reader for the Guild Wars .dat archive.
//
// The running client holds Gw.dat open exclusively, so we memory-map its handle and
// parse/decompress the archive ourselves - no game functions and no memory scans, so
// it is immune to the byte shifts that break Scanner-based access on client updates.
//
// The MFT/decompression logic is derived from GuildWarsMapBrowser
// (https://github.com/Jonathan-Greve/GuildWarsMapBrowser) - see GwDat/CREDITS.txt.
class GwDatArchive {
public:
    static GwDatArchive& Instance();

    // Maps the client's open dat handle on first use; retries until it succeeds. Thread-safe.
    bool EnsureLoaded();

    // Diagnostics, valid after EnsureLoaded().
    const std::wstring& DatPath() const { return m_dat_path; }
    bool Loaded() const { return m_loaded.load(std::memory_order_acquire); }

    // Decompressed bytes for a GW file id. A file's streams form a linked list of MFT
    // entries; stream_id selects one by its stream number (0 = the file's own data;
    // e.g. item models keep their UI icon at stream 1). False if absent.
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
    // The whole struct maps directly onto the 24-byte on-disk MFT entry.
    struct MftEntry {
        int64_t offset;
        int32_t size; // compressed size on disk
        uint16_t a;   // non-zero => the payload is GWDat-compressed
        uint8_t b;    // zero marks an empty/base slot with no payload
        uint8_t c;    // this entry's stream number within its file
        int32_t id;   // slot index of the next stream in the file's chain (0 = end)
        int32_t crc;
    };
#pragma pack(pop)
    static_assert(sizeof(MftEntry) == 24, "on-disk MFT entry must be 24 bytes");

    bool ParseIndex();      // resets state, then reads header + MFT; safe to retry
    bool AcquireMapping();  // maps the client's open dat handle; false if not found yet

    std::mutex m_load_mutex;
    std::atomic<bool> m_loaded{false};
    void* m_mapping = nullptr;      // file-mapping HANDLE for the dat, kept for the archive lifetime
    long long m_file_size = 0;      // dat size captured when the mapping was created
    std::wstring m_dat_path;
    std::vector<MftEntry> m_slots;                       // indexed by physical MFT slot
    std::unordered_map<uint32_t, int> m_fileid_to_slot;  // GW file id -> base MFT slot
};
