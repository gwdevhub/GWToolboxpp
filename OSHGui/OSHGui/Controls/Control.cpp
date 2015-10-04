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

namespace OSHGui
{
	//---------------------------------------------------------------------------
	//Constructor
	//---------------------------------------------------------------------------
	Control::Control()
		: type_((ControlType)0),
		  parent_(nullptr),
		  location_(6, 6),
		  size_(0, 0),
		  anchor_(AnchorStyles::Top | AnchorStyles::Left),
		  isEnabled_(true),
		  isVisible_(true),
		  isFocused_(false),
		  isClicked_(false),
		  isInside_(false),
		  isFocusable_(true),
		  hasCaptured_(false),
		  autoSize_(false),
		  canRaiseEvents_(true),
		  needsRedraw_(true),
		  cursor_(nullptr),
		  mouseOverFocusColor_(Drawing::Color::FromARGB(0, 20, 20, 20)),
		  geometry_(Application::Instance().GetRenderer().CreateGeometryBuffer())
	{
		
	}
	//---------------------------------------------------------------------------
	Control::~Control()
	{
		if (isInside_)
		{
			Application::Instance().MouseEnteredControl = nullptr;
		}
		if (isFocused_)
		{
			Application::Instance().FocusedControl = nullptr;
		}

		for (auto &control : internalControls_)
		{
			delete control;
		}
	}
	//---------------------------------------------------------------------------
	//Getter/Setter
	//---------------------------------------------------------------------------
	ControlType Control::GetType() const
	{
		return type_;
	}
	//---------------------------------------------------------------------------
	bool Control::IsContainer() const
	{
		return false;
	}
	//---------------------------------------------------------------------------
	bool Control::GetIsFocused() const
	{
		return isFocused_;
	}
	//---------------------------------------------------------------------------
	void Control::SetEnabled(bool isEnabled)
	{
		isEnabled_ = isEnabled;
		if (isEnabled == false && isFocused_)
		{
			OnLostFocus(nullptr);
		}

		Invalidate();
	}
	//---------------------------------------------------------------------------
	bool Control::GetEnabled() const
	{
		return isEnabled_;
	}
	//---------------------------------------------------------------------------
	void Control::SetVisible(bool isVisible)
	{
		isVisible_ = isVisible;
		if (isVisible == false && isFocused_)
		{
			OnLostFocus(nullptr);
		}

		Invalidate();
	}
	//---------------------------------------------------------------------------
	bool Control::GetVisible() const
	{
		return isVisible_;
	}
	//---------------------------------------------------------------------------
	void Control::SetAutoSize(bool autoSize)
	{
		autoSize_ = autoSize;

		Invalidate();
	}
	//---------------------------------------------------------------------------
	bool Control::GetAutoSize() const
	{
		return autoSize_;
	}
	//---------------------------------------------------------------------------
	void Control::SetBounds(int x, int y, int w, int h)
	{
		SetBounds(Drawing::PointF(x, y), Drawing::SizeF(w, h));
	}
	//---------------------------------------------------------------------------
	void Control::SetBounds(const Drawing::PointI &location, const Drawing::SizeI &size)
	{
		SetLocation(location);
		SetSize(size);
	}
	//---------------------------------------------------------------------------
	void Control::SetBounds(const Drawing::RectangleI &bounds)
	{
		SetBounds(bounds.GetLocation(), bounds.GetSize());
	}
	//---------------------------------------------------------------------------
	const Drawing::RectangleI Control::GetBounds() const
	{
		return Drawing::RectangleI(location_, size_);
	}
	//---------------------------------------------------------------------------
	void Control::SetLocation(const Drawing::PointI &location)
	{
		location_ = location;

		OnLocationChanged();
	}
	//---------------------------------------------------------------------------
	void Control::SetLocation(int x, int y)
	{
		SetLocation(Drawing::PointI(x, y));
	}
	//---------------------------------------------------------------------------
	const Drawing::PointI& Control::GetLocation() const
	{
		return location_;
	}
	//---------------------------------------------------------------------------
	void Control::SetSize(const Drawing::SizeI &size)
	{
		#ifndef OSHGUI_DONTUSEEXCEPTIONS
		if (size.Width < 0)
		{
			throw Misc::ArgumentOutOfRangeException("width");
		}
		if (size.Height < 0)
		{
			throw Misc::ArgumentOutOfRangeException("height");
		}
		#endif

		size_ = size;
		
		OnSizeChanged();

		Invalidate();

		auto offset = size_ - GetSize();
		for (auto &control : controls_)
		{
			AnchorStyles anchor = control->GetAnchor();

			if (anchor != (AnchorStyles::Top | AnchorStyles::Left))
			{
				if (anchor == (AnchorStyles::Top | AnchorStyles::Left | AnchorStyles::Bottom | AnchorStyles::Right))
				{
					control->SetSize(control->GetSize() + offset);
				}
				else if (anchor == (AnchorStyles::Top | AnchorStyles::Left | AnchorStyles::Right) || anchor == (AnchorStyles::Bottom | AnchorStyles::Left | AnchorStyles::Right))
				{
					control->SetLocation(control->GetLocation() + Drawing::PointF(0, offset.Height));
					control->SetSize(control->GetSize() + Drawing::SizeF(offset.Width, 0));
				}
				else if (anchor == (AnchorStyles::Top | AnchorStyles::Right) || anchor == (AnchorStyles::Bottom | AnchorStyles::Right))
				{
					control->SetLocation(control->GetLocation() + Drawing::PointF(offset.Width, offset.Height));
				}
			}
		}
	}
	//---------------------------------------------------------------------------
	void Control::SetSize(int width, int height)
	{
		SetSize(Drawing::SizeI(width, height));
	}
	//---------------------------------------------------------------------------
	const Drawing::SizeI& Control::GetSize() const
	{
		return size_;
	}
	//---------------------------------------------------------------------------
	int Control::GetLeft() const
	{
		return location_.Left;
	}
	//---------------------------------------------------------------------------
	int Control::GetTop() const
	{
		return location_.Top;
	}
	//---------------------------------------------------------------------------
	int Control::GetRight() const
	{
		return location_.Left + size_.Width;
	}
	//---------------------------------------------------------------------------
	int Control::GetBottom() const
	{
		return location_.Top + size_.Height;
	}
	//---------------------------------------------------------------------------
	int Control::GetWidth() const
	{
		return size_.Width;
	}
	//---------------------------------------------------------------------------
	int Control::GetHeight() const
	{
		return size_.Height;
	}
	//---------------------------------------------------------------------------
	void Control::SetAnchor(AnchorStyles anchor)
	{
		anchor_ = anchor;
	}
	//---------------------------------------------------------------------------
	AnchorStyles Control::GetAnchor() const
	{
		return anchor_;
	}
	//---------------------------------------------------------------------------
	void Control::SetTag(const Misc::Any &tag)
	{
		tag_ = tag;
	}
	//---------------------------------------------------------------------------
	const Misc::Any& Control::GetTag() const
	{
		return tag_;
	}
	//---------------------------------------------------------------------------
	void Control::SetName(const Misc::AnsiString &name)
	{
		name_ = name;
	}
	//---------------------------------------------------------------------------
	const Misc::AnsiString& Control::GetName() const
	{
		return name_;
	}
	//---------------------------------------------------------------------------
	void Control::SetForeColor(const Drawing::Color &color)
	{
		foreColor_ = color;

		Invalidate();
	}
	//---------------------------------------------------------------------------
	const Drawing::Color& Control::GetForeColor() const
	{
		return foreColor_;
	}
	//---------------------------------------------------------------------------
	void Control::SetBackColor(const Drawing::Color &color)
	{
		backColor_ = color;

		Invalidate();
	}
	//---------------------------------------------------------------------------
	const Drawing::Color& Control::GetBackColor() const
	{
		return backColor_;
	}
	//---------------------------------------------------------------------------
	void Control::SetMouseOverFocusColor(const Drawing::Color &color)
	{
		mouseOverFocusColor_ = color;

		Invalidate();
	}
	//---------------------------------------------------------------------------
	const Drawing::Color& Control::GetMouseOverFocusColor() const
	{
		return mouseOverFocusColor_;
	}
	//---------------------------------------------------------------------------
	void Control::SetFont(const Drawing::FontPtr &font)
	{
		#ifndef OSHGUI_DONTUSEEXCEPTIONS
		if (font == nullptr)
		{
			throw Misc::ArgumentNullException("font");
		}
		#endif
		
		font_ = font;

		Invalidate();
	}
	//---------------------------------------------------------------------------
	const Drawing::FontPtr& Control::GetFont() const
	{
		return font_ ? font_ : parent_ ? parent_->GetFont() : Application::Instance().GetDefaultFont();
	}
	//---------------------------------------------------------------------------
	void Control::SetCursor(const CursorPtr &cursor)
	{
		cursor_ = std::move(cursor);
	}
	//---------------------------------------------------------------------------
	const CursorPtr& Control::GetCursor() const
	{
		return cursor_ ? cursor_ : GetParent() ? GetParent()->GetCursor() : Cursors::Get(Cursors::Default);
	}
	//---------------------------------------------------------------------------
	LocationChangedEvent& Control::GetLocationChangedEvent()
	{
		return locationChangedEvent_;
	}
	//---------------------------------------------------------------------------
	SizeChangedEvent& Control::GetSizeChangedEvent()
	{
		return sizeChangedEvent_;
	}
	//---------------------------------------------------------------------------
	ClickEvent& Control::GetClickEvent()
	{
		return clickEvent_;
	}
	//---------------------------------------------------------------------------
	MouseClickEvent& Control::GetMouseClickEvent()
	{
		return mouseClickEvent_;
	}
	//---------------------------------------------------------------------------
	MouseDownEvent& Control::GetMouseDownEvent()
	{
		return mouseDownEvent_;
	}
	//---------------------------------------------------------------------------
	MouseUpEvent& Control::GetMouseUpEvent()
	{
		return mouseUpEvent_;
	}
	//---------------------------------------------------------------------------
	MouseMoveEvent& Control::GetMouseMoveEvent()
	{
		return mouseMoveEvent_;
	}
	//---------------------------------------------------------------------------
	MouseScrollEvent& Control::GetMouseScrollEvent()
	{
		return mouseScrollEvent_;
	}
	//---------------------------------------------------------------------------
	MouseEnterEvent& Control::GetMouseEnterEvent()
	{
		return mouseEnterEvent_;
	}
	//---------------------------------------------------------------------------
	MouseLeaveEvent& Control::GetMouseLeaveEvent()
	{
		return mouseLeaveEvent_;
	}
	//---------------------------------------------------------------------------
	KeyDownEvent& Control::GetKeyDownEvent()
	{
		return keyDownEvent_;
	}
	//---------------------------------------------------------------------------
	KeyPressEvent& Control::GetKeyPressEvent()
	{
		return keyPressEvent_;
	}
	//---------------------------------------------------------------------------
	KeyUpEvent& Control::GetKeyUpEvent()
	{
		return keyUpEvent_;
	}
	//---------------------------------------------------------------------------
	FocusGotEvent& Control::GetFocusGotEvent()
	{
		return focusGotEvent_;
	}
	//---------------------------------------------------------------------------
	FocusLostEvent& Control::GetFocusLostEvent()
	{
		return focusLostEvent_;
	}
	//---------------------------------------------------------------------------
	void Control::SetParent(Control *parent)
	{
		#ifndef OSHGUI_DONTUSEEXCEPTIONS
		if (parent == nullptr)
		{
			throw Misc::ArgumentNullException("parent");
		}
		#endif

		parent_ = parent;

		OnLocationChanged();
	}
	//---------------------------------------------------------------------------
	Control* Control::GetParent() const
	{
		return parent_;
	}
	//---------------------------------------------------------------------------
	const std::deque<Control*>& Control::GetControls() const
	{
		return controls_;
	}
	//---------------------------------------------------------------------------
	Control::PostOrderIterator Control::GetPostOrderEnumerator()
	{
		return PostOrderIterator(this);
	}
	//---------------------------------------------------------------------------
	void Control::GetRenderContext(Drawing::RenderContext &context) const
	{
		if (surface_)
		{
			context.Surface = surface_.get();
			context.Owner = this;
			context.Offset = GetLocation();
			context.QueueType = Drawing::RenderQueueType::Base;
		}
		else if (GetParent())
		{
			GetParent()->GetRenderContext(context);
		}
		else
		{
			context.Surface = &Application::Instance().GetRenderSurface();
			context.Owner = nullptr;
			context.Offset = Drawing::PointF(0.0f, 0.0f);
			context.QueueType = Drawing::RenderQueueType::Base;
		}
	}
	//---------------------------------------------------------------------------
	//Runtime-Functions
	//---------------------------------------------------------------------------
	void Control::Focus()
	{
		if (isFocusable_ && !isFocused_)
		{
			OnGotFocus(this);
		}
	}
	//---------------------------------------------------------------------------
	bool Control::Intersect(const Drawing::PointI &point) const
	{
		return Intersection::TestRectangle(absoluteLocation_, size_, point);
	}
	//---------------------------------------------------------------------------
	Drawing::PointI Control::PointToClient(const Drawing::PointI &point) const
	{
		return point - absoluteLocation_;
	}
	//---------------------------------------------------------------------------
	Drawing::PointI Control::PointToScreen(const Drawing::PointI &point) const
	{
		#ifndef OSHGUI_DONTUSEEXCEPTIONS
		if (!parent_)
		{
			throw Misc::ArgumentNullException("parent");
		}
		#endif
		
		if (parent_ != this)
		{
			return parent_->PointToScreen(point + location_);
		}

		return point + location_;
	}
	//---------------------------------------------------------------------------
	void Control::CalculateAbsoluteLocation()
	{
		if (parent_ != nullptr)
		{
			absoluteLocation_ = parent_->absoluteLocation_ + location_;
		}
		else
		{
			absoluteLocation_ = location_;
		}

		geometry_->SetTranslation(Drawing::Vector(absoluteLocation_.X, absoluteLocation_.Y, 0.0f));
		//TODO: set clipping here

		for (auto &control : internalControls_)
		{
			control->CalculateAbsoluteLocation();
		}
	}
	//---------------------------------------------------------------------------
	void Control::AddControl(Control *control)
	{
		#ifndef OSHGUI_DONTUSEEXCEPTIONS
		if (control == nullptr)
		{
			throw Misc::ArgumentNullException("control");
		}
		#endif

		if (control->GetType() == ControlType::Form)
		{
			return;
		}

		AddSubControl(control);

		controls_.push_front(control);
	}
	//---------------------------------------------------------------------------
	void Control::RemoveControl(Control *control)
	{
		if (control != nullptr)
		{
			controls_.erase(std::remove(std::begin(controls_), std::end(controls_), control), std::end(controls_));
			internalControls_.erase(std::remove(std::begin(internalControls_), std::end(internalControls_), control), std::end(internalControls_));

			Invalidate();
		}
	}
	//---------------------------------------------------------------------------
	void Control::AddSubControl(Control *subcontrol)
	{
		#ifndef OSHGUI_DONTUSEEXCEPTIONS
		if (subcontrol == nullptr)
		{
			throw Misc::ArgumentNullException("subcontrol");
		}
		#endif

		if (subcontrol->GetType() == ControlType::Form)
		{
			return;
		}

		for (auto &control : internalControls_)
		{
			if (subcontrol == control || (!subcontrol->GetName().empty() && subcontrol->GetName() == control->GetName()))
			{
				return;
			}
		}

		subcontrol->SetParent(this);

		internalControls_.push_front(subcontrol);

		Invalidate();
	}
	//---------------------------------------------------------------------------
	Control* Control::GetChildAtPoint(const Drawing::PointI &point) const
	{
		for (auto &control : make_reverse_range(controls_))
		{
			if (control->GetEnabled() && control->GetVisible() && control->Intersect(point))
			{
				return control;
			}
		}

		return nullptr;
	}
	//---------------------------------------------------------------------------
	Control* Control::GetChildByName(const Misc::AnsiString &name) const
	{
		for (auto &control : controls_)
		{
			if (control->GetName() == name)
			{
				return control;
			}
		}

		return nullptr;
	}
	//---------------------------------------------------------------------------
	void Control::InjectTime(const Misc::DateTime &time)
	{

	}
	//---------------------------------------------------------------------------
	void Control::Invalidate()
	{
		needsRedraw_ = true;

		Application::Instance().GetRenderSurface().Invalidate();
	}
	//---------------------------------------------------------------------------
	void Control::Render()
	{
		if (!isVisible_)
		{
			return;
		}

		using namespace Drawing;

		RenderContext ctx;
		GetRenderContext(ctx);

		// clear geometry from surface if it's ours
		if (ctx.Owner == this)
		{
			ctx.Surface->Reset();
		}

		// redraw if no surface set, or if surface is invalidated
		if (!surface_)// || surface->isInvalidated())
		{
			DrawSelf(ctx);

			Control *focusedControl = nullptr;
			for (auto &control : make_reverse_range(controls_))
			{
				if (control->GetIsFocused())
				{
					focusedControl = control;
				}
				else
				{
					control->Render();
				}
			}
			if (focusedControl != nullptr)
			{
				focusedControl->Render();
			}
		}

		if (ctx.Owner == this)
		{
			ctx.Surface->Draw();
		}
	}
	//---------------------------------------------------------------------------
	void Control::DrawSelf(Drawing::RenderContext &context)
	{
		BufferGeometry(context);
		QueueGeometry(context);
	}
	//---------------------------------------------------------------------------
	void Control::BufferGeometry(Drawing::RenderContext &context)
	{
		if (needsRedraw_)
		{
			geometry_->Reset();

			PopulateGeometry();

			needsRedraw_ = false;
		}
	}
	//---------------------------------------------------------------------------
	void Control::QueueGeometry(Drawing::RenderContext &context)
	{
		context.Surface->AddGeometry(context.QueueType, geometry_);
	}
	//---------------------------------------------------------------------------
	void Control::PopulateGeometry()
	{

	}
	//---------------------------------------------------------------------------
	void Control::ApplyTheme(const Drawing::Theme &theme)
	{
		auto &controlTheme = theme.GetControlColorTheme(ControlTypeToString(type_));
		SetForeColor(controlTheme.ForeColor);
		SetBackColor(controlTheme.BackColor);

		for (auto &control : GetControls())
		{
			control->ApplyTheme(theme);
		}
	}
	//---------------------------------------------------------------------------
	Misc::AnsiString Control::ControlTypeToString(ControlType controlType)
	{
		switch (controlType)
		{
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
		}
		throw Misc::Exception();
	}
	//---------------------------------------------------------------------------
	//Event-Handling
	//---------------------------------------------------------------------------
	void Control::OnLocationChanged()
	{
		CalculateAbsoluteLocation();

		Invalidate();
	
		locationChangedEvent_.Invoke(this);
	}
	//---------------------------------------------------------------------------
	void Control::OnSizeChanged()
	{
		sizeChangedEvent_.Invoke(this);
	}
	//---------------------------------------------------------------------------
	void Control::OnMouseDown(const MouseMessage &mouse)
	{
		isClicked_ = true;

		Invalidate();

		MouseEventArgs args(mouse);
		args.Location -= absoluteLocation_;
		mouseDownEvent_.Invoke(this, args);
	}
	//---------------------------------------------------------------------------
	void Control::OnMouseClick(const MouseMessage &mouse)
	{
		MouseEventArgs args(mouse);
		args.Location -= absoluteLocation_;
		mouseClickEvent_.Invoke(this, args);
	}
	//---------------------------------------------------------------------------
	void Control::OnMouseUp(const MouseMessage &mouse)
	{
		isClicked_ = false;

		Invalidate();

		MouseEventArgs args(mouse);
		args.Location -= absoluteLocation_;
		mouseUpEvent_.Invoke(this, args);
	}
	//---------------------------------------------------------------------------
	void Control::OnMouseMove(const MouseMessage &mouse)
	{
		MouseEventArgs args(mouse);
		args.Location -= absoluteLocation_;
		mouseMoveEvent_.Invoke(this, args);
	}
	//---------------------------------------------------------------------------
	bool Control::OnMouseScroll(const MouseMessage &mouse)
	{
		MouseEventArgs args(mouse);
		args.Location -= absoluteLocation_;
		mouseScrollEvent_.Invoke(this, args);
		return args.Handled;
	}
	//---------------------------------------------------------------------------
	void Control::OnMouseEnter(const MouseMessage &mouse)
	{
		isInside_ = true;

		auto &app = Application::Instance();

		if (app.MouseEnteredControl != nullptr && app.MouseEnteredControl->isInside_)
		{
			app.MouseEnteredControl->OnMouseLeave(mouse);
		}
		app.MouseEnteredControl = this;

		mouseEnterEvent_.Invoke(this);

		Invalidate();

		app.SetCursor(GetCursor());
	}
	//---------------------------------------------------------------------------
	void Control::OnMouseLeave(const MouseMessage &mouse)
	{
		isInside_ = false;

		Application::Instance().MouseEnteredControl = nullptr;

		mouseLeaveEvent_.Invoke(this);

		Invalidate();
	}
	//---------------------------------------------------------------------------
	void Control::OnGotMouseCapture()
	{
		auto &app = Application::Instance();
		if (app.CaptureControl != nullptr)
		{
			app.CaptureControl->OnLostMouseCapture();
		}
		app.CaptureControl = this;
		hasCaptured_ = true;

		isClicked_ = false;

		mouseCaptureChangedEvent_.Invoke(this);

		Invalidate();
	}
	//---------------------------------------------------------------------------
	void Control::OnLostMouseCapture()
	{
		hasCaptured_ = false;

		Application::Instance().CaptureControl = nullptr;

		mouseCaptureChangedEvent_.Invoke(this);

		Invalidate();
	}
	//---------------------------------------------------------------------------
	void Control::OnGotFocus(Control *newFocusedControl)
	{
		auto &app = Application::Instance();
		if (newFocusedControl != app.FocusedControl)
		{
			if (app.FocusedControl != nullptr)
			{
				app.FocusedControl->OnLostFocus(newFocusedControl);
			}

			app.FocusedControl = newFocusedControl;
			isFocused_ = true;

			focusGotEvent_.Invoke(this);

			Invalidate();
		}
	}
	//---------------------------------------------------------------------------
	void Control::OnLostFocus(Control *newFocusedControl)
	{
		isFocused_ = isClicked_ = false;
		
		Application::Instance().FocusedControl = nullptr;

		focusLostEvent_.Invoke(this, newFocusedControl);

		Invalidate();
	}
	//---------------------------------------------------------------------------
	bool Control::OnKeyDown(const KeyboardMessage &keyboard)
	{
		KeyEventArgs args(keyboard);
		keyDownEvent_.Invoke(this, args);

		return args.Handled;
	}
	//---------------------------------------------------------------------------
	bool Control::OnKeyPress(const KeyboardMessage &keyboard)
	{
		KeyPressEventArgs args(keyboard);
		keyPressEvent_.Invoke(this, args);

		return args.Handled;
	}
	//---------------------------------------------------------------------------
	bool Control::OnKeyUp(const KeyboardMessage &keyboard)
	{
		KeyEventArgs args(keyboard);
		keyUpEvent_.Invoke(this, args);

		return args.Handled;
	}
	//---------------------------------------------------------------------------
	bool Control::ProcessMouseMessage(const MouseMessage &mouse)
	{
		switch (mouse.GetState())
		{
			case MouseState::Down:
				if (canRaiseEvents_ && Intersect(mouse.GetLocation()))
				{
					if (mouse.GetButton() == MouseButton::Left && !isClicked_ && isEnabled_)
					{
						OnMouseDown(mouse);
						
						if (isFocusable_ && !isFocused_)
						{
							OnGotFocus(this);
						}
					}

					return true;
				}
				break;
			case MouseState::Up:
				if (canRaiseEvents_ && (hasCaptured_ || Intersect(mouse.GetLocation())))
				{
					if (isClicked_)
					{
						if (mouse.GetButton() == MouseButton::Left)
						{
							clickEvent_.Invoke(this);

							OnMouseClick(mouse);
						}
					}

					OnMouseUp(mouse);

					return true;
				}
				break;
			case MouseState::Move:
				if (hasCaptured_ || Intersect(mouse.GetLocation()))
				{
					if (canRaiseEvents_)
					{
						if (!isInside_)
						{
							OnMouseEnter(mouse);
						}

						OnMouseMove(mouse);
					}

					return true;
				}
				break;
			case MouseState::Scroll:
				if (hasCaptured_ || isFocused_ || Intersect(mouse.GetLocation()))
				{
					if (canRaiseEvents_)
					{
						if (mouse.GetDelta() != 0)
						{
							return OnMouseScroll(mouse);
						}
					}
				}
				break;
		}

		return false;
	}
	//---------------------------------------------------------------------------
	bool Control::ProcessKeyboardMessage(const KeyboardMessage &keyboard)
	{
		if (canRaiseEvents_)
		{
			switch (keyboard.GetState())
			{
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
	//---------------------------------------------------------------------------
	//Control::PostOrderIterator
	//---------------------------------------------------------------------------
	Control::PostOrderIterator::PostOrderIterator(Control *start)
	{
		start_ = start;

		if (start_->internalControls_.empty())
		{
			current_ = start_;
		}
		else
		{
			LoopThrough(start_);
			current_ = controlStack_.back();
			controlStack_.pop_back();
		}
	}
	//---------------------------------------------------------------------------
	void Control::PostOrderIterator::LoopThrough(Control *container)
	{
		controlStack_.push_back(container);

		for (auto &control : make_reverse_range(container->internalControls_))
		{
			if (control->GetVisible() && control->GetEnabled())
			{
				LoopThrough(control);
			}
		}
	}
	//---------------------------------------------------------------------------
	void Control::PostOrderIterator::operator++()
	{
		current_ = controlStack_.back();
		controlStack_.pop_back();
	}
	//---------------------------------------------------------------------------
	bool Control::PostOrderIterator::operator()()
	{
		return !controlStack_.empty();
	}
	//---------------------------------------------------------------------------
	Control* Control::PostOrderIterator::operator*()
	{
		return current_;
	}
	//---------------------------------------------------------------------------
}
