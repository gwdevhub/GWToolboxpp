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

namespace OSHGui
{
	/**
	 * Auflistung der Arten von Steuerelementen.
	 */
	enum class ControlType
	{
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
		HotkeyControl
	};

	/**
	 * Auflistung der Anker.
	 */
	enum class AnchorStyles : int
	{
		Top = 1,
		Bottom = 2,
		Left = 4,
		Right = 8
	};

	inline AnchorStyles operator|(AnchorStyles lhs, AnchorStyles rhs)
	{
		return static_cast<AnchorStyles>(static_cast<int>(lhs) | static_cast<int>(rhs));
	}
	inline AnchorStyles operator&(AnchorStyles lhs, AnchorStyles rhs)
	{
		return static_cast<AnchorStyles>(static_cast<int>(lhs) & static_cast<int>(rhs));
	}
	
	class Control;
	
	/**
	 * Tritt ein, wenn die Location-Eigenschaft geändert wird.
	 */
	typedef Event<void(Control*)> LocationChangedEvent;
	typedef EventHandler<void(Control*)> LocationChangedEventHandler;
	/**
	 * Tritt ein, wenn die SizeF-Eigenschaft geändert wird.
	 */
	typedef Event<void(Control*)> SizeChangedEvent;
	typedef EventHandler<void(Control*)> SizeChangedEventHandler;
	/**
	 * Tritt auf, wenn eine Taste gedrückt wird.
	 */
	typedef Event<void(Control*, KeyEventArgs&)> KeyDownEvent;
	typedef EventHandler<void(Control*, KeyEventArgs&)> KeyDownEventHandler;
	/**
	 * Tritt auf, wenn das Steuerelement fokusiert ist und eine Taste gedrückt gehalten wird.
	 */
	typedef Event<void(Control*, KeyPressEventArgs&)> KeyPressEvent;
	typedef EventHandler<void(Control*, KeyPressEventArgs&)> KeyPressEventHandler;
	/**
	 * Tritt auf, wenn eine Taste losgelassen wird.
	 */
	typedef Event<void(Control*, KeyEventArgs&)> KeyUpEvent;
	typedef EventHandler<void(Control*, KeyEventArgs&)> KeyUpEventHandler;
	/**
	 * Tritt auf, wenn auf das Steuerelement geklickt wurde.
	 */
	typedef Event<void(Control*)> ClickEvent;
	typedef EventHandler<void(Control*)> ClickEventHandler;
	/**
	 * Tritt auf, wenn mit der Maus auf das Steuerelement geklickt wurde.
	 */
	typedef Event<void(Control*, MouseEventArgs&)> MouseClickEvent;
	typedef EventHandler<void(Control*, MouseEventArgs&)> MouseClickEventHandler;
	/**
	 * Tritt auf, wenn sich der Mauszeiger über dem Steuerlement befindet und eine Maustaste gedrückt wurde.
	 */
	typedef Event<void(Control*, MouseEventArgs&)> MouseDownEvent;
	typedef EventHandler<void(Control*, MouseEventArgs&)> MouseDownEventHandler;
	/**
	 * Tritt auf, wenn sich der Mauszeiger über dem Steuerlement befindet und eine Maustaste losgelassen wurde.
	 */
	typedef Event<void(Control*, MouseEventArgs&)> MouseUpEvent;
	typedef EventHandler<void(Control*, MouseEventArgs&)> MouseUpEventHandler;
	/**
	 * Tritt auf, wenn der Mauszeiger über das Steuerlement bewegt wird.
	 */
	typedef Event<void(Control*, MouseEventArgs&)> MouseMoveEvent;
	typedef EventHandler<void(Control*, MouseEventArgs&)> MouseMoveEventHandler;
	/**
	 * Tritt auf, wenn sich der Mauszeiger über dem Steuerlement befindet und eine Maustaste losgelassen wurde.
	 */
	typedef Event<void(Control*, MouseEventArgs&)> MouseScrollEvent;
	typedef EventHandler<void(Control*, MouseEventArgs&)> MouseScrollEventHandler;
	/**
	 * Tritt ein, wenn die Maus in den Bereich des Steuerlements bewegt wird.
	 */
	typedef Event<void(Control*)> MouseEnterEvent;
	typedef EventHandler<void(Control*)> MouseEnterEventHandler;
	/**
	 * Tritt ein, wenn die Maus den Bereich des Steuerlements verlässt.
	 */
	typedef Event<void(Control*)> MouseLeaveEvent;
	typedef EventHandler<void(Control*)> MouseLeaveEventHandler;
	/**
	 * Tritt ein, nachdem sich die Mausaufzeichnung geändert hat.
	 */
	typedef Event<void(Control*)> MouseCaptureChangedEvent;
	typedef EventHandler<void(Control*)> MouseCaptureChangedEventHandler;
	/**
	 * Tritt auf, wenn das Steuerelement zu dem aktiven Steuerelement auf dem Formular wird.
	 */
	typedef Event<void(Control*)> FocusGotEvent;
	typedef EventHandler<void(Control*)> FocusGotEventHandler;
	/**
	 * Tritt auf, wenn das Steuerelement nicht mehr das aktive Steuerelement auf dem Formular ist.
	 */
	typedef Event<void(Control*, Control*)> FocusLostEvent;
	typedef EventHandler<void(Control*, Control*)> FocusLostEventHandler;

