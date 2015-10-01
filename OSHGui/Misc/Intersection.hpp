/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_INTERSECTION_HPP
#define OSHGUI_INTERSECTION_HPP

#include "../Drawing/Point.hpp"
#include "../Drawing/Size.hpp"

namespace OSHGui
{
	class Intersection
	{
	public:
		/**
		 * Prüft, ob sich der Punkt test innerhalb des von location und size aufgespannten Rechtecks liegt.
		 *
		 * \param location
		 * \param size
		 * \param test
		 */
		static bool TestRectangle(const Drawing::PointF &location, const Drawing::SizeF &size, const Drawing::PointF &test)
		{
			return (test.X >= location.X && test.X < location.X + size.Width)
				&& (test.Y >= location.Y && test.Y < location.Y + size.Height);
		}
	};
}

#endif