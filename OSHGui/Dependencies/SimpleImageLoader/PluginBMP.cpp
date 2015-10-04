#include "PluginBMP.hpp"
#include "SimpleImageLoader.hpp"
#include "Common.hpp"

namespace SimpleImageLoader
{
	const uint8_t BI_BITFIELDS = 3;
	const uint8_t BI_RGB = 0;

#ifdef _WIN32
#pragma pack(push, 1)
#else
#pragma pack(1)
#endif

	struct BITMAPCOREHEADER
	{
		uint32_t bcSize;
		uint16_t bcWidth;
		uint16_t bcHeight;
		uint16_t bcPlanes;
		uint16_t bcBitCnt;
	};

	struct BITMAPFILEHEADER
	{
		uint16_t bfType;
		uint32_t bfSize;
		uint16_t bfReserved1;
		uint16_t bfReserved2;
		uint32_t bfOffBits;
	};

	struct BITMAPINFOHEADER
	{
		uint32_t biSize;
		int32_t  biWidth;
		int32_t  biHeight;
		uint16_t biPlanes;
		uint16_t biBitCount;
		uint32_t biCompression;
		uint32_t biSizeImage;
		int32_t  biXPelsPerMeter;
		int32_t  biYPelsPerMeter;
		uint32_t biClrUsed;
		uint32_t biClrImportant;
	};

#ifdef _WIN32
#pragma pack(pop)
#else
#pragma pack()
#endif

#ifdef SIMPLEIMAGELOADER_BIGENDIAN