	/**
	 * Definiert die Basisklasse für Steuerelemente, die Komponenten mit visueller Darstellung sind.
	 */
	class OSHGUI_EXPORT Control
	{
		friend Application;

	public:
		class PostOrderIterator;

		virtual ~Control();
		
		/**
		 * Ruft den CONTROL_TYPE des Steuerelemts ab.
		 *
		 * \return der Typ
		 */
		virtual ControlType GetType() const;

		/**
		 * Gets whether the control is a container.
		 *
		 * \return true, falls Container
		 */
		virtual bool IsContainer() const;
		
		/**
		 * Gets whether the control has focus.
		 *
		 * \return isFocused
		 */
		virtual bool GetIsFocused() const;
		/**
		 * Determines whether the Steuerlement can respond to user interaction.
		 *
		 * \param isEnabled
		 */
		virtual void SetEnabled(bool isEnabled);
		/**
		 * Ruft ab, ob das Steuerlement auf Benutzerinteraktionen reagieren kann.
		 *
		 * \return isEnabled
		 */
		virtual bool GetEnabled() const;
		/**
		 * Determines whether the control and all its child controls
		 * are displayed.
		 *
		 * \param isVisible
		 */
		virtual void SetVisible(bool isVisible);
		/**
		 * Gets whether the control and all its child controls
		 * are displayed.
		 *
		 * \return isVisible
		 */
		virtual bool GetVisible() const;
		
