#ifndef SIMPLEIMAGELOADER_HPP
#define SIMPLEIMAGELOADER_HPP

#include <vector>
#include <cstdint>

namespace SimpleImageLoader
{

#if (defined(BYTE_ORDER) && BYTE_ORDER==BIG_ENDIAN) || (defined(__BYTE_ORDER) && __BYTE_ORDER==__BIG_ENDIAN) || defined(__BIG_ENDIAN__)
#define SIMPLEIMAGELOADER_BIGENDIAN
#endif

#define SIMPLEIMAGELOADER_COLORORDER_BGR 0
#define SIMPLEIMAGELOADER_COLORORDER_RGB 1
#if defined(SIMPLEIMAGELOADER_BIGENDIAN)
#define SIMPLEIMAGELOADER_COLORORDER SIMPLEIMAGELOADER_COLORORDER_RGB
#else
#define SIMPLEIMAGELOADER_COLORORDER SIMPLEIMAGELOADER_COLORORDER_BGR
#endif

#ifndef SIMPLEIMAGELOADER_BIGENDIAN
#if SIMPLEIMAGELOADER_COLORORDER == SIMPLEIMAGELOADER_COLORORDER_BGR
	// Little Endian (x86 / MS Windows, Linux) : BGR(A) order
#define SIL_RGBA_RED			2
#define SIL_RGBA_GREEN			1
#define SIL_RGBA_BLUE			0
#define SIL_RGBA_ALPHA			3
#define SIL_RGBA_RED_MASK		0x00FF0000
#define SIL_RGBA_GREEN_MASK		0x0000FF00
#define SIL_RGBA_BLUE_MASK		0x000000FF
#define SIL_RGBA_ALPHA_MASK		0xFF000000
#define SIL_RGBA_RED_SHIFT		16
#define SIL_RGBA_GREEN_SHIFT	8
#define SIL_RGBA_BLUE_SHIFT		0
#define SIL_RGBA_ALPHA_SHIFT	24
#else
	// Little Endian (x86 / MaxOSX) : RGB(A) order
#define SIL_RGBA_RED			0
#define SIL_RGBA_GREEN			1
#define SIL_RGBA_BLUE			2
#define SIL_RGBA_ALPHA			3
#define SIL_RGBA_RED_MASK		0x000000FF
#define SIL_RGBA_GREEN_MASK		0x0000FF00
#define SIL_RGBA_BLUE_MASK		0x00FF0000
#define SIL_RGBA_ALPHA_MASK		0xFF000000
#define SIL_RGBA_RED_SHIFT		0
#define SIL_RGBA_GREEN_SHIFT	8
#define SIL_RGBA_BLUE_SHIFT		16
#define SIL_RGBA_ALPHA_SHIFT	24
#endif
#else
#if SIMPLEIMAGELOADER_COLORORDER == SIMPLEIMAGELOADER_COLORORDER_BGR
	// Big Endian (PPC / none) : BGR(A) order
#define SIL_RGBA_RED			2
#define SIL_RGBA_GREEN			1
#define SIL_RGBA_BLUE			0
#define SIL_RGBA_ALPHA			3
#define SIL_RGBA_RED_MASK		0x0000FF00
#define SIL_RGBA_GREEN_MASK		0x00FF0000
#define SIL_RGBA_BLUE_MASK		0xFF000000
#define SIL_RGBA_ALPHA_MASK		0x000000FF
#define SIL_RGBA_RED_SHIFT		8
#define SIL_RGBA_GREEN_SHIFT	16
#define SIL_RGBA_BLUE_SHIFT		24
#define SIL_RGBA_ALPHA_SHIFT	0
#else
	// Big Endian (PPC / Linux, MaxOSX) : RGB(A) order
#define SIL_RGBA_RED			0
#define SIL_RGBA_GREEN			1
#define SIL_RGBA_BLUE			2
#define SIL_RGBA_ALPHA			3
#define SIL_RGBA_RED_MASK		0xFF000000
#define SIL_RGBA_GREEN_MASK		0x00FF0000
#define SIL_RGBA_BLUE_MASK		0x0000FF00
#define SIL_RGBA_ALPHA_MASK		0x000000FF
#define SIL_RGBA_RED_SHIFT		24
#define SIL_RGBA_GREEN_SHIFT	16
#define SIL_RGBA_BLUE_SHIFT		8
#define SIL_RGBA_ALPHA_SHIFT	0
#endif
#endif

#define SIL16_555_RED_MASK		0x7C00
#define SIL16_555_GREEN_MASK	0x03E0
#define SIL16_555_BLUE_MASK		0x001F
#define SIL16_555_RED_SHIFT		10
#define SIL16_555_GREEN_SHIFT	5
#define SIL16_555_BLUE_SHIFT	0
#define SIL16_565_RED_MASK		0xF800
#define SIL16_565_GREEN_MASK	0x07E0
#define SIL16_565_BLUE_MASK		0x001F
#define IL16_565_RED_SHIFT		11
#define IL16_565_GREEN_SHIFT	5
#define IL16_565_BLUE_SHIFT		0

#if (defined(_WIN32) || defined(__WIN32__))
#pragma pack(push, 1)
#else
#pragma pack(1)
#endif

	struct RGBQUAD
	{
#if SIMPLEIMAGELOADER_COLORORDER == SIMPLEIMAGELOADER_COLORORDER_BGR
		uint8_t rgbBlue;
		uint8_t rgbGreen;
		uint8_t rgbRed;
#else
		uint8_t rgbRed;
		uint8_t rgbGreen;
		uint8_t rgbBlue;
#endif
		uint8_t rgbReserved;
	};

	struct RGBTRIPLE
	{
#if SIMPLEIMAGELOADER_COLORORDER == SIMPLEIMAGELOADER_COLORORDER_BGR
		uint8_t rgbtBlue;
		uint8_t rgbtGreen;
		uint8_t rgbtRed;
#else
		uint8_t rgbtRed;
		uint8_t rgbtGreen;
		uint8_t rgbtBlue;
#endif
	};

#if (defined(_WIN32) || defined(__WIN32__))
#pragma pack(pop)
#else
#pragma pack()
#endif

	//using RawData = std::vector<uint8_t>;
	typedef std::vector<uint8_t> RawData;

	class ImageData
	{
	public:
		ImageData();
		ImageData(uint32_t width, uint32_t height, uint32_t bitCount, uint32_t redMask, uint32_t greenMask, uint32_t blueMask);

		uint32_t GetWidth() const;
		uint32_t GetHeight() const;
		uint32_t GetBitCount() const;
		uint32_t GetPitch() const;

		uint32_t GetRedMask() const;
		uint32_t GetGreenMask() const;
		uint32_t GetBlueMask() const;

		uint8_t* GetBits();
		uint8_t* GetScanLine(uint32_t scanline);

		RawData& GetData()
		{
			return data;
		}

		RGBQUAD* GetPalette();

	private:
		uint32_t width_;
		uint32_t height_;
		uint32_t bitCount_;
		uint32_t pitch_;
		uint32_t redMask_;
		uint32_t greenMask_;
		uint32_t blueMask_;
		RawData data;
		std::vector<RGBQUAD> palette;
	};

	ImageData LoadImage(const RawData &data);
}

#endif
