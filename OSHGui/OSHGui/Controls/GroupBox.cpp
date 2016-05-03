/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "GroupBox.hpp"
#include "../Misc/Exceptions.hpp"
#include "Label.hpp"

namespace OSHGui {
	//---------------------------------------------------------------------------
	//Constructor
	//---------------------------------------------------------------------------
	GroupBox::GroupBox(Control* parent) : Panel(parent) {
		type_ = ControlType::GroupBox;

		captionLabel_ = new Label(this);
		captionLabel_->SetLocation(Drawing::PointI(5, -1));
		captionLabel_->SetBackColor(Drawing::Color::Empty());
		controls_.push_back(captionLabel_);

		containerPanel_ = new Panel(this);
		containerPanel_->SetLocation(Drawing::PointI(3, 10));
		containerPanel_->SetBackColor(Drawing::Color::Empty());
		controls_.push_back(containerPanel_);

		ApplyTheme(Application::Instance().GetTheme());

		canRaiseEvents_ = false;
	}
	//---------------------------------------------------------------------------
	//Getter/Setter
	//---------------------------------------------------------------------------
	void GroupBox::SetSize(const Drawing::SizeI &size) {
		Control::SetSize(size);

		containerPanel_->SetSize(size.InflateEx(-3 * 2, -3 * 2 - 10));
	}
	//---------------------------------------------------------------------------
	void GroupBox::SetText(const Misc::UnicodeString &text) {
		captionLabel_->SetText(text);
	}
	//---------------------------------------------------------------------------
	const Misc::UnicodeString& GroupBox::GetText() const {
		return captionLabel_->GetText();
	}
	//---------------------------------------------------------------------------
	void GroupBox::SetFont(const Drawing::FontPtr &font) {
		Control::SetFont(font);

		captionLabel_->SetFont(font);
	}
	//---------------------------------------------------------------------------
	void GroupBox::SetForeColor(const Drawing::Color &color) {
		Control::SetForeColor(color);

		captionLabel_->SetForeColor(color);
	}
	//---------------------------------------------------------------------------
	const std::deque<Control*>& GroupBox::GetControls() const {
		return containerPanel_->GetControls();
	}
	void GroupBox::AddControl(Control *control) {
		containerPanel_->AddControl(control);
	}
	void GroupBox::RemoveControl(Control *control) {
		containerPanel_->RemoveControl(control);
	}
	//---------------------------------------------------------------------------
	//Runtime-Functions
	//---------------------------------------------------------------------------
	void GroupBox::PopulateGeometry() {
		using namespace Drawing;
		
		Graphics g(*geometry_);

		if (GetBackColor().GetAlpha() > 0) {
			g.FillRectangle(GetBackColor(), PointI(0, 0), GetSize());
		}

		g.FillRectangle(GetForeColor(), PointI(1, 5), SizeI(3, 1));
		g.FillRectangle(GetForeColor(), PointI(5 + captionLabel_->GetWidth(), 5), SizeI(GetWidth() - captionLabel_->GetWidth() - 6, 1));
		g.FillRectangle(GetForeColor(), PointI(0, 6), SizeI(1, GetHeight() - 7));
		g.FillRectangle(GetForeColor(), PointI(GetWidth() - 1, 6), SizeI(1, GetHeight() - 7));
		g.FillRectangle(GetForeColor(), PointI(1, GetHeight() - 1), SizeI(GetWidth() - 2, 1));
	}
	//---------------------------------------------------------------------------
}
