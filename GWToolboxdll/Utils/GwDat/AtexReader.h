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

DatTexture ProcessImageFile(unsigned char* img, int size);
