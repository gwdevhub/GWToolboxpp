/*
 * OldSchoolHack GUI
 *
 * Copyright (c) 2013 KN4CK3R http://www.oldschoolhack.de
 *
 * See license in OSHGui.hpp
 */

#include "Control.hpp"
#include "../Misc/Exceptions.hpp"
#include "../Drawing/FontManager.hpp"
#include "../Drawing/Vector.hpp"
#include "../Misc/ReverseIterator.hpp"
#include "../Misc/Intersection.hpp"
#include <algorithm>

namespace OSHGui {
	//---------------------------------------------------------------------------
	//Constructor
	//---------------------------------------------------------------------------
	Control::Control(Control* parent)
		: type_((ControlType)0),
		  parent_(parent),
		  location_(Padding, Padding),
		  size_(0, 0),
		  anchor_(AnchorStyles::Top | AnchorStyles::Left),
		  isEnabled_(true),
		  isVisible_(true),
		  clip_(Clipping::OnBounds),
		  isFocused_(false),
		  isPressed_(false),
		  isInside_(false),
		  isFocusable_(true),
		  hasCaptured_(false),
		  autoSize_(false),
		  canRaiseEvents_(true),
		  needsRedraw_(true),
		  cursor_(nullptr),
		  mouseOverFocusColor_(Drawing::Color::FromARGB(0, 20, 20, 20)),
		  geometry_(Application::Instance().GetRenderer().CreateGeometryBuffer()) {
	}
	//---------------------------------------------------------------------------
	Control::~Control() {
		if (isInside_) {
			Application::Instance().MouseEnteredControl = nullptr;
		}
		if (isFocused_) {
			Application::Instance().FocusedControl = nullptr;
		}
		for (Control* control : controls_) {
			delete control;
		}
	}
	//---------------------------------------------------------------------------
	//Getter/Setter
	//---------------------------------------------------------------------------
	ControlType Control::GetType() const {
		return type_;
	}
	//---------------------------------------------------------------------------
	bool Control::GetIsFocused() const {
		return isFocused_;
	}
	//---------------------------------------------------------------------------
	void Control::SetEnabled(bool isEnabled) {
		isEnabled_ = isEnabled;
		if (isEnabled == false && isFocused_) {
			OnLostFocus(nullptr);
		}

		Invalidate();
	}
	//---------------------------------------------------------------------------
	bool Control::GetEnabled() const {
		return isEnabled_;
	}
	//---------------------------------------------------------------------------
	void Control::SetVisible(bool isVisible) {
		isVisible_ = isVisible;
		if (isVisible == false && isFocused_) {
			OnLostFocus(nullptr);
		}
		Invalidate();
	}
	//---------------------------------------------------------------------------
	bool Control::GetVisible() const {
		return isVisible_;
	}
	//---------------------------------------------------------------------------
	void Control::SetClip(Clipping clip) {
		clip_ = clip;
		Invalidate();
	}
	//---------------------------------------------------------------------------
	Clipping Control::GetClip() const {
		return clip_;
	}
	//---------------------------------------------------------------------------
	void Control::SetAutoSize(bool autoSize) {
		autoSize_ = autoSize;
		Invalidate();
	}
	//---------------------------------------------------------------------------
	bool Control::GetAutoSize() const {
		return autoSize_;
	}
	//---------------------------------------------------------------------------
	void Control::SetBounds(int x, int y, int w, int h) {
		SetBounds(Drawing::PointI(x, y), Drawing::SizeI(w, h));
	}
	//---------------------------------------------------------------------------
	void Control::SetBounds(const Drawing::PointI &location, const Drawing::SizeI &size) {
		SetLocation(location);
		SetSize(size);
	}
	//---------------------------------------------------------------------------
	void Control::SetBounds(const Drawing::RectangleI &bounds) {
		SetBounds(bounds.GetLocation(), bounds.GetSize());
	}
	//---------------------------------------------------------------------------
	const Drawing::RectangleI Control::GetBounds() const {
		return Drawing::RectangleI(location_, size_);
	}
	//---------------------------------------------------------------------------
	void Control::SetLocation(const Drawing::PointI &location) {
		location_ = location;
		OnLocationChanged();
	}
	//---------------------------------------------------------------------------
	const Drawing::PointI& Control::GetLocation() const {
		return location_;
	}
	//---------------------------------------------------------------------------
	void Control::SetSize(const Drawing::SizeI &size) {
		#ifndef OSHGUI_DONTUSEEXCEPTIONS
		if (size.Width < 0) {
			throw Misc::ArgumentOutOfRangeException("width");
		}
		if (size.Height < 0) {
			throw Misc::ArgumentOutOfRangeException("height");
		}
		#endif

		size_ = size;

		auto offset = size_ - GetSize();
		for (Control* control : controls_) {
			AnchorStyles anchor = control->GetAnchor();

			if (anchor != (AnchorStyles::Top | AnchorStyles::Left)) {
				if (anchor == (AnchorStyles::Top | AnchorStyles::Left | AnchorStyles::Bottom | AnchorStyles::Right)) {
					control->SetSize(control->GetSize() + offset);
				} else if (anchor == (AnchorStyles::Top | AnchorStyles::Left | AnchorStyles::Right) || anchor == (AnchorStyles::Bottom | AnchorStyles::Left | AnchorStyles::Right)) {
					control->SetLocation(control->GetLocation() + Drawing::PointI(0, offset.Height));
					control->SetSize(control->GetSize() + Drawing::SizeI(offset.Width, 0));
				} else if (anchor == (AnchorStyles::Top | AnchorStyles::Right) || anchor == (AnchorStyles::Bottom | AnchorStyles::Right)) {
					control->SetLocation(control->GetLocation() + Drawing::PointI(offset.Width, offset.Height));
				}
			}
		}
		
		OnSizeChanged();

		Invalidate();
	}
	//---------------------------------------------------------------------------
	const Drawing::SizeI& Control::GetSize() const {
		return size_;
	}
	//---------------------------------------------------------------------------
	int Control::GetX() const {
		return location_.X;
	}
	//---------------------------------------------------------------------------
	int Control::GetLeft() const {
		return location_.Left;
	}
	//---------------------------------------------------------------------------
	void Control::SetX(int x) {
		SetLocation(Drawing::PointI(x, location_.Y));
	}
	//---------------------------------------------------------------------------
	void Control::SetLeft(int left) {
		SetLocation(Drawing::PointI(left, location_.Y));
	}
	//---------------------------------------------------------------------------
	int Control::GetY() const {
		return location_.Y;
	}
	//---------------------------------------------------------------------------
	int Control::GetTop() const {
		return location_.Top;
	}
	//---------------------------------------------------------------------------
	void Control::SetY(int y) {
		SetLocation(Drawing::PointI(location_.X, y));
	}
	//---------------------------------------------------------------------------
	void Control::SetTop(int top) {
		SetLocation(Drawing::PointI(location_.X, top));
	}
	//---------------------------------------------------------------------------
	int Control::GetRight() const {
		return location_.Left + size_.Width;
	}
	//---------------------------------------------------------------------------
	int Control::GetBottom() const {
		return location_.Top + size_.Height;
	}
	//---------------------------------------------------------------------------
	int Control::GetWidth() const {
		return size_.Width;
	}
	//---------------------------------------------------------------------------
	int Control::GetHeight() const {
		return size_.Height;
	}
	//---------------------------------------------------------------------------
	void Control::SetWidth(int width) {
		SetSize(Drawing::SizeI(width, size_.Height));
	}
	//---------------------------------------------------------------------------
	void Control::SetHeight(int height) {
		SetSize(Drawing::SizeI(size_.Width, height));
	}
	//---------------------------------------------------------------------------
	void Control::SetAnchor(AnchorStyles anchor) {
		anchor_ = anchor;
	}
	//---------------------------------------------------------------------------
	AnchorStyles Control::GetAnchor() const {
		return anchor_;
	}
	//---------------------------------------------------------------------------
	void Control::SetTag(const Misc::Any &tag) {
		tag_ = tag;
	}
	//---------------------------------------------------------------------------
	const Misc::Any& Control::GetTag() const {
		return tag_;
	}
	//---------------------------------------------------------------------------
	void Control::SetName(const Misc::UnicodeString &name) {
		name_ = name;
	}
	//---------------------------------------------------------------------------
	const Misc::UnicodeString& Control::GetName() const {
		return name_;
	}
	//---------------------------------------------------------------------------
	void Control::SetForeColor(const Drawing::Color &color) {
		foreColor_ = color;

		Invalidate();
	}
	//---------------------------------------------------------------------------
	const Drawing::Color& Control::GetForeColor() const {
		return foreColor_;
	}
	//---------------------------------------------------------------------------
	void Control::SetBackColor(const Drawing::Color &color) {
		backColor_ = color;

		Invalidate();
	}
	//---------------------------------------------------------------------------
	const Drawing::Color& Control::GetBackColor() const {
		return backColor_;
	}
	//---------------------------------------------------------------------------
	void Control::SetMouseOverFocusColor(const Drawing::Color &color) {
		mouseOverFocusColor_ = color;

		Invalidate();
	}
	//---------------------------------------------------------------------------
	const Drawing::Color& Control::GetMouseOverFocusColor() const {
		return mouseOverFocusColor_;
	}
	//---------------------------------------------------------------------------
	void Control::SetFont(const Drawing::FontPtr &font) {
		#ifndef OSHGUI_DONTUSEEXCEPTIONS
		if (font == nullptr) {
			throw Misc::ArgumentNullException("font");
		}
		#endif
		
		font_ = font;

		Invalidate();
	}
	//---------------------------------------------------------------------------
	const Drawing::FontPtr& Control::GetFont() const {
		if (font_) return font_;
		if (parent_) return parent_->GetFont();
		return Application::Instance().GetDefaultFont();
	}
	//---------------------------------------------------------------------------
	void Control::SetCursor(const CursorPtr &cursor) {
		cursor_ = std::move(cursor);
	}
	//---------------------------------------------------------------------------
	const CursorPtr& Control::GetCursor() const {
		return cursor_ ? cursor_ : GetParent() ? GetParent()->GetCursor() : Cursors::Get(Cursors::Default);
	}
	//---------------------------------------------------------------------------
	LocationChangedEvent& Control::GetLocationChangedEvent() {
		return locationChangedEvent_;
	}
	//---------------------------------------------------------------------------
	SizeChangedEvent& Control::GetSizeChangedEvent() {
		return sizeChangedEvent_;
	}
	//---------------------------------------------------------------------------
	ClickEvent& Control::GetClickEvent() {
		return clickEvent_;
	}
	//---------------------------------------------------------------------------
	MouseDownEvent& Control::GetMouseDownEvent() {
		return mouseDownEvent_;
	}
	//---------------------------------------------------------------------------
	MouseUpEvent& Control::GetMouseUpEvent() {
		return mouseUpEvent_;
	}
	//---------------------------------------------------------------------------
	MouseMoveEvent& Control::GetMouseMoveEvent() {
		return mouseMoveEvent_;
	}
	//---------------------------------------------------------------------------
	MouseScrollEvent& Control::GetMouseScrollEvent() {
		return mouseScrollEvent_;
	}
	//---------------------------------------------------------------------------
	MouseEnterEvent& Control::GetMouseEnterEvent() {
		return mouseEnterEvent_;
	}
	//---------------------------------------------------------------------------
	MouseLeaveEvent& Control::GetMouseLeaveEvent() {
		return mouseLeaveEvent_;
	}
	//---------------------------------------------------------------------------
	KeyDownEvent& Control::GetKeyDownEvent() {
		return keyDownEvent_;
	}
	//---------------------------------------------------------------------------
	KeyPressEvent& Control::GetKeyPressEvent() {
		return keyPressEvent_;
	}
	//---------------------------------------------------------------------------
	KeyUpEvent& Control::GetKeyUpEvent() {
		return keyUpEvent_;
	}
	//---------------------------------------------------------------------------
	FocusGotEvent& Control::GetFocusGotEvent() {
		return focusGotEvent_;
	}
	//---------------------------------------------------------------------------
	FocusLostEvent& Control::GetFocusLostEvent() {
		return focusLostEvent_;
	}
	//---------------------------------------------------------------------------
	Control* Control::GetParent() const {
		return parent_;
	}
	//---------------------------------------------------------------------------
	//Runtime-Functions
	//---------------------------------------------------------------------------
	void Control::Focus() {
		if (isFocusable_ && !isFocused_) {
			OnGotFocus(this);
		}
	}
	//---------------------------------------------------------------------------
	bool Control::Intersect(const Drawing::PointI &point) const {
		return Intersection::TestRectangleI(absoluteLocation_, size_, point);
	}
	//---------------------------------------------------------------------------
	Drawing::PointI Control::PointToClient(const Drawing::PointI &point) const {
		return point - absoluteLocation_;
	}
	//---------------------------------------------------------------------------
	Drawing::PointI Control::PointToScreen(const Drawing::PointI &point) const {
		#ifndef OSHGUI_DONTUSEEXCEPTIONS
		if (!parent_) {
			throw Misc::ArgumentNullException("parent");
		}
		#endif
		
		if (parent_ != this) {
			return parent_->PointToScreen(point + location_);
		}

		return point + location_;
	}
	//---------------------------------------------------------------------------
	void Control::CalculateAbsoluteLocation() {
		if (parent_ != nullptr) {
			absoluteLocation_ = parent_->absoluteLocation_ + location_;
		} else {
			absoluteLocation_ = location_;
		}

		for (Control* control : controls_) control->CalculateAbsoluteLocation();

		geometry_->SetTranslation(absoluteLocation_.cast<float>());

		Invalidate();
	}
	//---------------------------------------------------------------------------
	void Control::InjectTime(const Misc::DateTime &time) {
		for (Control* control : controls_) control->InjectTime(time);
	}
	//---------------------------------------------------------------------------
	void Control::Invalidate() {
		needsRedraw_ = true;
		Application::Instance().Invalidate();
	}
	//---------------------------------------------------------------------------
	void Control::Render(Drawing::RenderContext &context) {
		if (!isVisible_) return;
		Drawing::RectangleI clipbackup = context.Clip;
		Drawing::RectangleI clipnew;
		switch (clip_) {
		case OSHGui::Clipping::OnBounds:
			clipnew = context.Clip.GetIntersection(Drawing::RectangleI(absoluteLocation_, size_));
			break;
		case OSHGui::Clipping::OnParent:
			clipnew = context.Clip;
			break;
		case OSHGui::Clipping::OnControl:
			clipnew = Drawing::RectangleI(absoluteLocation_, size_);
			break;
		case OSHGui::Clipping::None:
			clipnew = Drawing::RectangleI(Drawing::PointI(0, 0), Application::Instance().GetRenderer().GetDisplaySize());
			break;
		}
		
		context.Clip = clipnew;
		DrawSelf(context);

		// render from last to first, to draw first on top
		for (auto it = controls_.rbegin(); it != controls_.rend(); ++it) {
			context.Clip = clipnew;
			(*it)->Render(context);
		}

		context.Clip = clipbackup;
	}
	//---------------------------------------------------------------------------
	void Control::DrawSelf(Drawing::RenderContext &context) {
		BufferGeometry(context);
		QueueGeometry(context);
	}
	//---------------------------------------------------------------------------
	void Control::BufferGeometry(Drawing::RenderContext &context) {
		if (needsRedraw_) {
			geometry_->Reset();
			geometry_->SetClippingRegion(context.Clip);
			geometry_->SetClippingActive(true);
			PopulateGeometry();
			needsRedraw_ = false;
		}
	}
	//---------------------------------------------------------------------------
	void Control::QueueGeometry(Drawing::RenderContext &context) {
		context.Surface->AddGeometry(context.QueueType, geometry_);
	}
	//---------------------------------------------------------------------------
	void Control::PopulateGeometry() {
	}
	//---------------------------------------------------------------------------
	void Control::ApplyTheme(const Drawing::Theme &theme) {
		auto &controlTheme = theme.GetControlColorTheme(ControlTypeToString(type_));
		SetForeColor(controlTheme.ForeColor);
		SetBackColor(controlTheme.BackColor);
	}
	//---------------------------------------------------------------------------
	const std::deque<Control*>& Control::GetControls() const {
		return controls_;
	}
	//---------------------------------------------------------------------------
	void Control::AddControl(Control *control) {
		controls_.push_back(control);
	}
	//---------------------------------------------------------------------------
	void Control::RemoveControl(Control *control) {
		if (control != nullptr) {
			controls_.erase(std::remove(controls_.begin(),
				controls_.end(), control), controls_.end());
			Invalidate();
		}
	}
	//---------------------------------------------------------------------------
	Misc::AnsiString Control::ControlTypeToString(ControlType controlType) {
		switch (controlType) {
			case ControlType::Panel: return "panel";
			case ControlType::Form: return "form";
			case ControlType::GroupBox: return "groupbox";
			case ControlType::Label: return "label";
			case ControlType::LinkLabel: return "linklabel";
			case ControlType::Button: return "button";
			case ControlType::CheckBox: return "checkbox";
			case ControlType::RadioButton: return "radiobutton";
			case ControlType::ScrollBar: return "scrollbar";
			case ControlType::ListBox: return "listbox";
			case ControlType::ProgressBar: return "progressbar";
			case ControlType::TrackBar: return "trackbar";
			case ControlType::ComboBox: return "combobox";
			case ControlType::TextBox: return "textbox";
			case ControlType::Timer: return "timer";
			case ControlType::TabControl: return "tabcontrol";
			case ControlType::TabPage: return "tabpage";
			case ControlType::PictureBox: return "picturebox";
			case ControlType::ColorPicker: return "colorpicker";
			case ControlType::ColorBar: return "colorbar";
			case ControlType::HotkeyControl: return "hotkeycontrol";
			case ControlType::ScrollPanel: return "scrollpanel";
		}
		throw Misc::Exception();
	}
	//---------------------------------------------------------------------------
	//Event-Handling
	//---------------------------------------------------------------------------
	void Control::OnLocationChanged() {
		CalculateAbsoluteLocation();
		Invalidate();
		locationChangedEvent_.Invoke(this);
	}
	//---------------------------------------------------------------------------
	void Control::OnSizeChanged() {
		CalculateAbsoluteLocation();
		Invalidate();
		sizeChangedEvent_.Invoke(this);
	}
	//---------------------------------------------------------------------------
	void Control::OnMouseDown(const MouseMessage &mouse) {
		isPressed_ = true;

		Invalidate();

		MouseEventArgs args(mouse);
		args.Location -= absoluteLocation_;
		mouseDownEvent_.Invoke(this, args);
	}
	//---------------------------------------------------------------------------
	void Control::OnMouseClick(const MouseMessage &mouse) {
		MouseEventArgs args(mouse);
		args.Location -= absoluteLocation_;
		clickEvent_.Invoke(this);
	}
	//---------------------------------------------------------------------------
	void Control::OnMouseUp(const MouseMessage &mouse) {
		isPressed_ = false;

		Invalidate();

		MouseEventArgs args(mouse);
		args.Location -= absoluteLocation_;
		mouseUpEvent_.Invoke(this, args);
	}
	//---------------------------------------------------------------------------
	void Control::OnMouseMove(const MouseMessage &mouse) {
		MouseEventArgs args(mouse);
		args.Location -= absoluteLocation_;
		mouseMoveEvent_.Invoke(this, args);
	}
	//---------------------------------------------------------------------------
	bool Control::OnMouseScroll(const MouseMessage &mouse) {
		MouseEventArgs args(mouse);
		args.Location -= absoluteLocation_;
		mouseScrollEvent_.Invoke(this, args);
		return args.Handled;
	}
	//---------------------------------------------------------------------------
	void Control::OnMouseEnter(const MouseMessage &mouse) {
		isInside_ = true;

		auto &app = Application::Instance();

		if (app.MouseEnteredControl != nullptr && app.MouseEnteredControl->isInside_) {
			app.MouseEnteredControl->OnMouseLeave(mouse);
		}
		app.MouseEnteredControl = this;

		mouseEnterEvent_.Invoke(this);

		Invalidate();

		app.SetCursor(GetCursor());
	}
	//---------------------------------------------------------------------------
	void Control::OnMouseLeave(const MouseMessage &mouse) {
		isInside_ = false;

		Application::Instance().MouseEnteredControl = nullptr;

		mouseLeaveEvent_.Invoke(this);

		Invalidate();
	}
	//---------------------------------------------------------------------------
	void Control::OnGotMouseCapture() {
		Application &app = Application::Instance();
		if (app.CaptureControl != nullptr) {
			app.CaptureControl->OnLostMouseCapture();
		}
		app.CaptureControl = this;
		hasCaptured_ = true;

		mouseCaptureChangedEvent_.Invoke(this);

		Invalidate();
	}
	//---------------------------------------------------------------------------
	void Control::OnLostMouseCapture() {
		hasCaptured_ = false;

		Application &app = Application::Instance();
		if (app.CaptureControl == this) app.CaptureControl = nullptr;

		mouseCaptureChangedEvent_.Invoke(this);

		Invalidate();
	}
	//---------------------------------------------------------------------------
	void Control::OnGotFocus(Control *newFocusedControl) {
		auto &app = Application::Instance();
		if (newFocusedControl != app.FocusedControl) {
			if (app.FocusedControl != nullptr) {
				app.FocusedControl->OnLostFocus(newFocusedControl);
			}

			app.FocusedControl = newFocusedControl;
			isFocused_ = true;

			focusGotEvent_.Invoke(this);

			Invalidate();
		}
	}
	//---------------------------------------------------------------------------
	void Control::OnLostFocus(Control *newFocusedControl) {
		isFocused_ = isPressed_ = false;
		
		Application::Instance().FocusedControl = nullptr;

		focusLostEvent_.Invoke(this, newFocusedControl);

		Invalidate();
	}
	//---------------------------------------------------------------------------
	bool Control::OnKeyDown(const KeyboardMessage &keyboard) {
		KeyEventArgs args(keyboard);
		keyDownEvent_.Invoke(this, args);

		return args.Handled;
	}
	//---------------------------------------------------------------------------
	bool Control::OnKeyPress(const KeyboardMessage &keyboard) {
		KeyPressEventArgs args(keyboard);
		keyPressEvent_.Invoke(this, args);

		return args.Handled;
	}
	//---------------------------------------------------------------------------
	bool Control::OnKeyUp(const KeyboardMessage &keyboard) {
		KeyEventArgs args(keyboard);
		keyUpEvent_.Invoke(this, args);

		return args.Handled;
	}
	//---------------------------------------------------------------------------
	bool Control::ProcessMouseDown(const MouseMessage &mouse) {
		if (!isEnabled_) return false;
		if (!isVisible_) return false;
		if (!Intersect(mouse.GetLocation())) return false;

		if (canRaiseEvents_	&& mouse.GetButton() == MouseButton::Left && !isPressed_) {
			if (isFocusable_ && !isFocused_) {
				OnGotFocus(this);
			}
			OnMouseDown(mouse);
			return true;
		}

		for (Control* control : controls_) {
			if (control->ProcessMouseDown(mouse)) return true;
		}

		return true; // click was inside after all
	}
	//---------------------------------------------------------------------------
	bool Control::ProcessMouseUp(const MouseMessage &mouse) {
		if (!isEnabled_) return false;
		if (!isVisible_) return false;
		if (!hasCaptured_ && !Intersect(mouse.GetLocation())) return false;

		if (canRaiseEvents_ && (isPressed_ || hasCaptured_)) {
			if (isPressed_ && mouse.GetButton() == MouseButton::Left) {
				OnMouseClick(mouse);
			}
			OnMouseUp(mouse);
			return true;
		}

		for (Control* control : controls_) {
			if (control->ProcessMouseUp(mouse)) return true;
		}

		return true; // click was inside after all
	}
	//---------------------------------------------------------------------------
	bool Control::ProcessMouseMove(const MouseMessage &mouse) {
		if (!isEnabled_) return false;
		if (!isVisible_) return false;
		if (!hasCaptured_ && !Intersect(mouse.GetLocation())) return false;

		if (canRaiseEvents_) {
			if (!isInside_) {
				OnMouseEnter(mouse);
			}
			OnMouseMove(mouse);
			return true;
		}

		for (Control* control : controls_) {
			if (control->ProcessMouseMove(mouse)) return true;
		}
		return true;
	}
	//---------------------------------------------------------------------------
	bool Control::ProcessMouseScroll(const MouseMessage &mouse) {
		if (!isEnabled_) return false;
		if (!isVisible_) return false;
		if (!hasCaptured_ && !Intersect(mouse.GetLocation())) return false;

		if (canRaiseEvents_) {
			OnMouseScroll(mouse);
			return true;
		}

		for (Control* control : controls_) {
			if (control->ProcessMouseScroll(mouse)) return true;
		}

		return true;
	}
	bool Control::ProcessMouseMessage(const MouseMessage &mouse) {
		switch (mouse.GetState()) {
			case MouseState::Down: return ProcessMouseDown(mouse);
			case MouseState::Up: return ProcessMouseUp(mouse);
			case MouseState::Move: return ProcessMouseMove(mouse);
			case MouseState::Scroll: return ProcessMouseScroll(mouse);
		}
		return false;
	}
	//---------------------------------------------------------------------------
	bool Control::ProcessKeyboardMessage(const KeyboardMessage &keyboard) {
		if (canRaiseEvents_) {
			switch (keyboard.GetState()) {
				case KeyboardState::KeyDown:
					return OnKeyDown(keyboard);
				case KeyboardState::KeyUp:
					return OnKeyUp(keyboard);
				case KeyboardState::Character:
					return OnKeyPress(keyboard);
			}
		}
		
		return false;
	}
}
