/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "Circle.hpp"
#include "../Drawing/Graphics.hpp"

namespace OSHGui {
	//---------------------------------------------------------------------------
	//Runtime-Functions
	//---------------------------------------------------------------------------
	void CircleCursor::Initialize() {
		using namespace Drawing;

		Graphics g(*geometry_);

		PointF offset(-5, -5);

		g.FillRectangle(Color::Black(), PointI(3, 0) + offset, SizeI(5, 1));
		g.FillRectangle(Color::Black(), PointI(0, 3) + offset, SizeI(1, 5));
		g.FillRectangle(Color::Black(), PointI(3, 10) + offset, SizeI(5, 1));
		g.FillRectangle(Color::Black(), PointI(10, 3) + offset, SizeI(1, 5));
		g.FillRectangle(Color::Black(), PointI(2, 1) + offset, SizeI(7, 1));
		g.FillRectangle(Color::Black(), PointI(1, 2) + offset, SizeI(1, 7));
		g.FillRectangle(Color::Black(), PointI(2, 9) + offset, SizeI(7, 1));
		g.FillRectangle(Color::Black(), PointI(9, 2) + offset, SizeI(1, 7));

		g.FillRectangle(Color::White(), PointI(3, 1) + offset, SizeI(5, 1));
		g.FillRectangle(Color::White(), PointI(1, 3) + offset, SizeI(1, 5));
		g.FillRectangle(Color::White(), PointI(3, 9) + offset, SizeI(5, 1));
		g.FillRectangle(Color::White(), PointI(9, 3) + offset, SizeI(1, 5));
		g.FillRectangle(Color::White(), PointI(2, 2) + offset, SizeI(1, 1));
		g.FillRectangle(Color::White(), PointI(8, 2) + offset, SizeI(1, 1));
		g.FillRectangle(Color::White(), PointI(2, 8) + offset, SizeI(1, 1));
		g.FillRectangle(Color::White(), PointI(8, 8) + offset, SizeI(1, 1));
	}
	//---------------------------------------------------------------------------
}