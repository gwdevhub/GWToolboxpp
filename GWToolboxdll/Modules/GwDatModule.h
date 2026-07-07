#pragma once

#include <ToolboxModule.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// Guild Wars .dat module: memory-maps the client's on-disk Gw.dat, parses its MFT, and decodes textures
// from it for the UI. It does not interface with the game's streaming/download subsystem - a file the dat
// doesn't already contain won't load (ReadFile hints the user at -image). The MFT/decompression logic is
// derived from GuildWarsMapBrowser (see GwDat/CREDITS.txt).
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
    void Terminate() override;

    // --- Texture decoding ---
    static IDirect3DTexture9** LoadGreyscaleTextureFromFileId(uint32_t file_id);
    static bool ReadDatFile(const wchar_t* fileHash, std::vector<uint8_t>* bytes_out, uint32_t stream_id = 0);

    // stream_id picks a stream within the file; item/armor UI icons live at stream 1 of the model file.
    static IDirect3DTexture9** LoadTextureFromFileId(uint32_t file_id, uint32_t stream_id = 0);

    // Loads an item's UI icon (stream 1 of the model file), recolouring its dyeable region (the stream
    // 0xc mask). `dyes` packs up to four GW::DyeColor values, one per byte, blended as GW combines dye
    // slots; 0 (no applied slot) yields the undyed icon.
    // failed_out, if given, is set true once the decode has actually run and permanently failed (as
    // opposed to simply not having decoded yet) - callers can use this to render a placeholder.
    static IDirect3DTexture9** LoadItemImage(uint32_t model_file_id, uint32_t dyes = 0, bool* failed_out = nullptr);

    // Decodes the texture for file_id from the dat and writes it to disk (format chosen by extension).
    static void SaveTextureFromFileIdToFile(uint32_t file_id, const std::filesystem::path& file_path);

    // --- Archive reading ---
    // Decompressed bytes for a GW file id; stream_id picks a stream (0 = the file's own data). False if absent.
    static bool ReadFile(uint32_t file_id, std::vector<uint8_t>& out, uint32_t stream_id = 0);

    // True once any read found the dat mapped but did not contain the requested file - i.e. an incomplete
    // (streaming/Steam) install. UI can surface this to suggest running Guild Wars with -image.
    static bool MissingDatData();

private:
    // Maps the client's open dat handle on first use (idempotent, thread-safe).
    bool EnsureLoaded();

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

    bool ParseIndex();                                  // first load: map + parse into members
    bool AcquireMappingInto(void*& mapping, long long& size);
    bool ParseFrom(void* mapping, long long size, std::vector<MftEntry>& slots,
                   std::unordered_map<uint32_t, int>& fileid_to_slot); // header + MFT + hash list -> temporaries
    bool ReadRaw(uint32_t file_id, uint32_t stream_id, std::vector<uint8_t>& input, bool& compressed);

    std::mutex m_load_mutex;                        // serialises the one-time ParseIndex
    std::atomic<bool> m_loaded{false};
    std::atomic<bool> m_handle_enum_failed{false}; // set when the handle scan itself failed (AV/anti-cheat)
    std::atomic<bool> m_missing_data{false};       // set when the dat is mapped but lacks a requested file

    // Written once by ParseIndex() under m_load_mutex, then immutable and read lock-free.
    void* m_mapping = nullptr; // file-mapping HANDLE for the dat
    std::vector<MftEntry> m_slots;                       // indexed by physical MFT slot
    std::unordered_map<uint32_t, int> m_fileid_to_slot;  // GW file id -> base MFT slot
};
