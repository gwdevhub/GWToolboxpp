#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// ArenaNet File Format (FFNA) Parser
// Used in Guild Wars and Guild Wars 2 for 3D models, textures, and game assets
namespace ArenaNetFileParser {

    void FileIdToFileHash(uint32_t file_id, wchar_t* fileHash);
    uint32_t FileHashToFileId(const wchar_t* fileHash);
    struct Vertex {
        uint32_t FVF;
        uint32_t vertex_size;
        std::vector<float> position; // x, y, z if FVF & 1
        uint32_t group = 0;          // if FVF & 2
        std::vector<float> normal;   // normal_x, normal_y, normal_z if FVF & 4
        std::vector<float> extra_data;
    };

    struct GeometrySubChunk {
        uint32_t sub_model_id;
        uint32_t indices0;
        uint32_t indices1;
        uint32_t indices2;
        uint32_t num_vertices;
        uint32_t FVF;
        uint32_t u0, u1, u2;
        std::vector<uint16_t> indices;
        std::vector<Vertex> vertices;
        std::vector<uint8_t> extra_data;
    };

    struct InteractiveModel {
        uint32_t num_indices;
        uint32_t num_vertices;
        std::vector<uint16_t> indices;
        std::vector<Vertex> vertices;
    };

    struct UnknownTexStruct0 {
        uint8_t using_no_cull;
        uint8_t f0x1;
        uint32_t f0x2;
        uint8_t pixel_shader_id;
        uint8_t num_uv_coords_to_use;
    };

    struct UnknownTexStruct1 {
        uint16_t some_flags0;
        uint16_t some_flags1;
        uint8_t f0x4, f0x5, f0x6, f0x7, f0x8;
    };

    struct TextureAndVertexShader {
        std::vector<UnknownTexStruct0> uts0;
        std::vector<uint16_t> flags0;
        std::vector<uint8_t> tex_array;
        std::vector<uint8_t> zeros;
        std::vector<uint8_t> blend_state;
        std::vector<uint8_t> texture_index_UV_mapping_maybe;
        std::vector<uint8_t> u1;
    };

    struct SomeStruct2 {
        uint8_t unknown[8];
        uint32_t f0x8, f0xC, f0x10, f0x14;
        std::vector<uint8_t> data;
    };

    struct Chunk1Sub1 {
        uint32_t some_type_maybe;
        uint32_t f0x4;
        int32_t f0x8;
        uint32_t f0xC, f0x10;
        uint8_t f0x14, f0x15, f0x16, f0x17;
        uint8_t some_num0; // Num pixel shaders?
        uint8_t f0x19, f0x1a, f0x1b;
        uint8_t some_num1;
        uint8_t f0x1d, f0x1e, f0x1f;
        uint32_t f0x20, f0x24;
        uint32_t magic_num0, magic_num;
        uint8_t num_some_struct;
        uint8_t f0x31[3];
        uint32_t f0x34, f0x38, f0x3C, f0x40;
        uint32_t num_models;
        uint32_t f0x48;
        uint16_t collision_count;
        uint8_t f0x4E[2];
        uint16_t num_some_struct2;
        uint16_t f0x52;
    };

    struct ChunkFA1Sub1 {
        uint32_t some_type_maybe;
        uint32_t f0x4;
        int32_t f0x8;
        uint32_t f0xC, f0x10, f0x14;
        uint8_t some_num0;
        uint8_t f0x19, f0x1a, f0x1b;
        uint8_t some_num1;
        uint8_t f0x1d, f0x1e, f0x1f;
        uint32_t f0x20, f0x24;
        uint32_t magic_num0;
        uint32_t animation_count;
        uint8_t num_some_struct;
        uint8_t f0x31[3];
        uint32_t f0x34, f0x38, f0x3C, f0x40;
        uint32_t num_models;
        uint32_t f0x48;
        uint16_t f0x4C;
        uint8_t f0x4E[2];
        uint16_t num_some_struct2;
        uint16_t f0x52;
    };

    enum ChunkType { 
        GEOMETRY = 0xFA0, 
        ANIMATION = 0xFA1, 
        ATEXFILE = 0xFA3,
        FILENAMES_FA5 = 0xFA5, 
        FILENAMES_FA6 = 0xFA6, 
        FILENAMES_FAD = 0xFAD, 
        FILENAMES_BBC = 0xbbc, 
        FILENAMES_BBD = 0xbbd, 
        SOUND_FILES_1 = 0x1, 
        UNKNOWN 
    };

    // Memory-mapped structures (these map directly to file data)

    #pragma warning(push)
#pragma warning(disable : 4200)
    struct Chunk {
        ChunkType chunk_id;
        uint32_t chunk_size;
        // Data follows...
    };
    struct GeometryChunk : Chunk {
        Chunk1Sub1 sub_1;
        // Data follows...
    };
    struct FileName {
        wchar_t filename[3];
    };
    struct FileNamesChunk : Chunk {
        uint32_t unk;
        uint32_t num_filenames;
        FileName filenames[];
        // FileName array follows...
    };
    struct FileNamesChunkWithoutLength : Chunk {
        FileName filenames[];
        size_t num_filenames() { return chunk_size / sizeof(FileName); };
        // FileName array follows...
    };
    struct ChunkFA1 : Chunk {
        ChunkFA1Sub1 sub_1;
        uint32_t unknown;
        // Data follows...
    };

    struct UnknownChunk : Chunk {
        uint8_t data[];
    };

    #pragma warning(pop)
    struct GameAssetFile {
        std::vector<uint8_t> data; // Reference to the original data
        size_t data_size;          // Size of the data
        GameAssetFile() {
            data.clear();
            data_size = 0;
        }
        GameAssetFile(std::vector<uint8_t>& _data) : GameAssetFile() { parse(_data); }
        char* fileType();

        virtual bool parse(std::vector<uint8_t>& _data);

        virtual const bool isValid() { return fileType() != 0; }
        bool readFromDat(const wchar_t* file_hash);
        bool readFromDat(const uint32_t file_id);
    };

    struct ArenaNetFile : GameAssetFile {

        // Copy parent constructors
        ArenaNetFile() : GameAssetFile() {}
        ArenaNetFile(std::vector<uint8_t>& _data) : ArenaNetFile() { parse(_data); }

        const uint8_t getFFNAType() const;

        const bool isValid() override;
        // Get chunk by type
        const Chunk* FindChunk(ChunkType chunk_type);
    };

    struct ATexFile : GameAssetFile {
        const bool isValid() override { return GameAssetFile::isValid() && strncmp(fileType(), "atex", 4) == 0; }
    };
};
