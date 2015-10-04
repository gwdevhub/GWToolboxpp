#ifndef SIMPLEIMAGELOADER_CONVERT_HPP
#define SIMPLEIMAGELOADER_CONVERT_HPP

namespace SimpleImageLoader
{
	class ImageData;

	void ConvertTo32Bits(ImageData &src);
	void SwapRedBlue32(ImageData &src);
}

#endif
