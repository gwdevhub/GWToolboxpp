#include <Windows.h>

#include <cstring>
#include <filesystem>

#include "GwDatArchive.h"
#include "xentax.h"

namespace {
    const uint8_t kDatMagic[4] = {0x33, 0x41, 0x4e, 0x1a}; // "3ANa"
    constexpr size_t kMaxView = 0x800000;                  // cap each mapped window at 8 MB

    // Resolves Gw.dat next to the running executable. The client always keeps the
    // dat in its own directory, so the exe path is the authoritative source.
    std::wstring ResolveDatPath()
    {
        wchar_t exe[MAX_PATH] = {0};
        const DWORD n = GetModuleFileNameW(nullptr, exe, MAX_PATH);
        if (!n || n >= MAX_PATH)
            return {};
        return (std::filesystem::path(exe).parent_path() / L"Gw.dat").wstring();
    }

    DWORD AllocationGranularity()
    {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        return si.dwAllocationGranularity;
    }

    // Reads `count` bytes at absolute `offset` through the file mapping. Views are
    // served by the memory manager's paging path, so this never issues ReadFile on
    // the underlying handle - critical, because the client's dat handle is bound to
    // its I/O completion port and a stray ReadFile would corrupt its I/O loop.
    // Windows are granularity-aligned and capped so we never map a huge span.
    bool ReadAt(HANDLE mapping, int64_t offset, void* buffer, size_t count)
    {
        static const DWORD gran = AllocationGranularity();
        auto* dst = static_cast<uint8_t*>(buffer);
        while (count > 0) {
            const int64_t base = offset - (offset % gran);
            const size_t delta = static_cast<size_t>(offset - base);
            const size_t chunk = count < kMaxView ? count : kMaxView;
            void* view = MapViewOfFile(mapping, FILE_MAP_READ,
                                       static_cast<DWORD>((static_cast<uint64_t>(base) >> 32) & 0xFFFFFFFF),
                                       static_cast<DWORD>(base & 0xFFFFFFFF), delta + chunk);
            if (!view)
                return false;
            memcpy(dst, static_cast<uint8_t*>(view) + delta, chunk);
            UnmapViewOfFile(view);
            dst += chunk;
            offset += static_cast<int64_t>(chunk);
            count -= chunk;
        }
        return true;
    }

    // A whole-file, read-only mapping of the given dat handle, or nullptr. Verifies
    // the archive magic through a view (no ReadFile).
    HANDLE MapDat(HANDLE file)
    {
        const HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (!mapping)
            return nullptr;
        void* head = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof(kDatMagic));
        const bool is_dat = head && memcmp(head, kDatMagic, sizeof(kDatMagic)) == 0;
        if (head)
            UnmapViewOfFile(head);
        if (is_dat)
            return mapping;
        CloseHandle(mapping);
        return nullptr;
    }
} // namespace

GwDatArchive& GwDatArchive::Instance()
{
    static GwDatArchive instance;
    return instance;
}

bool GwDatArchive::EnsureLoaded()
{
    if (m_loaded.load(std::memory_order_acquire))
        return true;
    std::lock_guard<std::mutex> lock(m_load_mutex);
    if (!m_loaded.load(std::memory_order_relaxed) && ParseIndex())
        m_loaded.store(true, std::memory_order_release);
    return m_loaded.load(std::memory_order_relaxed);
}

bool GwDatArchive::AcquireMapping()
{
    // The running client opens Gw.dat read/write with no sharing (NtFile.cpp), so
    // we can't open our own handle, and it binds the handle to its I/O completion
    // port, so we must not ReadFile it either. Instead map the client's already-open
    // handle: scan the process handle table for the disk handle that maps to the
    // archive magic. If none is found the client hasn't opened the dat yet.
    for (ULONG_PTR v = 4; v <= 0x40000; v += 4) {
        const HANDLE h = reinterpret_cast<HANDLE>(v);
        if (GetFileType(h) != FILE_TYPE_DISK)
            continue;
        const HANDLE mapping = MapDat(h);
        if (!mapping)
            continue;
        LARGE_INTEGER sz;
        if (GetFileSizeEx(h, &sz))
            m_file_size = sz.QuadPart;
        m_mapping = mapping;
        return true;
    }
    return false;
}

