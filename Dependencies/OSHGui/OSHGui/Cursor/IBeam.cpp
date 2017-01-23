/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "IBeam.hpp"
#include "../Drawing/Graphics.hpp"

namespace OSHGui {
	//---------------------------------------------------------------------------
	//Runtime-Functions
	//---------------------------------------------------------------------------
	void IBeamCursor::Initialize() {
		using namespace Drawing;

		Graphics g(*geometry_);

		PointI offset(-4, -7);

		g.FillRectangle(Color::White(), PointI(0, 0) + offset, SizeI(7, 1));
		g.FillRectangle(Color::White(), PointI(0, 13) + offset, SizeI(7, 1));
		g.FillRectangle(Color::White(), PointI(3, 1) + offset, SizeI(1, 12));
	}
	//---------------------------------------------------------------------------
}
