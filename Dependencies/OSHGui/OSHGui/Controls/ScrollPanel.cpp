/*
* OldSchoolHack GUI
*
* by KN4CK3R http://www.oldschoolhack.me
*
* See license in OSHGui.hpp
*/

#include "ScrollPanel.hpp"

namespace OSHGui {
	//---------------------------------------------------------------------------
	//static attributes
	//---------------------------------------------------------------------------
	const Drawing::SizeI ScrollPanel::DefaultSize(200, 200);
	//---------------------------------------------------------------------------
	//Constructor
	//---------------------------------------------------------------------------
	ScrollPanel::ScrollPanel(Control* parent) : Control(parent),
	container_(new Panel(this)),
	scrollBar_(new ScrollBar(this)),
	offset_(0) {

		type_ = ControlType::ScrollPanel;

		scrollBar_->SetDeltaFactor(10); // pixes per mouse delta
		scrollBar_->GetScrollEvent() += ScrollEventHandler([this](Control*, ScrollEventArgs &args) {
			SetOffset(args.NewValue);
		});

		container_->SetLocation(Drawing::PointI(0, 0));

		Control::AddControl(scrollBar_);
		Control::AddControl(container_);

		SetSize(DefaultSize);

		ApplyTheme(Application::Instance().GetTheme());

		isFocusable_ = false;
		canRaiseEvents_ = false;
	}
	//---------------------------------------------------------------------------
	void ScrollPanel::SetSize(const Drawing::SizeI &size) {
		size_ = size;

		container_->SetWidth(size.Width - scrollBar_->GetWidth());

		scrollBar_->SetLocation(Drawing::PointI(size.Width - scrollBar_->GetWidth() - 1, 0));
		scrollBar_->SetHeight(size.Height);

		UpdateScrollBar();

		OnSizeChanged();
	}
	//---------------------------------------------------------------------------
	void ScrollPanel::SetInternalHeight(int height) {
		container_->SetHeight(height);

		UpdateScrollBar();
	}
	//---------------------------------------------------------------------------
	void ScrollPanel::SetOffset(int offset) {
		offset_ = offset;
		container_->SetTop(-offset);
		Invalidate();
	}
	void ScrollPanel::UpdateScrollBar() {
		int hidden = container_->GetHeight() - GetHeight();
		scrollBar_->SetMaximum(std::max(hidden, 0));
	}
	//---------------------------------------------------------------------------
	const std::deque<Control*>& ScrollPanel::GetControls() const {
		return container_->GetControls();
	}
	//---------------------------------------------------------------------------
	void ScrollPanel::AddControl(Control *control) {
		container_->AddControl(control);
		if (autoSize_ && control->GetBottom() + Padding > container_->GetHeight()) {
			SetInternalHeight(control->GetBottom() + Padding);
		}
	}
	//---------------------------------------------------------------------------
	void ScrollPanel::RemoveControl(Control *control) {
		container_->RemoveControl(control);
	}
	//---------------------------------------------------------------------------
	bool ScrollPanel::ProcessMouseScroll(const MouseMessage &mouse) {
		if (!isEnabled_) return false;
		if (!isVisible_) return false;
		if (!Intersect(mouse.GetLocation())) return false;

		scrollBar_->InjectDelta(mouse.GetDelta());

		return true;
	}
}
