#pragma once

#include <ToolboxModule.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// Guild Wars .dat module: reads/decompresses the client's .dat archive (memory-mapped, and aware of
// files the client streams in on demand, e.g. a Steam .snapshot) and decodes textures from it for the
// UI. The MFT/decompression logic is derived from GuildWarsMapBrowser (see GwDat/CREDITS.txt).
class GwDatModule : public ToolboxModule {
    GwDatModule() = default;
    ~GwDatModule() override = default;

public:
    static GwDatModule& Instance()
    {
        static GwDatModule instance;
        return instance;
    }

    const char* Name() const override { return "GW Dat Module"; };
    bool HasSettings() override { return false; }
    void Initialize() override;
    void Terminate() override;

    // --- Texture decoding ---
    static IDirect3DTexture9** LoadGreyscaleTextureFromFileId(uint32_t file_id);
    static bool ReadDatFile(const wchar_t* fileHash, std::vector<uint8_t>* bytes_out, uint32_t stream_id = 0);

    // stream_id picks a stream within the file; item/armor UI icons live at stream 1 of the model file.
    static IDirect3DTexture9** LoadTextureFromFileId(uint32_t file_id, uint32_t stream_id = 0);

    // Loads an item's UI icon (stream 1 of the model file), recolouring its dyeable region (the stream
    // 0xc mask). `dyes` packs up to four GW::DyeColor values, one per byte, blended as GW combines dye
    // slots; 0 (no applied slot) yields the undyed icon.
    static IDirect3DTexture9** LoadItemImage(uint32_t model_file_id, uint32_t dyes = 0);

    // Decodes the texture for file_id from the dat and writes it to disk (format chosen by extension).
    static void SaveTextureFromFileIdToFile(uint32_t file_id, const std::filesystem::path& file_path);

    // --- Archive reading ---
    // Decompressed bytes for a GW file id; stream_id picks a stream (0 = the file's own data). False if absent.
    static bool ReadFile(uint32_t file_id, std::vector<uint8_t>& out, uint32_t stream_id = 0);

    // Async ReadFile: the worker retries until the file resolves, then calls `callback` (worker thread)
    // with the bytes, or with an empty vector after ~1 min; dropped if the worker stops. For files the
    // client streams in on demand.
    using ReadCallback = std::function<void(std::vector<uint8_t>&)>;
    static void ReadFileAsync(uint32_t file_id, ReadCallback callback, uint32_t stream_id = 0);

private:
    // Maps the client's open dat handle on first use; retries until it succeeds. Thread-safe.
    bool EnsureLoaded();
    const std::wstring& DatPath() const { return m_dat_path; } // diagnostics, valid after EnsureLoaded()
    bool Loaded() const { return m_loaded.load(std::memory_order_acquire); }
    // True if the handle scan itself failed (NtQuerySystemInformation blocked, usually AV/anti-cheat).
    bool HandleEnumerationBlocked() const { return m_handle_enum_failed.load(std::memory_order_acquire); }

    // Optional hook to ask the client to fetch file ids. Read misses accumulate (throttled per id) and
    // are handed to this in batches by the worker. Wired by Initialize; the reader works without it.
    using TriggerFn = std::function<void(const uint32_t* file_ids, size_t count)>;
    void SetTrigger(TriggerFn trigger);

    void StartWorker();                 // spins up the async-read poll worker (idempotent)
    void StopWorker();
    void WorkerLoop();
    void ProcessPendingReads();         // deliver/expire queued ReadFileAsync requests (worker thread)
    void TickleFetch(uint32_t file_id); // queue a missed file for the client to fetch (throttled per id)
    void FlushTickles();                // hand the queued fetch ids to the trigger as one batch (worker thread)

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

    std::thread m_worker;       // dedicated read/decode worker (polls for pending async reads)
    std::mutex m_worker_mutex;  // guards the worker lifecycle flags below + the wait
    std::condition_variable m_worker_cv;
    bool m_worker_running = false;
    bool m_worker_stop = false;

    struct PendingRead {
        uint32_t file_id;
        uint32_t stream_id;
        ReadCallback callback;
    };
    std::mutex m_pending_mutex; // guards m_pending_reads
    std::vector<PendingRead> m_pending_reads; // retried until they resolve (no timeout)

    std::mutex m_trigger_mutex; // guards m_trigger + m_tickled_at + m_tickle_batch
    TriggerFn m_trigger;
    std::unordered_map<uint32_t, uint32_t> m_tickled_at; // file_id -> GetTickCount() of last fetch tickle
    std::vector<uint32_t> m_tickle_batch;                // file ids queued for the next FlushTickles

    std::shared_mutex m_index_mutex;               // guards m_mapping/m_slots/m_fileid_to_slot against MaybeRefresh
    uint32_t m_last_refresh_ms = 0;                // GetTickCount of the last re-parse, to throttle refreshes
    long long m_indexed_size = 0;                  // dat size at the last parse; skip re-parsing if unchanged (guarded by m_load_mutex)
    void* m_mapping = nullptr;      // file-mapping HANDLE for the dat, replaced by MaybeRefresh when the dat grows
    long long m_file_size = 0;      // dat size captured when the mapping was created
    std::wstring m_dat_path;
    std::vector<MftEntry> m_slots;                       // indexed by physical MFT slot
    std::unordered_map<uint32_t, int> m_fileid_to_slot;  // GW file id -> base MFT slot
};
