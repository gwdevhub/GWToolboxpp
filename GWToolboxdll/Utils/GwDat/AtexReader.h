#pragma once

#include <cstdint>
#include <vector>

union RGBA
{
    unsigned char c[4];
    struct
    {
        unsigned char r, g, b, a;
    };
    unsigned int dw;
};

enum TextureType
{
    Placeholder0,
	BC1,
    NormalMap,
    BC3,
    Placeholder2,
    BC5,
    DDSt
};

struct DatTexture
{
    int width;
    int height;
    std::vector<RGBA> rgba_data;
    TextureType texture_type;
};

struct DatTextureRaw
{
    int width = 0;
    int height = 0;
    char cmptype = 0;
    std::vector<uint8_t> blocks;
};

DatTexture ProcessImageFile(unsigned char* img, int size);
DatTextureRaw ProcessImageFileRaw(unsigned char* img, int size);
