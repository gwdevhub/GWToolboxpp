/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "TrackBar.hpp"
#include "../Misc/Exceptions.hpp"
#include "../Misc/Intersection.hpp"

namespace OSHGui {
	//---------------------------------------------------------------------------
	//static attributes
	//---------------------------------------------------------------------------
	const Drawing::SizeI TrackBar::SliderSize(8, 16);
	const Drawing::SizeI TrackBar::DefaultSize(110, TrackBar::SliderSize.Height + 2);
	//---------------------------------------------------------------------------
	//Constructor
	//---------------------------------------------------------------------------
	TrackBar::TrackBar(Control* parent) : Control(parent),
		drag_(false),
		minimum_(1),
		maximum_(10),
		tickFrequency_(1),
		sliderLocation_(-SliderSize.Width / 2, 1) {

		type_ = ControlType::TrackBar;
	
		SetSize(DefaultSize);

		SetValueInternal(0);

		ApplyTheme(Application::Instance().GetTheme());
	}
	//---------------------------------------------------------------------------
	//Getter/Setter
	//---------------------------------------------------------------------------
	void TrackBar::SetSize(const Drawing::SizeI &size) {
		if (size.Height < SliderSize.Height + 2) {
			Control::SetSize(Drawing::SizeI(size.Width, SliderSize.Height + 2));
		} else {
			Control::SetSize(size);
		}

		SetValueInternal(value_);
	}
	//---------------------------------------------------------------------------
	void TrackBar::SetMinimum(int minimum) {
		minimum_ = minimum;
		
		SetValueInternal(value_);
	}
	//---------------------------------------------------------------------------
	int TrackBar::GetMinimum() const {
		return minimum_;
	}
	//---------------------------------------------------------------------------
	void TrackBar::SetMaximum(int maximum) {
		maximum_ = maximum;
		
		SetValueInternal(value_);
	}
	//---------------------------------------------------------------------------
	int TrackBar::GetMaximum() const {
		return maximum_;
	}
	//---------------------------------------------------------------------------
	void TrackBar::SetTickFrequency(int tickFrequency) {
		tickFrequency_ = tickFrequency;
		
		SetValueInternal(value_);
	}
	//---------------------------------------------------------------------------
	int TrackBar::GetTickFrequency() const {
		return tickFrequency_;
	}
	//---------------------------------------------------------------------------
	void TrackBar::SetValue(int value) {
		SetValueInternal(value - minimum_);
	}
	//---------------------------------------------------------------------------
	int TrackBar::GetValue() const {
		return value_ + minimum_;
	}
	//---------------------------------------------------------------------------
	ValueChangedEvent& TrackBar::GetValueChangedEvent() {
		return valueChangedEvent_;
	}
	//---------------------------------------------------------------------------
	//Runtime-Functions
	//---------------------------------------------------------------------------
	void TrackBar::SetValueInternal(int value) {
		pixelsPerTick_ = (float)(GetWidth() - SliderSize.Width) / ((maximum_ - minimum_) / tickFrequency_);

		if (value < 0) {
			value = 0;
		}
		if (value > maximum_ - minimum_) {
			value = maximum_ - minimum_;
		}
		
		if (value_ != value) {
			value_ = value;
			
			valueChangedEvent_.Invoke(this);

			int tick = value / tickFrequency_;
			sliderLocation_.Left = tick * pixelsPerTick_;
			sliderAbsoluteLocation_.Left = absoluteLocation_.Left + sliderLocation_.Left;

			Invalidate();
		}
	}
	//---------------------------------------------------------------------------
	void TrackBar::CalculateAbsoluteLocation() {
		Control::CalculateAbsoluteLocation();

		sliderAbsoluteLocation_ = absoluteLocation_ + sliderLocation_;
	}
	//---------------------------------------------------------------------------
	void TrackBar::PopulateGeometry() {
		using namespace Drawing;

		Graphics g(*geometry_);

		if (!GetBackColor().IsTranslucent()) {
			g.FillRectangle(GetBackColor(), PointI(0, 0), GetSize());
		}

		auto color = isFocused_ || isInside_ ? GetForeColor() + Color::FromARGB(0, 43, 43, 43) : GetForeColor();

		auto tickCount = 1 + (maximum_ - minimum_) / tickFrequency_;
		for (int i = 0; i < tickCount; ++i) {
			int x = SliderSize.Width / 2 + i * pixelsPerTick_;
			g.FillRectangle(color, PointI(x, DefaultTickOffset), SizeI(1, 5));
		}

		g.FillRectangle(color, sliderLocation_, SliderSize);
	}
	//---------------------------------------------------------------------------
	//Event-Handling
	//---------------------------------------------------------------------------
	void TrackBar::OnMouseDown(const MouseMessage &mouse) {
		Control::OnMouseDown(mouse);

		if (Intersection::TestRectangleI(sliderAbsoluteLocation_, SliderSize, mouse.GetLocation())) {
			drag_ = true;
			OnGotMouseCapture();
		}
	}
	//---------------------------------------------------------------------------
	void TrackBar::OnMouseUp(const MouseMessage &mouse) {
		Control::OnMouseUp(mouse);

		drag_ = false;
		OnLostMouseCapture();
	}
	//---------------------------------------------------------------------------
	void TrackBar::OnMouseClick(const MouseMessage &mouse) {
		Control::OnMouseClick(mouse);

		if (!drag_) {
			HandleMouseEvent(mouse);
		}
	}
	//---------------------------------------------------------------------------
	void TrackBar::OnMouseMove(const MouseMessage &mouse) {
		Control::OnMouseMove(mouse);

		if (drag_) {
			HandleMouseEvent(mouse);
		}
	}
	//---------------------------------------------------------------------------
	void TrackBar::HandleMouseEvent(const MouseMessage &mouse) {
		int tick = (mouse.GetLocation().Left - absoluteLocation_.Left) / pixelsPerTick_;
		SetValueInternal(tick * tickFrequency_);
	}
	//---------------------------------------------------------------------------
	bool TrackBar::OnMouseScroll(const MouseMessage &mouse) {
		Control::OnMouseScroll(mouse);

		SetValueInternal(value_ + mouse.GetDelta());

		return true;
	}
	//---------------------------------------------------------------------------
	bool TrackBar::OnKeyDown(const KeyboardMessage &keyboard) {
		if (!Control::OnKeyDown(keyboard)) {
			switch (keyboard.GetKeyCode()) {
				case Key::Home:
					SetValueInternal(0);
					break;;
				case Key::End:
					SetValueInternal(maximum_ - minimum_);
					break;
				case Key::Left:
				case Key::Down:
					SetValueInternal(value_ - 1);
					break;
				case Key::Right:
				case Key::Up:
					SetValueInternal(value_ + 1);
					break;
				case Key::PageDown:
					SetValueInternal(value_ - std::max(10, (maximum_ - minimum_) / 10));
					break;
				case Key::PageUp:
					SetValueInternal(value_ + std::max(10, (maximum_ - minimum_) / 10));
					break;
			}
		}

		return true;
	}
	//---------------------------------------------------------------------------
}
