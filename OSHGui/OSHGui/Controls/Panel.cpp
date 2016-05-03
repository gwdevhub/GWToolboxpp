/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "Panel.hpp"
#include "../Misc/Exceptions.hpp"

namespace OSHGui {
	//---------------------------------------------------------------------------
	//static attributes
	//---------------------------------------------------------------------------
	const Drawing::SizeI Panel::DefaultSize(200, 200);
	//---------------------------------------------------------------------------
	//Constructor
	//---------------------------------------------------------------------------
	Panel::Panel(Control* parent) : Control(parent) {
		type_ = ControlType::Panel;

		SetSize(DefaultSize);
		
		ApplyTheme(Application::Instance().GetTheme());

		isFocusable_ = false;
		canRaiseEvents_ = false;
	}
	bool Panel::Intersect(const Drawing::PointI &point) const {
		if (Control::Intersect(point)) return true;
		for (Control* control : controls_) {
			if (control->Intersect(point)) return true;
		}
		return false;
	}
	//---------------------------------------------------------------------------
	void Panel::ApplyTheme(const Drawing::Theme &theme) {
		Control::ApplyTheme(theme);
		for (Control* control : GetControls()) {
			control->ApplyTheme(theme);
		}
	}
	//---------------------------------------------------------------------------
	//Runtime-Functions
	//---------------------------------------------------------------------------
	void Panel::PopulateGeometry() {
		using namespace Drawing;

		Graphics g(*geometry_);

		if (GetBackColor().GetAlpha() > 0) {
			g.FillRectangle(GetBackColor() - Color::FromARGB(0, 100, 100, 100), 0, 0, GetWidth(), GetHeight());
			auto color = GetBackColor() - Color::FromARGB(0, 90, 90, 90);
			g.FillRectangleGradient(ColorRectangle(GetBackColor(), GetBackColor(), color, color), 0, 0, GetWidth(), GetHeight());
		}
	}
	//---------------------------------------------------------------------------
}
