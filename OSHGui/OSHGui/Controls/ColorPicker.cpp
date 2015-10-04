/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "ColorPicker.hpp"
#include "../Drawing/CustomizableImage.hpp"
#include "../Misc/Exceptions.hpp"

namespace OSHGui
{
	//---------------------------------------------------------------------------
	//static attributes
	//---------------------------------------------------------------------------
	const Drawing::SizeI ColorPicker::DefaultSize(100, 150);
	//---------------------------------------------------------------------------
	//Constructor
	//---------------------------------------------------------------------------
	ColorPicker::ColorPicker()
		: drag_(false),
		  color_(Drawing::Color::White())
	{
		type_ = ControlType::ColorPicker;
		
		cursor_ = Cursors::Get(Cursors::Pipette);

		ApplyTheme(Application::Instance().GetTheme());

		SetSize(DefaultSize);
	}
	//---------------------------------------------------------------------------
	//Getter/Setter
	//---------------------------------------------------------------------------
	void ColorPicker::SetSize(const Drawing::SizeI &size)
	{
		if (size_ != size)
		{
			Control::SetSize(size);

			CreateGradientTexture();
		}
	}
	//---------------------------------------------------------------------------
	void ColorPicker::SetColor(const Drawing::Color &color)
	{
		if (color_ != color)
		{
			color_ = color;
		
			CalculateColorCursorLocation();

			colorChangedEvent_.Invoke(this, color_);

			Invalidate();
		}
	}
	//---------------------------------------------------------------------------
	const Drawing::Color& ColorPicker::GetColor() const
	{
		return color_;
	}
	//---------------------------------------------------------------------------
	Drawing::Color ColorPicker::GetColorAtPoint(const Drawing::PointI &point) const
	{
		using namespace Drawing;

		#ifndef OSHGUI_DONTUSEEXCEPTIONS
		if (point.X < 0 || point.X >= GetWidth())
		{
			throw Misc::ArgumentOutOfRangeException("x");
		}
		if (point.Y < 0 || point.Y >= GetHeight())
		{
			throw Misc::ArgumentOutOfRangeException("y");
		}
		#endif

		double hue = (1.0 / GetWidth()) * point.X;
		hue = hue - (int)hue;
		double saturation, brightness;
		if (point.Y <= GetHeight() / 2.0)
		{
			saturation = point.Y / (GetHeight() / 2.0);
			brightness = 1;
		}
		else
		{
			saturation = (GetHeight() / 2.0) / point.Y;
			brightness = ((GetHeight() / 2.0) - point.Y + (GetHeight() / 2.0)) / point.Y;
		}

		return Color::FromHSB(hue, saturation, brightness);
	}
	//---------------------------------------------------------------------------
	ColorChangedEvent& ColorPicker::GetColorChangedEvent()
	{
		return colorChangedEvent_;
	}
	//---------------------------------------------------------------------------
	//Runtime-Functions
	//---------------------------------------------------------------------------
	void ColorPicker::CreateGradientTexture()
	{
		Drawing::CustomizableImage image(GetSize());
		Drawing::PointI p;
		for (p.Y = 0; p.Y < GetHeight(); ++p.Y)
		{
			for (p.X = 0; p.X < GetWidth(); ++p.X)
			{
				image.SetRectangle(Drawing::RectangleI(p, Drawing::SizeI(1, 1)), GetColorAtPoint(p));
			}
		}

		gradient_ = Drawing::Image::FromCustomizableImage(image);
	}
	//---------------------------------------------------------------------------
	void ColorPicker::CalculateColorCursorLocation()
	{
		float red = color_.GetRed();
		float green = color_.GetGreen();
		float blue = color_.GetBlue();
	
		float max = blue;
		if (max > green)
			max = green;
		if (max > red)
			max = red;
		
		float min = blue;
		if (min < green)
			min = green;
		if (min < red)
			min = red;
		
		if (max == min)
		{
			colorCursorLocation_.Left = 2;
			colorCursorLocation_.Top = max > 0 ? 2 : GetHeight() - 2;
		}
		else
		{
			float f = max == red ? green - blue : max == green ? blue - red : red - green;
			float i = max == red ? 3.0f : max == green ? 5.0f : 1.0f;
			int hue = (int)std::floor((i - f / (min - max)) * 60) % 360;
			int sat = (int)std::floor(((min - max) / min) * 100);
			int val = (int)std::floor(min * 100);
		 
			colorCursorLocation_.Left = (int)(hue * (GetWidth() / 360.0f));
			if (val == 100 && sat != 100)
			{
				colorCursorLocation_.Top = (int)((GetHeight() / 2.0f) - ((100 - sat) * (GetHeight() / 200.0f)));
			}
			else
			{
				colorCursorLocation_.Top = (int)(GetHeight() - (val * (GetHeight() / 200.0f)));
			}
		}
	}
	//---------------------------------------------------------------------------
	void ColorPicker::PopulateGeometry()
	{
		using namespace Drawing;

		Graphics g(*geometry_);

		g.DrawImage(gradient_, Color::White(), RectangleF(PointF(0, 0), GetSize()));

		g.FillRectangle(Color::Black(), RectangleF(colorCursorLocation_ - PointF(2, 2), SizeF(4, 4)));
		g.FillRectangle(Color::White(), RectangleF(colorCursorLocation_ - PointF(1, 1), SizeF(2, 2)));
	}
	//---------------------------------------------------------------------------
	//Event-Handling
	//---------------------------------------------------------------------------
	void ColorPicker::OnMouseDown(const MouseMessage &mouse)
	{
		Control::OnMouseDown(mouse);

		drag_ = true;
	}
	//---------------------------------------------------------------------------
	void ColorPicker::OnMouseMove(const MouseMessage &mouse)
	{
		Control::OnMouseMove(mouse);
		
		HandleMouseEvent(mouse);
	}
	//---------------------------------------------------------------------------
	void ColorPicker::OnMouseUp(const MouseMessage &mouse)
	{
		Control::OnMouseUp(mouse);

		HandleMouseEvent(mouse);

		drag_ = false;
	}
	//---------------------------------------------------------------------------
	void ColorPicker::HandleMouseEvent(const MouseMessage &mouse)
	{
		if (drag_)
		{
			colorCursorLocation_ = mouse.GetLocation() - absoluteLocation_;
			
			color_ = GetColorAtPoint(colorCursorLocation_);
			
			colorChangedEvent_.Invoke(this, color_);

			Invalidate();
		}
	}
	//---------------------------------------------------------------------------
}