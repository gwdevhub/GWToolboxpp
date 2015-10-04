/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "Panel.hpp"
#include "../Misc/Exceptions.hpp"

namespace OSHGui
{
	//---------------------------------------------------------------------------
	//static attributes
	//---------------------------------------------------------------------------
	const Drawing::SizeI Panel::DefaultSize(200, 200);
	//---------------------------------------------------------------------------
	//Constructor
	//---------------------------------------------------------------------------
	Panel::Panel()
	{
		type_ = ControlType::Panel;

		SetSize(DefaultSize);
		
		ApplyTheme(Application::Instance().GetTheme());

		isFocusable_ = false;
	}
	//---------------------------------------------------------------------------
	//Runtime-Functions
	//---------------------------------------------------------------------------
	void Panel::PopulateGeometry()
	{
		using namespace Drawing;

		Graphics g(*geometry_);

		if (GetBackColor().GetAlpha() > 0)
		{
			g.FillRectangle(GetBackColor() - Color::FromARGB(0, 100, 100, 100), 
				0, 0, (float)GetWidth(), (float)GetHeight());
			auto color = GetBackColor() - Color::FromARGB(0, 90, 90, 90);
			g.FillRectangleGradient(ColorRectangle(GetBackColor(), GetBackColor(), color, color), 
				1, 1, (float)GetWidth()-2, (float)GetHeight()-2);
		}
	}
	//---------------------------------------------------------------------------
}