		/**
		 * Specifies whether the size of the control automatically adjusts to its contents.
		 *
		 * \param autoSize
		 */
		virtual void SetAutoSize(bool autoSize);
		/**
		 * Gets whether the size of the control automatically adjusts to its contents.
		 *
		 * \return autoSize
		 */
		virtual bool GetAutoSize() const;
		/**
		 * Sets the size and position of control element fixed relative to the parent control.
		 *
		 * \param bounds
		 */
		virtual void SetBounds(const Drawing::RectangleI &bounds);
		/**
		 * Sets the size and position of control element fixed relative to the parent control.
		 *
		 * \param location
		 * \param size
		 */
		virtual void SetBounds(const Drawing::PointI &location, const Drawing::SizeI &size);
		/**
		 * Sets the size and position of control element fixed relative to the parent control.
		 *
		 * \param x
		 * \param y
		 * \param w
		 * \param h
		 */
		virtual void SetBounds(int x, int y, int w, int h);
		/**
		 * Gets the size and position of the control relative to the parent control from.
		 *
		 * \return bounds
		 */
		virtual const Drawing::RectangleI GetBounds() const;
		/**
		 * Sets the coordinates of the upper left corner of the control relative to the
		 * fixed upper left corner of the container.
		 *
		 * \param x
		 * \param y
		 */
		virtual void SetLocation(int x, int y);
		/**
		 * Legt die Koordinaten der linken oberen Ecke des Steuerelements relativ zur
		 * linken oberen Ecke des Containers fest.
		 *
		 * \param location
		 */
		virtual void SetLocation(const Drawing::PointI &location);
		/**
		 * Ruft die Koordinaten der linken oberen Ecke des Steuerelements relativ zur
		 * linken oberen Ecke des Containers ab.
		 *
		 * \return location
		 */
		virtual const Drawing::PointI& GetLocation() const;
		/**
		 * Legt die Höhe und Breite des Steuerelements fest.
		 *
		 * \param width
		 * \param height
		 */
		virtual void SetSize(int width, int height);
		/**
		 * Legt die Höhe und Breite des Steuerelements fest.
		 *
		 * \param size
		 */
		virtual void SetSize(const Drawing::SizeI &size);
		/**
		 * Ruft die Höhe und Breite des Steuerelements ab.
		 *
		 * \return size
		 */
		virtual const Drawing::SizeI& GetSize() const;
		/**
		 * Gets the distance between the left edge of the control and the left
 		 * margin from the client area of its container.
		 *
		 * \return left
		 */
		virtual int GetLeft() const;
		/**
		 * Gets the distance between the top edge of the control and the top
		 * edge of the client area of its container.
		 *
		 * \return top
		 */
		virtual int GetTop() const;
		/**
		 * Gets the distance between the right edge of the control and the 
		 * left edge of the client area of its container.
		 *
		 * \return right
		 */
		virtual int GetRight() const;
		/**
		 * Gets the distance between the bottom of the control and the 
		 * top edge of the client area of its container.
		 *
		 * \return bottom
		 */
		virtual int GetBottom() const;
		/**
		 * Gets the width of the control.
		 *
		 * \return width
		 */
		virtual int GetWidth() const;
		/**
		 * Gets the height of the control.
		 *
		 * \return height
		 */
		virtual int GetHeight() const;
		/**
		 * Sets from the edges of the container to which a control is 
		 * bound and determines how the size of the control is changed with its parent.
		 *
		 * \param anchor
		 */
		virtual void SetAnchor(AnchorStyles anchor);
		/**
		 * Gets the edges of the container to which a control is bound 
		 * and determines how the size of the control is changed with its parent.
		 *
		 * \return anchor
		 */
		virtual AnchorStyles GetAnchor() const;
		/**
		 * Sets the control linked with the custom data securely.
		 *
		 * \param tag
		 */
		virtual void SetTag(const Misc::Any &tag);
		/**
		 * Gets the user-defined data associated with the control.
		 *
		 * \return tag
		 */
		virtual const Misc::Any& GetTag() const;
		/**
		 * Specifies the name used to identify the control.
		 *
		 * \param name
		 */
		virtual void SetName(const Misc::AnsiString &name);
		/**
		 * Gets the name used to identify the control.
		 *
		 * \return name
		 */
		virtual const Misc::AnsiString& GetName() const;
		/**
		 * Sets the font of the text in the control.
		 *
		 * \param font
		 */
		virtual void SetFont(const Drawing::FontPtr &font);
		/**
		 * Gets the font of the text from the control.
		 *
		 * \return font
		 */
		virtual const Drawing::FontPtr& GetFont() const;
		/**
		 *Sets the cursor displayed when the mouse is over the control.
		 *
		 * \param cursor
		 */
		virtual void SetCursor(const CursorPtr &cursor);
		/** 
		 * Gets the cursor that appears when the mouse is over the control.
		 *
		 * \return cursor
		 */
		virtual const CursorPtr& GetCursor() const;
		/**
		 * Sets the foreground color of the control.
		 *
		 * \param color
		 */
		virtual void SetForeColor(const Drawing::Color &color);
		/**
		 * Gets the foreground color of the control.
		 *
		 * \return color
		 */
		virtual const Drawing::Color& GetForeColor() const;
		/**
		 * Specifies the background color of the control.
		 *
		 * \param color
		 */
		virtual void SetBackColor(const Drawing::Color &color);
		/**
		 * Gets the background color of the control.
		 *
		 * \return color
		 */
		virtual const Drawing::Color& GetBackColor() const;
		/**
		 * Sets the color for the unfocused control.
		 *
		 * \param color
		 */
		virtual void SetMouseOverFocusColor(const Drawing::Color &color);
		/**
		 * Gets the color from the unfocused control.
		 *
		 * \return color
		 */
		virtual const Drawing::Color& GetMouseOverFocusColor() const;
		/**
		 * Gets the LocationChangedEvent from for the control.
		 *
		 * \return locationChangedEvent
		 */
		LocationChangedEvent& GetLocationChangedEvent();
		/**
		 * Gets the SizeChangedEvent from for the control.
		 *
		 * \return sizeChangedEvent
		 */
		SizeChangedEvent& GetSizeChangedEvent();
		/**
		 * Gets the Click event from the control.
		 *
		 * \return clickEvent
		 */
		ClickEvent& GetClickEvent();
		/**
		 * Gets the Mouse Click Event from the control.
		 *
		 * \return mouseClickEvent
		 */
		MouseClickEvent& GetMouseClickEvent();
		/**
		 * Gets the MouseDown event from the control.
		 *
		 * \return mouseDownEvent
		 */
		MouseDownEvent& GetMouseDownEvent();
		/**
		 * Gets the MouseUpEvent from for the control.
		 *
		 * \return mouseUpEvent
		 */
		MouseUpEvent& GetMouseUpEvent();
		/**
		 * Gets the MouseMove event from the control.
		 *
		 * \return mouseMoveEvent
		 */
		MouseMoveEvent& GetMouseMoveEvent();
		/**
		 * Gets the Mouse scroll event from the control.
		 *
		 * \return mouseScrollEvent
		 */
		MouseScrollEvent& GetMouseScrollEvent();
		/**
		 * Gets the MouseEnter event from the control.
		 *
		 * \return mouseEnterEvent
		 */
		MouseEnterEvent& GetMouseEnterEvent();
		/**
		 * Ruft das MouseLeaveEvent für das Steuerelement ab.
		 *
		 * \return mouseLeaveEvent
		 */
		MouseLeaveEvent& GetMouseLeaveEvent();
		/**
		 * Ruft das MouseCaptureChangedEvent für das Steuerelement ab.
		 *
		 * \return mouseCaptureChangedEvent
		 */
		MouseCaptureChangedEvent& GetMouseCaptureChangedEvent();
		/**
		 * Ruft das KeyDownEvent für das Steuerelement ab.
		 *
		 * \return keyPressEvent
		 */
		KeyDownEvent& GetKeyDownEvent();
		/**
		 * Ruft das KeyPressEvent für das Steuerelement ab.
		 *
		 * \return keyPressEvent
		 */
		KeyPressEvent& GetKeyPressEvent();
		/**
		 * Ruft das KeyUpEvent für das Steuerelement ab.
		 *
		 * \return keyPressEvent
		 */
		KeyUpEvent& GetKeyUpEvent();
		/**
		 * Ruft das FocusGotEvent für das Steuerelement ab.
		 *
		 * \return forcusInEvent
		 */
		FocusGotEvent& GetFocusGotEvent();
		/**
		 * Ruft das FocusLostEvent für das Steuerelement ab.
		 *
		 * \return forcusOutEvent
		 */
		FocusLostEvent& GetFocusLostEvent();
		/**
		 * Legt das übergeordnete Steuerelement fest.
		 *
		 * \param parent
		 */
		virtual void SetParent(Control *parent);
		/**
		 * Ruft das übergeordnete Steuerelement ab.
		 *
		 * \return parent
		 */
		virtual Control* GetParent() const;
		/**
		* Gibt eine Liste der untergeordneten Steuerelemente zurück.
		*
		* \return parent
		*/
		virtual const std::deque<Control*>& GetControls() const;

