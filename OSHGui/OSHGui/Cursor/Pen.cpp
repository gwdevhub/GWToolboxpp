/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "Pen.hpp"
#include "../Drawing/Graphics.hpp"

namespace OSHGui {
	//---------------------------------------------------------------------------
	//Runtime-Functions
	//---------------------------------------------------------------------------
	void PenCursor::Initialize() {
		using namespace Drawing;

		Graphics g(*geometry_);

		PointI offset(0, -15);

		g.FillRectangle(Color::Black(), PointI(6, 0) + offset, SizeI(3, 1));
		for (int i = 0; i < 5; ++i) {
			g.FillRectangle(Color::Black(), PointI(1 + i, 9 - i * 2) + offset, SizeI(1, 2));
			g.FillRectangle(Color::Black(), PointI(5 + i, 9 - i * 2) + offset, SizeI(1, 2));
			g.FillRectangle(Color::Black(), PointI(0, 11 + i) + offset, SizeI(5 - i, 1));
			g.FillRectangle(Color::White(), PointI(2 + i, 9 - i * 2) + offset, SizeI(3, 2));
		}
		g.FillRectangle(Color::White(), PointI(2, 11) + offset, SizeI(2, 1));
	}
	//---------------------------------------------------------------------------
}