#include "ArenaNetFileParser.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <vector>

ArenaNetFileParser::ArenaNetFileParser(const uint8_t* data, size_t size) 
    : data(data), data_size(size), offset(0) {
}

bool ArenaNetFileParser::LoadGameAssetFile(const wchar_t* file_name, GameAssetFile* asset) {
    if (!file_name || !asset) {
        return false;
    }

    // Open file in binary mode
    std::ifstream file(file_name, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return false;
    }

    // Get file size
    std::streamsize size = file.tellg();
    if (size <= 0) {
        return false;
    }
    
    file.seekg(0, std::ios::beg);

    // Read entire file into buffer
    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return false;
    }

    // Parse the file data
    ArenaNetFileParser parser(buffer.data(), buffer.size());
    return parser.parse(*asset);
}

template<typename T>
bool ArenaNetFileParser::read(T& value) {
    if (offset + sizeof(T) > data_size) {
        return false;
    }
    std::memcpy(&value, data + offset, sizeof(T));
    offset += sizeof(T);
    return true;
}

template<typename T>
bool ArenaNetFileParser::peek(T& value, size_t peek_offset) const {
    if (offset + peek_offset + sizeof(T) > data_size) {
        return false;
    }
    std::memcpy(&value, data + offset + peek_offset, sizeof(T));
    return true;
}

bool ArenaNetFileParser::readString(std::string& result) {
    result.clear();
    while (offset < data_size) {
        char c;
        if (!read(c)) return false;
        if (c == 0) break;
        result += c;
    }
    return true;
}

bool ArenaNetFileParser::skip(size_t bytes) {
    if (offset + bytes > data_size) {
        return false;
    }
    offset += bytes;
    return true;
}

uint32_t ArenaNetFileParser::getFVF(uint32_t dat_fvf) {
    return ((dat_fvf & 0xff0) << 4) | ((dat_fvf >> 8) & 0x30) | (dat_fvf & 0xf);
}

uint32_t ArenaNetFileParser::getVertexSizeFromFVF(uint32_t fvf) {
    return fvf_array_0[(fvf >> 0xc) & 0xf] + 
           fvf_array_0[(fvf >> 8) & 0xf] + 
           fvf_array_1[(fvf >> 4) & 7] +
           fvf_array_2[fvf & 0xf];
}

bool ArenaNetFileParser::parse(GameAssetFile& asset) {
    offset = 0;
    asset.chunks.clear();
    // Read ArenaNet file header
    for (int i = 0; i < 4; ++i) {
        if (!read(asset.ffna[i])) {
            return false;
        }
    }
    if (!read(asset.file_type)) {
        return false;
    }
    
    // Parse chunk headers and record their locations
    while (offset < data_size) {
        if (offset + 8 > data_size) break;
        
        size_t chunk_start = offset;
        ChunkType chunk_id;
        uint32_t chunk_size;
        
        if (!read(chunk_id) || !read(chunk_size)) {
            return false;
        }
        
        // Create chunk info
        asset.chunks.push_back((Chunk*)(data + chunk_start));
        
        // Skip to next chunk
        if (!skip(chunk_size)) {
            return false;
        }
    }
    
    return true;
}

// Explicit template instantiations for the types we use
template bool ArenaNetFileParser::read<uint8_t>(uint8_t&);
template bool ArenaNetFileParser::read<uint16_t>(uint16_t&);
template bool ArenaNetFileParser::read<uint32_t>(uint32_t&);
template bool ArenaNetFileParser::read<int32_t>(int32_t&);
template bool ArenaNetFileParser::read<float>(float&);
template bool ArenaNetFileParser::read<char>(char&);
template bool ArenaNetFileParser::read<wchar_t>(wchar_t&);

template bool ArenaNetFileParser::peek<uint8_t>(uint8_t&, size_t) const;
template bool ArenaNetFileParser::peek<uint16_t>(uint16_t&, size_t) const;
template bool ArenaNetFileParser::peek<uint32_t>(uint32_t&, size_t) const;
template bool ArenaNetFileParser::peek<int32_t>(int32_t&, size_t) const;
template bool ArenaNetFileParser::peek<float>(float&, size_t) const;
template bool ArenaNetFileParser::peek<char>(char&, size_t) const;
template bool ArenaNetFileParser::peek<wchar_t>(wchar_t&, size_t) const;
