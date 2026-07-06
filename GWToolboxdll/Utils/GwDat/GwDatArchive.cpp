#include <Windows.h>

#include <chrono>
#include <cstring>
#include <cwchar>
#include <utility>

#include "GwDatArchive.h"
#include "xentax.h"

namespace {
    const uint8_t kDatMagic[4] = {0x33, 0x41, 0x4e, 0x1a}; // "3ANa"
    constexpr size_t kMaxView = 0x800000;                  // cap each mapped window at 8 MB
    constexpr uint32_t kRefreshThrottleMs = 2000;          // min gap between re-parses on a read miss
    constexpr uint32_t kAsyncReadTimeoutMs = 60000;        // give up on an async read after ~1 minute
    constexpr uint32_t kAsyncPollMs = 250;                 // worker re-scan cadence for pending async reads

    DWORD AllocationGranularity()
    {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        return si.dwAllocationGranularity;
    }

    // Read via mapped views, never ReadFile: the client's dat handle is bound to its I/O port.
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

    // Full DOS path of a handle (empty on failure); length-sized so installs past MAX_PATH resolve.
    std::wstring HandlePath(HANDLE file)
    {
        const DWORD needed = GetFinalPathNameByHandleW(file, nullptr, 0, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
        if (!needed)
            return {};
        std::wstring path(needed, L'\0');
        const DWORD n = GetFinalPathNameByHandleW(file, path.data(), needed, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
        if (!n || n >= needed)
            return {};
        path.resize(n);
        return path;
    }

    // Match by extension, not "Gw.dat", so a renamed/-dat-relocated archive resolves (MapDat checks the magic).
    bool HasDatExtension(const std::wstring& path)
    {
        return path.ends_with(L".dat") || path.ends_with(L".snapshot");
    }

    // Whole-file read-only mapping, or nullptr unless the contents start with the 3ANa magic.
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

    // NtQuerySystemInformation(SystemExtendedHandleInformation) enumerates our real open handles.
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

void GwDatArchive::WorkerLoop()
{
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(m_task_mutex);
            // Wake for queued tasks, or periodically to re-scan pending async reads.
            m_task_cv.wait_for(lock, std::chrono::milliseconds(kAsyncPollMs),
                               [this] { return m_worker_stop || !m_tasks.empty(); });
            if (m_worker_stop)
                return; // drop any queued tasks on shutdown
            if (!m_tasks.empty()) {
                task = std::move(m_tasks.front());
                m_tasks.pop();
            }
        }
        if (task)
            task();
        ProcessPendingReads();
    }
}

void GwDatArchive::StartWorker()
{
    std::scoped_lock<std::mutex> lock(m_task_mutex);
    if (m_worker_running)
        return;
    m_worker_stop = false;
    m_worker_running = true;
    m_worker = std::thread([this] { WorkerLoop(); });
}

void GwDatArchive::StopWorker()
{
    {
        std::scoped_lock<std::mutex> lock(m_task_mutex);
        if (!m_worker_running)
            return;
        m_worker_stop = true;
    }
    m_task_cv.notify_all();
    if (m_worker.joinable())
        m_worker.join();
    {
        std::scoped_lock<std::mutex> lock(m_task_mutex);
        m_worker_running = false;
    }
    std::scoped_lock<std::mutex> lock(m_pending_mutex);
    m_pending_reads.clear(); // undelivered async reads are dropped on stop
}

void GwDatArchive::EnqueueTask(std::function<void()> task)
{
    {
        std::scoped_lock<std::mutex> lock(m_task_mutex);
        if (m_worker_running) {
            m_tasks.push(std::move(task));
            m_task_cv.notify_one();
            return;
        }
    }
    task(); // no worker running: run inline
}

bool GwDatArchive::EnsureLoaded()
{
    if (m_loaded.load(std::memory_order_acquire))
        return true;
    std::scoped_lock<std::mutex> lock(m_load_mutex);
    if (!m_loaded.load(std::memory_order_relaxed) && ParseIndex())
        m_loaded.store(true, std::memory_order_release);
    return m_loaded.load(std::memory_order_relaxed);
}

bool GwDatArchive::AcquireMappingInto(void*& out_mapping, long long& out_size, std::wstring& out_path)
{
    // The client holds Gw.dat exclusively, so we locate its open handle and map that instead.
    const std::vector<uint8_t> snapshot = QueryProcessHandles();
    if (snapshot.empty()) {
        // Scan failed (NtQuerySystemInformation blocked, usually AV/anti-cheat); flag for the UI.
        m_handle_enum_failed.store(true, std::memory_order_release);
        return false;
    }
    m_handle_enum_failed.store(false, std::memory_order_release);
    const auto* info = reinterpret_cast<const SystemHandleInfoEx*>(snapshot.data());
    const ULONG_PTR pid = GetCurrentProcessId();
    for (ULONG_PTR i = 0; i < info->NumberOfHandles; ++i) {
        const SystemHandleEntryEx& entry = info->Handles[i];
        if (entry.UniqueProcessId != pid)
            continue;
        const HANDLE h = reinterpret_cast<HANDLE>(entry.HandleValue);
        if (GetFileType(h) != FILE_TYPE_DISK)
            continue;
        const std::wstring path = HandlePath(h);
        if (path.empty() || !HasDatExtension(path))
            continue;
        // Re-map whole-file each time: the dat grows as files stream in, and a size-0 mapping is fixed at creation.
        const HANDLE mapping = MapDat(h);
        if (!mapping)
            continue;
        LARGE_INTEGER sz;
        out_size = GetFileSizeEx(h, &sz) ? sz.QuadPart : 0;
        out_mapping = mapping;
        out_path = path; // the archive we actually bound to, for diagnostics
        return true;
    }
    return false;
}

// Reads the header + MFT + hash list from `mapping` into fresh temporaries (untouched on failure).
bool GwDatArchive::ParseFrom(void* mapping_v, long long file_size, std::vector<MftEntry>& slots,
                             std::unordered_map<uint32_t, int>& fileid_to_slot)
{
    const HANDLE mapping = reinterpret_cast<HANDLE>(mapping_v);
    MainHeader head = {};
    if (!ReadAt(mapping, 0, &head, sizeof(head)))
        return false;
    if (memcmp(head.id, kDatMagic, sizeof(kDatMagic)) != 0) // "3ANa" archive magic
        return false;

    MftHeader mft_head = {};
    if (!ReadAt(mapping, head.mft_offset, &mft_head, sizeof(mft_head)))
        return false;
    if (mft_head.entry_count <= 16 || file_size <= head.mft_offset)
        return false;

    // Copy the whole 24-byte-per-slot MFT out in one pass rather than mapping a view per 600k+ entry.
    const int64_t table_bytes = static_cast<int64_t>(mft_head.entry_count) * 24;
    const int64_t avail = file_size - head.mft_offset;
    const size_t read_bytes = static_cast<size_t>(table_bytes < avail ? table_bytes : avail);
    if (read_bytes < static_cast<size_t>(17 * 24))
        return false;
    std::vector<uint8_t> table(read_bytes);
    if (!ReadAt(mapping, head.mft_offset, table.data(), read_bytes))
        return false;
    const int slot_count = static_cast<int>(read_bytes / 24);

    // Keep every physical slot: the game addresses a file as base_slot + stream_id.
    slots.resize(static_cast<size_t>(slot_count));
    memcpy(slots.data(), table.data(), static_cast<size_t>(slot_count) * 24);

    // Hash list is MFT[1] = slot 2 (slot 0 is the header, slots 1..15 reserved).
    const MftEntry& hash_list = slots[2];
    std::vector<MftExpansion> mftx;
    const int expansion_count = hash_list.size / static_cast<int>(sizeof(MftExpansion));
    mftx.resize(static_cast<size_t>(expansion_count));
    if (expansion_count > 0 && !ReadAt(mapping, hash_list.offset, mftx.data(),
                                       expansion_count * sizeof(MftExpansion)))
        return false;

    // Map each file id to its base slot; several ids may alias the same slot.
    for (const auto& e : mftx) {
        if (e.file_offset >= 16 && e.file_offset < slot_count)
            fileid_to_slot[static_cast<uint32_t>(e.file_number)] = e.file_offset;
    }
    return !fileid_to_slot.empty();
}

bool GwDatArchive::ParseIndex()
{
    // First load (under m_load_mutex, before any reads): map and parse straight into members.
    if (m_mapping) {
        CloseHandle(reinterpret_cast<HANDLE>(m_mapping));
        m_mapping = nullptr;
    }
    m_file_size = 0;
    m_slots.clear();
    m_fileid_to_slot.clear();

    void* mapping = nullptr;
    long long size = 0;
    std::wstring path;
    if (!AcquireMappingInto(mapping, size, path))
        return false;
    if (!ParseFrom(mapping, size, m_slots, m_fileid_to_slot)) {
        CloseHandle(reinterpret_cast<HANDLE>(mapping));
        return false;
    }
    m_mapping = mapping;
    m_file_size = size;
    m_dat_path = path;
    return true;
}

// Re-read the on-disk MFT (on a read miss, throttled) to pick up files streamed in since the last parse.
void GwDatArchive::MaybeRefresh()
{
    {
        std::scoped_lock<std::mutex> guard(m_load_mutex);
        const uint32_t now = GetTickCount();
        if (m_last_refresh_ms && now - m_last_refresh_ms < kRefreshThrottleMs)
            return;
        m_last_refresh_ms = now;
    }

    void* mapping = nullptr;
    long long size = 0;
    std::wstring path;
    if (!AcquireMappingInto(mapping, size, path))
        return;
    std::vector<MftEntry> slots;
    std::unordered_map<uint32_t, int> fileid_to_slot;
    if (!ParseFrom(mapping, size, slots, fileid_to_slot)) {
        CloseHandle(reinterpret_cast<HANDLE>(mapping));
        return;
    }

    std::unique_lock<std::shared_mutex> lock(m_index_mutex);
    CloseHandle(reinterpret_cast<HANDLE>(m_mapping));
    m_mapping = mapping;
    m_file_size = size;
    m_dat_path = path;
    m_slots.swap(slots);
    m_fileid_to_slot.swap(fileid_to_slot);
}

// Copies the raw (still-compressed) bytes for file_id/stream_id, shared-locked against MaybeRefresh.
bool GwDatArchive::ReadRaw(uint32_t file_id, uint32_t stream_id, std::vector<uint8_t>& input, bool& compressed)
{
    std::shared_lock<std::shared_mutex> lock(m_index_mutex);
    const auto found = m_fileid_to_slot.find(file_id);
    if (found == m_fileid_to_slot.end())
        return false;
    // A file's streams are a linked list: `c` is the stream number, `id` the next stream's slot.
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

    input.resize(static_cast<size_t>(e->size));
    compressed = e->a != 0;
    return ReadAt(reinterpret_cast<HANDLE>(m_mapping), e->offset, input.data(), static_cast<size_t>(e->size));
}

bool GwDatArchive::ReadFile(uint32_t file_id, std::vector<uint8_t>& out, uint32_t stream_id)
{
    out.clear();
    if (!EnsureLoaded() || !file_id)
        return false;

    std::vector<uint8_t> input;
    bool compressed = false;
    if (!ReadRaw(file_id, stream_id, input, compressed)) {
        // Missing: the client may have streamed this file in since our last parse - re-read and retry.
        MaybeRefresh();
        if (!ReadRaw(file_id, stream_id, input, compressed))
            return false;
    }

    if (compressed) {
        unsigned char* output = nullptr;
        int out_size = 0;
        UnpackGWDat(input.data(), static_cast<int>(input.size()), output, out_size);
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

void GwDatArchive::ReadFileAsync(uint32_t file_id, ReadCallback callback, uint32_t stream_id)
{
    if (!callback)
        return;
    if (!file_id) {
        std::vector<uint8_t> empty;
        callback(empty);
        return;
    }
    StartWorker(); // the poll loop delivers
    const uint32_t deadline = GetTickCount() + kAsyncReadTimeoutMs;
    std::scoped_lock<std::mutex> lock(m_pending_mutex);
    m_pending_reads.push_back({file_id, stream_id, deadline, std::move(callback), false});
}

void GwDatArchive::SetTrigger(TriggerFn trigger)
{
    std::scoped_lock<std::mutex> lock(m_pending_mutex);
    m_trigger = std::move(trigger);
}

// Worker pass: deliver resolved reads, time out stale ones, tickle the client once per pending file.
void GwDatArchive::ProcessPendingReads()
{
    std::vector<std::pair<ReadCallback, std::vector<uint8_t>>> ready; // callbacks fire outside the lock
    {
        std::scoped_lock<std::mutex> lock(m_pending_mutex);
        const uint32_t now = GetTickCount();
        for (auto it = m_pending_reads.begin(); it != m_pending_reads.end();) {
            std::vector<uint8_t> data;
            if (ReadFile(it->file_id, data, it->stream_id))
                ready.emplace_back(std::move(it->callback), std::move(data));
            else if (static_cast<int32_t>(now - it->deadline_ms) >= 0) // deadline passed (wrap-safe)
                ready.emplace_back(std::move(it->callback), std::vector<uint8_t>{});
            else {
                if (m_trigger && !it->triggered) { // ask the client to fetch it, once
                    m_trigger(it->file_id);
                    it->triggered = true;
                }
                ++it;
                continue;
            }
            it = m_pending_reads.erase(it);
        }
    }
    for (auto& [callback, data] : ready)
        callback(data);
}
