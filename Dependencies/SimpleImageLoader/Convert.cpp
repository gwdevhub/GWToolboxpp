#include "Convert.hpp"
#include "SimpleImageLoader.hpp"
#include "Common.hpp"

namespace SimpleImageLoader
{
	void ConvertLine1To32(uint8_t *target, uint8_t *source, int width, RGBQUAD *palette)
	{
		for (auto cols = 0; cols < width; cols++)
		{
			auto index = (source[cols >> 3] & (0x80 >> (cols & 0x07))) != 0 ? 1 : 0;

			target[SIL_RGBA_BLUE] = palette[index].rgbBlue;
			target[SIL_RGBA_GREEN] = palette[index].rgbGreen;
			target[SIL_RGBA_RED] = palette[index].rgbRed;
			target[SIL_RGBA_ALPHA] = 0xFF;
			target += 4;
		}
	}
	//---------------------------------------------------------------------------
	void ConvertLine4To32(uint8_t *target, uint8_t *source, int width, RGBQUAD *palette)
	{
		auto lowNibble = false;
		auto x = 0;

		for (auto cols = 0; cols < width; ++cols)
		{
			if (lowNibble)
			{
				target[SIL_RGBA_BLUE] = palette[LowNibble(source[x])].rgbBlue;
				target[SIL_RGBA_GREEN] = palette[LowNibble(source[x])].rgbGreen;
				target[SIL_RGBA_RED] = palette[LowNibble(source[x])].rgbRed;

				x++;
			}
			else
			{
				target[SIL_RGBA_BLUE] = palette[HighNibble(source[x]) >> 4].rgbBlue;
				target[SIL_RGBA_GREEN] = palette[HighNibble(source[x]) >> 4].rgbGreen;
				target[SIL_RGBA_RED] = palette[HighNibble(source[x]) >> 4].rgbRed;
			}

			lowNibble = !lowNibble;

			target[SIL_RGBA_ALPHA] = 0xFF;
			target += 4;
		}
	}
	//---------------------------------------------------------------------------
	void ConvertLine8To32(uint8_t *target, uint8_t *source, int width, RGBQUAD *palette)
	{
		for (int cols = 0; cols < width; cols++)
		{
			target[SIL_RGBA_BLUE] = palette[source[cols]].rgbBlue;
			target[SIL_RGBA_GREEN] = palette[source[cols]].rgbGreen;
			target[SIL_RGBA_RED] = palette[source[cols]].rgbRed;
			target[SIL_RGBA_ALPHA] = 0xFF;
			target += 4;
		}
	}
	//---------------------------------------------------------------------------
	void ConvertLine16To32_555(uint8_t *target, uint8_t *source, int width)
	{
		auto bits = reinterpret_cast<uint16_t*>(source);

		for (auto cols = 0; cols < width; ++cols)
		{
			target[SIL_RGBA_RED] = (uint8_t)((((bits[cols] & SIL16_555_RED_MASK) >> SIL16_555_RED_SHIFT) * 0xFF) / 0x1F);
			target[SIL_RGBA_GREEN] = (uint8_t)((((bits[cols] & SIL16_555_GREEN_MASK) >> SIL16_555_GREEN_SHIFT) * 0xFF) / 0x1F);
			target[SIL_RGBA_BLUE] = (uint8_t)((((bits[cols] & SIL16_555_BLUE_MASK) >> SIL16_555_BLUE_SHIFT) * 0xFF) / 0x1F);
			target[SIL_RGBA_ALPHA] = 0xFF;
			target += 4;
		}
	}
	//---------------------------------------------------------------------------
	void ConvertLine16To32_565(uint8_t *target, uint8_t *source, int width)
	{
		auto bits = reinterpret_cast<uint16_t*>(source);

		for (auto cols = 0; cols < width; ++cols)
		{
			target[SIL_RGBA_RED] = (uint8_t)((((bits[cols] & SIL16_565_RED_MASK) >> IL16_565_RED_SHIFT) * 0xFF) / 0x1F);
			target[SIL_RGBA_GREEN] = (uint8_t)((((bits[cols] & SIL16_565_GREEN_MASK) >> IL16_565_GREEN_SHIFT) * 0xFF) / 0x3F);
			target[SIL_RGBA_BLUE] = (uint8_t)((((bits[cols] & SIL16_565_BLUE_MASK) >> IL16_565_BLUE_SHIFT) * 0xFF) / 0x1F);
			target[SIL_RGBA_ALPHA] = 0xFF;
			target += 4;
		}
	}
	//---------------------------------------------------------------------------
	void ConvertLine24To32(uint8_t *target, uint8_t *source, int width)
	{
		for (int cols = 0; cols < width; ++cols)
		{
			target[SIL_RGBA_RED] = source[SIL_RGBA_RED];
			target[SIL_RGBA_GREEN] = source[SIL_RGBA_GREEN];
			target[SIL_RGBA_BLUE] = source[SIL_RGBA_BLUE];
			target[SIL_RGBA_ALPHA] = 0xFF;
			target += 4;
			source += 3;
		}
	}
	//---------------------------------------------------------------------------
	void ConvertTo32Bits(ImageData &src)
	{
		if (src.GetBitCount() == 32)
		{
			return;
		}

		auto width = src.GetWidth();
		auto height = src.GetHeight();

		ImageData temp(width, height, 32, SIL_RGBA_RED_MASK, SIL_RGBA_GREEN_MASK, SIL_RGBA_BLUE_MASK);

		switch (src.GetBitCount())
		{
			case 1:
				{
					for (auto rows = 0u; rows < height; ++rows)
					{
						ConvertLine1To32(temp.GetScanLine(rows), src.GetScanLine(rows), width, src.GetPalette());
					}
				}
				break;
			case 4:
				{
					for (auto rows = 0u; rows < height; ++rows)
					{
						ConvertLine4To32(temp.GetScanLine(rows), src.GetScanLine(rows), width, src.GetPalette());
					}
				}
				break;
			case 8:
				{
					for (auto rows = 0u; rows < height; ++rows)
					{
						ConvertLine8To32(temp.GetScanLine(rows), src.GetScanLine(rows), width, src.GetPalette());
					}
				}
				break;
			case 16:
				{
					auto convert = src.GetRedMask() == SIL16_565_RED_MASK && src.GetGreenMask() == SIL16_565_GREEN_MASK && src.GetBlueMask() == SIL16_565_BLUE_MASK
								 ? ConvertLine16To32_565
								 : ConvertLine16To32_555;

					for (auto rows = 0u; rows < height; ++rows)
					{
						convert(temp.GetScanLine(rows), src.GetScanLine(rows), width);
					}
				}
				break;
			case 24:
				{
					for (auto rows = 0u; rows < height; ++rows)
					{
						ConvertLine24To32(temp.GetScanLine(rows), src.GetScanLine(rows), width);
					}
				}
				break;
			default:
				throw;
		}

		src = temp;
	}
	//---------------------------------------------------------------------------
	void SwapRedBlue32(ImageData &src)
	{
		auto bytesPerPixel = src.GetBitCount() / 8;

		if (bytesPerPixel > 4 || bytesPerPixel < 3)
		{
			throw;
		}

		auto height = src.GetHeight();
		auto pitch = src.GetPitch();
		auto lineSize = ((src.GetWidth() * src.GetBitCount()) + 7) / 8;

		auto line = src.GetBits();
		for (auto y = 0u; y < height; ++y, line += pitch)
		{
			for (auto pixel = line; pixel < line + lineSize; pixel += bytesPerPixel)
			{
				std::swap(pixel[0], pixel[2]);
			}
		}
	}
	//---------------------------------------------------------------------------
}
