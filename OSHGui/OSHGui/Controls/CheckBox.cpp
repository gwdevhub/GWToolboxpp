/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "CheckBox.hpp"
#include "Label.hpp"
#include "../Misc/Exceptions.hpp"

namespace OSHGui
{
	//---------------------------------------------------------------------------
	//static attributes
	//---------------------------------------------------------------------------
	const Drawing::PointI CheckBox::DefaultLabelOffset(20, 2);
	//---------------------------------------------------------------------------
	//Constructor
	//---------------------------------------------------------------------------
	CheckBox::CheckBox()
		: checked_(false),
		  label_(new Label())
	{
		type_ = ControlType::CheckBox;
		
		SetSize(DefaultCheckBoxSize, DefaultCheckBoxSize);
		SetAutoSize(true);

		label_->SetLocation(DefaultLabelOffset);
		label_->SetBackColor(Drawing::Color::Empty());

		ApplyTheme(Application::Instance().GetTheme());
	}
	//---------------------------------------------------------------------------
	//Getter/Setter
	//---------------------------------------------------------------------------
	void CheckBox::SetChecked(bool checked)
	{
		if (checked_ != checked)
		{
			checked_ = checked;
			
			checkedChangedEvent_.Invoke(this);

			Invalidate();
		}
	}
	//---------------------------------------------------------------------------
	bool CheckBox::GetChecked() const
	{
		return checked_;
	}
	//---------------------------------------------------------------------------
	void CheckBox::SetText(const Misc::UnicodeString &text)
	{
		auto offset = label_->GetLocation();

		label_->SetText(text);
		if (autoSize_)
		{
			size_ = label_->GetSize().InflateEx(offset.Left, offset.Top);
			if (size_.Height < DefaultCheckBoxSize)
			{
				size_.Height = DefaultCheckBoxSize;
			}
		}
	}
	//---------------------------------------------------------------------------
	const Misc::UnicodeString& CheckBox::GetText() const
	{
		return label_->GetText();
	}
	//---------------------------------------------------------------------------
	void CheckBox::SetFont(const Drawing::FontPtr &font)
	{
		Control::SetFont(font);

		label_->SetFont(font);
		if (autoSize_)
		{
			size_ = label_->GetSize();
			if (GetFont()->GetFontHeight() < DefaultCheckBoxSize)
			{
				checkBoxLocation_ = Drawing::PointI(0, 0);
				int y = (int)(DefaultCheckBoxSize / 2.0f - GetFont()->GetFontHeight() / 2.0f + 0.5f);
				label_->SetLocation(Drawing::PointI(DefaultLabelOffset.Left, y));
			}
			else
			{
				label_->SetLocation(DefaultLabelOffset);
				int y = (int)(GetFont()->GetFontHeight() / 2.0f - DefaultCheckBoxSize / 2.0f + 0.5f);
				checkBoxLocation_ = Drawing::PointI(0, y);
			}
		}
	}
	//---------------------------------------------------------------------------
	void CheckBox::SetForeColor(const Drawing::Color &color)
	{
		Control::SetForeColor(color);

		label_->SetForeColor(color);
	}
	//---------------------------------------------------------------------------
	CheckedChangedEvent& CheckBox::GetCheckedChangedEvent()
	{
		return checkedChangedEvent_;
	}
	//---------------------------------------------------------------------------
	//Runtime-Functions
	//---------------------------------------------------------------------------
	void CheckBox::CalculateAbsoluteLocation()
	{
		Control::CalculateAbsoluteLocation();

		checkBoxAbsoluteLocation_ = absoluteLocation_ + checkBoxLocation_;

		label_->SetParent(this);
	}
	//---------------------------------------------------------------------------
	void CheckBox::DrawSelf(Drawing::RenderContext &context)
	{
		Control::DrawSelf(context);

		label_->Render();
	}
	//---------------------------------------------------------------------------
	void CheckBox::PopulateGeometry()
	{
		using namespace Drawing;

		Graphics g(*geometry_);

		g.FillRectangle(GetBackColor(), RectangleF(PointF(0, 0), SizeF(DefaultCheckBoxSize, DefaultCheckBoxSize)));
		
		g.FillRectangleGradient(ColorRectangle(Color::White(), Color::White() - Color::FromARGB(0, 137, 137, 137)), RectangleF(PointF(1, 1), SizeF(15, 15)));
		g.FillRectangleGradient(ColorRectangle(GetBackColor(), GetBackColor() + Color::FromARGB(0, 55, 55, 55)), RectangleF(PointF(2, 2), SizeF(13, 13)));
		
		if (checked_)
		{
			g.FillRectangle(Color::White(), RectangleF(PointF(5, 5), SizeF(7, 7)));
			g.FillRectangleGradient(ColorRectangle(Color::White(), Color::White() - Color::FromARGB(0, 137, 137, 137)), RectangleF(PointF(6, 6), SizeF(5, 5)));
		}
	}
	//---------------------------------------------------------------------------
	//Event-Handling
	//---------------------------------------------------------------------------
	void CheckBox::OnMouseClick(const MouseMessage &mouse)
	{
		Control::OnMouseClick(mouse);

		SetChecked(!GetChecked());
	}
	//---------------------------------------------------------------------------
	bool CheckBox::OnKeyUp(const KeyboardMessage &keyboard)
	{
		if (!Control::OnKeyUp(keyboard))
		{
			if (keyboard.GetKeyCode() == Key::Space)
			{
				SetChecked(!GetChecked());
			}
		}

		return true;
	}
	//---------------------------------------------------------------------------
}