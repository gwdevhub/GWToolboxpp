/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "Label.hpp"
#include "../Misc/Exceptions.hpp"

namespace OSHGui {
	//---------------------------------------------------------------------------
	//Constructor
	//---------------------------------------------------------------------------
	Label::Label(Control* parent) : Control(parent),
		textHelper_(GetFont()) {
		type_ = ControlType::Label;
		clip_ = Clipping::OnParent;
		
		SetAutoSize(true);
		
		ApplyTheme(Application::Instance().GetTheme());

		canRaiseEvents_ = false;
	}
	//---------------------------------------------------------------------------
	//Getter/Setter
	//---------------------------------------------------------------------------
	void Label::SetText(const Misc::UnicodeString &text) {
		textHelper_.SetText(text);
		if (autoSize_) {
			Control::SetSize(textHelper_.GetSize().cast<int>());
		}
	}
	//---------------------------------------------------------------------------
	const Misc::UnicodeString& Label::GetText() const {
		return textHelper_.GetText();
	}
	//---------------------------------------------------------------------------
	void Label::SetFont(const Drawing::FontPtr &font) {
		textHelper_.SetFont(font);
		if (autoSize_) {
			Control::SetSize(textHelper_.GetSize().cast<int>());
		}

		Control::SetFont(font);
	}
	//---------------------------------------------------------------------------
	void Label::PopulateGeometry() {
		using namespace Drawing;

		Graphics g(*geometry_);

		if (GetBackColor().GetAlpha() > 0) {
			g.FillRectangle(GetBackColor(), RectangleI(PointI(), GetSize()));
		}
		
		g.DrawString(textHelper_.GetText(), GetFont(), GetForeColor(), PointI(0, 0));
	}
	//---------------------------------------------------------------------------
}
