/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "ScrollBar.hpp"
#include "../Misc/Exceptions.hpp"
#include "../Misc/Intersection.hpp"

namespace OSHGui
{
	//---------------------------------------------------------------------------
	//static attributes
	//---------------------------------------------------------------------------
	const Drawing::SizeI ScrollBar::DefaultSize(14, 110);
	//---------------------------------------------------------------------------
	//Constructor
	//---------------------------------------------------------------------------
	ScrollBar::ScrollBar()
		: drag_(false),
		  value_(0),
		  pixelsPerTick_(1.0f),
		  maximum_(0)
	{
		type_ = ControlType::ScrollBar;
		
		upButton_ = new ScrollBarButton(ScrollBarButton::ScrollBarDirection::Up);
		upButton_->GetClickEvent() += ClickEventHandler([this](Control *control)
		{
			if (value_ > 0)
			{
				SetValueInternal(value_ - 1);
			}
		});
		AddSubControl(upButton_);

		downButton_ = new ScrollBarButton(ScrollBarButton::ScrollBarDirection::Down);
		downButton_->GetClickEvent() += ClickEventHandler([this](Control *control)
		{
			if (value_ < maximum_)
			{
				SetValueInternal(value_ + 1);
			}
		});
		AddSubControl(downButton_);

		SetSize(DefaultSize);
		trackLocation_ = Drawing::PointI(0, upButton_->GetBottom() + 1);

		ApplyTheme(Application::Instance().GetTheme());

		SetMaximum(0); //indirect init

		isFocusable_ = false;
	}
	//---------------------------------------------------------------------------
	//Getter/Setter
	//---------------------------------------------------------------------------
	void ScrollBar::SetSize(const Drawing::SizeI &size)
	{
		Control::SetSize(size);

		upButton_->SetSize(Drawing::SizeI(size.Width, upButton_->GetHeight()));
		downButton_->SetSize(Drawing::SizeI(size.Width, downButton_->GetHeight()));

		downButton_->SetLocation(0, size.Height - downButton_->GetHeight());

		trackSize_ = Drawing::SizeI(size.Width, size.Height - 2 - upButton_->GetHeight() * 2);

		sliderSize_.Width = size.Width;
		sliderSize_.Height = trackSize_.Height / (maximum_ + 1);
		if (sliderSize_.Height < MinimumSliderHeight)
		{
			sliderSize_.Height = MinimumSliderHeight;
		}

		SetValueInternal(value_);
	}
	//---------------------------------------------------------------------------
	void ScrollBar::SetForeColor(const Drawing::Color &color)
	{
		Control::SetForeColor(color);

		upButton_->SetForeColor(color);
		downButton_->SetForeColor(color);
	}
	//---------------------------------------------------------------------------
	void ScrollBar::SetMaximum(int maximum)
	{
		if (maximum < 0)
		{
			#ifndef OSHGUI_DONTUSEEXCEPTIONS
			throw Misc::ArgumentOutOfRangeException("maximum");
			#endif
			return;
		}

		maximum_ = maximum;

		sliderSize_.Height = trackSize_.Height / (maximum + 1);
		if (sliderSize_.Height < MinimumSliderHeight)
		{
			sliderSize_.Height = MinimumSliderHeight;
		}

		SetValueInternal(value_ > maximum ? maximum : value_);
	}
	//---------------------------------------------------------------------------
	int ScrollBar::GetMaximum() const
	{
		return maximum_;
	}
	//---------------------------------------------------------------------------
	void ScrollBar::SetValue(int value)
	{
		SetValueInternal(value);
	}
	//---------------------------------------------------------------------------
	int ScrollBar::GetValue() const
	{
		return value_;
	}
	//---------------------------------------------------------------------------
	ScrollEvent& ScrollBar::GetScrollEvent()
	{
		return scrollEvent_;
	}
	//---------------------------------------------------------------------------
	//Runtime-Functions
	//---------------------------------------------------------------------------
	void ScrollBar::SetValueInternal(int value)
	{
		pixelsPerTick_ = (trackSize_.Height - sliderSize_.Height) / (maximum_ > 0 ? (float)maximum_ : 1.0f);

		if (value < 0)
		{
			value = 0;
		}
		if (value > maximum_)
		{
			value = maximum_;
		}
		
		if (value_ != value)
		{
			ScrollEventArgs args(value, value_);

			value_ = value;
			
			scrollEvent_.Invoke(this, args);
		}

		sliderLocation_.Top = trackLocation_.Top + value * pixelsPerTick_;
		sliderAbsoluteLocation_.Top = absoluteLocation_.Top + sliderLocation_.Top;

		Invalidate();
	}
	//---------------------------------------------------------------------------
	bool ScrollBar::Intersect(const Drawing::PointI &point) const
	{
		return Intersection::TestRectangle(absoluteLocation_.OffsetEx(0, upButton_->GetHeight()), size_.InflateEx(0, -upButton_->GetHeight() * 2), point);
	}
	//---------------------------------------------------------------------------
	void ScrollBar::CalculateAbsoluteLocation()
	{
		Control::CalculateAbsoluteLocation();

		sliderAbsoluteLocation_ = absoluteLocation_ + sliderLocation_;
		trackAbsoluteLocation_ = absoluteLocation_ + trackLocation_;
	}
	//---------------------------------------------------------------------------
	void ScrollBar::ScrollToTop()
	{
		SetValue(0);
	}
	//---------------------------------------------------------------------------
	void ScrollBar::ScrollToBottom()
	{
		SetValue(GetMaximum());
	}
	//---------------------------------------------------------------------------
	void ScrollBar::DrawSelf(Drawing::RenderContext &context)
	{
		Control::DrawSelf(context);

		upButton_->Render();
		downButton_->Render();
	}
	//---------------------------------------------------------------------------
	void ScrollBar::PopulateGeometry()
	{
		using namespace Drawing;

		Graphics g(*geometry_);

		g.FillRectangle(GetBackColor(), sliderLocation_ + PointF(1, 1), sliderSize_ - SizeF(2, 2));
		g.FillRectangle(GetBackColor(), sliderLocation_ + PointF(sliderSize_.Width - 1, 1), SizeF(1, sliderSize_.Height - 2));
		g.FillRectangle(GetBackColor(), sliderLocation_ + PointF(1, sliderSize_.Height - 1), SizeF(sliderSize_.Width - 2, 1));
		g.FillRectangle(GetBackColor(), sliderLocation_ + PointF(0, 1), SizeF(1, sliderSize_.Height - 2));
		g.FillRectangle(GetBackColor(), sliderLocation_ + PointF(1, 0), SizeF(sliderSize_.Width - 2, 1));

		auto color = isInside_ ? GetForeColor() + Color::FromARGB(0, 50, 50, 50) : GetForeColor();
		int sliderHalfHeight = sliderLocation_.Y + sliderSize_.Height / 2 - 3;
		int sliderLeftPos = sliderLocation_.X + 5;
		for (int i = 0; i < 3; ++i)
		{
			g.FillRectangle(color, PointF(sliderLeftPos, sliderHalfHeight + i * 3), SizeF(5, 1));
		}
	}
	//---------------------------------------------------------------------------
	//Event-Handling
	//---------------------------------------------------------------------------
	void ScrollBar::OnMouseDown(const MouseMessage &mouse)
	{
		Control::OnMouseDown(mouse);

		if (Intersection::TestRectangle(sliderAbsoluteLocation_, sliderSize_, mouse.GetLocation()))
		{
			drag_ = true;

			OnGotMouseCapture();
		}
	}
	//---------------------------------------------------------------------------
	void ScrollBar::OnMouseMove(const MouseMessage &mouse)
	{
		Control::OnMouseMove(mouse);

		if (drag_)
		{
			if (maximum_ >= 1)
			{
				float valuePerPixel = (float)maximum_ / (trackSize_.Height - sliderSize_.Height);

				int yPos = mouse.GetLocation().Y - trackAbsoluteLocation_.Top - sliderSize_.Height / 2;
				if (yPos < 0)
				{
					yPos = 0;
				}
				else if (yPos + sliderSize_.Height > trackSize_.Height)
				{
					yPos = trackSize_.Height - sliderSize_.Height;
				}

				SetValueInternal(yPos * valuePerPixel + 0.5f);
			}
		}
	}
	//---------------------------------------------------------------------------
	void ScrollBar::OnMouseUp(const MouseMessage &mouse)
	{
		Control::OnMouseUp(mouse);

		if (drag_)
		{
			drag_ = false;

			OnLostMouseCapture();
		}
	}
	//---------------------------------------------------------------------------
	void ScrollBar::OnMouseClick(const MouseMessage &mouse)
	{
		Control::OnMouseClick(mouse);

		if (!drag_ && maximum_ > 1)
		{
			if (Intersection::TestRectangle(trackAbsoluteLocation_, trackSize_, mouse.GetLocation()))
			{
				float valuePerPixel = (float)maximum_ / (trackSize_.Height - sliderSize_.Height);
				int yPos = mouse.GetLocation().Y - trackAbsoluteLocation_.Top - sliderSize_.Height / 2;
				SetValueInternal(yPos * valuePerPixel + 0.5f);
			}
		}
	}
	//---------------------------------------------------------------------------
	bool ScrollBar::OnMouseScroll(const MouseMessage &mouse)
	{
		Control::OnMouseScroll(mouse);

		SetValueInternal(value_ + mouse.GetDelta());

		return true;
	}
	//---------------------------------------------------------------------------
	//ScrollBar::ScrollBarButton
	//---------------------------------------------------------------------------
	const Drawing::SizeI ScrollBar::ScrollBarButton::DefaultSize(14, 14);
	//---------------------------------------------------------------------------
	ScrollBar::ScrollBarButton::ScrollBarButton(ScrollBarDirection direction)
		: direction_(direction)
	{
		SetLocation(0, 0);
		SetSize(DefaultSize);

		isFocusable_ = false;
	}
	//---------------------------------------------------------------------------
	void ScrollBar::ScrollBarButton::SetSize(const Drawing::SizeI &size)
	{
		Control::SetSize(size);

		iconLocation_ = Drawing::PointI(size.Width / 2, size.Height / 2);
	}
	//---------------------------------------------------------------------------
	void ScrollBar::ScrollBarButton::PopulateGeometry()
	{
		using namespace Drawing;

		Graphics g(*geometry_);

		auto color = isInside_ ? GetForeColor() + Color(0, 50, 50, 50) : GetForeColor();

		if (direction_ == ScrollBarDirection::Up)
		{
			for (int i = 0; i < 4; ++i)
			{
				g.FillRectangle(color, iconLocation_ + PointF(-i, i), SizeF(1 + i * 2, 1));
			}
		}
		else
		{
			for (int i = 0; i < 4; ++i)
			{
				g.FillRectangle(color, iconLocation_ - PointF(i, i), SizeF(1 + i * 2, 1));
			}
		}
	}
	//---------------------------------------------------------------------------
}
