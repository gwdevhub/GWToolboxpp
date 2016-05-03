/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "LinkLabel.hpp"

namespace OSHGui {
	//---------------------------------------------------------------------------
	//Constructor
	//---------------------------------------------------------------------------
	LinkLabel::LinkLabel(Control* parent) : Label(parent) {
		type_ = ControlType::LinkLabel;
		
		cursor_ = Cursors::Get(Cursors::Hand);

		canRaiseEvents_ = true;

		ApplyTheme(Application::Instance().GetTheme());
	}
	//---------------------------------------------------------------------------
	//Runtime-Functions
	//---------------------------------------------------------------------------
	bool LinkLabel::Intersect(const Drawing::PointI &point) const {
		return Control::Intersect(point);
	}
	//---------------------------------------------------------------------------
	void LinkLabel::PopulateGeometry() {
		using namespace Drawing;

		Label::PopulateGeometry();

		Graphics g(*geometry_);
		g.FillRectangle(GetForeColor(), RectangleI(PointI(0, GetHeight()), SizeI(GetWidth(), 1)));
	}
	//---------------------------------------------------------------------------
}
