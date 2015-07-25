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

namespace OSHGui
{
	//---------------------------------------------------------------------------
	//Constructor
	//---------------------------------------------------------------------------
	GroupBox::GroupBox()
	{
		type_ = ControlType::GroupBox;

		captionLabel_ = new Label();
		captionLabel_->SetLocation(Drawing::PointI(5, -1));
		captionLabel_->SetBackColor(Drawing::Color::Empty());
		AddSubControl(captionLabel_);

		containerPanel_ = new Panel();
		containerPanel_->SetLocation(Drawing::PointI(3, 10));
		containerPanel_->SetBackColor(Drawing::Color::Empty());
		AddSubControl(containerPanel_);

		ApplyTheme(Application::Instance().GetTheme());

		canRaiseEvents_ = false;
	}
	//---------------------------------------------------------------------------
	//Getter/Setter
	//---------------------------------------------------------------------------
	void GroupBox::SetSize(const Drawing::SizeI &size)
	{
		Control::SetSize(size);

		containerPanel_->SetSize(size.InflateEx(-3 * 2, -3 * 2 - 10));
	}
	//---------------------------------------------------------------------------
	void GroupBox::SetText(const Misc::AnsiString &text)
	{
		captionLabel_->SetText(text);
	}
	//---------------------------------------------------------------------------
	const Misc::AnsiString& GroupBox::GetText() const
	{
		return captionLabel_->GetText();
	}
	//---------------------------------------------------------------------------
	void GroupBox::SetFont(const Drawing::FontPtr &font)
	{
		Control::SetFont(font);

		captionLabel_->SetFont(font);
	}
	//---------------------------------------------------------------------------
	void GroupBox::SetForeColor(const Drawing::Color &color)
	{
		Control::SetForeColor(color);

		captionLabel_->SetForeColor(color);
	}
	//---------------------------------------------------------------------------
	const std::deque<Control*>& GroupBox::GetControls() const
	{
		return containerPanel_->GetControls();
	}
	//---------------------------------------------------------------------------
	//Runtime-Functions
	//---------------------------------------------------------------------------
	void GroupBox::AddControl(Control *control)
	{
		containerPanel_->AddControl(control);
	}
	//---------------------------------------------------------------------------
	void GroupBox::RemoveControl(Control *control)
	{
		containerPanel_->RemoveControl(control);
	}
	//---------------------------------------------------------------------------
	void GroupBox::DrawSelf(Drawing::RenderContext &context)
	{
		Control::DrawSelf(context);

		captionLabel_->Render();
		containerPanel_->Render();
	}
	//---------------------------------------------------------------------------
	void GroupBox::PopulateGeometry()
	{
		using namespace Drawing;
		
		Graphics g(*geometry_);

		if (GetBackColor().GetAlpha() > 0)
		{
			g.FillRectangle(GetBackColor(), PointF(0, 0), GetSize());
		}

		g.FillRectangle(GetForeColor(), PointF(1, 5), SizeF(3, 1));
		g.FillRectangle(GetForeColor(), PointF(5 + captionLabel_->GetWidth(), 5), SizeF(GetWidth() - captionLabel_->GetWidth() - 6, 1));
		g.FillRectangle(GetForeColor(), PointF(0, 6), SizeF(1, GetHeight() - 7));
		g.FillRectangle(GetForeColor(), PointF(GetWidth() - 1, 6), SizeF(1, GetHeight() - 7));
		g.FillRectangle(GetForeColor(), PointF(1, GetHeight() - 1), SizeF(GetWidth() - 2, 1));
	}
	//---------------------------------------------------------------------------
}
