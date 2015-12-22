/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_APPLICATION_HPP
#define OSHGUI_APPLICATION_HPP

#include <memory>
#include <vector>
#include "Drawing/Renderer.hpp"
#include "Drawing/RenderContext.hpp"
#include "Drawing/Font.hpp"
#include "Drawing/Theme.hpp"
#include "Misc/DateTime.hpp"
#include "Cursor/Cursor.hpp"
#include "Event/MouseMessage.hpp"
#include "Event/KeyboardMessage.hpp"
#include "Exports.hpp"
#include "FormManager.hpp"
#include "Event/Hotkey.hpp"

namespace OSHGui
{
	class Control;
	class Form;
	
	/**
	 * Provides methods and properties to manage an application available, 
	 * eg Methods to start and stop an application and 
	 * for retrieving information about an application.
	 */
	class OSHGUI_EXPORT Application
	{
		friend Control;
		friend Form;

	public:
		class GuiRenderSurface;

		/**
		 * Initializes the Application class.
		 *
		 * \param renderer Instance of the renderer used
		 */
		static void Initialize(std::unique_ptr<Drawing::Renderer> &&renderer);
		
		/**
		 * Gets whether the GUI is activated.
		 *
		 * return isEnabled
		 */
		const bool IsEnabled() const;
		/**
		 * Gets the current time.
		 *
		 * \return DateTime::Now
		 */
		const Misc::DateTime& GetNow() const;

		/**
		 * Gets the renderer used.
		 *
		 * \return renderer
		 */
		Drawing::Renderer& GetRenderer() const;
		/**
		 * Gets the Render Surface of Gui.
		 *
		 * \return GuiRenderSurface
		 */
		GuiRenderSurface& GetRenderSurface();
		/**
		 * Specifies the screen size.
		 *
		 * @param size
		 */
		void DisplaySizeChanged(const Drawing::SizeF &size);
		
		/**
		 * Sets the default font for the GUI.
		 *
		 * \param font Standardschrift
		 */
		void SetDefaultFont(const Drawing::FontPtr &font);
		/**
		 * Gets the default font for the GUI.
		 *
		 * \return Standardschrift
		 */
		Drawing::FontPtr& GetDefaultFont();
		
		/**
		 * Gets the current mouse position.
		 *
		 * \return cursorLocation
		 */
		const Drawing::PointF& GetCursorLocation() const;
		/**
		 * Gets the cursor.
		 *
		 * \return cursor
		 */
		const std::shared_ptr<Cursor>& GetCursor() const;
		/**
		 * Sets the cursor.
		 *
		 * \param cursor
		 */
		void SetCursor(const std::shared_ptr<Cursor> &cursor);
		/**
		 * Defines whether the cursor is to be drawn.
		 *
		 * \param enabled
		 */
		void SetCursorEnabled(bool enabled);
		
		/**
		 * Specifies the Theme for Gui.
		 *
		 * \param theme Theme
		 */
		void SetTheme(const Drawing::Theme &theme);
		/**
		 * Gets the Theme for Gui.
		 *
		 * \return Theme
		 */
		Drawing::Theme& GetTheme();
	
		/**
		 * Activates the GUI.
		 */
		void Enable();
		/**
		 * Disables the GUI.
		 */
		void Disable();
		/**
		 * Toggle between Enabled and Disabled.
		 */
		void Toggle();

		/**
		 * Removes focus from the GUI
		 */
		void clearFocus();

		/**
		 * Sets the main form of the GUI.
		 *
		 * \param mainForm the main form that is displayed when the GUI is activated
		 */
		void Run(const std::shared_ptr<Form> &mainForm);
		/**
		 * Is a Mouse Message to the open forms on.
		 *
		 * \param mouse
		 * \return true, if the message was processed
		 */
		bool ProcessMouseMessage(const MouseMessage &mouse);
		/**
		 * Indicates a keyboard message to the open forms on.
		 *
		 * \param keyboard
		 * \return true, if the message was processed
		 */
		bool ProcessKeyboardMessage(const KeyboardMessage &keyboard);
		
		/**
		 * Draws the open forms.
		 */
		void Render();

		/**
		 * Registers a new hotkey.
		 *
		 * \param hotkey
		 */
		void RegisterHotkey(const Hotkey &hotkey);
		/**
		 * Removes a hotkey.
		 *
		 * \param hotkey
		 */
		void UnregisterHotkey(const Hotkey &hotkey);
		
		/**
		 * Gets the current instance of the Application.
		 *
		 * \return instance
		 */
		static Application& Instance();
		static Application* InstancePtr();

		static bool HasBeenInitialized();

		class GuiRenderSurface : public Drawing::RenderSurface
		{
		public:
			GuiRenderSurface(Drawing::RenderTarget &target);

			void Invalidate();

			virtual void Draw() override;

		private:
			friend void Application::Render();

			bool needsRedraw_;
		};

	private:
		static Application *instance;
		Application(std::unique_ptr<Drawing::Renderer> &&renderer);

		//copying prohibited
		Application(const Application&);
		void operator=(const Application&);

		void InjectTime();

		std::unique_ptr<Drawing::Renderer> renderer_;
		GuiRenderSurface guiSurface_;
		Drawing::FontPtr defaultFont_;
		
		Drawing::Theme defaultTheme_;
		Drawing::Theme currentTheme_;
	
		FormManager formManager_;
		
		Misc::DateTime now_;

		struct
		{
			Drawing::PointF Location;
			std::shared_ptr<Cursor> Cursor;
			bool Enabled;
		} mouse_;

		std::vector<Hotkey> hotkeys_;

		Control *FocusedControl;
		Control *CaptureControl;
		Control *MouseEnteredControl;

		bool isEnabled_;
	};
}

#endif
