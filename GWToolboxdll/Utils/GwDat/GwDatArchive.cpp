#include <Windows.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <filesystem>

#include "GwDatArchive.h"
#include "xentax.h"

namespace {
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

    // Reads `count` bytes at absolute `offset`. Returns false on short/failed read.
    bool ReadAt(HANDLE file, int64_t offset, void* buffer, DWORD count)
    {
        LARGE_INTEGER pos;
        pos.QuadPart = offset;
        if (!SetFilePointerEx(file, pos, nullptr, FILE_BEGIN))
            return false;
        DWORD read = 0;
        return ReadFile(file, buffer, count, &read, nullptr) && read == count;
    }
} // namespace

GwDatArchive& GwDatArchive::Instance()
{
    static GwDatArchive instance;
    return instance;
}

bool GwDatArchive::EnsureLoaded()
{
    std::call_once(m_load_once, [this] { m_loaded = ParseIndex(); });
    return m_loaded;
}

bool GwDatArchive::ParseIndex()
{
    m_dat_path = ResolveDatPath();
    if (m_dat_path.empty())
        return false;

    const HANDLE file = CreateFileW(m_dat_path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                    nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;

    bool ok = false;
    do {
        MainHeader head = {};
        if (!ReadAt(file, 0, &head, sizeof(head)))
            break;
        // "3ANa" archive magic.
        if (!(head.id[0] == 0x33 && head.id[1] == 0x41 && head.id[2] == 0x4e && head.id[3] == 0x1a))
            break;

        MftHeader mft_head = {};
        if (!ReadAt(file, head.mft_offset, &mft_head, sizeof(mft_head)))
            break;
        if (mft_head.entry_count <= 16)
            break;

        // The MFT is one contiguous table of 24-byte slots starting at mft_offset:
        // slot 0 is the header, slots 1..15 are reserved (slot [2] -> hash list),
        // and slots 16.. are the real files. Pull it into memory in one read rather
        // than issuing a syscall per (potentially 600k+) entry.
        LARGE_INTEGER file_size;
        if (!GetFileSizeEx(file, &file_size))
            break;
        const int64_t table_bytes = static_cast<int64_t>(mft_head.entry_count) * 24;
        const int64_t avail = file_size.QuadPart - head.mft_offset;
        const size_t read_bytes = static_cast<size_t>((table_bytes < avail ? table_bytes : avail));
        std::vector<uint8_t> table(read_bytes);
        if (read_bytes < static_cast<size_t>(17 * 24) ||
            !ReadAt(file, head.mft_offset, table.data(), static_cast<DWORD>(read_bytes)))
            break;
        const int slot_count = static_cast<int>(read_bytes / 24);
        const auto slot = [&table](int s) { MftEntry e = {}; memcpy(&e, table.data() + static_cast<size_t>(s) * 24, 24); return e; };

        // Reserved entries occupy slots 1..15; browser indexes these as MFT[0..14],
        // so MFT[1] (the hash list) is slot 2.
        m_mft.reserve(static_cast<size_t>(slot_count));
        for (int s = 1; s <= 15; ++s)
            m_mft.push_back(slot(s));

        const MftEntry& hash_list = m_mft[1];
        std::vector<MftExpansion> mftx;
        const int expansion_count = hash_list.size / static_cast<int>(sizeof(MftExpansion));
        mftx.resize(static_cast<size_t>(expansion_count));
        if (expansion_count > 0 && !ReadAt(file, hash_list.offset, mftx.data(),
                                           static_cast<DWORD>(expansion_count * sizeof(MftExpansion))))
            break;
        std::sort(mftx.begin(), mftx.end(),
                  [](const MftExpansion& a, const MftExpansion& b) { return a.file_offset < b.file_offset; });

        size_t hashcounter = 0;
        while (hashcounter < mftx.size() && mftx[hashcounter].file_offset < 16)
            ++hashcounter;

        for (int x = 16; x < slot_count && x < mft_head.entry_count - 1; ++x) {
            MftEntry e = slot(x);
            if (hashcounter < mftx.size() && x == mftx[hashcounter].file_offset) {
                e.hash = static_cast<uint32_t>(mftx[hashcounter].file_number);
                m_mft.push_back(e);
                // A single physical entry can be referenced by several file numbers.
                while (hashcounter + 1 < mftx.size() &&
                       mftx[hashcounter].file_offset == mftx[hashcounter + 1].file_offset) {
                    ++hashcounter;
                    e.hash = static_cast<uint32_t>(mftx[hashcounter].file_number);
                    m_mft.push_back(e);
                }
                ++hashcounter;
            }
            else {
                m_mft.push_back(e);
            }
        }

        for (int i = 0; i < static_cast<int>(m_mft.size()); ++i) {
            if (m_mft[i].hash != 0)
                m_hash_index[m_mft[i].hash].push_back(i);
        }
        ok = !m_hash_index.empty();
    } while (false);

    CloseHandle(file);
    return ok;
}

bool GwDatArchive::ReadFile(uint32_t file_id, std::vector<uint8_t>& out, uint32_t stream_id)
{
    out.clear();
    if (!EnsureLoaded() || !file_id)
        return false;

    const auto found = m_hash_index.find(file_id);
    if (found == m_hash_index.end() || found->second.empty())
        return false;
    const auto& indices = found->second;
    const int index = indices[stream_id < indices.size() ? stream_id : 0];
    const MftEntry& e = m_mft[static_cast<size_t>(index)];
    if (!e.b || e.size <= 0) // b == 0 marks an empty/base slot with no payload
        return false;

    const HANDLE file = CreateFileW(m_dat_path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                    nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;

    std::vector<uint8_t> input(static_cast<size_t>(e.size));
    const bool read_ok = ReadAt(file, e.offset, input.data(), static_cast<DWORD>(e.size));
    CloseHandle(file);
    if (!read_ok)
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
