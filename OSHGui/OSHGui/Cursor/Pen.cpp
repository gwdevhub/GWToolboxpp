/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "Pen.hpp"
#include "../Drawing/Graphics.hpp"

namespace OSHGui
{
	//---------------------------------------------------------------------------
	//Runtime-Functions
	//---------------------------------------------------------------------------
	void PenCursor::Initialize()
	{
		using namespace Drawing;

		Graphics g(*geometry_);

		PointF offset(0, -15);

		g.FillRectangle(Color::Black(), PointF(6, 0) + offset, SizeF(3, 1));
		for (int i = 0; i < 5; ++i)
		{
			g.FillRectangle(Color::Black(), PointF(1 + i, 9 - i * 2) + offset, SizeF(1, 2));
			g.FillRectangle(Color::Black(), PointF(5 + i, 9 - i * 2) + offset, SizeF(1, 2));
			g.FillRectangle(Color::Black(), PointF(0, 11 + i) + offset, SizeF(5 - i, 1));
			g.FillRectangle(Color::White(), PointF(2 + i, 9 - i * 2) + offset, SizeF(3, 2));
		}
		g.FillRectangle(Color::White(), PointF(2, 11) + offset, SizeF(2, 1));
	}
	//---------------------------------------------------------------------------
}