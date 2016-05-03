/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_CONTROL_HPP
#define OSHGUI_CONTROL_HPP

#include <vector>
#include <deque>
#include <algorithm>

#include "../Exports.hpp"

#include "../Application.hpp"

#include "../Drawing/Point.hpp"
#include "../Drawing/Size.hpp"
#include "../Drawing/Rectangle.hpp"
#include "../Drawing/Renderer.hpp"
#include "../Drawing/RenderContext.hpp"
#include "../Drawing/Graphics.hpp"
#include "../Drawing/Theme.hpp"

#include "../Misc/Strings.hpp"
#include "../Misc/Any.hpp"

#include "../Event/KeyboardMessage.hpp"
#include "../Event/MouseMessage.hpp"
#include "../Event/Event.hpp"
#include "../Event/KeyEventArgs.hpp"
#include "../Event/KeyPressEventArgs.hpp"
#include "../Event/MouseEventArgs.hpp"
#include "../Event/ScrollEventArgs.hpp"

#include "../Cursor/Cursors.hpp"

namespace OSHGui {
	/**
	 * Listing the types of controls.
	 */
	enum class ControlType {
		Panel = 1,
		Form,
		GroupBox,
		Label,
		LinkLabel,
		Button,
		CheckBox,
		RadioButton,
		ScrollBar,
		ListBox,
		ProgressBar,
		TrackBar,
		ComboBox,
		TextBox,
		Timer,
		TabControl,
		TabPage,
		PictureBox,
		ColorPicker,
		ColorBar,
		HotkeyControl,
		ScrollPanel
	};

	/**
	 * List of anchors.
	 */
	enum class AnchorStyles : int {
		Top = 1,
		Bottom = 2,
		Left = 4,
		Right = 8
	};

	enum class Clipping : int {
		OnBounds,	// in the intersection between parent and current control
		OnParent,	// just on the parent boundaries
		OnControl,	// just on the control boundaries
		None
	};

	inline AnchorStyles operator|(AnchorStyles lhs, AnchorStyles rhs) {
		return static_cast<AnchorStyles>(static_cast<int>(lhs) | static_cast<int>(rhs));
	}
	inline AnchorStyles operator&(AnchorStyles lhs, AnchorStyles rhs) {
		return static_cast<AnchorStyles>(static_cast<int>(lhs) & static_cast<int>(rhs));
	}
	
	class Control;
	
	/**
	 * Occurs when the Location property changes.
	 */
	typedef Event<void(Control*)> LocationChangedEvent;
	typedef EventHandler<void(Control*)> LocationChangedEventHandler;
	/**
	 * Occurs when the Size property is changed.
	 */
	typedef Event<void(Control*)> SizeChangedEvent;
	typedef EventHandler<void(Control*)> SizeChangedEventHandler;
	/**
	 * Occurs when a key is pressed.
	 */
	typedef Event<void(Control*, KeyEventArgs&)> KeyDownEvent;
	typedef EventHandler<void(Control*, KeyEventArgs&)> KeyDownEventHandler;
	/**
	 * Occurs when the control is focused and a key is pressed.
	 */
	typedef Event<void(Control*, KeyPressEventArgs&)> KeyPressEvent;
	typedef EventHandler<void(Control*, KeyPressEventArgs&)> KeyPressEventHandler;
	/**
	 * Occurs when a key is released.
	 */
	typedef Event<void(Control*, KeyEventArgs&)> KeyUpEvent;
	typedef EventHandler<void(Control*, KeyEventArgs&)> KeyUpEventHandler;
	/**
	 * Occurs when clicked on the control.
	 */
	typedef Event<void(Control*)> ClickEvent;
	typedef EventHandler<void(Control*)> ClickEventHandler;
	/**
	 * Occurs when the mouse pointer is over the control and a mouse button was pressed.
	 */
	typedef Event<void(Control*, MouseEventArgs&)> MouseDownEvent;
	typedef EventHandler<void(Control*, MouseEventArgs&)> MouseDownEventHandler;
	/**
	 * Occurs when the mouse pointer is over the control and a mouse button was released.
	 */
	typedef Event<void(Control*, MouseEventArgs&)> MouseUpEvent;
	typedef EventHandler<void(Control*, MouseEventArgs&)> MouseUpEventHandler;
	/**
	 * Occurs when the mouse pointer moves over the control.
	 */
	typedef Event<void(Control*, MouseEventArgs&)> MouseMoveEvent;
	typedef EventHandler<void(Control*, MouseEventArgs&)> MouseMoveEventHandler;
	/**
	 * Occurs when the mouse pointer is over the control and a mouse button was released.
	 */
	typedef Event<void(Control*, MouseEventArgs&)> MouseScrollEvent;
	typedef EventHandler<void(Control*, MouseEventArgs&)> MouseScrollEventHandler;
	/**
	 * Occurs when the mouse is moved in the area of the control.
	 */
	typedef Event<void(Control*)> MouseEnterEvent;
	typedef EventHandler<void(Control*)> MouseEnterEventHandler;
	/**
	 * Occurs when the mouse leaves the area of the control.
	 */
	typedef Event<void(Control*)> MouseLeaveEvent;
	typedef EventHandler<void(Control*)> MouseLeaveEventHandler;
	/**
	 * Occurs after the mouse capture has changed.
	 */
	typedef Event<void(Control*)> MouseCaptureChangedEvent;
	typedef EventHandler<void(Control*)> MouseCaptureChangedEventHandler;
	/**
	 * Occurs when the control obtains focus.
	 */
	typedef Event<void(Control*)> FocusGotEvent;
	typedef EventHandler<void(Control*)> FocusGotEventHandler;
	/**
	 * Occurs when the control loses focus.
	 */
	typedef Event<void(Control*, Control*)> FocusLostEvent;
	typedef EventHandler<void(Control*, Control*)> FocusLostEventHandler;

