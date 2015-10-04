/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "Pipette.hpp"
#include "../Drawing/Graphics.hpp"

namespace OSHGui
{
	//---------------------------------------------------------------------------
	//Runtime-Functions
	//---------------------------------------------------------------------------
	void PipetteCursor::Initialize()
	{
		using namespace Drawing;

		Graphics g(*geometry_);

		PointF offset(0, -15);

		g.FillRectangle(Color::Black(), PointF(0, 14) + offset, SizeF(1, 1));
		for (int i = 0; i < 2; ++i)
		{
			g.FillRectangle(Color::Black(), PointF(0 + i, 13 - i) + offset, SizeF(1, 1));
			g.FillRectangle(Color::Black(), PointF(1 + i, 14 - i) + offset, SizeF(1, 1));
		}
		for (int i = 0; i < 6; ++i)
		{
			g.FillRectangle(Color::Black(), PointF(1 + i, 11 - i) + offset, SizeF(1, 1));
			g.FillRectangle(Color::Black(), PointF(3 + i, 13 - i) + offset, SizeF(1, 1));

			g.FillRectangle(Color::White(), PointF(2 + i, 11 - i) + offset, SizeF(1, 1));
			g.FillRectangle(Color::White(), PointF(3 + i, 12 - i) + offset, SizeF(1, 1));
			g.FillRectangle(Color::White(), PointF(2 + i, 12 - i) + offset, SizeF(1, 1));
		}
		g.FillRectangle(Color::Black(), PointF(8, 6) + offset, SizeF(1, 1));
		g.FillRectangle(Color::Black(), PointF(7, 3) + offset, SizeF(6, 3));
		g.FillRectangle(Color::Black(), PointF(9, 2) + offset, SizeF(3, 6));
		g.FillRectangle(Color::Black(), PointF(11, 0) + offset, SizeF(3, 5));
		g.FillRectangle(Color::Black(), PointF(10, 1) + offset, SizeF(5, 3));
	}
	//---------------------------------------------------------------------------
}