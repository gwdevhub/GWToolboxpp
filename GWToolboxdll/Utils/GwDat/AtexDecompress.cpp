#include <cstdint>
#include <cstdio>
#include <cstring>

#include "AtexDecompress.h"
#include "AtexAsm.h"

int ImgFmt(unsigned int Format)
{
    if (Format >= 0x17)
    {
        // Bail (not exit()) on a bad format - this runs in the game process and must not kill it.
        return 0;
    }
    return ImageFormats[Format];
}

void AtexDecompress(unsigned int* InputBuffer, unsigned int BufferSize, unsigned int ImageFormat, const SImageDescriptor& ImageDescriptor, unsigned int* OutBuffer)
{
    unsigned int HeaderSize = 12;

    SImageData ImageData;

    int AlphaDataSize2 = ((ImageFormat && 21) - 1) & 2;

    int ColorDataSize = ImgFmt(ImageFormat);
    int AlphaDataSize = ColorDataSize;

    AlphaDataSize &= 640;
    if (AlphaDataSize)
    {
        AlphaDataSize = 2;
    }

    ColorDataSize &= 528;
    if (ColorDataSize)
    {
        ColorDataSize = 2;
    }

    int BlockSize = ColorDataSize + AlphaDataSize2 + AlphaDataSize;

    int BlockCount = ImageDescriptor.xres * ImageDescriptor.yres / 16;

    if (! BlockCount)
    {
        printf("BlockCount zero\n");
        return;
    }

    unsigned int* DcmpBuffer1 = new unsigned int[BlockCount];
    unsigned int* DcmpBuffer2 = DcmpBuffer1 + BlockCount / 2;
    memset(DcmpBuffer1, 0, BlockCount * 4);

    ImageData.xres = ImageDescriptor.xres;
    ImageData.yres = ImageDescriptor.yres;

    unsigned int DataSize = InputBuffer[HeaderSize >> 2];

    if (HeaderSize + 8 >= BufferSize)
    {
        printf("Error 567h\n");
    }
    if (DataSize <= 8)
    {
        printf("Error 569h\n");
    }
    if (DataSize + HeaderSize > BufferSize)
    {
        printf("Error 56Ah\n");
    }

    int CompressionCode = InputBuffer[(HeaderSize + 4) >> 2];

    ImageData.DataPos = InputBuffer + ((HeaderSize + 8) >> 2);

    if (CompressionCode)
    {
        ImageData.currentBits = ImageData.remainingBits = ImageData.nextBits = 0;
        ImageData.EndPos = ImageData.DataPos + ((DataSize - 8) >> 2);

        if (ImageData.DataPos != ImageData.EndPos)
        {
            ImageData.currentBits = ImageData.DataPos[0];
            ImageData.DataPos++;
        }

        if (CompressionCode & 0x10 && ImageData.xres == 256 && ImageData.yres == 256 &&
            (ImageFormat == 0x11 || ImageFormat == 0x10))
        {
            AtexSubCode1_Cpp(DcmpBuffer1, DcmpBuffer2, BlockCount);
        }
        if (CompressionCode & 1 && ColorDataSize && ! AlphaDataSize && ! AlphaDataSize2)
        {
            AtexSubCode2_Cpp(OutBuffer, DcmpBuffer1, DcmpBuffer2, &ImageData, BlockCount, BlockSize);
        }
        if (CompressionCode & 2 && ImageFormat >= 0x10 && ImageFormat <= 0x11)
        {
            AtexSubCode3_Cpp(OutBuffer, DcmpBuffer1, DcmpBuffer2, &ImageData, BlockCount, BlockSize);
        }
        if (CompressionCode & 4 && ImageFormat >= 0x12 && ImageFormat <= 0x15)
        {
            AtexSubCode4_Cpp(OutBuffer, DcmpBuffer1, DcmpBuffer2, &ImageData, BlockCount, BlockSize);
        }
        if (CompressionCode & 8 && ColorDataSize)
        {
            AtexSubCode5_Cpp(OutBuffer + AlphaDataSize2 + AlphaDataSize,
                          DcmpBuffer1, DcmpBuffer2, &ImageData,
                          BlockCount, BlockSize, ImageFormat == 0xf);
        }
        ImageData.DataPos--;
    }

    [[maybe_unused]] unsigned int* DataEnd = InputBuffer + ((HeaderSize + DataSize) >> 2);

    if ((AlphaDataSize || AlphaDataSize2) && BlockCount)
    {
        unsigned int* BufferVar = OutBuffer;

        for (int x = 0; x < BlockCount; x++)
        {
            if (! (DcmpBuffer1[x >> 5] & 1 << x))
            {
                BufferVar[0] = ImageData.DataPos[0];
                BufferVar[1] = ImageData.DataPos[1];
                ImageData.DataPos += 2;
            }
            BufferVar += BlockSize;
        }
    }

    if (ColorDataSize && BlockCount)
    {
        unsigned int* BufferVar = OutBuffer + AlphaDataSize2 + AlphaDataSize;

        for (int x = 0; x < BlockCount; x++)
        {
            if (! (DcmpBuffer2[x >> 5] & 1 << x))
            {
                BufferVar[0] = ImageData.DataPos[0];
                ImageData.DataPos++;
            }
            BufferVar += BlockSize;
        }

        BufferVar = OutBuffer + AlphaDataSize2 + AlphaDataSize + 1;

        for (int x = 0; x < BlockCount; x++)
        {
            if (! (DcmpBuffer2[x >> 5] & 1 << x))
            {
                BufferVar[0] = ImageData.DataPos[0];
                ImageData.DataPos++;
            }
            BufferVar += BlockSize;
        }
    }

    if (CompressionCode & 0x10 && ImageData.xres == 256 && ImageData.yres == 256 &&
        (ImageFormat == 0x10 || ImageFormat == 0x11))
    {
        AtexSubCode7_Cpp(OutBuffer, BlockCount);
    }

    delete[] (unsigned char*)DcmpBuffer1;
}