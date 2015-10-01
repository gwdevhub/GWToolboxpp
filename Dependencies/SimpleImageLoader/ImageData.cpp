#include "SimpleImageLoader.hpp"
#include "Common.hpp"

namespace SimpleImageLoader
{
	ImageData::ImageData()
		: width_(0),
		  height_(0),
		  bitCount_(0),
		  pitch_(0)
	{

	}
	//---------------------------------------------------------------------------
	ImageData::ImageData(uint32_t width, uint32_t height, uint32_t bits, uint32_t redMask, uint32_t greenMask, uint32_t blueMask)
		: width_(width),
		  height_(height),
		  bitCount_(bits),
		  pitch_(CalculatePitch(CalculateLine(width, bits))),
		  redMask_(redMask),
		  greenMask_(greenMask),
		  blueMask_(blueMask),
		  data(height_ * pitch_),
		  palette(CalculateUsedPaletteEntries(bitCount_))
	{

	}
	//---------------------------------------------------------------------------
	uint32_t ImageData::GetWidth() const
	{
		return width_;
	}
	//---------------------------------------------------------------------------
	uint32_t ImageData::GetHeight() const
	{
		return height_;
	}
	//---------------------------------------------------------------------------
	uint32_t ImageData::GetBitCount() const
	{
		return bitCount_;
	}
	//---------------------------------------------------------------------------
	uint32_t ImageData::GetPitch() const
	{
		return pitch_;
	}
	//---------------------------------------------------------------------------
	uint32_t ImageData::GetRedMask() const
	{
		return redMask_;
	}
	//---------------------------------------------------------------------------
	uint32_t ImageData::GetGreenMask() const
	{
		return greenMask_;
	}
	//---------------------------------------------------------------------------
	uint32_t ImageData::GetBlueMask() const
	{
		return blueMask_;
	}
	//---------------------------------------------------------------------------
	uint8_t* ImageData::GetBits()
	{
		return data.data();
	}
	//---------------------------------------------------------------------------
	uint8_t* ImageData::GetScanLine(uint32_t scanline)
	{
		return CalculateScanLine(GetBits(), GetPitch(), scanline);
	}
	//---------------------------------------------------------------------------
	RGBQUAD* ImageData::GetPalette()
	{
		return palette.data();
	}
	//---------------------------------------------------------------------------
}