#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <string>
#include <thread>
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

    // True if the handle scan itself failed (NtQuerySystemInformation blocked, usually AV/anti-cheat).
    bool HandleEnumerationBlocked() const { return m_handle_enum_failed.load(std::memory_order_acquire); }

    // Decompressed bytes for a GW file id; stream_id picks a stream (0 = the file's own data). False if absent.
    bool ReadFile(uint32_t file_id, std::vector<uint8_t>& out, uint32_t stream_id = 0);

    // Dedicated worker so reads/decodes run off the caller's thread; EnqueueTask runs inline if not started.
    void StartWorker();
    void StopWorker();
    void EnqueueTask(std::function<void()> task);

private:
    GwDatArchive() = default;

    void WorkerLoop();

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

    bool ParseIndex();                                                     // first load: map + parse into members
    bool AcquireMappingInto(void*& mapping, long long& size, std::wstring& path); // maps the client's dat handle
    bool ParseFrom(void* mapping, long long size, std::vector<MftEntry>& slots,
                   std::unordered_map<uint32_t, int>& fileid_to_slot);     // header + MFT + hash list -> temporaries
    void MaybeRefresh();                                                   // throttled re-read to pick up streamed-in files
    bool ReadRaw(uint32_t file_id, uint32_t stream_id, std::vector<uint8_t>& input, bool& compressed);

    std::mutex m_load_mutex;
    std::atomic<bool> m_loaded{false};
    std::atomic<bool> m_handle_enum_failed{false}; // set when the handle scan itself failed (AV/anti-cheat)

    std::thread m_worker;                          // dedicated read/decode worker
    std::mutex m_task_mutex;
    std::condition_variable m_task_cv;
    std::queue<std::function<void()>> m_tasks;
    bool m_worker_running = false;
    bool m_worker_stop = false;
    std::shared_mutex m_index_mutex;               // guards m_mapping/m_slots/m_fileid_to_slot against MaybeRefresh
    uint32_t m_last_refresh_ms = 0;                // GetTickCount of the last re-parse, to throttle refreshes
    void* m_mapping = nullptr;      // file-mapping HANDLE for the dat, replaced by MaybeRefresh when the dat grows
    long long m_file_size = 0;      // dat size captured when the mapping was created
    std::wstring m_dat_path;
    std::vector<MftEntry> m_slots;                       // indexed by physical MFT slot
    std::unordered_map<uint32_t, int> m_fileid_to_slot;  // GW file id -> base MFT slot
};