	/**
	 * Defines the base class for controls, which are components with visual representation.
	 */
	class OSHGUI_EXPORT Control {
		friend Application;

	public:
		class PostOrderIterator;

		virtual ~Control();
		
		ControlType GetType() const;
		
		/**
		 * Gets whether the control has focus.
		 */
		virtual bool GetIsFocused() const;

		virtual void SetEnabled(bool isEnabled);
		virtual bool GetEnabled() const;

		virtual void SetVisible(bool isVisible);
		virtual bool GetVisible() const;

		virtual void SetClip(Clipping isClipping);
		virtual Clipping GetClip() const;
		
		/**
		 * Specifies whether the size of the control automatically adjusts to its contents.
		 */
		virtual void SetAutoSize(bool autoSize);
		virtual bool GetAutoSize() const;
		/**
		 * Sets the size and position of control element fixed relative to the parent control.
		 */
		virtual void SetBounds(const Drawing::RectangleI &bounds);
		virtual void SetBounds(const Drawing::PointI &location, const Drawing::SizeI &size);
		virtual void SetBounds(int x, int y, int w, int h);
		virtual const Drawing::RectangleI GetBounds() const;
		/**
		 * Sets the coordinates of the upper left corner of the control 
		 * relative to the fixed upper left corner of the container.
		 */
		virtual void SetLocation(const Drawing::PointI &location);
		virtual const Drawing::PointI& GetLocation() const;

		virtual void SetSize(const Drawing::SizeI &size);
		virtual const Drawing::SizeI& GetSize() const;

		int GetX() const;
		int GetLeft() const;
		void SetX(int x);
		void SetLeft(int left);
		int GetY() const;
		int GetTop() const;
		void SetY(int y);
		void SetTop(int top);
		int GetRight() const;
		int GetBottom() const;
		int GetWidth() const;
		void SetWidth(int width);
		int GetHeight() const;
		void SetHeight(int height);
		/**
		 * Sets from the edges of the container to which a control is 
		 * bound and determines how the size of the control is changed with its parent.
		 */
		virtual void SetAnchor(AnchorStyles anchor);
		virtual AnchorStyles GetAnchor() const;
		/**
		 * Sets the control linked with the custom data securely.
		 */
		virtual void SetTag(const Misc::Any &tag);
		virtual const Misc::Any& GetTag() const;
		/**
		 * Specifies the name used to identify the control.
		 */
		virtual void SetName(const Misc::UnicodeString &name);
		virtual const Misc::UnicodeString& GetName() const;
		/**
		 * Sets the font of the text in the control.
		 */
		virtual void SetFont(const Drawing::FontPtr &font);
		virtual const Drawing::FontPtr& GetFont() const;
		/**
		 *Sets the cursor displayed when the mouse is over the control.
		 */
		virtual void SetCursor(const CursorPtr &cursor);
		virtual const CursorPtr& GetCursor() const;

		virtual void SetForeColor(const Drawing::Color &color);
		virtual const Drawing::Color& GetForeColor() const;

		virtual void SetBackColor(const Drawing::Color &color);
		virtual const Drawing::Color& GetBackColor() const;

		virtual void SetMouseOverFocusColor(const Drawing::Color &color);
		virtual const Drawing::Color& GetMouseOverFocusColor() const;

		LocationChangedEvent& GetLocationChangedEvent();
		SizeChangedEvent& GetSizeChangedEvent();
		ClickEvent& GetClickEvent();
		MouseDownEvent& GetMouseDownEvent();
		MouseUpEvent& GetMouseUpEvent();
		MouseMoveEvent& GetMouseMoveEvent();
		MouseScrollEvent& GetMouseScrollEvent();
		MouseEnterEvent& GetMouseEnterEvent();
		MouseLeaveEvent& GetMouseLeaveEvent();
		MouseCaptureChangedEvent& GetMouseCaptureChangedEvent();
		KeyDownEvent& GetKeyDownEvent();
		KeyPressEvent& GetKeyPressEvent();
		KeyUpEvent& GetKeyUpEvent();
		FocusGotEvent& GetFocusGotEvent();
		FocusLostEvent& GetFocusLostEvent();

