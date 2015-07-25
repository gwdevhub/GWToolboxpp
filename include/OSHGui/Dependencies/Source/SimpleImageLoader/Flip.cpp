#include "Flip.hpp"
#include "SimpleImageLoader.hpp"

namespace SimpleImageLoader
{
	void FlipVertical(ImageData &src)
	{
		auto pitch = src.GetPitch();
		auto height = src.GetHeight();

		RawData temp;
		temp.reserve(pitch);

		auto from = src.GetBits();

		auto line1 = 0u;
		auto line2 = (height - 1) * pitch;
		for (auto y = 0u; y < height / 2; ++y)
		{
			std::memcpy(temp.data(), from + line1, pitch);
			std::memcpy(from + line1, from + line2, pitch);
			std::memcpy(from + line2, temp.data(), pitch);

			line1 += pitch;
			line2 -= pitch;
		}
	}
	//---------------------------------------------------------------------------
}
