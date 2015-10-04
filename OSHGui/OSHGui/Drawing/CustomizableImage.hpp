/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_CUSTOMIZABLEIMAGE_HPP
#define OSHGUI_DRAWING_CUSTOMIZABLEIMAGE_HPP

#include "Color.hpp"
#include "ColorRectangle.hpp"
#include "Size.hpp"
#include "Rectangle.hpp"
#include <vector>

namespace OSHGui
{
	namespace Drawing
	{
		class OSHGUI_EXPORT CustomizableImage
		{
		public:
			/**
			 * Constructs an empty image.
			 */
			CustomizableImage(SizeI size);

			const SizeI& GetSize() const;
			std::vector<argb_t> GetRGBAData() const;

			void SetRectangle(RectangleI area, const ColorRectangle &color);

		private:
			SizeI size;
			std::vector<Color> data;
		};
	}
}

#endif