	void SwapInfoHeader(BITMAPINFOHEADER *header)
	{
		SwapUInt32(&header->biSize);
		SwapUInt32((DWORD *)&header->biWidth);
		SwapUInt32((DWORD *)&header->biHeight);
		SwapUInt16(&header->biPlanes);
		SwapUInt16(&header->biBitCount);
		SwapUInt32(&header->biCompression);
		SwapUInt32(&header->biSizeImage);
		SwapUInt32((DWORD *)&header->biXPelsPerMeter);
		SwapUInt32((DWORD *)&header->biYPelsPerMeter);
		SwapUInt32(&header->biClrUsed);
		SwapUInt32(&header->biClrImportant);
	}
	//---------------------------------------------------------------------------
	void SwapCoreHeader(BITMAPCOREHEADER *header)
	{
		SwapUInt32(&header->bcSize);
		SwapUInt16(&header->bcWidth);
		SwapUInt16(&header->bcHeight);
		SwapUInt16(&header->bcPlanes);
		SwapUInt16(&header->bcBitCnt);
	}
	//---------------------------------------------------------------------------
	void SwapFileHeader(BITMAPFILEHEADER *header)
	{
		SwapUInt16(&header->bfType);
		SwapUInt32(&header->bfSize);
		SwapUInt16(&header->bfReserved1);
		SwapUInt16(&header->bfReserved2);
		SwapUInt32(&header->bfOffBits);
	}
	//---------------------------------------------------------------------------
#endif
	//---------------------------------------------------------------------------
	bool PluginBMP::IsValidFormat(const RawData &data)
	{
		if (data.size() < 2)
		{
			return false;
		}

		const uint8_t magic[] = { 0x42, 0x4D };

		return std::memcmp(data.data(), magic, sizeof(magic)) == 0;
	}
	//---------------------------------------------------------------------------
	void LoadPixels(Stream &data, bool isUpSideDown, ImageData &image)
	{
		if (isUpSideDown)
		{
			data.Read(image.GetBits(), image.GetHeight() * image.GetPitch());
		}
		else
		{
			for (auto c = 0u; c < image.GetHeight(); ++c)
			{
				data.Read(image.GetScanLine(image.GetHeight() - c - 1), image.GetPitch());
			}
		}

#ifdef SIMPLEIMAGELOADER_BIGENDIAN
		if (image.GetBitCount() == 16)
		{
			for (auto y = 0u; y < image.GetHeight(); ++y)
			{
				auto pixel = reinterpret_cast<uint16_t*>(image.GetScanLine(y));
				for (auto x = 0u; x < image.GetWidth(); ++x)
				{
					SwapUInt16(pixel);
					pixel++;
				}
			}
		}
#endif
#if SIMPLEIMAGELOADER_COLORORDER == SIMPLEIMAGELOADER_COLORORDER_RGB
		if (image.GetBitCount() == 24 || image.GetBitCount() == 32)
		{
			for (auto y = 0u; y < image.GetHeight(); ++y)
			{
				auto pixel = image.GetScanLine(y);
				for (auto x = 0u; x < image.GetWidth(); ++x)
				{
					std::swap(pixel[0], pixel[2]);
					pixel += (image.GetBitCount() >> 3);
				}
			}
		}
#endif
	}
	//---------------------------------------------------------------------------
	ImageData PluginBMP::Load(Stream &data)
	{
		BITMAPFILEHEADER fileHeader;
		data.Read(&fileHeader, 1);
#ifdef SIMPLEIMAGELOADER_BIGENDIAN
		SwapFileHeader(&fileHeader);
#endif

		if (fileHeader.bfType != 0x4D42)
		{
			//no valid magic number
			throw;
		}

		BITMAPINFOHEADER infoHeader;
		data.Read(&infoHeader, 1);
#ifdef SIMPLEIMAGELOADER_BIGENDIAN
		SwapInfoHeader(&infoHeader);
#endif

		if (infoHeader.biSize != 40)
		{
			//not Windows format
			throw;
		}

		ImageData image;

		switch (infoHeader.biBitCount)
		{
			case 1:
			case 4:
			case 8:
			{
				auto usedColors = infoHeader.biClrUsed;
				if (usedColors == 0 || usedColors > CalculateUsedPaletteEntries(infoHeader.biBitCount))
				{
					usedColors = CalculateUsedPaletteEntries(infoHeader.biBitCount);
				}

				image = ImageData(infoHeader.biWidth, std::abs(infoHeader.biHeight), infoHeader.biBitCount, 0, 0, 0);

				data.Read(image.GetPalette(), usedColors * sizeof(RGBQUAD), 1);

#if SIMPLEIMAGELOADER_COLORORDER == SIMPLEIMAGELOADER_COLORORDER_RGB
				auto palette = image.GetPalette();
				for (auto i = 0u; i < usedColors; ++i)
				{
					std::swap(palette[i].rgbRed, palette[i].rgbBlue);
				}
#endif

				if (fileHeader.bfOffBits > sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + (usedColors * sizeof(RGBQUAD)))
				{
					data.Seek(fileHeader.bfOffBits, Stream::SeekPosition::Begin);
				}

				switch (infoHeader.biCompression)
				{
					case BI_RGB:
						LoadPixels(data, infoHeader.biHeight > 0, image);
						break;
					default:
						throw;
				}
			}
			break;
		case 16:
			{
				if (infoHeader.biCompression == BI_BITFIELDS)
				{
					uint32_t bitfields[3];
					data.Read(bitfields, sizeof(uint32_t), 3);

					image = ImageData(infoHeader.biWidth, std::abs(infoHeader.biHeight), infoHeader.biBitCount, bitfields[0], bitfields[1], bitfields[2]);
				}
				else
				{
					image = ImageData(infoHeader.biWidth, std::abs(infoHeader.biHeight), infoHeader.biBitCount, SIL16_555_RED_MASK, SIL16_555_GREEN_MASK, SIL16_555_BLUE_MASK);
				}

				if (fileHeader.bfOffBits > sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER))
				{
					data.Seek(fileHeader.bfOffBits, Stream::SeekPosition::Begin);
				}

				LoadPixels(data, infoHeader.biHeight > 0, image);
			}
			break;
		case 24:
		case 32:
			{
				if (infoHeader.biCompression == BI_BITFIELDS)
				{
					uint32_t bitfields[3];
					data.Read(bitfields, sizeof(uint32_t), 3);

					image = ImageData(infoHeader.biWidth, std::abs(infoHeader.biHeight), infoHeader.biBitCount, bitfields[0], bitfields[1], bitfields[2]);
				}
				else
				{
					image = ImageData(infoHeader.biWidth, std::abs(infoHeader.biHeight), infoHeader.biBitCount, SIL_RGBA_RED_MASK, SIL_RGBA_GREEN_MASK, SIL_RGBA_BLUE_MASK);
				}

				if (infoHeader.biClrUsed > 0)
				{
					data.Seek(infoHeader.biClrUsed * sizeof(uint32_t), Stream::SeekPosition::Current);
				}
				else if (infoHeader.biCompression != BI_BITFIELDS && fileHeader.bfOffBits > sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER))
				{
					data.Seek(fileHeader.bfOffBits, Stream::SeekPosition::Begin);
				}

				LoadPixels(data, infoHeader.biHeight > 0, image);
			}
			break;
		}

		return image;
	}
	//---------------------------------------------------------------------------
}