		PostOrderIterator GetPostOrderEnumerator();

		/**
		 * Sets input focus to the control.
		 */
		virtual void Focus();
		/**
		 * Checks to see if the point is within the control.
		 *
		 * \param point
		 * \return ja / nein
		 */
		virtual bool Intersect(const Drawing::PointI &point) const;
		/**
		 * Berechnet die absolute Position des Steuerelements.
		 */
		virtual void CalculateAbsoluteLocation();
		/**
		 * Computes the location of the specified screen point into client coordinates.
		 *
		 * \param point
		 * \return Clientkoordinaten
		 */
		virtual Drawing::PointI PointToClient(const Drawing::PointI &point) const;
		/**
		 * Computes the location of the specified client point into screen coordinates.
		 *
		 * \param point
		 * \return Bildschirmkoordinaten
		 */
		virtual Drawing::PointI PointToScreen(const Drawing::PointI &point) const;

		/**
		* Adds a child control added.
		*
		* \param control
		*/
		virtual void AddControl(Control *control);
		/**
		* Removes a child control.
		*
		* \param control
		*/
		virtual void RemoveControl(Control *control);
		/**
		* Retrieves the child control that is located at the specified coordinates.
		*
		* \param point
		* \return 0, falls sich dort kein Steuerelement befindet
		*/
		Control* GetChildAtPoint(const Drawing::PointI &point) const;
		/**
		* Retrieves the child control with the appropriate name.
		*
		* \param name
		* \return 0, falls kein Steuerelement mit diesem Namen existiert
		*/
		Control* GetChildByName(const Misc::AnsiString &name) const;
		
