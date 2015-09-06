/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include <locale>
#include "ListBox.hpp"
#include "ScrollBar.hpp"
#include "../Misc/Exceptions.hpp"
#include "../Misc/Intersection.hpp"

namespace OSHGui
{
	//---------------------------------------------------------------------------
	//static attributes
	//---------------------------------------------------------------------------
	const Drawing::SizeI ListBox::DefaultSize(120, 106);
	const Drawing::SizeI ListBox::DefaultItemAreaPadding(8, 8);
	const int ListBox::DefaultItemPadding(2);
	//---------------------------------------------------------------------------
	//Constructor
	//---------------------------------------------------------------------------
	ListBox::ListBox()
		: selectedIndex_(-1), hoveredIndex_(-1),
		  firstVisibleItemIndex_(0),
		  autoScrollEnabled_(false)
	{
		type_ = ControlType::ListBox;
	
		scrollBar_ = new ScrollBar();
		scrollBar_->SetVisible(false);
		scrollBar_->GetScrollEvent() += ScrollEventHandler([this](Control*, ScrollEventArgs &args)
		{
			firstVisibleItemIndex_ = args.NewValue;
			Invalidate();
		});
		scrollBar_->GetFocusLostEvent() += FocusLostEventHandler([this](Control*, Control *newFocusedControl)
		{
			if (newFocusedControl != this)
			{
				OnLostFocus(newFocusedControl);
			}
		});
		AddSubControl(scrollBar_);

		SetSize(DefaultSize);
		
		maxVisibleItems_ = GetHeight() / (GetFont()->GetFontHeight() + DefaultItemPadding);

		ApplyTheme(Application::Instance().GetTheme());
	}
	//---------------------------------------------------------------------------
	ListBox::~ListBox()
	{
		Clear();
	}
	//---------------------------------------------------------------------------
	//Getter/Setter
	//---------------------------------------------------------------------------
	void ListBox::SetSize(const Drawing::SizeI &size)
	{
		Control::SetSize(size);

		itemAreaSize_ = size.InflateEx(-8, -8);
		if (scrollBar_->GetVisible())
		{
			itemAreaSize_.Width -= scrollBar_->GetWidth();
		}

		scrollBar_->SetLocation(size.Width - scrollBar_->GetWidth() - 1, 0);
		scrollBar_->SetSize(scrollBar_->GetWidth(), size.Height);

		CheckForScrollBar();
	}
	//---------------------------------------------------------------------------
	void ListBox::SetFont(const Drawing::FontPtr &font)
	{
		Control::SetFont(font);

		CheckForScrollBar();
	}
	//---------------------------------------------------------------------------
	void ListBox::SetAutoScrollEnabled(bool autoScrollEnabled)
	{
		autoScrollEnabled_ = autoScrollEnabled;
	}
	//---------------------------------------------------------------------------
	bool ListBox::GetAutoScrollEnabled() const
	{
		return autoScrollEnabled_;
	}
	//---------------------------------------------------------------------------
	const Misc::AnsiString& ListBox::GetItem(int index) const
	{
		#ifndef OSHGUI_DONTUSEEXCEPTIONS
		if (index < 0 || index >= (int)items_.size())
		{
			throw Misc::ArgumentOutOfRangeException("index");
		}
		#endif

		return items_[index];
	}
	//---------------------------------------------------------------------------
	void ListBox::SetSelectedIndex(int index)
	{
		if (selectedIndex_ == index)
		{
			return;
		}

		selectedIndex_ = index;

		selectedIndexChangedEvent_.Invoke(this);

		if (index - firstVisibleItemIndex_ >= maxVisibleItems_ || index - firstVisibleItemIndex_ < 0)
		{
			for (firstVisibleItemIndex_ = 0; firstVisibleItemIndex_ <= index; firstVisibleItemIndex_ += maxVisibleItems_);
			firstVisibleItemIndex_ -= maxVisibleItems_;
			if (firstVisibleItemIndex_ < 0)
			{
				firstVisibleItemIndex_ = 0;
			}
			scrollBar_->SetValue(firstVisibleItemIndex_);
		}

		Invalidate();
	}
	//---------------------------------------------------------------------------
	int ListBox::GetSelectedIndex() const
	{
		return selectedIndex_;
	}
	//---------------------------------------------------------------------------
	void ListBox::SetSelectedItem(const Misc::AnsiString &item)
	{
		for (int i = items_.size() - 1; i >= 0; --i)
		{
			if (items_[i] == item)
			{
				SetSelectedIndex(i);
				return;
			}
		}
	}
	//---------------------------------------------------------------------------
	const Misc::AnsiString& ListBox::GetSelectedItem() const
	{
		return GetItem(GetSelectedIndex());
	}
	//---------------------------------------------------------------------------
	int ListBox::GetItemsCount() const
	{
		return items_.size();
	}
	//---------------------------------------------------------------------------
	SelectedIndexChangedEvent& ListBox::GetSelectedIndexChangedEvent()
	{
		return selectedIndexChangedEvent_;
	}
	//---------------------------------------------------------------------------
	//Runtime-Functions
	//---------------------------------------------------------------------------
	bool ListBox::Intersect(const Drawing::PointI &point) const
	{
		return Intersection::TestRectangle(absoluteLocation_, scrollBar_->GetVisible() ? size_.InflateEx(-scrollBar_->GetWidth(), 0) : size_, point);
	}
	//---------------------------------------------------------------------------
	void ListBox::ExpandSizeToShowItems(int count)
	{
		auto itemHeight = GetFont()->GetFontHeight() + DefaultItemPadding;
		int newHeight = count * itemHeight;

		SetSize(GetWidth(), newHeight + DefaultItemAreaPadding.Height);
	}
	//---------------------------------------------------------------------------
	void ListBox::AddItem(const Misc::AnsiString &text)
	{
		InsertItem(!items_.empty() ? items_.size() : 0, text);
	}
	//---------------------------------------------------------------------------
	void ListBox::InsertItem(int index, const Misc::AnsiString &text)
	{
		items_.insert(items_.begin() + index, text);

		CheckForScrollBar();

		if (autoScrollEnabled_)
		{
			scrollBar_->SetValue(index);
		}

		Invalidate();
	}
	//---------------------------------------------------------------------------
	void ListBox::RemoveItem(int index)
	{
		#ifndef OSHGUI_DONTUSEEXCEPTIONS
		if (index < 0 || index >= (int)items_.size())
		{
			throw Misc::ArgumentOutOfRangeException("index");
		}
		#endif
		
		items_.erase(items_.begin() + index);

		if (scrollBar_->GetVisible())
		{
			scrollBar_->SetMaximum(items_.size() - maxVisibleItems_);
		}
		if (selectedIndex_ >= (int)items_.size())
		{
			selectedIndex_ = items_.size() - 1;
			
			selectedIndexChangedEvent_.Invoke(this);
		}

		Invalidate();
	}
	//---------------------------------------------------------------------------
	void ListBox::Clear()
	{
		items_.clear();
		
		scrollBar_->SetMaximum(1);
		
		selectedIndex_ = -1;

		CheckForScrollBar();

		Invalidate();
	}
	//---------------------------------------------------------------------------
	void ListBox::CheckForScrollBar()
	{
		int itemHeight = GetFont()->GetFontHeight() + DefaultItemPadding;

		maxVisibleItems_ = std::max(1, itemAreaSize_.Height / itemHeight);

		if (!items_.empty() && items_.size() * itemHeight > itemAreaSize_.Height)
		{
			if (!scrollBar_->GetVisible())
			{
				itemAreaSize_.Width -= scrollBar_->GetWidth();
			}
			scrollBar_->SetMaximum(items_.size() - maxVisibleItems_);
			scrollBar_->SetVisible(true);
		}
		else if (scrollBar_->GetVisible())
		{
			scrollBar_->SetVisible(false);
			itemAreaSize_.Width += scrollBar_->GetWidth();
		}
	}
	//---------------------------------------------------------------------------
	void ListBox::DrawSelf(Drawing::RenderContext &context)
	{
		Control::DrawSelf(context);

		scrollBar_->Render();
	}
	//---------------------------------------------------------------------------
	void ListBox::PopulateGeometry()
	{
		using namespace Drawing;

		Graphics g(*geometry_);

		g.FillRectangle(GetBackColor(), PointF(1, 1), GetSize() - SizeF(2, 2));

		auto color = GetBackColor() + Color::FromARGB(0, 54, 53, 52);
		g.FillRectangle(color, PointF(1, 0), SizeF(GetWidth() - 2, 1));
		g.FillRectangle(color, PointF(0, 1), SizeF(1, GetHeight() - 2));
		g.FillRectangle(color, PointF(GetWidth() - 1, 1), SizeF(1, GetHeight() - 2));
		g.FillRectangle(color, PointF(1, GetHeight() - 1), SizeF(GetWidth() - 2, 1));

		int itemX = 4;
		int itemY = 5;
		int padding = GetFont()->GetFontHeight() + DefaultItemPadding;
		for (int i = 0; i < maxVisibleItems_ && i + firstVisibleItemIndex_ < (int)items_.size(); ++i)
		{
			if (hoveredIndex_ >= 0) {
				if (firstVisibleItemIndex_ + i == hoveredIndex_) {
					g.FillRectangle(Color::Red(), PointF(itemX - 1, itemY + i * padding - 1), SizeF(itemAreaSize_.Width + 2, padding));
				}
			} else {
				if (firstVisibleItemIndex_ + i == selectedIndex_) {
					g.FillRectangle(Color::Red(), PointF(itemX - 1, itemY + i * padding - 1), SizeF(itemAreaSize_.Width + 2, padding));
				}
			}

			g.DrawString(items_[firstVisibleItemIndex_ + i], GetFont(), GetForeColor(), PointF(itemX + 1, itemY + i * padding));
		}
	}
	//---------------------------------------------------------------------------
	//Event-Handling
	//---------------------------------------------------------------------------
	void ListBox::OnMouseMove(const MouseMessage &mouse) {
		Control::OnMouseMove(mouse);

		if (Intersection::TestRectangle(absoluteLocation_.OffsetEx(4, 4), itemAreaSize_, mouse.GetLocation())) {
			int hoveredIndex = firstVisibleItemIndex_ + (mouse.GetLocation().Y - absoluteLocation_.Y - 4) / (GetFont()->GetFontHeight() + DefaultItemPadding);
			if (hoveredIndex < items_.size()) {
				if (hoveredIndex != hoveredIndex_) {
					hoveredIndex_ = hoveredIndex;
					Invalidate();
				}
			}
		}
	}
	void ListBox::OnMouseLeave(const MouseMessage &mouse) {
		Control::OnMouseLeave(mouse);
		if (hoveredIndex_ != -1) Invalidate();
		hoveredIndex_ = -1;
	}
	void ListBox::OnMouseClick(const MouseMessage &mouse)
	{
		Control::OnMouseClick(mouse);

		if (Intersection::TestRectangle(absoluteLocation_.OffsetEx(4, 4), itemAreaSize_, mouse.GetLocation()))
		{
			int clickedIndex = firstVisibleItemIndex_ + (mouse.GetLocation().Y - absoluteLocation_.Y - 4) / (GetFont()->GetFontHeight() + DefaultItemPadding);
			if (clickedIndex < items_.size())
			{
				SetSelectedIndex(clickedIndex);
			}
		}
	}
	//---------------------------------------------------------------------------
	void ListBox::OnMouseScroll(const MouseMessage &mouse)
	{
		Control::OnMouseScroll(mouse);

		int newScrollValue = scrollBar_->GetValue() + mouse.GetDelta();
		if (newScrollValue < 0)
		{
			newScrollValue = 0;
		}
		else if (newScrollValue > items_.size() - maxVisibleItems_)
		{
			newScrollValue = items_.size() - maxVisibleItems_;
		}
		scrollBar_->SetValue(newScrollValue);
	}
	//---------------------------------------------------------------------------
	bool ListBox::OnKeyDown(const KeyboardMessage &keyboard)
	{
		if (!Control::OnKeyDown(keyboard))
		{
			switch (keyboard.GetKeyCode())
			{
				case Key::Up:
				case Key::Down:
				case Key::Home:
				case Key::End:
				case Key::PageUp:
				case Key::PageDown:
				{
					int newSelectedIndex = selectedIndex_;

					switch (keyboard.GetKeyCode())
					{
						case Key::Up:
							--newSelectedIndex;
							break;
						case Key::Down:
							++newSelectedIndex;
							break;
						case Key::Home:
							newSelectedIndex = 0;
							break;
						case Key::End:
							newSelectedIndex = items_.size() - 1;
							break;
						case Key::PageUp:
							newSelectedIndex += maxVisibleItems_;
							break;
						case Key::PageDown:
							newSelectedIndex -= maxVisibleItems_;
							break;
					}

					if (newSelectedIndex < 0)
					{
						newSelectedIndex = 0;
					}
					if (newSelectedIndex >= (int)items_.size())
					{
						newSelectedIndex = items_.size() - 1;
					}

					SetSelectedIndex(newSelectedIndex);

					return true;
				}
			}
		}

		return false;
	}
	//---------------------------------------------------------------------------
	bool ListBox::OnKeyPress(const KeyboardMessage &keyboard)
	{
		if (!Control::OnKeyPress(keyboard))
		{
			if (keyboard.IsAlphaNumeric())
			{
				std::locale loc;
				Misc::AnsiChar keyChar = std::tolower(keyboard.GetKeyChar(), loc);
				int foundIndex = 0;
				for (auto &c : items_)
				{
					Misc::AnsiChar check = std::tolower(c[0], loc);
					if (check == keyChar && foundIndex != selectedIndex_)
					{
						break;
					}

					++foundIndex;
				}
					
				if (foundIndex < (int)items_.size())
				{
					SetSelectedIndex(foundIndex);
				}
			}
		}

		return true;
	}
	//---------------------------------------------------------------------------
}