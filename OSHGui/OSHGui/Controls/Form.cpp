/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "Form.hpp"
#include "../Application.hpp"
#include "../FormManager.hpp"
#include "../Misc/Exceptions.hpp"
#include "Label.hpp"
#include "Panel.hpp"

namespace OSHGui {
	//---------------------------------------------------------------------------
	//static attributes
	//---------------------------------------------------------------------------
	const Drawing::PointI Form::DefaultLocation(50, 50);
	const Drawing::SizeI Form::DefaultSize(300, 300);
	//---------------------------------------------------------------------------
	//Constructor
	//---------------------------------------------------------------------------
	Form::Form() : Form(true) {}
	Form::Form(bool create_caption) : Control(nullptr),
		isModal_(false),
		dialogResult_(DialogResult::None) {
		
		type_ = ControlType::Form;
		
		parent_ = nullptr;//this;
		isVisible_ = false;
		isEnabled_ = false;
		isFocusable_ = true;

		if (create_caption) {
			captionBar_ = new CaptionBar(this);
			captionBar_->SetLocation(Drawing::PointI(0, 0));
			controls_.push_back(captionBar_);
		} else {
			captionBar_ = nullptr;
		}

		containerPanel_ = new Panel(this);
		containerPanel_->SetLocation(Drawing::PointI(Padding, Padding + CaptionBar::DefaultCaptionBarHeight));
		containerPanel_->SetBackColor(Drawing::Color::Empty());
		controls_.push_back(containerPanel_);

		SetLocation(DefaultLocation);
		SetSize(DefaultSize);

		ApplyTheme(Application::Instance().GetTheme());
	}
	//---------------------------------------------------------------------------
	//Getter/Setter
	//---------------------------------------------------------------------------
	void Form::SetSize(const Drawing::SizeI &size) {
		Control::SetSize(size);

		if (captionBar_) captionBar_->SetSize(size);
		containerPanel_->SetSize(Drawing::SizeI(size.Width - Padding * 2, 
			size.Height - Padding * 2 - CaptionBar::DefaultCaptionBarHeight));
	}
	//---------------------------------------------------------------------------
	void Form::SetText(const Misc::UnicodeString &text) {
		if (captionBar_) captionBar_->SetText(text);
	}
	//---------------------------------------------------------------------------
	const Misc::UnicodeString& Form::GetText() const {
		if (captionBar_) {
			return captionBar_->GetText();
		} else {
			return L"";
		}
	}
	//---------------------------------------------------------------------------
	void Form::SetForeColor(const Drawing::Color &color) {
		Control::SetForeColor(color);
		if (captionBar_) captionBar_->SetForeColor(color);
	}
	//---------------------------------------------------------------------------
	void Form::SetDialogResult(DialogResult result) {
		dialogResult_ = result;
	}
	//---------------------------------------------------------------------------
	DialogResult Form::GetDialogResult() const {
		return dialogResult_;
	}
	//---------------------------------------------------------------------------
	FormClosingEvent& Form::GetFormClosingEvent() {
		return formClosingEvent_;
	}
	//---------------------------------------------------------------------------
	bool Form::IsModal() const {
		return isModal_;
	}
	//---------------------------------------------------------------------------
	const std::deque<Control*>& Form::GetControls() const {
		return containerPanel_->GetControls();
	}
	void Form::AddControl(Control *control) {
		containerPanel_->AddControl(control);
	}
	void Form::RemoveControl(Control *control) {
		containerPanel_->RemoveControl(control);
	}
	//---------------------------------------------------------------------------
	//Runtime-Functions
	//---------------------------------------------------------------------------
	void Form::Show(const std::shared_ptr<Form> &instance) {
		instance_ = std::weak_ptr<Form>(instance);
	
		Application::Instance().formManager_.RegisterForm(instance);

		isVisible_ = true;
		isEnabled_ = true;

		CalculateAbsoluteLocation();
	}
	//---------------------------------------------------------------------------
	void Form::ShowDialog(const std::shared_ptr<Form> &instance) {
		ShowDialog(instance, std::function<void()>());
	}
	//---------------------------------------------------------------------------
	void Form::ShowDialog(const std::shared_ptr<Form> &instance, const std::function<void()> &closeFunction) {
		isModal_ = true;

		instance_ = std::weak_ptr<Form>(instance);
	
		Application::Instance().formManager_.RegisterForm(instance_.lock(), closeFunction);

		isVisible_ = true;
		isEnabled_ = true;

		CalculateAbsoluteLocation();
	}
	//---------------------------------------------------------------------------
	void Form::Close() {
		bool canClose = true;
		formClosingEvent_.Invoke(this, canClose);
		if (canClose) {
			Application::Instance().formManager_.UnregisterForm(instance_.lock());
		}
	}
	//---------------------------------------------------------------------------
	void Form::PopulateGeometry() {
		using namespace Drawing;

		Graphics g(*geometry_);

		g.FillRectangle(GetBackColor() - Color::FromARGB(0, 100, 100, 100), RectangleI(PointI(), GetSize()));
		auto color = GetBackColor() - Color::FromARGB(0, 90, 90, 90);
		g.FillRectangleGradient(ColorRectangle(GetBackColor(), GetBackColor(), color, color), 
			RectangleI(PointI(1, 1), GetSize() - SizeI(2, 2)));
		g.FillRectangle(GetBackColor() - Color::FromARGB(0, 50, 50, 50), 
			RectangleI(PointI(5, (captionBar_ ? captionBar_->GetBottom() : 0) + 2),
			SizeI(GetWidth() - 10, 1)));
	}
	//---------------------------------------------------------------------------
	//Form::Captionbar::Button
	//---------------------------------------------------------------------------
	const Drawing::SizeI Form::CaptionBar::CaptionBarButton::DefaultSize(17, 17);
	const Drawing::PointI Form::CaptionBar::CaptionBarButton::DefaultCrossOffset(8, 6);
	//---------------------------------------------------------------------------
	Form::CaptionBar::CaptionBarButton::CaptionBarButton(Control* parent) : Control(parent) {
		isFocusable_ = false;
		SetSize(DefaultSize);
	}
	//---------------------------------------------------------------------------
	void Form::CaptionBar::CaptionBarButton::CalculateAbsoluteLocation() {
		Control::CalculateAbsoluteLocation();
		crossAbsoluteLocation_ = absoluteLocation_ + DefaultCrossOffset;
		geometry_->SetTranslation(crossAbsoluteLocation_.cast<float>());
	}
	//---------------------------------------------------------------------------
	void Form::CaptionBar::CaptionBarButton::OnMouseUp(const MouseMessage &mouse) {
		Control::OnMouseUp(mouse);

		auto owner = static_cast<Form*>(parent_->GetParent());
		owner->Close();
	}
	//---------------------------------------------------------------------------
	void Form::CaptionBar::CaptionBarButton::PopulateGeometry() {
		using namespace Drawing;

		Graphics g(*geometry_);

		auto color = GetParent()->GetForeColor();

		for (int i = 0; i < 4; ++i) {
			g.FillRectangle(color, PointI(i, i), SizeI(3, 1));
			g.FillRectangle(color, PointI(6 - i, i), SizeI(3, 1));
			g.FillRectangle(color, PointI(i, 7 - i), SizeI(3, 1));
			g.FillRectangle(color, PointI(6 - i, 7 - i), SizeI(3, 1));
		}
	}
	//---------------------------------------------------------------------------
	//Form::Captionbar
	//---------------------------------------------------------------------------
	const Drawing::PointI Form::CaptionBar::DefaultTitleOffset(4, 4);
	//---------------------------------------------------------------------------
	Form::CaptionBar::CaptionBar(Control* parent) : Control(parent) {
		isFocusable_ = false;
		drag_ = false;

		titleLabel_ = new Label(this);
		titleLabel_->SetLocation(DefaultTitleOffset);
		titleLabel_->SetBackColor(Drawing::Color::Empty());

		closeButton_ = new CaptionBarButton(this);
		closeButton_->SetBackColor(Drawing::Color::Empty());
	}
	//---------------------------------------------------------------------------
	void Form::CaptionBar::SetSize(const Drawing::SizeI &size) {
		Control::SetSize(Drawing::SizeI(size.Width, DefaultCaptionBarHeight));

		closeButton_->SetLocation(Drawing::PointI(size.Width - 
			CaptionBarButton::DefaultSize.Width - DefaultButtonPadding, 0));
	}
	//---------------------------------------------------------------------------
	void Form::CaptionBar::SetText(const Misc::UnicodeString &text) {
		titleLabel_->SetText(text);
	}
	//---------------------------------------------------------------------------
	const Misc::UnicodeString& Form::CaptionBar::GetText() const {
		return titleLabel_->GetText();
	}
	//---------------------------------------------------------------------------
	void Form::CaptionBar::SetForeColor(const Drawing::Color &color) {
		Control::SetForeColor(color);

		closeButton_->SetForeColor(color);
		titleLabel_->SetForeColor(color);
	}
	//---------------------------------------------------------------------------
	void Form::CaptionBar::OnMouseDown(const MouseMessage &mouse) {
		Control::OnMouseDown(mouse);

		drag_ = true;
		OnGotMouseCapture();
		dragStart_ = mouse.GetLocation();
	}
	//---------------------------------------------------------------------------
	void Form::CaptionBar::OnMouseMove(const MouseMessage &mouse) {
		Control::OnMouseMove(mouse);
		
		if (drag_) {
			GetParent()->SetLocation(GetParent()->GetLocation() + (mouse.GetLocation() - dragStart_));
			dragStart_ = mouse.GetLocation();
		}
	}
	//---------------------------------------------------------------------------
	void Form::CaptionBar::OnMouseUp(const MouseMessage &mouse) {
		Control::OnMouseUp(mouse);

		if (drag_) {
			drag_ = false;
			OnLostMouseCapture();
		}
	}
	//---------------------------------------------------------------------------
}
