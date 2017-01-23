/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */
#include "ComboBox.hpp"
#include "ListBox.hpp"
#include "ScrollBar.hpp"
#include "../Misc/Exceptions.hpp"
#include "../Misc/Intersection.hpp"

namespace OSHGui {
	const int ComboBox::DefaultMaxShowItems(10);
	//---------------------------------------------------------------------------
	//Constructor
	//---------------------------------------------------------------------------
	ComboBox::ComboBox(Control* parent) : Control(parent),
		droppedDown_(false),
		maxShowItems_(DefaultMaxShowItems) {

		type_ = ControlType::ComboBox;

		canRaiseEvents_ = false;
		//clip_ = Clipping::OnParent;
	
		button_ = new ComboBoxButton(this);
		button_->SetLocation(Drawing::PointI(0, 0));
		button_->GetClickEvent() += ClickEventHandler([this](Control*) {
			if (!droppedDown_) {
				Expand();
			} else {
				Collapse();
			}
		});
		button_->SetClip(Clipping::OnParent);
		button_->GetKeyDownEvent() += KeyDownEventHandler([this](Control*, KeyEventArgs &args) {
			switch (args.GetKeyCode()) {
				case Key::Up:
				case Key::Down:
				case Key::Home:
				case Key::End:
				case Key::PageUp:
				case Key::PageDown: {
					int newSelectedIndex = listBox_->GetSelectedIndex();

					switch (args.GetKeyCode()) {
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
							newSelectedIndex = listBox_->GetItemsCount() - 1;
							break;
						case Key::PageUp:
							newSelectedIndex += 4;
							break;
						case Key::PageDown:
							newSelectedIndex -= 4;
							break;
					}

					if (newSelectedIndex < 0) {
						newSelectedIndex = 0;
					}
					if (newSelectedIndex >= listBox_->GetItemsCount()) {
						newSelectedIndex = listBox_->GetItemsCount() - 1;
					}

					listBox_->SetSelectedIndex(newSelectedIndex);
				}
			}
			button_->Focus();
		});
		button_->GetFocusLostEvent() += FocusLostEventHandler([this](Control*, Control *newFocusedControl) {
			if (newFocusedControl == 0 || newFocusedControl->GetParent() == this || newFocusedControl->GetParent()->GetParent() == this) {
				return;
			}
			Collapse();
		});
		
		listBox_ = new ListBox(this);
		listBox_->SetClip(Clipping::OnControl);
		listBox_->SetLocation(Drawing::PointI(0, button_->GetBottom() + 2));
		listBox_->SetSize(Drawing::SizeI(listBox_->GetWidth(), 4));
		listBox_->SetVisible(false);
		listBox_->ExpandSizeToShowItems(4);
		listBox_->GetClickEvent() += ClickEventHandler([this](Control*) {
			Collapse();
		});
		listBox_->GetSelectedIndexChangedEvent() += SelectedIndexChangedEventHandler([this](Control*) {
			if (listBox_->GetSelectedIndex() >= 0) {
				button_->SetText(listBox_->GetSelectedItem());
			}
			if (listBox_->GetVisible()) {
				Collapse();
				button_->Focus();
			}
		});
		listBox_->GetFocusLostEvent() += FocusLostEventHandler([this](Control*, Control *newFocusedControl) {
			if (newFocusedControl == nullptr) {
				Collapse();
				return;
			}
			
			Control* parent = newFocusedControl->GetParent();
			if (parent == this) return;
			parent = parent->GetParent();
			if (parent == this) return;
			parent = parent->GetParent();
			if (parent == this) return;
			Collapse();
		});

		AddControl(button_);
		AddControl(listBox_);

		SetSize(Drawing::SizeI(160, 24));
		
		ApplyTheme(Application::Instance().GetTheme());
	}
	//---------------------------------------------------------------------------
	//Getter/Setter
	//---------------------------------------------------------------------------
	void ComboBox::SetSize(const Drawing::SizeI &size) {
		Control::SetSize(size);

		button_->SetSize(size);
		listBox_->SetLocation(Drawing::PointI(0, button_->GetBottom() + 2));
		listBox_->SetWidth(size.Width);
	}
	//---------------------------------------------------------------------------
	void ComboBox::SetAutoSize(bool b) {
		Control::SetAutoSize(b);
		listBox_->SetAutoSize(b);
	}
	//---------------------------------------------------------------------------
	void ComboBox::SetFont(const Drawing::FontPtr &font) {
		Control::SetFont(font);

		button_->SetFont(font);
		listBox_->SetLocation(Drawing::PointI(0, button_->GetBottom() + 2));
		listBox_->SetFont(font);
	}
	//---------------------------------------------------------------------------
	void ComboBox::SetForeColor(const Drawing::Color &color) {
		Control::SetForeColor(color);

		button_->SetForeColor(color);
		listBox_->SetForeColor(color);
	}
	//---------------------------------------------------------------------------
	void ComboBox::SetBackColor(const Drawing::Color &color) {
		Control::SetBackColor(color);

		button_->SetBackColor(color);
		listBox_->SetBackColor(color);
	}
	//---------------------------------------------------------------------------
	void ComboBox::SetText(const Misc::UnicodeString &text) {
		button_->SetText(text);
	}
	//---------------------------------------------------------------------------
	const Misc::UnicodeString& ComboBox::GetText() const {
		return button_->GetText();
	}
	//---------------------------------------------------------------------------
	bool ComboBox::GetIsFocused() const {
		return Control::GetIsFocused() || button_->GetIsFocused() || listBox_->GetIsFocused();
	}
	//---------------------------------------------------------------------------
	const Misc::UnicodeString& ComboBox::GetItem(int index) const {
		return listBox_->GetItem(index);
	}
	//---------------------------------------------------------------------------
	void ComboBox::SetSelectedIndex(int index) {
		listBox_->SetSelectedIndex(index);

		Collapse();
	}
	//---------------------------------------------------------------------------
	int ComboBox::GetSelectedIndex() const {
		return listBox_->GetSelectedIndex();
	}
	//---------------------------------------------------------------------------
	void ComboBox::SetSelectedItem(const Misc::UnicodeString &item) {
		listBox_->SetSelectedItem(item);

		Collapse();
	}
	//---------------------------------------------------------------------------
	const Misc::UnicodeString& ComboBox::GetSelectedItem() const {
		return listBox_->GetSelectedItem();
	}
	//---------------------------------------------------------------------------
	int ComboBox::GetItemsCount() const {
		return listBox_->GetItemsCount();
	}
	//---------------------------------------------------------------------------
	void ComboBox::SetMaxShowItems(int items) {
		maxShowItems_ = items;
	}
	//---------------------------------------------------------------------------
	int ComboBox::GetMaxShowItems() const {
		return maxShowItems_;
	}
	//---------------------------------------------------------------------------
	SelectedIndexChangedEvent& ComboBox::GetSelectedIndexChangedEvent() {
		return listBox_->GetSelectedIndexChangedEvent();
	}
	//---------------------------------------------------------------------------
	//Runtime-Functions
	//---------------------------------------------------------------------------
	void ComboBox::Expand() {
		droppedDown_ = true;
		listBox_->SetVisible(true);
		listBox_->Focus();
	}
	//---------------------------------------------------------------------------
	void ComboBox::Collapse() {
		droppedDown_ = false;
		listBox_->SetVisible(false);
	}
	//---------------------------------------------------------------------------
	void ComboBox::AddItem(const Misc::UnicodeString &text) {
		InsertItem(listBox_->GetItemsCount(), text);
	}
	//---------------------------------------------------------------------------
	void ComboBox::InsertItem(int index, const Misc::UnicodeString &text) {
		listBox_->InsertItem(index, text);
		listBox_->ExpandSizeToShowItems(std::min(listBox_->GetItemsCount(), maxShowItems_));
	}
	//---------------------------------------------------------------------------
	void ComboBox::RemoveItem(int index) {
		listBox_->RemoveItem(index);
	}
	//---------------------------------------------------------------------------
	void ComboBox::Clear() {
		Collapse();

		listBox_->Clear();
	}
	//---------------------------------------------------------------------------
	bool ComboBox::Intersect(const Drawing::PointI &point) const {
		if (Control::Intersect(point)) return true;						// if on the button
		if (droppedDown_ && Intersection::TestRectangleI(absoluteLocation_.OffsetEx(0, GetHeight()),
			Drawing::SizeI(GetWidth(), 2), point)) return true;			// if on the space between button and listbox
		if (droppedDown_ && listBox_->Intersect(point)) return true;	// if on the listbox

		return false;
	}
	//---------------------------------------------------------------------------
	void ComboBox::Focus() {
		button_->Focus();
	}
	//---------------------------------------------------------------------------
	//ComboBox::ComboBoxButton
	//---------------------------------------------------------------------------
	void ComboBox::ComboBoxButton::SetSize(const Drawing::SizeI &size) {
		realSize_ = size;
		Button::SetSize(size.InflateEx(-24, 0));
	}
	//---------------------------------------------------------------------------
	bool ComboBox::ComboBoxButton::Intersect(const Drawing::PointI &point) const {
		return Intersection::TestRectangleI(absoluteLocation_, realSize_, point);
	}
	//---------------------------------------------------------------------------
	void ComboBox::ComboBoxButton::CalculateLabelLocation() {
		label_->SetLocation(Drawing::PointI(6, GetSize().Height / 2 - label_->GetSize().Height / 2));
	}
	//---------------------------------------------------------------------------
	bool ComboBox::ComboBoxButton::OnKeyDown(const KeyboardMessage &keyboard) {
		return Control::OnKeyDown(keyboard);
	}
	//---------------------------------------------------------------------------
	void ComboBox::ComboBoxButton::PopulateGeometry() {
		using namespace Drawing;

		Graphics g(*geometry_);

		auto color = isFocused_ || isInside_ ? GetBackColor() + GetMouseOverFocusColor() : GetBackColor();

		g.FillRectangle(color, PointI(0, 1), realSize_ - SizeI(0, 2));
		g.FillRectangle(color, PointI(1, 0), realSize_ - SizeI(2, 0));
		g.FillRectangleGradient(ColorRectangle(color, color - Color::FromARGB(0, 20, 20, 20)), PointI(1, 1), realSize_ - SizeI(2, 2));

		int arrowLeft = realSize_.Width - 9;
		int arrowTop = realSize_.Height - 11;
		for (int i = 0; i < 4; ++i) {
			g.FillRectangle(GetForeColor(), PointI(arrowLeft - i, arrowTop - i), SizeI(1 + i * 2, 1));
		}
	}
	//---------------------------------------------------------------------------
}
