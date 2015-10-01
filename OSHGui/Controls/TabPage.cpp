/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "TabPage.hpp"
#include "TabControl.hpp"
#include "../Misc/Exceptions.hpp"

namespace OSHGui
{
	//---------------------------------------------------------------------------
	//Constructor
	//---------------------------------------------------------------------------
	TabPage::TabPage()
		: button_(nullptr)
	{
		type_ = ControlType::TabPage;
	
		containerPanel_ = new Panel();
		containerPanel_->SetLocation(Drawing::PointI(2, 2));
		containerPanel_->SetBackColor(Drawing::Color::Empty());
		AddSubControl(containerPanel_);
		
		ApplyTheme(Application::Instance().GetTheme());
	}
	//---------------------------------------------------------------------------
	//Getter/Setter
	//---------------------------------------------------------------------------
	void TabPage::SetSize(const Drawing::SizeI &size)
	{
		Control::SetSize(size);

		containerPanel_->SetSize(size.InflateEx(-4, -4));
	}
	//---------------------------------------------------------------------------
	void TabPage::SetParent(Control *parent)
	{
		#ifndef OSHGUI_DONTUSEEXCEPTIONS
		if (parent == nullptr)
		{
			throw Misc::ArgumentNullException("parent");
		}
		if (parent->GetType() != ControlType::TabControl)
		{
			throw Misc::ArgumentException("parent");
		}
		#endif
	
		Panel::SetParent(parent);
	}
	//---------------------------------------------------------------------------
	void TabPage::SetText(const Misc::AnsiString &text)
	{
		if (button_)
		{
			button_->SetText(text);
		}

		text_ = text;

		Invalidate();
	}
	//---------------------------------------------------------------------------
	const Misc::AnsiString& TabPage::GetText() const
	{
		return text_;
	}
	//---------------------------------------------------------------------------
	const std::deque<Control*>& TabPage::GetControls() const
	{
		return containerPanel_->GetControls();
	}
	//---------------------------------------------------------------------------
	//Runtime-Functions
	//---------------------------------------------------------------------------
	void TabPage::AddControl(Control *control)
	{
		containerPanel_->AddControl(control);
	}
	//---------------------------------------------------------------------------
	void TabPage::RemoveControl(Control *control)
	{
		containerPanel_->RemoveControl(control);
	}
	//---------------------------------------------------------------------------
	void TabPage::DrawSelf(Drawing::RenderContext &context)
	{
		Control::DrawSelf(context);

		containerPanel_->Render();
	}
	//---------------------------------------------------------------------------
	void TabPage::PopulateGeometry()
	{
		using namespace Drawing;

		Graphics g(*geometry_);

		if (!GetBackColor().IsTranslucent())
		{
			g.FillRectangle(GetBackColor() + Color::FromARGB(0, 32, 32, 32), PointF(0, 0), GetSize());
			g.FillRectangleGradient(ColorRectangle(GetBackColor(), GetBackColor() - Color::FromARGB(0, 20, 20, 20)), PointF(1, 1), GetSize() - SizeF(2, 2));
		}
	}
	//---------------------------------------------------------------------------
}
