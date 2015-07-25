/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_VERTEX_HPP
#define OSHGUI_DRAWING_VERTEX_HPP

#include "Vector.hpp"
#include "Color.hpp"

namespace OSHGui
{
	namespace Drawing
	{
		class OSHGUI_EXPORT Vertex
		{
		public:
			Vector Position;
			Drawing::Color Color;
			PointF TextureCoordinates;
		};
	}
}

#endif
