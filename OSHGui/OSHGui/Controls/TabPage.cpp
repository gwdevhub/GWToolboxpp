/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "TabPage.hpp"
#include "TabControl.hpp"
#include "../Misc/Exceptions.hpp"

namespace OSHGui {
	//---------------------------------------------------------------------------
	//Constructor
	//---------------------------------------------------------------------------
	TabPage::TabPage(Control* parent) : Panel(parent),
		button_(nullptr) {
		type_ = ControlType::TabPage;
	
		containerPanel_ = new Panel(this);
		containerPanel_->SetLocation(Drawing::PointI(2, 2));
		containerPanel_->SetBackColor(Drawing::Color::Empty());
		Control::AddControl(containerPanel_);
		
		ApplyTheme(Application::Instance().GetTheme());
	}
	//---------------------------------------------------------------------------
	//Getter/Setter
	//---------------------------------------------------------------------------
	void TabPage::SetSize(const Drawing::SizeI &size) {
		Control::SetSize(size);

		containerPanel_->SetSize(size.InflateEx(-4, -4));
	}
	//---------------------------------------------------------------------------
	void TabPage::SetText(const Misc::UnicodeString &text) {
		if (button_) {
			button_->SetText(text);
		}

		text_ = text;

		Invalidate();
	}
	//---------------------------------------------------------------------------
	const Misc::UnicodeString& TabPage::GetText() const {
		return text_;
	}
	//---------------------------------------------------------------------------
	void TabPage::PopulateGeometry() {
		using namespace Drawing;

		Graphics g(*geometry_);

		if (!GetBackColor().IsTranslucent()) {
			g.FillRectangle(GetBackColor() + Color::FromARGB(0, 32, 32, 32), PointI(0, 0), GetSize());
			g.FillRectangleGradient(ColorRectangle(GetBackColor(), 
				GetBackColor() - Color::FromARGB(0, 20, 20, 20)), PointI(1, 1), GetSize() - SizeI(2, 2));
		}
	}
	//---------------------------------------------------------------------------
}
