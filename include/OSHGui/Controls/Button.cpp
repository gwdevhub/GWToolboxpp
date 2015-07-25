/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "Button.hpp"
#include "../Misc/Exceptions.hpp"

namespace OSHGui
{
	//---------------------------------------------------------------------------
	//static attributes
	//---------------------------------------------------------------------------
	const Drawing::SizeI Button::DefaultSize(92, 24);
	const Drawing::PointI Button::DefaultLabelOffset(6, 5);
	//---------------------------------------------------------------------------
	//Constructor
	//---------------------------------------------------------------------------
	Button::Button()
		: label_(new Label())
	{
		type_ = ControlType::Button;

		label_->SetLocation(DefaultLabelOffset);
		label_->SetBackColor(Drawing::Color::Empty());

		SetSize(DefaultSize);
		
		ApplyTheme(Application::Instance().GetTheme());
	}
	//---------------------------------------------------------------------------
	//Getter/Setter
	//---------------------------------------------------------------------------
	void Button::SetAutoSize(bool autoSize)
	{
		Control::SetAutoSize(autoSize);

		label_->SetAutoSize(autoSize);
	}
	//---------------------------------------------------------------------------
	void Button::SetSize(const Drawing::SizeI &size)
	{
		Control::SetSize(size);

		CalculateLabelLocation();
	}
	//---------------------------------------------------------------------------
	void Button::SetText(const Misc::AnsiString &text)
	{
		label_->SetText(text);
		if (autoSize_)
		{
			SetSize(label_->GetSize().InflateEx(DefaultLabelOffset.Left * 2, DefaultLabelOffset.Top * 2));
		}

		CalculateLabelLocation();
	}
	//---------------------------------------------------------------------------
	const Misc::AnsiString& Button::GetText() const
	{
		return label_->GetText();
	}
	//---------------------------------------------------------------------------
	void Button::SetFont(const Drawing::FontPtr &font)
	{
		Control::SetFont(font);

		label_->SetFont(font);
		if (autoSize_)
		{
			SetSize(label_->GetSize().InflateEx(DefaultLabelOffset.Left * 2, DefaultLabelOffset.Top * 2));
		}
	}
	//---------------------------------------------------------------------------
	void Button::SetForeColor(const Drawing::Color &color)
	{
		Control::SetForeColor(color);

		label_->SetForeColor(color);
	}
	//---------------------------------------------------------------------------
	//Runtime-Functions
	//---------------------------------------------------------------------------
	void Button::CalculateAbsoluteLocation()
	{
		Control::CalculateAbsoluteLocation();

		label_->SetParent(this);
	}
	//---------------------------------------------------------------------------
	void Button::CalculateLabelLocation()
	{
		label_->SetLocation(Drawing::PointI(GetSize().Width / 2.f - label_->GetSize().Width / 2.f, GetSize().Height / 2.f - label_->GetSize().Height / 2.f));
	}
	//---------------------------------------------------------------------------
	void Button::DrawSelf(Drawing::RenderContext &context)
	{
		Control::DrawSelf(context);

		label_->Render();
	}
	//---------------------------------------------------------------------------
	void Button::PopulateGeometry()
	{
		using namespace Drawing;

		Graphics g(*geometry_);

		auto tempColor = GetBackColor();
		if ((isFocused_ || isInside_) && !(isFocused_ && isClicked_))
		{
			tempColor = tempColor + GetMouseOverFocusColor();
		}

		auto color = tempColor + Color::FromARGB(0, 10, 10, 10);
		g.FillRectangle(color, RectangleF(PointF(1, 0), GetSize() - SizeF(2, 1)));
		g.FillRectangle(color, RectangleF(PointF(0, 1), GetSize() - SizeF(0, 3)));
		color = tempColor - Color::FromARGB(0, 50, 50, 50);
		g.FillRectangle(color, RectangleF(PointF(1, GetHeight() - 2), SizeF(GetWidth() - 2, 2)));
		g.FillRectangle(color, RectangleF(PointF(GetWidth() - 1, 1), SizeF(1, GetHeight() - 2)));

		ColorRectangle colors(tempColor, GetBackColor() - Color::FromARGB(0, 20, 20, 20));
		g.FillRectangleGradient(colors, RectangleF(PointF(1, 2), GetSize() - SizeF(2, 4)));
		g.FillRectangleGradient(colors, RectangleF(PointF(2, 1), GetSize() - SizeF(4, 2)));
	}
	//---------------------------------------------------------------------------
	//Event-Handling
	//---------------------------------------------------------------------------
	bool Button::OnKeyUp(const KeyboardMessage &keyboard)
	{
		if (!Control::OnKeyUp(keyboard))
		{
			if (keyboard.GetKeyCode() == Key::Return)
			{
				clickEvent_.Invoke(this);
			}
		}

		return true;
	}
	//---------------------------------------------------------------------------
}
