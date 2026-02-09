#include "ArenaNetFileParser.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <vector>
#include <Modules/GwDatTextureModule.h>

namespace {
    // FVF lookup tables (from the pattern)
    static constexpr uint32_t fvf_array_0[22] = {0x0, 0x8, 0x8, 0x10, 0x8, 0x10, 0x10, 0x18, 0x8, 0x10, 0x10, 0x18, 0x10, 0x18, 0x18, 0x20, 0x0, 0x0, 0x0, 0x1, 0xFFFFFFFF, 0xFFFFFFFF};

    static constexpr uint32_t fvf_array_1[8] = {0x0, 0xC, 0xC, 0x18, 0xC, 0x18, 0x18, 0x24};

    static constexpr uint32_t fvf_array_2[16] = {0x0, 0xC, 0x4, 0x10, 0xC, 0x18, 0x10, 0x1C, 0x4, 0x10, 0x8, 0x14, 0x10, 0x1C, 0x14, 0x20};

    // Helper functions
    uint32_t getFVF(uint32_t dat_fvf) {
        return ((dat_fvf & 0xff0) << 4) | ((dat_fvf >> 8) & 0x30) | (dat_fvf & 0xf);
    }
    uint32_t getVertexSizeFromFVF(uint32_t fvf) {
        return fvf_array_0[(fvf >> 0xc) & 0xf] + fvf_array_0[(fvf >> 8) & 0xf] + fvf_array_1[(fvf >> 4) & 7] + fvf_array_2[fvf & 0xf];
    }
} // namespace
namespace ArenaNetFileParser {
    void FileIdToFileHash(uint32_t file_id, wchar_t* fileHash)
    {
        fileHash[0] = static_cast<wchar_t>(((file_id - 1) % 0xff00) + 0x100);
        fileHash[1] = static_cast<wchar_t>(((file_id - 1) / 0xff00) + 0x100);
        fileHash[2] = 0;
    }
    uint32_t FileHashToFileId(const wchar_t* fileHash)
    {
        if (!fileHash) return 0;
        if (((0xff < *fileHash) && (0xff < fileHash[1])) && ((fileHash[2] == 0 || ((0xff < fileHash[2] && (fileHash[3] == 0)))))) {
            return (*fileHash - 0xff00ff) + (uint32_t)fileHash[1] * 0xff00;
        }
        return 0;
    }

    char* GameAssetFile::fileType()
    {
        if (data_size < 4) return 0;
        return (char*)data.data(); // Read file type from the first 4 bytes
    }
    bool GameAssetFile::parse(std::vector<uint8_t>& _data)
    {
        data = std::move(_data);
        data_size = data.size();
        return isValid();
    }
    bool GameAssetFile::readFromDat(const uint32_t file_id, uint32_t stream_id)
    {
        wchar_t fileHash[4] = {0};
        FileIdToFileHash(file_id, fileHash);
        return readFromDat(fileHash, stream_id);
    }
    bool GameAssetFile::readFromDat(const wchar_t* file_hash, uint32_t stream_id)
    {
        std::vector<uint8_t> bytes;
        if (!GwDatTextureModule::ReadDatFile(file_hash, &bytes, stream_id)) return false;
        return parse(bytes);
    }
    const uint8_t ArenaNetFile::getFFNAType() const
    {
        return (uint8_t)data[4];
    }
    const bool ArenaNetFile::isValid() {
        return GameAssetFile::isValid() && strncmp(fileType(), "ffna", 4) == 0;
    }
    const bool ATexFile::isValid() { 
        return GameAssetFile::isValid() && strncmp(fileType(), "ATEX", 4) == 0; 
    }
    const Chunk* ArenaNetFile::FindChunk(ChunkType chunk_type)
    {
        ASSERT(isValid());
        size_t offset = 5;
        // Parse chunk headers and record their locations
        while (offset < data_size) {
            const auto chunk = (Chunk*)&data[offset];
            if (chunk->chunk_id == chunk_type)
                return chunk;
            offset += chunk->chunk_size + 8;
        }
        return nullptr;
    }
}

