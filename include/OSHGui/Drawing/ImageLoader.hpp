/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_IMAGELOADER_HPP
#define OSHGUI_DRAWING_IMAGELOADER_HPP

#include "../Misc/RawDataContainer.hpp"
#include "Size.hpp"
#include <vector>

namespace OSHGui
{
	namespace Drawing
	{
		struct OSHGUI_EXPORT ImageData
		{
			std::vector<uint8_t> Data;
			SizeF Size;
		};

		ImageData OSHGUI_EXPORT LoadImageFromContainerToRGBABuffer(Misc::RawDataContainer &data);
		ImageData OSHGUI_EXPORT LoadImageFromFileToRGBABuffer(const Misc::AnsiString &data);
	}
}

#endif