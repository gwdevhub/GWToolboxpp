/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "TabControl.hpp"
#include "TabPage.hpp"
#include "Label.hpp"
#include "../Misc/TextHelper.hpp"
#include "../Misc/Exceptions.hpp"
#include <algorithm>

namespace OSHGui {
	//---------------------------------------------------------------------------
	//static attributes
	//---------------------------------------------------------------------------
	const Drawing::SizeI TabControl::DefaultSize(200, 200);
	//---------------------------------------------------------------------------
	//Constructor
	//---------------------------------------------------------------------------
	TabControl::TabControl(Control* parent) : Control(parent),
		startIndex_(0),
		maxIndex_(0),
		selected_(nullptr) {

		type_ = ControlType::TabControl;

		lastSwitchButton_ = new TabControlSwitchButton(this, TabControlSwitchButton::TabControlSwitchButtonDirection::Left);
		lastSwitchButton_->GetClickEvent() += ClickEventHandler([this](Control *control) {
			if (startIndex_ > 0) {
				--startIndex_;
				CalculateButtonLocationAndCount();
			}
		});
		AddControl(lastSwitchButton_);

		nextSwitchButton_ = new TabControlSwitchButton(this, TabControlSwitchButton::TabControlSwitchButtonDirection::Right);
		nextSwitchButton_->GetClickEvent() += ClickEventHandler([this](Control *control) {
			if (maxIndex_ < (int)bindings_.size()) {
				++startIndex_;
				CalculateButtonLocationAndCount();
			}
		});
		AddControl(nextSwitchButton_);

		SetSize(DefaultSize);
		
		ApplyTheme(Application::Instance().GetTheme());

		canRaiseEvents_ = false;
	}
	//---------------------------------------------------------------------------
	//Getter/Setter
	//---------------------------------------------------------------------------
	void TabControl::SetSize(const Drawing::SizeI &size) {
		Control::SetSize(size);

		CalculateButtonLocationAndCount();

		lastSwitchButton_->SetLocation(Drawing::PointI(GetWidth() - TabControlSwitchButton::DefaultSize.Width, 0));
		nextSwitchButton_->SetLocation(Drawing::PointI(GetWidth() - TabControlSwitchButton::DefaultSize.Width, 
			TabControlSwitchButton::DefaultSize.Height + 1));

		if (selected_ != nullptr) {
			auto tabPageSize = size.InflateEx(0, -selected_->Button->GetHeight());

			for (auto &binding : bindings_) {
				binding->TabPage->SetSize(tabPageSize);
			}
		}
	}
	//---------------------------------------------------------------------------
	void TabControl::SetForeColor(const Drawing::Color &color) {
		Control::SetForeColor(color);

		lastSwitchButton_->SetForeColor(color);
		nextSwitchButton_->SetForeColor(color);

		for (auto &binding : bindings_) {
			binding->Button->SetForeColor(color);
			binding->TabPage->SetForeColor(color);
		}
	}
	//---------------------------------------------------------------------------
	void TabControl::SetBackColor(const Drawing::Color &color) {
		Control::SetBackColor(color);

		lastSwitchButton_->SetBackColor(color);
		nextSwitchButton_->SetBackColor(color);

		for (auto &binding : bindings_) {
			binding->Button->SetBackColor(color);
			binding->TabPage->SetBackColor(color);
		}
	}
	//---------------------------------------------------------------------------
	TabPage* TabControl::GetTabPage(const Misc::UnicodeString &text) const {
		for (auto &binding : bindings_) {
			if (binding->TabPage->GetText() == text) {
				return binding->TabPage;
			}
		}

		return 0;
	}
	//---------------------------------------------------------------------------
	TabPage* TabControl::GetTabPage(int index) const {
		if (index > 0 && index < (int)bindings_.size()) {
			return bindings_[index]->TabPage;
		}

		return 0;
	}
	//---------------------------------------------------------------------------
	void TabControl::SetSelectedIndex(int index) {
		for (auto &binding : bindings_) {
			if (binding->Index == index) {
				SelectBinding(*binding);
				return;
			}
		}
	}
	//---------------------------------------------------------------------------
	int TabControl::GetSelectedIndex() const {
		return selected_->Index;
	}
	//---------------------------------------------------------------------------
	void TabControl::SetSelectedTabPage(TabPage *tabPage) {
		for (auto &binding : bindings_) {
			if (binding->TabPage == tabPage) {
				SelectBinding(*binding);
				return;
			}
		}
	}
	//---------------------------------------------------------------------------
	TabPage* TabControl::GetSelectedTabPage() const {
		return selected_->TabPage;
	}
	//---------------------------------------------------------------------------
	SelectedIndexChangedEvent& TabControl::GetSelectedIndexChangedEvent() {
		return selectedIndexChangedEvent_;
	}
	//---------------------------------------------------------------------------
	//Runtime-Functions
	//---------------------------------------------------------------------------
	void TabControl::AddTabPage(TabPage *tabPage) {
		if (tabPage == nullptr) {
			#ifndef OSHGUI_DONTUSEEXCEPTIONS
			throw Misc::ArgumentNullException("tabPage");
			#endif
			return;
		}

		for (auto &binding : bindings_) {
			if (binding->TabPage == tabPage)
			{
				return;
			}
		}

		std::unique_ptr<TabPageButtonBinding> binding(new TabPageButtonBinding());
		binding->Index = bindings_.size();
		binding->TabPage = tabPage;

		auto button = new TabControlButton(this, *binding);
		button->SetLocation(Drawing::PointI(0, 0));
		button->SetForeColor(GetForeColor());
		button->SetBackColor(GetBackColor());

		tabPage->SetSize(size_.InflateEx(0, -button->GetHeight()));

		tabPage->button_ = button;
		binding->Button = button;

		if (bindings_.empty()) {
			button->SetActive(true);
			tabPage->SetVisible(true);
			selected_ = binding.get();
			tabPage->SetLocation(Drawing::PointI(0, button->GetSize().Height));
		} else {
			tabPage->SetVisible(false);
		}

		bindings_.push_back(std::move(binding));

		CalculateButtonLocationAndCount();
	}
	//---------------------------------------------------------------------------
	void TabControl::RemoveTabPage(TabPage *tabPage) {
		if (tabPage == nullptr) {
			#ifndef OSHGUI_DONTUSEEXCEPTIONS
			throw Misc::ArgumentNullException("tabPage");
			#endif
			return;
		}

		for (auto &binding : bindings_) {
			if (binding->TabPage == tabPage) {
				delete binding->Button;
				binding->TabPage->button_ = nullptr;

				bindings_.erase(std::remove(std::begin(bindings_), std::end(bindings_), binding), std::end(bindings_));

				if (selected_->TabPage == tabPage) {
					if (!bindings_.empty()) {
						selected_ = bindings_.front().get();
						selected_->Button->SetActive(true);
					} else {
						selected_->Index = -1;
						selected_->TabPage = nullptr;
						selected_->Button = nullptr;
					}
				}

				break;
			}
		}

		CalculateButtonLocationAndCount();
	}
	//---------------------------------------------------------------------------
	void TabControl::CalculateAbsoluteLocation() {
		Control::CalculateAbsoluteLocation();

		CalculateButtonLocationAndCount();
	}
	//---------------------------------------------------------------------------
	void TabControl::CalculateButtonLocationAndCount() {
		if (!bindings_.empty()) {
			maxIndex_ = startIndex_;

			int tempWidth = 0;
			int maxWidth = size_.Width - TabControlSwitchButton::DefaultSize.Width;
			for (auto &binding : bindings_) {
				auto button = binding->Button;
				button->SetVisible(false);

				if (tempWidth + button->GetSize().Width <= maxWidth) {
					button->SetLocation(Drawing::PointI(tempWidth, 0));
					button->SetVisible(true);

					++maxIndex_;
					tempWidth += button->GetSize().Width + 2;
				} else {
					break;
				}
			}

			if (selected_ != nullptr) {
				selected_->TabPage->SetLocation(Drawing::PointI(0, selected_->Button->GetSize().Height));
			}

			if (startIndex_ != 0) {
				lastSwitchButton_->SetVisible(true);
			} else {
				lastSwitchButton_->SetVisible(false);
			}
			if (maxIndex_ < (int)bindings_.size()) {
				nextSwitchButton_->SetVisible(true);
			} else {
				nextSwitchButton_->SetVisible(false);
			}

			Invalidate();
		}
	}
	//---------------------------------------------------------------------------
	void TabControl::SelectBinding(TabPageButtonBinding &binding) {
		selected_->TabPage->SetVisible(false);
		selected_->Button->SetActive(false);
		selected_ = &binding;
		selected_->Button->SetActive(true);
		selected_->TabPage->SetVisible(true);
		CalculateButtonLocationAndCount();

		selectedIndexChangedEvent_.Invoke(this);

		Invalidate();
	}
	//---------------------------------------------------------------------------
	void TabControl::ApplyTheme(const Drawing::Theme &theme) {
		Control::ApplyTheme(theme);
		
		for (auto &binding : bindings_) {
			binding->TabPage->ApplyTheme(theme);
		}
	}
	//---------------------------------------------------------------------------
	//TabControl::TabControlButton
	//---------------------------------------------------------------------------
	const Drawing::PointI TabControl::TabControlButton::DefaultLabelOffset(4, 2);
	//---------------------------------------------------------------------------
	TabControl::TabControlButton::TabControlButton(Control* parent, TabPageButtonBinding &binding) : Control(parent),
		binding_(binding),
		label_(new Label(this)) {
		active_ = false;

		label_->SetLocation(DefaultLabelOffset);
		label_->SetText(binding.TabPage->GetText());
		label_->SetBackColor(Drawing::Color::Empty());

		size_ = label_->GetSize().InflateEx(DefaultLabelOffset.Left * 2, DefaultLabelOffset.Top * 2);
	}
	//---------------------------------------------------------------------------
	void TabControl::TabControlButton::SetForeColor(const Drawing::Color &color) {
		Control::SetForeColor(color);

		label_->SetForeColor(color);
	}
	//---------------------------------------------------------------------------
	void TabControl::TabControlButton::SetText(const Misc::UnicodeString &text) {
		label_->SetText(text);

		size_ = label_->GetSize().InflateEx(DefaultLabelOffset.Left * 2, DefaultLabelOffset.Top * 2);
	}
	//---------------------------------------------------------------------------
	void TabControl::TabControlButton::SetActive(bool active) {
		active_ = active;

		Invalidate();
	}
	//---------------------------------------------------------------------------
	void TabControl::TabControlButton::OnMouseClick(const MouseMessage &mouse) {
		Control::OnMouseClick(mouse);

		if (!active_) {
			if (parent_ != nullptr) {
				static_cast<TabControl*>(parent_)->SetSelectedIndex(binding_.Index);
			}
		}
	}
	//---------------------------------------------------------------------------
	void TabControl::TabControlButton::PopulateGeometry() {
		using namespace Drawing;

		Graphics g(*geometry_);

		if (active_) {
			g.FillRectangleGradient(ColorRectangle(GetBackColor() + Color::FromARGB(0, 43, 43, 43), 
				GetBackColor() - Color::FromARGB(0, 10, 10, 10)), PointI(0, 0), GetSize());
			g.FillRectangleGradient(ColorRectangle(GetBackColor(), 
				GetBackColor() - Color::FromARGB(0, 42, 42, 42)), PointI(1, 1), GetSize() - SizeI(2, 0));
		} else {
			auto backInactive = (isInside_ ? GetBackColor() + Color::FromARGB(0, 50, 50, 50) : GetBackColor()) - Color::FromARGB(0, 47, 47, 47);

			g.FillRectangle(backInactive + Color::FromARGB(0, 9, 9, 9), PointI(0, 0), GetSize());
			g.FillRectangleGradient(ColorRectangle(backInactive, 
				backInactive - Color::FromARGB(0, 20, 20, 20)), PointI(1, 1), GetSize() - SizeI(2, 1));
		}
	}
	//---------------------------------------------------------------------------
	//TabControl::TabControlSwitchButton
	//---------------------------------------------------------------------------
	const Drawing::SizeI TabControl::TabControlSwitchButton::DefaultSize(9, 9);
	//---------------------------------------------------------------------------
	TabControl::TabControlSwitchButton::TabControlSwitchButton(Control* parent, 
		TabControlSwitchButtonDirection direction) : Control(parent),
		direction_(direction) {

		SetSize(DefaultSize);
	}
	//---------------------------------------------------------------------------
	void TabControl::TabControlSwitchButton::PopulateGeometry() {
		using namespace Drawing;

		Graphics g(*geometry_);

		auto base = isInside_ ? GetBackColor() : GetBackColor() - Color::FromARGB(0, 47, 47, 47);
		auto borderColor = GetBackColor() + Color::FromARGB(0, 9, 9, 9);

		g.FillRectangle(borderColor, PointI(0, 0), GetSize());
		g.FillRectangle(base, PointI(1, 1), GetSize() - SizeI(2, 2));

		if (direction_ == TabControlSwitchButtonDirection::Left) {
			for (int i = 0; i < 3; ++i) {
				g.FillRectangle(GetForeColor(), PointI(3 + i, 4 - i), SizeI(1, 1 + i * 2));
			}
		} else {
			for (int i = 0; i < 3; ++i) {
				g.FillRectangle(GetForeColor(), PointI(3 + i, 2 + i), SizeI(1, 5 - i * 2));
			}
		}
	}
	//---------------------------------------------------------------------------
}
