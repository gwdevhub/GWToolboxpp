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
#include "../Misc/TextHelper.hpp"

namespace OSHGui {
	//---------------------------------------------------------------------------
	//static attributes
	//---------------------------------------------------------------------------
	const Drawing::SizeI ListBox::DefaultSize(120, 106);
	const Drawing::SizeI ListBox::DefaultItemAreaPadding(8, 8);
	const int ListBox::DefaultItemPadding(2);
	//---------------------------------------------------------------------------
	//Constructor
	//---------------------------------------------------------------------------
	ListBox::ListBox(Control* parent) : Control(parent),
		selectedIndex_(-1), 
		hoveredIndex_(-1),
		firstVisibleItemIndex_(0),
		autoScrollEnabled_(false) {
		type_ = ControlType::ListBox;
	
		scrollBar_ = new ScrollBar(this);
		scrollBar_->SetVisible(false);
		scrollBar_->GetScrollEvent() += ScrollEventHandler([this](Control*, ScrollEventArgs &args) {
			firstVisibleItemIndex_ = args.NewValue;
			Invalidate();
		});
		scrollBar_->GetFocusLostEvent() += FocusLostEventHandler([this](Control*, Control *newFocusedControl) {
			if (newFocusedControl != this) {
				OnLostFocus(newFocusedControl);
			}
		});
		AddControl(scrollBar_);

		ComputeItemHeight();

		SetSize(DefaultSize);

		ApplyTheme(Application::Instance().GetTheme());
	}
	//---------------------------------------------------------------------------
	ListBox::~ListBox() {
		Clear();
	}
	//---------------------------------------------------------------------------
	//Getter/Setter
	//---------------------------------------------------------------------------
	void ListBox::SetSize(const Drawing::SizeI &size) {
		Control::SetSize(size);

		itemAreaSize_ = size.InflateEx(-8, -8);
		if (scrollBar_->GetVisible()) {
			itemAreaSize_.Width -= scrollBar_->GetWidth();
		}
		maxVisibleItems_ = std::max(1l, std::lround((float)(itemAreaSize_.Height) / itemHeight_));

		scrollBar_->SetLocation(Drawing::PointI(size.Width - scrollBar_->GetWidth() - 1, 0));
		scrollBar_->SetHeight(size.Height);

		CheckForScrollBar();
		CheckForWidth();
	}
	//---------------------------------------------------------------------------
	void ListBox::SetFont(const Drawing::FontPtr &font) {
		Control::SetFont(font);

		ComputeItemHeight();
		CheckForScrollBar();
		CheckForWidth();
	}
	//---------------------------------------------------------------------------
	void ListBox::SetAutoScrollEnabled(bool autoScrollEnabled) {
		autoScrollEnabled_ = autoScrollEnabled;
	}
	//---------------------------------------------------------------------------
	bool ListBox::GetAutoScrollEnabled() const {
		return autoScrollEnabled_;
	}
	//---------------------------------------------------------------------------
	const Misc::UnicodeString& ListBox::GetItem(int index) const {
		#ifndef OSHGUI_DONTUSEEXCEPTIONS
		if (index < 0 || index >= (int)items_.size()) {
			throw Misc::ArgumentOutOfRangeException("index");
		}
		#endif

		return items_[index];
	}
	//---------------------------------------------------------------------------
	void ListBox::SetSelectedIndex(int index) {
		if (selectedIndex_ == index) return;
		if (index >= static_cast<int>(items_.size())) return;

		selectedIndex_ = index;

		selectedIndexChangedEvent_.Invoke(this);

		if (index >= static_cast<int>(items_.size())) return;
		
		if (index - firstVisibleItemIndex_ >= maxVisibleItems_ || index - firstVisibleItemIndex_ < 0) {
			for (firstVisibleItemIndex_ = 0; firstVisibleItemIndex_ <= index; firstVisibleItemIndex_ += maxVisibleItems_);
			firstVisibleItemIndex_ -= maxVisibleItems_;
			if (firstVisibleItemIndex_ < 0) {
				firstVisibleItemIndex_ = 0;
			}
			scrollBar_->SetValue(firstVisibleItemIndex_);
		}
		Invalidate();
	}
	//---------------------------------------------------------------------------
	int ListBox::GetSelectedIndex() const {
		return selectedIndex_;
	}
	//---------------------------------------------------------------------------
	void ListBox::SetSelectedItem(const Misc::UnicodeString &item) {
		for (int i = items_.size() - 1; i >= 0; --i) {
			if (items_[i] == item) {
				SetSelectedIndex(i);
				return;
			}
		}
	}
	//---------------------------------------------------------------------------
	const Misc::UnicodeString& ListBox::GetSelectedItem() const {
		return GetItem(GetSelectedIndex());
	}
	//---------------------------------------------------------------------------
	int ListBox::GetItemsCount() const {
		return items_.size();
	}
	//---------------------------------------------------------------------------
	SelectedIndexChangedEvent& ListBox::GetSelectedIndexChangedEvent() {
		return selectedIndexChangedEvent_;
	}
	//---------------------------------------------------------------------------
	//Runtime-Functions
	//---------------------------------------------------------------------------
	void ListBox::ExpandSizeToShowItems(int count) {
		int newHeight = count * itemHeight_;

		SetSize(Drawing::SizeI(GetWidth(), newHeight + DefaultItemAreaPadding.Height));
	}
	//---------------------------------------------------------------------------
	void ListBox::AddItem(const Misc::UnicodeString &text) {
		InsertItem(!items_.empty() ? items_.size() : 0, text);
	}
	//---------------------------------------------------------------------------
	void ListBox::InsertItem(int index, const Misc::UnicodeString &text) {
		items_.insert(items_.begin() + index, text);

		CheckForScrollBar();
		CheckForWidth();

		if (autoScrollEnabled_) {
			scrollBar_->SetValue(index);
		}

		Invalidate();
	}
	//---------------------------------------------------------------------------
	void ListBox::RemoveItem(int index) {
		#ifndef OSHGUI_DONTUSEEXCEPTIONS
		if (index < 0 || index >= (int)items_.size()) {
			throw Misc::ArgumentOutOfRangeException("index");
		}
		#endif
		
		items_.erase(items_.begin() + index);

		if (scrollBar_->GetVisible()) {
			scrollBar_->SetMaximum(items_.size() - maxVisibleItems_);
		}
		if (selectedIndex_ >= (int)items_.size()) {
			selectedIndex_ = items_.size() - 1;
			
			selectedIndexChangedEvent_.Invoke(this);
		}

		CheckForScrollBar();

		Invalidate();
	}
	//---------------------------------------------------------------------------
	void ListBox::Clear() {
		items_.clear();
		
		scrollBar_->SetMaximum(1);
		
		selectedIndex_ = -1;
		hoveredIndex_ = -1;
		firstVisibleItemIndex_ = 0;

		CheckForScrollBar();

		Invalidate();
	}
	//---------------------------------------------------------------------------
	void ListBox::CheckForScrollBar() {
		maxVisibleItems_ = std::max(1, itemAreaSize_.Height / itemHeight_);

		if (!items_.empty() && (int)(items_.size()) * itemHeight_ > itemAreaSize_.Height) {
			if (!scrollBar_->GetVisible()) {
				itemAreaSize_.Width -= scrollBar_->GetWidth();
			}
			scrollBar_->SetMaximum(items_.size() - maxVisibleItems_);
			scrollBar_->SetVisible(true);
		} else if (scrollBar_->GetVisible()) {
			scrollBar_->SetVisible(false);
			itemAreaSize_.Width += scrollBar_->GetWidth();
		}
	}
	//---------------------------------------------------------------------------
	void ListBox::CheckForWidth() {
		if (!autoSize_) return;

		int maxwidth = 0;
		for (Misc::UnicodeString &item : items_) {
			int width = std::lroundf(GetFont()->GetTextAdvance(item));
			if (maxwidth < width) maxwidth = width;
		}

		if (maxwidth > GetWidth()) SetWidth(maxwidth);
	}
	//---------------------------------------------------------------------------
	void ListBox::ComputeItemHeight() {
		int fontheight = std::lroundf(GetFont()->GetFontHeight());
		itemHeight_ = std::max(1, fontheight + DefaultItemPadding);
	}
	//---------------------------------------------------------------------------
	void ListBox::SetHoveredIndex(int index) {
		if (hoveredIndex_ == index) {
			return;
		}

		hoveredIndex_ = index;

		if (index - firstVisibleItemIndex_ >= maxVisibleItems_ || index - firstVisibleItemIndex_ < 0) {
			for (firstVisibleItemIndex_ = 0; firstVisibleItemIndex_ <= index; firstVisibleItemIndex_ += maxVisibleItems_);
			firstVisibleItemIndex_ -= maxVisibleItems_;
			if (firstVisibleItemIndex_ < 0) {
				firstVisibleItemIndex_ = 0;
			}
			scrollBar_->SetValue(firstVisibleItemIndex_);
		}
		Invalidate();
	}
	//---------------------------------------------------------------------------
	void ListBox::PopulateGeometry() {
		using namespace Drawing;

		Graphics g(*geometry_);

		Color color = GetBackColor() + Color::FromARGB(150, 0, 0, 0);
		g.FillRectangle(color, PointI(1, 1), GetSize() - SizeI(2, 2));

		color = GetBackColor() + Color::FromARGB(100, 54, 53, 52);
		g.FillRectangle(color, PointI(1, 0), SizeI(GetWidth() - 2, 1));
		g.FillRectangle(color, PointI(0, 1), SizeI(1, GetHeight() - 2));
		g.FillRectangle(color, PointI(GetWidth() - 1, 1), SizeI(1, GetHeight() - 2));
		g.FillRectangle(color, PointI(1, GetHeight() - 1), SizeI(GetWidth() - 2, 1));

		int itemX = 4;
		int itemY = 4;
		for (int i = 0; i < maxVisibleItems_ && i + firstVisibleItemIndex_ < (int)items_.size(); ++i) {
			if (hoveredIndex_ >= 0) {
				if (firstVisibleItemIndex_ + i == hoveredIndex_) {
					g.FillRectangle(Color::Red(), 
						PointI(itemX - 1, itemY + i * itemHeight_ - 1),
						SizeI(itemAreaSize_.Width + 2, itemHeight_ - DefaultItemPadding + 2));
				}
			} else {
				if (firstVisibleItemIndex_ + i == selectedIndex_) {
					g.FillRectangle(Color::Red(), PointI(itemX - 1, itemY + i * itemHeight_ - 1), SizeI(itemAreaSize_.Width + 2, itemHeight_));
				}
			}

			g.DrawString(items_[firstVisibleItemIndex_ + i], GetFont(), GetForeColor(), PointI(itemX + 1, itemY + i * itemHeight_));
		}
	}
	//---------------------------------------------------------------------------
	//Event-Handling
	//---------------------------------------------------------------------------
	void ListBox::OnMouseMove(const MouseMessage &mouse) {
		Control::OnMouseMove(mouse);
		if (scrollBar_->GetVisible() && scrollBar_->Intersect(mouse.GetLocation())) scrollBar_->ProcessMouseMove(mouse);

		if (Intersection::TestRectangleI(absoluteLocation_.OffsetEx(4, 4), itemAreaSize_, mouse.GetLocation())) {
			int hoveredIndex = firstVisibleItemIndex_ + 
				std::lround((float)(mouse.GetLocation().Y - absoluteLocation_.Y - 8) / itemHeight_);
			if (hoveredIndex < (int)(items_.size())) {
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
	void ListBox::OnMouseClick(const MouseMessage &mouse) {
		if (scrollBar_->GetVisible() && scrollBar_->Intersect(mouse.GetLocation())) return;

		Control::OnMouseClick(mouse);

		if (Intersection::TestRectangleI(absoluteLocation_.OffsetEx(4, 4), itemAreaSize_, mouse.GetLocation())) {
			int clickedIndex = firstVisibleItemIndex_ +
				std::lround((float)(mouse.GetLocation().Y - absoluteLocation_.Y - 8) / itemHeight_);
			if (clickedIndex < (int)(items_.size())) {
				SetSelectedIndex(clickedIndex);
			}
		}
	}
	void ListBox::OnMouseDown(const MouseMessage &mouse) {
		Control::OnMouseDown(mouse);
		if (scrollBar_->GetVisible() && scrollBar_->Intersect(mouse.GetLocation())) scrollBar_->ProcessMouseDown(mouse);
	}
	void ListBox::OnMouseUp(const MouseMessage &mouse) {
		Control::OnMouseUp(mouse);
		if (scrollBar_->GetVisible() && scrollBar_->Intersect(mouse.GetLocation())) scrollBar_->ProcessMouseUp(mouse);
	}
	//---------------------------------------------------------------------------
	bool ListBox::OnMouseScroll(const MouseMessage &mouse) {
		Control::OnMouseScroll(mouse);

		int newScrollValue = scrollBar_->GetValue() + mouse.GetDelta();
		if (newScrollValue < 0) {
			newScrollValue = 0;
		} else if (newScrollValue > (int)(items_.size()) - maxVisibleItems_) {
			newScrollValue = items_.size() - maxVisibleItems_;
		}
		scrollBar_->SetValue(newScrollValue);

		hoveredIndex_ = hoveredIndex_ + mouse.GetDelta();
		if (hoveredIndex_ < 0) {
			hoveredIndex_ = 0;
		} else if (hoveredIndex_ > (int)(items_.size()) - maxVisibleItems_) {
			hoveredIndex_ = items_.size() - maxVisibleItems_;
		}

		Invalidate();

		return true;
	}
	//---------------------------------------------------------------------------
	bool ListBox::OnKeyDown(const KeyboardMessage &keyboard) {
		if (!Control::OnKeyDown(keyboard)) {
			switch (keyboard.GetKeyCode()) {
				case Key::Up:
				case Key::Down:
				case Key::Home:
				case Key::End:
				case Key::PageUp:
				case Key::PageDown: {
					int newHoveredIndex = hoveredIndex_;

					switch (keyboard.GetKeyCode()) {
						case Key::Up:
							--newHoveredIndex;
							break;
						case Key::Down:
							++newHoveredIndex;
							break;
						case Key::Home:
							newHoveredIndex = 0;
							break;
						case Key::End:
							newHoveredIndex = items_.size() - 1;
							break;
						case Key::PageUp:
							newHoveredIndex += maxVisibleItems_;
							break;
						case Key::PageDown:
							newHoveredIndex -= maxVisibleItems_;
							break;
					}

					if (newHoveredIndex < 0) {
						newHoveredIndex = 0;
					}
					if (newHoveredIndex >= (int)items_.size()) {
						newHoveredIndex = items_.size() - 1;
					}

					SetHoveredIndex(newHoveredIndex);

					Invalidate();

					return true;
				}
			}
		}

		return false;
	}
	//---------------------------------------------------------------------------
	bool ListBox::OnKeyPress(const KeyboardMessage &keyboard) {
		if (!Control::OnKeyPress(keyboard)) {
			if (keyboard.IsAlphaNumeric()) {
				std::locale loc;
				Misc::AnsiChar keyChar = std::tolower(keyboard.GetKeyChar(), loc);
				int foundIndex = 0;
				for (Misc::UnicodeString &c : items_) {
					Misc::AnsiChar check = std::tolower(c[0], loc);
					if (check == keyChar && foundIndex != selectedIndex_) {
						break;
					}

					++foundIndex;
				}
				
				if (foundIndex < (int)items_.size()) {
					SetHoveredIndex(foundIndex);
				}
			}
		}

		return true;
	}
	//---------------------------------------------------------------------------
}