		/**
		 * Processes a mouse message. Should not be invoked by the user.
		 *
		 * \param mouse
		 * \return true, falls die Nachricht verarbeitet wurde
		 */
		bool ProcessMouseMessage(const MouseMessage &mouse);
		/**
		 * Processes a keyboard message. Should not be invoked by the user.
		 *
		 * \param keyboard
		 * \return true, falls die Nachricht verarbeitet wurde
		 */
		bool ProcessKeyboardMessage(const KeyboardMessage &keyboard);
		/**
		 * Causes the control to redraw.
		 */
		void Invalidate();
		/**
		 * Draws the control.
		 */
		virtual void Render();
		/**
		 * Forces the control to adjust its appearance the Theme.
		 *
		 * \param theme
		 */
		virtual void ApplyTheme(const Drawing::Theme &theme);
	
		/**
		 * Gets the Stringrepresentation of Control Types.
		 *
		 * \param controlType controltype
		 * \return Stringrepresentation
		 */
		static Misc::AnsiString ControlTypeToString(ControlType controlType);

		class PostOrderIterator
		{
		public:
			PostOrderIterator(Control *start);

			void operator++();
			bool operator()();
			Control* operator*();

		private:
			void LoopThrough(Control *container);

			Control *start_;
			Control *current_;
			std::vector<Control*> controlStack_;
		};

	private:
		void GetRenderContext(Drawing::RenderContext &context) const;

	protected:
		/**
		 * Konstruktor der Klasse.
		 */
		Control();

		virtual void InjectTime(const Misc::DateTime &time);

		virtual void OnLocationChanged();
		virtual void OnSizeChanged();
		virtual void OnMouseDown(const MouseMessage &mouse);
		virtual void OnMouseClick(const MouseMessage &mouse);
		virtual void OnMouseUp(const MouseMessage &mouse);
		virtual void OnMouseMove(const MouseMessage &mouse);
		virtual void OnMouseScroll(const MouseMessage &mouse);
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
		virtual void BufferGeometry(Drawing::RenderContext &context);
		virtual void QueueGeometry(Drawing::RenderContext &context);
		virtual void PopulateGeometry();
		
		void AddSubControl(Control* subcontrol);

		static const int DefaultBorderPadding = 6;

		std::deque<Control*> internalControls_;
		std::deque<Control*> controls_;

		Misc::AnsiString name_;
		ControlType type_;

		bool canRaiseEvents_;
		bool isEnabled_;
		bool isVisible_;
		bool isInside_;
		bool isClicked_;
		bool isFocusable_;
		bool isFocused_;
		bool hasCaptured_;
		bool autoSize_;
			 
		Misc::Any tag_;
		
		Drawing::PointI location_;
		Drawing::PointI absoluteLocation_;
		Drawing::SizeI size_;
		
		LocationChangedEvent locationChangedEvent_;
		SizeChangedEvent sizeChangedEvent_;
		ClickEvent clickEvent_;
		MouseClickEvent mouseClickEvent_;
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
		std::unique_ptr<Drawing::RenderSurface> surface_;

		Control *parent_;

	private:
		Control(const Control&);
		void operator=(const Control&);

		AnchorStyles anchor_;
	};
}

#endif