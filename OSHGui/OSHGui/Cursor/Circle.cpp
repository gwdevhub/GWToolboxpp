/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "Circle.hpp"
#include "../Drawing/Graphics.hpp"

namespace OSHGui
{
	//---------------------------------------------------------------------------
	//Runtime-Functions
	//---------------------------------------------------------------------------
	void CircleCursor::Initialize()
	{
		using namespace Drawing;

		Graphics g(*geometry_);

		PointF offset(-5, -5);

		g.FillRectangle(Color::Black(), PointF(3, 0) + offset, SizeF(5, 1));
		g.FillRectangle(Color::Black(), PointF(0, 3) + offset, SizeF(1, 5));
		g.FillRectangle(Color::Black(), PointF(3, 10) + offset, SizeF(5, 1));
		g.FillRectangle(Color::Black(), PointF(10, 3) + offset, SizeF(1, 5));
		g.FillRectangle(Color::Black(), PointF(2, 1) + offset, SizeF(7, 1));
		g.FillRectangle(Color::Black(), PointF(1, 2) + offset, SizeF(1, 7));
		g.FillRectangle(Color::Black(), PointF(2, 9) + offset, SizeF(7, 1));
		g.FillRectangle(Color::Black(), PointF(9, 2) + offset, SizeF(1, 7));

		g.FillRectangle(Color::White(), PointF(3, 1) + offset, SizeF(5, 1));
		g.FillRectangle(Color::White(), PointF(1, 3) + offset, SizeF(1, 5));
		g.FillRectangle(Color::White(), PointF(3, 9) + offset, SizeF(5, 1));
		g.FillRectangle(Color::White(), PointF(9, 3) + offset, SizeF(1, 5));
		g.FillRectangle(Color::White(), PointF(2, 2) + offset, SizeF(1, 1));
		g.FillRectangle(Color::White(), PointF(8, 2) + offset, SizeF(1, 1));
		g.FillRectangle(Color::White(), PointF(2, 8) + offset, SizeF(1, 1));
		g.FillRectangle(Color::White(), PointF(8, 8) + offset, SizeF(1, 1));
	}
	//---------------------------------------------------------------------------
}