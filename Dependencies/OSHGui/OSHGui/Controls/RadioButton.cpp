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
#include "Panel.hpp"
#include "GroupBox.hpp"

namespace OSHGui
{
	//---------------------------------------------------------------------------
	//Constructor
	//---------------------------------------------------------------------------
	RadioButton::RadioButton(Control* parent) : CheckBox(parent) {
		type_ = ControlType::RadioButton;
	}
	//---------------------------------------------------------------------------
	//Getter/Setter
	//---------------------------------------------------------------------------
	void RadioButton::SetChecked(bool checked) {
		if (checked_ != checked) {
			if (GetParent() != nullptr) {
				if (GetParent()->GetType() == ControlType::Panel) {
					for (Control* control : static_cast<Panel*>(GetParent())->GetControls()) {
						if (control->GetType() == ControlType::RadioButton) {
							static_cast<RadioButton*>(control)->SetCheckedInternal(false);
						}
					}
				}
				if (GetParent()->GetType() == ControlType::GroupBox) {
					for (Control* control : static_cast<GroupBox*>(GetParent())->GetControls()) {
						if (control->GetType() == ControlType::RadioButton) {
							static_cast<RadioButton*>(control)->SetCheckedInternal(false);
						}
					}
				}
			
				SetCheckedInternal(checked);
			}
		}
	}
	//---------------------------------------------------------------------------
	void RadioButton::SetCheckedInternal(bool checked) {
		if (checked_ != checked) {
			checked_ = checked;
			
			checkedChangedEvent_.Invoke(this);

			Invalidate();
		}
	}
	//---------------------------------------------------------------------------
	void RadioButton::PopulateGeometry() {
		using namespace Drawing;

		Graphics g(*geometry_);

		g.FillRectangle(GetBackColor(), RectangleI(PointI(0, 0), SizeI(DefaultCheckBoxSize, DefaultCheckBoxSize)));

		g.FillRectangleGradient(ColorRectangle(Color::White(), Color::White() - Color::FromARGB(0, 137, 137, 137)), 
			RectangleI(PointI(1, 1), SizeI(15, 15)));
		g.FillRectangleGradient(ColorRectangle(GetBackColor(), GetBackColor() + Color::FromARGB(0, 55, 55, 55)), 
			RectangleI(PointI(2, 2), SizeI(13, 13)));

		if (checked_) {
			g.FillRectangle(Color::White() - Color::FromARGB(0, 128, 128, 128), 
				RectangleI(PointI(5, 7), SizeI(7, 3)));
			ColorRectangle colors(Color::White(), Color::White() - Color::FromARGB(0, 137, 137, 137));
			g.FillRectangleGradient(colors, RectangleI(PointI(7, 5), SizeI(3, 7)));
			g.FillRectangleGradient(colors, RectangleI(PointI(6, 6), SizeI(5, 5)));
		}
	}
	//---------------------------------------------------------------------------
	//Event-Handling
	//---------------------------------------------------------------------------
	void RadioButton::OnMouseClick(const MouseMessage &mouse) {
		Control::OnMouseClick(mouse);

		SetChecked(true);
	}
	//---------------------------------------------------------------------------
	bool RadioButton::OnKeyUp(const KeyboardMessage &keyboard) {
		if (!Control::OnKeyUp(keyboard)) {
			if (keyboard.GetKeyCode() == Key::Space) {
				SetChecked(true);

				clickEvent_.Invoke(this);
			}
		}

		return true;
	}
	//---------------------------------------------------------------------------
	
}