bool GwDatArchive::ParseIndex()
{
    // Reset any state from a previous failed attempt so retries start clean.
    if (m_mapping) {
        CloseHandle(reinterpret_cast<HANDLE>(m_mapping));
        m_mapping = nullptr;
    }
    m_file_size = 0;
    m_slots.clear();
    m_fileid_to_slot.clear();

    if (m_dat_path.empty())
        m_dat_path = ResolveDatPath();
    if (m_dat_path.empty())
        return false;

    if (!AcquireMapping())
        return false;
    const HANDLE mapping = reinterpret_cast<HANDLE>(m_mapping);

    bool ok = false;
    do {
        MainHeader head = {};
        if (!ReadAt(mapping, 0, &head, sizeof(head)))
            break;
        // "3ANa" archive magic.
        if (!(head.id[0] == 0x33 && head.id[1] == 0x41 && head.id[2] == 0x4e && head.id[3] == 0x1a))
            break;

        MftHeader mft_head = {};
        if (!ReadAt(mapping, head.mft_offset, &mft_head, sizeof(mft_head)))
            break;
        if (mft_head.entry_count <= 16 || m_file_size <= head.mft_offset)
            break;

        // The MFT is one contiguous table of 24-byte slots starting at mft_offset:
        // slot 0 is the header, slots 1..15 are reserved (slot [2] -> hash list),
        // and slots 16.. are the real files. Copy it out in one pass rather than
        // resolving a view per (potentially 600k+) entry.
        const int64_t table_bytes = static_cast<int64_t>(mft_head.entry_count) * 24;
        const int64_t avail = m_file_size - head.mft_offset;
        const size_t read_bytes = static_cast<size_t>(table_bytes < avail ? table_bytes : avail);
        std::vector<uint8_t> table(read_bytes);
        if (read_bytes < static_cast<size_t>(17 * 24) ||
            !ReadAt(mapping, head.mft_offset, table.data(), read_bytes))
            break;
        const int slot_count = static_cast<int>(read_bytes / 24);

        // Keep every physical slot: the game addresses a file as base_slot + stream_id,
        // so a file's extra streams are the slots that immediately follow its base.
        m_slots.resize(static_cast<size_t>(slot_count));
        memcpy(m_slots.data(), table.data(), static_cast<size_t>(slot_count) * 24);

        // MFT[1] (the hash list) is slot 2: slot 0 is the header, and the 15 reserved
        // entries the game reads after it are slots 1..15.
        const MftEntry& hash_list = m_slots[2];
        std::vector<MftExpansion> mftx;
        const int expansion_count = hash_list.size / static_cast<int>(sizeof(MftExpansion));
        mftx.resize(static_cast<size_t>(expansion_count));
        if (expansion_count > 0 && !ReadAt(mapping, hash_list.offset, mftx.data(),
                                           expansion_count * sizeof(MftExpansion)))
            break;

        // Each expansion maps a file id (file_number) to its base slot (file_offset).
        // Several ids can alias the same slot; each still gets its own mapping.
        for (const auto& e : mftx) {
            if (e.file_offset >= 16 && e.file_offset < slot_count)
                m_fileid_to_slot[static_cast<uint32_t>(e.file_number)] = e.file_offset;
        }
        ok = !m_fileid_to_slot.empty();
    } while (false);

    // m_mapping stays open for the archive lifetime; reads reuse it.
    return ok;
}

bool GwDatArchive::ReadFile(uint32_t file_id, std::vector<uint8_t>& out, uint32_t stream_id)
{
    out.clear();
    if (!EnsureLoaded() || !file_id)
        return false;

    const auto found = m_fileid_to_slot.find(file_id);
    if (found == m_fileid_to_slot.end())
        return false;
    // stream_id offsets from the file's base slot (stream 0 = base, 1 = next, ...).
    const int64_t target = static_cast<int64_t>(found->second) + stream_id;
    if (target < 0 || target >= static_cast<int64_t>(m_slots.size()))
        return false;
    const MftEntry& e = m_slots[static_cast<size_t>(target)];
    if (!e.b || e.size <= 0) // b == 0 marks an empty/base slot with no payload
        return false;

    std::vector<uint8_t> input(static_cast<size_t>(e.size));
    if (!ReadAt(reinterpret_cast<HANDLE>(m_mapping), e.offset, input.data(), static_cast<size_t>(e.size)))
        return false;

    if (e.a) {
        unsigned char* output = nullptr;
        int out_size = 0;
        UnpackGWDat(input.data(), e.size, output, out_size);
        if (output) {
            if (out_size > 0)
                out.assign(output, output + out_size);
            delete[] output;
        }
    }
    else {
        out = std::move(input);
    }
    return !out.empty();
}
