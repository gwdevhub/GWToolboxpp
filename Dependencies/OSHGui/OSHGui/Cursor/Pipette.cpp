/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "Pipette.hpp"
#include "../Drawing/Graphics.hpp"

namespace OSHGui {
	//---------------------------------------------------------------------------
	//Runtime-Functions
	//---------------------------------------------------------------------------
	void PipetteCursor::Initialize() {
		using namespace Drawing;

		Graphics g(*geometry_);

		PointI offset(0, -15);

		g.FillRectangle(Color::Black(), PointI(0, 14) + offset, SizeI(1, 1));
		for (int i = 0; i < 2; ++i) {
			g.FillRectangle(Color::Black(), PointI(0 + i, 13 - i) + offset, SizeI(1, 1));
			g.FillRectangle(Color::Black(), PointI(1 + i, 14 - i) + offset, SizeI(1, 1));
		}
		for (int i = 0; i < 6; ++i) {
			g.FillRectangle(Color::Black(), PointI(1 + i, 11 - i) + offset, SizeI(1, 1));
			g.FillRectangle(Color::Black(), PointI(3 + i, 13 - i) + offset, SizeI(1, 1));

			g.FillRectangle(Color::White(), PointI(2 + i, 11 - i) + offset, SizeI(1, 1));
			g.FillRectangle(Color::White(), PointI(3 + i, 12 - i) + offset, SizeI(1, 1));
			g.FillRectangle(Color::White(), PointI(2 + i, 12 - i) + offset, SizeI(1, 1));
		}
		g.FillRectangle(Color::Black(), PointI(8, 6) + offset, SizeI(1, 1));
		g.FillRectangle(Color::Black(), PointI(7, 3) + offset, SizeI(6, 3));
		g.FillRectangle(Color::Black(), PointI(9, 2) + offset, SizeI(3, 6));
		g.FillRectangle(Color::Black(), PointI(11, 0) + offset, SizeI(3, 5));
		g.FillRectangle(Color::Black(), PointI(10, 1) + offset, SizeI(5, 3));
	}
	//---------------------------------------------------------------------------
}