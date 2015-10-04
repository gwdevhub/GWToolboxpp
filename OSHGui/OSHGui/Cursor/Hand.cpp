/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "Hand.hpp"
#include "../Drawing/Graphics.hpp"

namespace OSHGui
{
	//---------------------------------------------------------------------------
	//Runtime-Functions
	//---------------------------------------------------------------------------
	void HandCursor::Initialize()
	{
		using namespace Drawing;

		Graphics g(*geometry_);

		PointF offset(-5, 0);

		g.FillRectangle(Color::Black(), PointF(6, 5) + offset, SizeF(7, 9));
		g.FillRectangle(Color::White(), PointF(0, 7) + offset, SizeF(1, 2));
		g.FillRectangle(Color::White(), PointF(1, 6) + offset, SizeF(1, 1));
		for (int i = 0; i < 3; ++i)
		{
			g.FillRectangle(Color::White(), PointF(2 + i, 6 + i) + offset, SizeF(1, 1));
		}
		for (int i = 0; i < 4; ++i)
		{
			g.FillRectangle(Color::White(), PointF(1 + i, 9 + i) + offset, SizeF(1, 1));
			g.FillRectangle(Color::Black(), PointF(1 + i, 7 + i) + offset, SizeF(1, 1));
			g.FillRectangle(Color::Black(), PointF(2 + i, 7 + i) + offset, SizeF(1, 1));
			g.FillRectangle(Color::Black(), PointF(1 + i, 8 + i) + offset, SizeF(1, 1));
			g.FillRectangle(Color::Black(), PointF(3 + i, 11 + i) + offset, SizeF(1, 1));
		}
		g.FillRectangle(Color::White(), PointF(4, 1) + offset, SizeF(1, 7));
		g.FillRectangle(Color::White(), PointF(5, 0) + offset, SizeF(2, 1));
		g.FillRectangle(Color::White(), PointF(5, 15) + offset, SizeF(5, 1));
		g.FillRectangle(Color::White(), PointF(7, 1) + offset, SizeF(1, 6));
		g.FillRectangle(Color::White(), PointF(7, 9) + offset, SizeF(1, 4));
		g.FillRectangle(Color::White(), PointF(9, 9) + offset, SizeF(1, 4));
		g.FillRectangle(Color::White(), PointF(11, 9) + offset, SizeF(1, 4));
		g.FillRectangle(Color::White(), PointF(8, 4) + offset, SizeF(4, 1));
		g.FillRectangle(Color::White(), PointF(10, 5) + offset, SizeF(1, 2));
		g.FillRectangle(Color::White(), PointF(12, 5) + offset, SizeF(1, 2));
		g.FillRectangle(Color::White(), PointF(13, 5) + offset, SizeF(1, 1));
		g.FillRectangle(Color::White(), PointF(14, 6) + offset, SizeF(1, 5));
		g.FillRectangle(Color::White(), PointF(13, 11) + offset, SizeF(1, 2));
		g.FillRectangle(Color::White(), PointF(12, 13) + offset, SizeF(1, 3));
		g.FillRectangle(Color::Black(), PointF(5, 1) + offset, SizeF(2, 13));
		g.FillRectangle(Color::Black(), PointF(7, 14) + offset, SizeF(3, 1));
		g.FillRectangle(Color::Black(), PointF(13, 6) + offset, SizeF(1, 5));
		g.FillRectangle(Color::Black(), PointF(11, 14) + offset, SizeF(1, 1));
		for (int i = 0; i < 2; ++i)
		{
			g.FillRectangle(Color::White(), PointF(10 + i, 14 + i) + offset, SizeF(1, 1));
			g.FillRectangle(Color::White(), PointF(4 + i, 13 + i) + offset, SizeF(1, 1));
		}
	}
	//---------------------------------------------------------------------------
}