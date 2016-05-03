/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "ColorBar.hpp"
#include "../Misc/Exceptions.hpp"
#include "../Misc/Intersection.hpp"

namespace OSHGui {
	//---------------------------------------------------------------------------
	//static attributes
	//---------------------------------------------------------------------------
	const Drawing::SizeI ColorBar::DefaultSize(150, 45);
	const Drawing::SizeI ColorBar::DefaultBarSize(150, 10);
	//---------------------------------------------------------------------------
	//Constructor
	//---------------------------------------------------------------------------
	ColorBar::ColorBar(Control* parent) : Control(parent), 
		barIndex_(0),
		color_(Drawing::Color::Black()) {
		type_ = ControlType::ColorBar;
	
		for (int i = 0; i < 3; ++i) {
			bars_.push_back(Drawing::Color::Black());
			barSliderLocation_.push_back(Drawing::PointI(0, i * 15 + 9));
			barSliderAbsoluteLocation_.push_back(Drawing::PointI(0, 0));
			drag_[i] = false;
		}
		
		SetSize(DefaultSize);
		
		ApplyTheme(Application::Instance().GetTheme());

		UpdateBars();
	}
	//---------------------------------------------------------------------------
	//Getter/Setter
	//---------------------------------------------------------------------------
	void ColorBar::SetSize(const Drawing::SizeI &size) {
		if (size_.Width != size.Width) {
			Control::SetSize(size);

			UpdateBars();
		} else {
			Control::SetSize(size);
		}
	}
	//---------------------------------------------------------------------------
	void ColorBar::SetColor(const Drawing::Color &color) {
		if (color_ != color) {
			color_ = color;

			UpdateBars();

			colorChangedEvent_.Invoke(this, color_);
		}
	}
	//---------------------------------------------------------------------------
	const Drawing::Color& ColorBar::GetColor() const {
		return color_;
	}
	//---------------------------------------------------------------------------
	ColorChangedEvent& ColorBar::GetColorChangedEvent() {
		return colorChangedEvent_;
	}
	//---------------------------------------------------------------------------
	//Runtime-Functions
	//---------------------------------------------------------------------------
	void ColorBar::UpdateColor() {
		float multi = 1.0f / ((float)GetWidth() - 2);
		float newColor = multi * barSliderLocation_[barIndex_].Left;
		
		SetColor(Drawing::Color(barIndex_ == 0 ? newColor : color_.GetRed(), barIndex_ == 1 ? newColor : color_.GetGreen(), barIndex_ == 2 ? newColor : color_.GetBlue()));
	}
	//---------------------------------------------------------------------------
	void ColorBar::UpdateBars() {
		float multi = (float)GetWidth() - 2;
		for (int i = 0; i < 3; ++i) {
			bars_[i].TopLeft = bars_[i].BottomLeft = i == 0 ? Drawing::Color(0, color_.GetGreen(), color_.GetBlue()) : i == 1 ? Drawing::Color(color_.GetRed(), 0, color_.GetBlue()) : Drawing::Color(color_.GetRed(), color_.GetGreen(), 0);
			bars_[i].TopRight = bars_[i].BottomRight = color_;
			
			barSliderLocation_[i].Left = (int)((i == 0 ? color_.GetRed() : i == 1 ? color_.GetGreen() : color_.GetBlue()) * multi);
			barSliderLocation_[i].Top = i * 15 + 9;
			barSliderAbsoluteLocation_[i] = absoluteLocation_ + barSliderLocation_[i];
		}

		Invalidate();
	}
	//---------------------------------------------------------------------------
	void ColorBar::PopulateGeometry() {
		using namespace Drawing;

		Graphics g(*geometry_);

		for (int i = 0; i < 3; ++i) {
			g.FillRectangleGradient(bars_[i], RectangleI(PointI(0, i * 15), SizeI(GetWidth(), 8)));

			auto sliderPos = barSliderLocation_[i] + PointI(1, 0);
			for (int j = 0; j < 3; ++j) {
				g.FillRectangle(GetForeColor(), RectangleI(PointI(sliderPos.X - j, sliderPos.Y + j), SizeI(1 + j * 2, 1)));
			}
		}
	}
	//---------------------------------------------------------------------------
	//Event-Handling
	//---------------------------------------------------------------------------
	void ColorBar::OnMouseDown(const MouseMessage &mouse) {
		Control::OnMouseDown(mouse);

		drag_[0] = drag_[1] = drag_[2] = false;

		for (int i = 0; i < 3; ++i) {
			auto barLocation = Drawing::PointI(absoluteLocation_.Left, absoluteLocation_.Top + i * 15);
			if (Intersection::TestRectangleI(barLocation, DefaultBarSize, mouse.GetLocation())) {
				barIndex_ = i;

				drag_[barIndex_] = true;

				OnGotMouseCapture();

				int localLocation = mouse.GetLocation().Left - absoluteLocation_.Left;
				if (localLocation < 0) {
					barSliderLocation_[barIndex_].Left = 0;
				} else if (localLocation > GetWidth() - 2) {
					barSliderLocation_[barIndex_].Left = GetWidth() - 2;
				} else {
					barSliderLocation_[barIndex_].Left = localLocation;
				}

				UpdateColor();
			
				UpdateBars();

				return;
			}
		}
	}
	//---------------------------------------------------------------------------
	void ColorBar::OnMouseUp(const MouseMessage &mouse) {
		Control::OnMouseUp(mouse);

		if (drag_[barIndex_]) {
			drag_[barIndex_] = false;

			colorChangedEvent_.Invoke(this, color_);

			OnLostMouseCapture();
		}
	}
	//---------------------------------------------------------------------------
	void ColorBar::OnMouseMove(const MouseMessage &mouse) {
		Control::OnMouseMove(mouse);

		if (drag_[barIndex_]) {
			int localLocation = mouse.GetLocation().Left - absoluteLocation_.Left;
			if (localLocation < 0) {
				barSliderLocation_[barIndex_].Left = 0;
			} else if (localLocation > GetWidth() - 2) {
				barSliderLocation_[barIndex_].Left = GetWidth() - 2;
			} else {
				barSliderLocation_[barIndex_].Left = localLocation;
			}
			
			UpdateColor();

			UpdateBars();
		}
	}
	//---------------------------------------------------------------------------
	bool ColorBar::OnKeyDown(const KeyboardMessage &keyboard) {
		if (!Control::OnKeyDown(keyboard)) {
			if (keyboard.GetKeyCode() == Key::Left || keyboard.GetKeyCode() == Key::Right) {
				barSliderLocation_[barIndex_].Left += keyboard.GetKeyCode() == Key::Left ? -1 : 1;
				
				if (barSliderLocation_[barIndex_].Left < 0) {
					barSliderLocation_[barIndex_].Left = 0;
				}
				if (barSliderLocation_[barIndex_].Left >= GetWidth() - 2) {
					barSliderLocation_[barIndex_].Left = GetWidth() - 2;
				}

				UpdateColor();

				UpdateBars();
			}
		}

		return true;
	}
	//---------------------------------------------------------------------------
}
