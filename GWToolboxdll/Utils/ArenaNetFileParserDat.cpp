#include "ArenaNetFileParser.h"
#include <Modules/GwDatModule.h>

namespace ArenaNetFileParser {
    bool GameAssetFile::readFromDat(const uint32_t id, uint32_t stream_id)
    {
        wchar_t hash[4] = {0};
        FileIdToFileHash(id, hash);
        return readFromDat(hash, stream_id);
    }

    bool GameAssetFile::readFromDat(const wchar_t* hash, uint32_t stream_id)
    {
        std::vector<uint8_t> bytes;
        file_id = FileHashToFileId(hash);
        data_size = 0;
        data.clear();
        if (!GwDatModule::ReadDatFile(hash, &bytes, stream_id)) return false;
        return parse(bytes);
    }
}
