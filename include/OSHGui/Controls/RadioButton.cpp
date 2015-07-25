/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "RadioButton.hpp"
#include "Label.hpp"
#include "../Misc/Exceptions.hpp"

namespace OSHGui
{
	//---------------------------------------------------------------------------
	//Constructor
	//---------------------------------------------------------------------------
	RadioButton::RadioButton()
	{
		type_ = ControlType::RadioButton;
	}
	//---------------------------------------------------------------------------
	//Getter/Setter
	//---------------------------------------------------------------------------
	void RadioButton::SetChecked(bool checked)
	{
		if (checked_ != checked)
		{
			if (GetParent() != nullptr)
			{
				//uncheck other radiobuttons
				for (auto &control : GetParent()->GetControls())
				{
					if (control->GetType() == ControlType::RadioButton)
					{
						static_cast<RadioButton*>(control)->SetCheckedInternal(false);
					}
				}
			
				SetCheckedInternal(checked);
			}
		}
	}
	//---------------------------------------------------------------------------
	void RadioButton::SetCheckedInternal(bool checked)
	{
		if (checked_ != checked)
		{
			checked_ = checked;
			
			checkedChangedEvent_.Invoke(this);

			Invalidate();
		}
	}
	//---------------------------------------------------------------------------
	void RadioButton::PopulateGeometry()
	{
		using namespace Drawing;

		Graphics g(*geometry_);

		g.FillRectangle(GetBackColor(), RectangleF(PointF(0, 0), SizeF(DefaultCheckBoxSize, DefaultCheckBoxSize)));

		g.FillRectangleGradient(ColorRectangle(Color::White(), Color::White() - Color::FromARGB(0, 137, 137, 137)), RectangleF(PointF(1, 1), SizeF(15, 15)));
		g.FillRectangleGradient(ColorRectangle(GetBackColor(), GetBackColor() + Color::FromARGB(0, 55, 55, 55)), RectangleF(PointF(2, 2), SizeF(13, 13)));

		if (checked_)
		{
			g.FillRectangle(Color::White() - Color::FromARGB(0, 128, 128, 128), RectangleF(PointF(5, 7), SizeF(7, 3)));
			ColorRectangle colors(Color::White(), Color::White() - Color::FromARGB(0, 137, 137, 137));
			g.FillRectangleGradient(colors, RectangleF(PointF(7, 5), SizeF(3, 7)));
			g.FillRectangleGradient(colors, RectangleF(PointF(6, 6), SizeF(5, 5)));
		}
	}
	//---------------------------------------------------------------------------
	//Event-Handling
	//---------------------------------------------------------------------------
	void RadioButton::OnMouseClick(const MouseMessage &mouse)
	{
		Control::OnMouseClick(mouse);

		SetChecked(true);
	}
	//---------------------------------------------------------------------------
	bool RadioButton::OnKeyUp(const KeyboardMessage &keyboard)
	{
		if (!Control::OnKeyUp(keyboard))
		{
			if (keyboard.GetKeyCode() == Key::Space)
			{
				SetChecked(true);

				clickEvent_.Invoke(this);
			}
		}

		return true;
	}
	//---------------------------------------------------------------------------
	
}