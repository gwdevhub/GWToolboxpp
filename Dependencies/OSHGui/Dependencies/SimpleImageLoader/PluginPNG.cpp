#include "PluginPNG.hpp"
#include "SimpleImageLoader.hpp"
#include "Common.hpp"
#include "LibPNG/png.h"
#include <algorithm>

namespace SimpleImageLoader
{
	bool PluginPNG::IsValidFormat(const RawData &data)
	{
		if (data.size() < 8)
		{
			return false;
		}

		const uint8_t magic[] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };

		return std::memcmp(data.data(), magic, sizeof(magic)) == 0;
	}
	//---------------------------------------------------------------------------
	void error_handler(png_structp png_ptr, const char *error)
	{
		throw error;
	}
	//---------------------------------------------------------------------------
	void warning_handler(png_structp png_ptr, const char *warning)
	{

	}
	//---------------------------------------------------------------------------
	void _ReadProc(png_structp png_ptr, unsigned char *data, png_size_t size)
	{
		auto stream = static_cast<Stream*>(png_get_io_ptr(png_ptr));

		auto n = stream->Read(data, 1, size);
		if (size > 0 && n == 0)
		{
			throw;
		}
	}
	//---------------------------------------------------------------------------
	ImageData PluginPNG::Load(Stream &data)
	{
		png_structp png = nullptr;
		png_infop info = nullptr;

		try
		{
			png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, error_handler, warning_handler);
			if (png == nullptr)
			{
				throw;
			}

			info = png_create_info_struct(png);
			if (info == nullptr)
			{
				png_destroy_read_struct(&png, nullptr, nullptr);
				png = nullptr;
				throw;
			}

			png_set_read_fn(png, &data, _ReadProc);

			if (setjmp(png_jmpbuf(png)))
			{
				throw;
			}

			png_read_info(png, info);

			png_uint_32 width;
			png_uint_32 height;
			int bitDepth;
			int colorType;
			png_get_IHDR(png, info, &width, &height, &bitDepth, &colorType, nullptr, nullptr, nullptr);

			auto pixelDepth = png_get_bit_depth(png, info) * png_get_channels(png, info);
			if (bitDepth == 16)
			{
				switch (colorType)
				{
					case PNG_COLOR_TYPE_GRAY:
					case PNG_COLOR_TYPE_RGB:
					case PNG_COLOR_TYPE_RGB_ALPHA:
						throw;
					default:
						png_set_strip_16(png);
						bitDepth = 8;
						break;
				}
			}

			switch (colorType)
			{
				case PNG_COLOR_TYPE_RGB:
				case PNG_COLOR_TYPE_RGB_ALPHA:
#if SIMPLEIMAGELOADER_COLORORDER == SIMPLEIMAGELOADER_COLORORDER_BGR
					png_set_bgr(png);
#endif
					break;
				case PNG_COLOR_TYPE_PALETTE:
					if (pixelDepth == 2)
					{
						png_set_packing(png);
						pixelDepth = 8;
					}
					break;
				case PNG_COLOR_TYPE_GRAY:
					if (pixelDepth == 2)
					{
						png_set_expand_gray_1_2_4_to_8(png);
						pixelDepth = 8;
					}
					break;
				case PNG_COLOR_TYPE_GRAY_ALPHA:
					png_set_gray_to_rgb(png);
#if SIMPLEIMAGELOADER_COLORORDER == SIMPLEIMAGELOADER_COLORORDER_BGR
					png_set_bgr(png);
#endif
					pixelDepth = 32;
					break;
				default:
					throw;
			}

			if (png_get_valid(png, info, PNG_INFO_gAMA))
			{
				double gamma;
				if (png_get_gAMA(png, info, &gamma))
				{
					png_set_gamma(png, 2.2, gamma);
				}
			}

			png_read_update_info(png, info);

			colorType = png_get_color_type(png, info);

			ImageData image;

			switch (colorType)
			{
				case PNG_COLOR_TYPE_RGB:
					image = ImageData(width, height, 24, SIL_RGBA_RED_MASK, SIL_RGBA_GREEN_MASK, SIL_RGBA_BLUE_MASK);

					png_set_invert_alpha(png);
					break;
				case PNG_COLOR_TYPE_RGB_ALPHA:
					image = ImageData(width, height, 32, SIL_RGBA_RED_MASK, SIL_RGBA_GREEN_MASK, SIL_RGBA_BLUE_MASK);
					break;
				case PNG_COLOR_TYPE_PALETTE:
					{
						image = ImageData(width, height, pixelDepth, 0, 0, 0);

						png_colorp pngPalette = nullptr;
						int paletteEntries;
						png_get_PLTE(png, info, &pngPalette, &paletteEntries);

						paletteEntries = std::min((uint32_t)paletteEntries, CalculateUsedPaletteEntries(pixelDepth));

						auto palette = image.GetPalette();
						for (auto i = 0; i < paletteEntries; ++i)
						{
							palette[i].rgbRed = pngPalette[i].red;
							palette[i].rgbGreen = pngPalette[i].green;
							palette[i].rgbBlue = pngPalette[i].blue;
						}
					}
					break;
				case PNG_COLOR_TYPE_GRAY:
					{
						image = ImageData(width, height, pixelDepth, 0, 0, 0);

						if (pixelDepth <= 8)
						{
							auto palette = image.GetPalette();
							auto paletteEntries = 1 << pixelDepth;

							for (auto i = 0; i < paletteEntries; ++i)
							{
								palette[i].rgbRed =
								palette[i].rgbGreen =
								palette[i].rgbBlue = (uint8_t)((i * 255) / (paletteEntries - 1));
							}
						}
					}
					break;
				default:
					throw;
			}

			std::vector<png_bytep> rows(height);
			for (png_uint_32 k = 0; k < height; k++)
			{
				rows[height - 1 - k] = image.GetScanLine(k);
			}

			png_set_benign_errors(png, 1);
			png_read_image(png, rows.data());

			png_read_end(png, info);

			if (png)
			{
				png_destroy_read_struct(&png, &info, nullptr);
			}

			return image;
		}
		catch (...)
		{
			if (png)
			{
				png_destroy_read_struct(&png, &info, nullptr);
			}
		}

		throw;
	}
	//---------------------------------------------------------------------------
}
