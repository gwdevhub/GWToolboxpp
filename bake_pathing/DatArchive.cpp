#include "DatArchive.h"
#include "gw_inflate.h"

#include <algorithm>
#include <cstring>

extern unsigned char TableData1[112];
extern unsigned char Table2[256];
extern unsigned char TableData3[768];
extern unsigned char Table5[32];
extern unsigned char TableData6[92];

namespace {
    GwInflateTables MakeTables()
    {
        GwInflateTables tables{};
        for (size_t i = 0; i < 14; ++i) {
            memcpy(&tables.table1_first[i], TableData1 + i * 8, 4);
            memcpy(&tables.table1_index[i], TableData1 + i * 8 + 4, 2);
        }
        memcpy(tables.table2, Table2, sizeof(tables.table2));
        memcpy(tables.table3, TableData3 + 256, 29);
        memcpy(tables.length_bits, TableData3 + 0x1dc + 256, 29);
        memcpy(tables.distance_bits, Table5, sizeof(tables.distance_bits));
        memcpy(tables.distance_base, TableData6, sizeof(tables.distance_base));
        return tables;
    }

    bool Inflate(const std::vector<uint8_t>& input, std::vector<uint8_t>& out)
    {
        static const auto tables = MakeTables();
        constexpr size_t limit = 96 << 20;
        size_t capacity = std::min(limit, std::max<size_t>(65536, input.size() * 3));
        while (capacity) {
            out.resize(capacity);
            size_t written = 0;
            const int result = gw_inflate_all(input.data(), input.size(), out.data(), capacity, &written, &tables);
            if (result == 0 && written <= capacity) {
                out.resize(written);
                return out.size() >= 4 && memcmp(out.data(), "ffna", 4) == 0;
            }
            if (result != 1 || capacity == limit) break;
            capacity = std::min(limit, capacity * 2);
        }
        out.clear();
        return false;
    }
}

DatArchive::DatArchive(const std::filesystem::path& path)
{
    file_ = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file_ == INVALID_HANDLE_VALUE) return;
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file_, &size)) return;
    size_ = size.QuadPart;
    mapping_ = CreateFileMappingW(file_, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!mapping_) return;

    MainHeader head{};
    if (!ReadAt(0, &head, sizeof(head)) || memcmp(head.id, "3AN\x1a", 4) != 0) return;
    MftHeader mft{};
    if (!ReadAt(head.mft_offset, &mft, sizeof(mft)) || mft.entry_count < 17) return;
    const int64_t byte_count = static_cast<int64_t>(mft.entry_count) * sizeof(MftEntry);
    if (head.mft_offset < 0 || byte_count > size_ - head.mft_offset) return;
    slots_.resize(mft.entry_count);
    if (!ReadAt(head.mft_offset, slots_.data(), static_cast<size_t>(byte_count))) return;

    const auto& hash = slots_[2];
    if (hash.size < 0 || hash.size % sizeof(HashEntry) != 0) return;
    std::vector<HashEntry> entries(static_cast<size_t>(hash.size) / sizeof(HashEntry));
    if (!ReadAt(hash.offset, entries.data(), hash.size)) return;
    for (const auto& entry : entries) {
        if (entry.slot >= 16 && entry.slot < static_cast<int>(slots_.size()))
            by_file_id_[static_cast<uint32_t>(entry.file_id)] = entry.slot;
    }
    valid_ = !by_file_id_.empty();
}

DatArchive::~DatArchive()
{
    if (mapping_) CloseHandle(mapping_);
    if (file_ != INVALID_HANDLE_VALUE) CloseHandle(file_);
}

bool DatArchive::ReadAt(int64_t offset, void* buffer, size_t size) const
{
    if (!mapping_ || offset < 0 || offset > size_ || size > static_cast<uint64_t>(size_ - offset)) return false;
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    const int64_t granularity = system.dwAllocationGranularity;
    auto* dst = static_cast<uint8_t*>(buffer);
    while (size) {
        const int64_t base = offset - offset % granularity;
        const size_t delta = static_cast<size_t>(offset - base);
        const size_t count = std::min<size_t>(size, 0x800000);
        auto* view = static_cast<const uint8_t*>(MapViewOfFile(
            mapping_, FILE_MAP_READ, static_cast<DWORD>(static_cast<uint64_t>(base) >> 32),
            static_cast<DWORD>(base), delta + count));
        if (!view) return false;
        memcpy(dst, view + delta, count);
        UnmapViewOfFile(view);
        dst += count;
        offset += count;
        size -= count;
    }
    return true;
}

bool DatArchive::ReadFile(uint32_t file_id, uint8_t stream_id, std::vector<uint8_t>& out) const
{
    out.clear();
    if (!Valid()) return false;
    const auto it = by_file_id_.find(file_id);
    if (it == by_file_id_.end()) return false;
    int32_t idx = it->second;
    for (int tries = 0; tries < 256; ++tries) {
        if (idx < 16 || idx >= static_cast<int32_t>(slots_.size())) return false;
        const auto& slot = slots_[idx];
        if (slot.stream == stream_id) {
            if (!slot.payload || slot.size <= 0) return false;
            std::vector<uint8_t> input(slot.size);
            if (!ReadAt(slot.offset, input.data(), input.size())) return false;
            if (slot.compressed) return Inflate(input, out);
            out = std::move(input);
            return true;
        }
        idx = slot.next;
        if (idx <= 0) return false;
    }
    return false;
}