		Control* GetParent() const;

		/**
		 * Sets input focus to the control.
		 */
		virtual void Focus();
		/**
		 * Checks to see if the point is within the control.
		 */
		virtual bool Intersect(const Drawing::PointI &point) const;
		/**
		 * Calculates the absolute position of the control.
		 */
		virtual void CalculateAbsoluteLocation();
		/**
		 * Computes the location of the specified screen point into client coordinates.
		 */
		virtual Drawing::PointI PointToClient(const Drawing::PointI &point) const;
		/**
		 * Computes the location of the specified client point into screen coordinates.
		 */
		virtual Drawing::PointI PointToScreen(const Drawing::PointI &point) const;
		

		bool ProcessMouseMessage(const MouseMessage &mouse);
		virtual bool ProcessMouseDown(const MouseMessage &mouse);
		virtual bool ProcessMouseUp(const MouseMessage &mouse);
		virtual bool ProcessMouseMove(const MouseMessage &mouse);
		virtual bool ProcessMouseScroll(const MouseMessage &mouse);

		virtual bool ProcessKeyboardMessage(const KeyboardMessage &keyboard);
		/**
		 * Causes the control to redraw.
		 */
		void Invalidate();
		/**
		 * Draws the control.
		 */
		virtual void Render(Drawing::RenderContext& context);
		/**
		 * Forces the control to adjust its appearance the Theme.
		 */
		virtual void ApplyTheme(const Drawing::Theme &theme);
	
		static Misc::AnsiString ControlTypeToString(ControlType controlType);

		virtual const std::deque<Control*>& GetControls() const;
		virtual void AddControl(Control *control);
		virtual void RemoveControl(Control *control);

	protected:
		Control(Control* parent);

		virtual void InjectTime(const Misc::DateTime &time);

		void OnLocationChanged();
		void OnSizeChanged();
		virtual void OnMouseDown(const MouseMessage &mouse);
		virtual void OnMouseClick(const MouseMessage &mouse);
		virtual void OnMouseUp(const MouseMessage &mouse);
		virtual void OnMouseMove(const MouseMessage &mouse);
		virtual bool OnMouseScroll(const MouseMessage &mouse);
		virtual void OnMouseEnter(const MouseMessage &mouse);
		virtual void OnMouseLeave(const MouseMessage &mouse);
		virtual void OnGotMouseCapture();
		virtual void OnLostMouseCapture();
		virtual void OnGotFocus(Control *newFocusedControl);
		virtual void OnLostFocus(Control *newFocusedControl);
		virtual bool OnKeyDown(const KeyboardMessage &keyboard);
		virtual bool OnKeyPress(const KeyboardMessage &keyboard);
		virtual bool OnKeyUp(const KeyboardMessage &keyboard);

		virtual void DrawSelf(Drawing::RenderContext &context);
		virtual void PopulateGeometry();
		void QueueGeometry(Drawing::RenderContext &context);
		void BufferGeometry(Drawing::RenderContext &context);

		static const int Padding = 6;

		Misc::UnicodeString name_;
		ControlType type_;

		bool canRaiseEvents_;	// if this specific control can capture and raise events
		bool isEnabled_;		// if this control and its children can capture and raise events
		bool isVisible_;		// if the control is visible
		bool isInside_;			// if the cursor is inside the control
		bool isPressed_;		// if the mouse is pressed on the control
		bool isFocusable_;		// if the control can gain focus
		bool isFocused_;		// if the control is currently focused
		bool hasCaptured_;
		bool autoSize_;
		Clipping clip_;		// if the control clips itself and its children within its boundaries
		
		Misc::Any tag_;
		
		Drawing::PointI location_;
		Drawing::PointI absoluteLocation_;
		Drawing::SizeI size_;
		
		LocationChangedEvent locationChangedEvent_;
		SizeChangedEvent sizeChangedEvent_;
		ClickEvent clickEvent_;
		MouseDownEvent mouseDownEvent_;
		MouseUpEvent mouseUpEvent_;
		MouseMoveEvent mouseMoveEvent_;
		MouseScrollEvent mouseScrollEvent_;
		MouseEnterEvent mouseEnterEvent_;
		MouseLeaveEvent mouseLeaveEvent_;
		MouseCaptureChangedEvent mouseCaptureChangedEvent_;
		KeyDownEvent keyDownEvent_;
		KeyPressEvent keyPressEvent_;
		KeyUpEvent keyUpEvent_;
		FocusGotEvent focusGotEvent_;
		FocusLostEvent focusLostEvent_;
		
		Drawing::Color foreColor_;
		Drawing::Color backColor_;
		Drawing::Color mouseOverFocusColor_;
		
		Drawing::FontPtr font_;
		std::shared_ptr<Cursor> cursor_;

		bool needsRedraw_;
		Drawing::GeometryBufferPtr geometry_;

		Control *parent_;
		std::deque<Control*> controls_;

	private:
		Control(const Control&);
		void operator=(const Control&);

		AnchorStyles anchor_;
	};
}

#endif