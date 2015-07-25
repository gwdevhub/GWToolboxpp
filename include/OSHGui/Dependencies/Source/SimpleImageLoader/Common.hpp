#ifndef IMAGELOADER_COMMON_HPP
#define IMAGELOADER_COMMON_HPP

#include <cstdint>
#include <cstdlib>

namespace SimpleImageLoader
{
	inline uint16_t __SwapUInt16(uint16_t arg)
	{
#if defined(_MSC_VER) && _MSC_VER >= 1310 
		return _byteswap_ushort(arg);
#elif defined(__i386__) && defined(__GNUC__) 
		__asm__("xchgb %b0, %h0" : "+q" (arg));
		return arg;
#else 
		uint16_t result;
		result = ((arg << 8) & 0xFF00) | ((arg >> 8) & 0x00FF);
		return result;
#endif 
	}
	//---------------------------------------------------------------------------
	inline uint32_t __SwapUInt32(uint32_t arg)
	{
#if defined(_MSC_VER) && _MSC_VER >= 1310 
		return _byteswap_ulong(arg);
#elif defined(__i386__) && defined(__GNUC__) 
		__asm__("bswap %0" : "+r" (arg));
		return arg;
#else 
		uint32_t result;
		result = ((arg & 0x000000FF) << 24) | ((arg & 0x0000FF00) << 8) | ((arg >> 8) & 0x0000FF00) | ((arg >> 24) & 0x000000FF);
		return result;
#endif 
	}
	//---------------------------------------------------------------------------
	inline void SwapUInt16(uint16_t *p)
	{
		*p = __SwapUInt16(*p);
	}
	//---------------------------------------------------------------------------
	inline void SwapUInt32(uint32_t *p)
	{
		*p = __SwapUInt32(*p);
	}
	//---------------------------------------------------------------------------
	inline uint8_t HighNibble(uint8_t byte)
	{
		return byte & 0xF0;
	}
	//---------------------------------------------------------------------------
	inline uint8_t LowNibble(uint8_t byte)
	{
		return byte & 0x0F;
	}
	//---------------------------------------------------------------------------
	inline uint32_t CalculateLine(uint32_t width, uint32_t bitdepth)
	{
		return (uint32_t)(((uint64_t)width * bitdepth + 7) / 8);
	}
	//---------------------------------------------------------------------------
	inline uint32_t CalculatePitch(uint32_t line)
	{
		return line + 3 & ~3;
	}
	//---------------------------------------------------------------------------
	inline uint32_t CalculateUsedPaletteEntries(uint32_t bit_count)
	{
		if (bit_count >= 1 && bit_count <= 8)
		{
			return 1 << bit_count;
		}

		return 0;
	}
	//---------------------------------------------------------------------------
	inline uint8_t* CalculateScanLine(uint8_t *bits, uint32_t pitch, int32_t scanline)
	{
		return bits + (pitch * scanline);
	}
	//---------------------------------------------------------------------------
}

#endif
