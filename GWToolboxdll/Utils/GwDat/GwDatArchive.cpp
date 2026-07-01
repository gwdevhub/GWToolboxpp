#include <Windows.h>

#include <cstring>
#include <cwchar>
#include <filesystem>

#include "GwDatArchive.h"
#include "xentax.h"

namespace {
    const uint8_t kDatMagic[4] = {0x33, 0x41, 0x4e, 0x1a}; // "3ANa"
    constexpr size_t kMaxView = 0x800000;                  // cap each mapped window at 8 MB

    // The client keeps Gw.dat in its own directory, so derive it from the exe path.
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

    // Read via mapped views, never ReadFile: the client's dat handle is bound to its
    // I/O completion port and a stray ReadFile on it would corrupt its I/O loop.
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

    // True if the handle refers to a file named "Gw.dat". GetFinalPathNameByHandle is
    // a metadata query, so it's safe on the client's port-bound handle (no I/O issued).
    bool HandleIsGwDat(HANDLE file)
    {
        wchar_t path[MAX_PATH];
        const DWORD n = GetFinalPathNameByHandleW(file, path, MAX_PATH, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
        if (!n || n >= MAX_PATH)
            return false;
        const wchar_t* name = wcsrchr(path, L'\\');
        return _wcsicmp(name ? name + 1 : path, L"Gw.dat") == 0;
    }

    // Whole-file read-only mapping, but only if the contents start with the archive
    // magic; nullptr otherwise. Confirms via a view, never ReadFile.
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

    // NtQuerySystemInformation(SystemExtendedHandleInformation) lets us enumerate our
    // real open handles instead of guessing handle values.
    constexpr ULONG kSystemExtendedHandleInformation = 64;
    constexpr ULONG kStatusInfoLengthMismatch = 0xC0000004UL;

    struct SystemHandleEntryEx {
        void* Object;
        ULONG_PTR UniqueProcessId;
        ULONG_PTR HandleValue;
        ULONG GrantedAccess;
        USHORT CreatorBackTraceIndex;
        USHORT ObjectTypeIndex;
        ULONG HandleAttributes;
        ULONG Reserved;
    };
    struct SystemHandleInfoEx {
        ULONG_PTR NumberOfHandles;
        ULONG_PTR Reserved;
        SystemHandleEntryEx Handles[1];
    };
    using NtQuerySystemInformation_t = LONG(__stdcall*)(ULONG, void*, ULONG, ULONG*);

    // Snapshots this process's handle table. Empty on failure.
    std::vector<uint8_t> QueryProcessHandles()
    {
        const auto ntdll = GetModuleHandleW(L"ntdll.dll");
        const auto NtQuerySystemInformation =
            ntdll ? reinterpret_cast<NtQuerySystemInformation_t>(GetProcAddress(ntdll, "NtQuerySystemInformation"))
                  : nullptr;
        if (!NtQuerySystemInformation)
            return {};

        std::vector<uint8_t> buffer(0x10000);
        for (;;) {
            ULONG needed = 0;
            const LONG status = NtQuerySystemInformation(kSystemExtendedHandleInformation, buffer.data(),
                                                         static_cast<ULONG>(buffer.size()), &needed);
            if (static_cast<ULONG>(status) == kStatusInfoLengthMismatch) {
                buffer.resize(needed ? needed + 0x10000 : buffer.size() * 2);
                continue;
            }
            if (status != 0)
                return {};
            return buffer;
        }
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
    // The client holds Gw.dat exclusively and bound to its I/O completion port, so we
    // can neither open nor ReadFile it - locate its open handle and map that instead.
    const std::vector<uint8_t> snapshot = QueryProcessHandles();
    if (snapshot.empty())
        return false;
    const auto* info = reinterpret_cast<const SystemHandleInfoEx*>(snapshot.data());
    const ULONG_PTR pid = GetCurrentProcessId();
    for (ULONG_PTR i = 0; i < info->NumberOfHandles; ++i) {
        const SystemHandleEntryEx& entry = info->Handles[i];
        if (entry.UniqueProcessId != pid)
            continue;
        const HANDLE h = reinterpret_cast<HANDLE>(entry.HandleValue);
        if (GetFileType(h) != FILE_TYPE_DISK || !HandleIsGwDat(h))
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

        // Copy the whole 24-byte-per-slot MFT out in one pass rather than mapping a view per 600k+ entry.
        const int64_t table_bytes = static_cast<int64_t>(mft_head.entry_count) * 24;
        const int64_t avail = m_file_size - head.mft_offset;
        const size_t read_bytes = static_cast<size_t>(table_bytes < avail ? table_bytes : avail);
        std::vector<uint8_t> table(read_bytes);
        if (read_bytes < static_cast<size_t>(17 * 24) ||
            !ReadAt(mapping, head.mft_offset, table.data(), read_bytes))
            break;
        const int slot_count = static_cast<int>(read_bytes / 24);

        // Keep every physical slot: the game addresses a file as base_slot + stream_id.
        m_slots.resize(static_cast<size_t>(slot_count));
        memcpy(m_slots.data(), table.data(), static_cast<size_t>(slot_count) * 24);

        // Hash list is MFT[1] = slot 2 (slot 0 is the header, slots 1..15 reserved).
        const MftEntry& hash_list = m_slots[2];
        std::vector<MftExpansion> mftx;
        const int expansion_count = hash_list.size / static_cast<int>(sizeof(MftExpansion));
        mftx.resize(static_cast<size_t>(expansion_count));
        if (expansion_count > 0 && !ReadAt(mapping, hash_list.offset, mftx.data(),
                                           expansion_count * sizeof(MftExpansion)))
            break;

        // Map each file id to its base slot; several ids may alias the same slot.
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
    // A file's streams form a linked list, not consecutive slots: each entry's `c`
    // holds its stream number and `id` points at the next stream's slot. Walk from the
    // base entry until the requested stream turns up (the game's own lookup does this).
    int idx = found->second;
    const MftEntry* e = nullptr;
    for (int guard = 0; guard < 256; ++guard) {
        if (idx < 16 || idx >= static_cast<int>(m_slots.size()))
            return false;
        const MftEntry& candidate = m_slots[static_cast<size_t>(idx)];
        if (candidate.c == stream_id) {
            e = &candidate;
            break;
        }
        idx = candidate.id; // follow the chain to the next stream
        if (idx <= 0)
            return false;
    }
    if (!e || !e->b || e->size <= 0) // b == 0 marks an empty/base slot with no payload
        return false;

    std::vector<uint8_t> input(static_cast<size_t>(e->size));
    if (!ReadAt(reinterpret_cast<HANDLE>(m_mapping), e->offset, input.data(), static_cast<size_t>(e->size)))
        return false;

    if (e->a) {
        unsigned char* output = nullptr;
        int out_size = 0;
        UnpackGWDat(input.data(), e->size, output, out_size);
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
