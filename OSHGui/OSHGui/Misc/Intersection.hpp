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

namespace OSHGui {
	class Intersection {
	public:
		/**
		 * Prüft, ob sich der Punkt test innerhalb des von location und size aufgespannten Rechtecks liegt.
		 *
		 * \param location
		 * \param size
		 * \param test
		 */
		static bool TestRectangleF(const Drawing::PointF &location, 
			const Drawing::SizeF &size, 
			const Drawing::PointF &test) {

			return (test.X >= location.X && test.X < location.X + size.Width)
				&& (test.Y >= location.Y && test.Y < location.Y + size.Height);
		}

		static bool TestRectangleI(const Drawing::PointI &location,
			const Drawing::SizeI &size,
			const Drawing::PointI &test) {

			return (test.X >= location.X && test.X < location.X + size.Width)
				&& (test.Y >= location.Y && test.Y < location.Y + size.Height);
		}
	};
}

#endif
