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

namespace OSHGui {
	class Control;
	class Form;
	
	/**
	 * Provides methods and properties to manage an application available, 
	 * eg Methods to start and stop an application and 
	 * for retrieving information about an application.
	 */
	class OSHGUI_EXPORT Application {
		friend Control;
		friend Form;

	public:

		static void Initialize(std::unique_ptr<Drawing::Renderer> &&renderer);
		
		const bool IsEnabled() const;

		const Misc::DateTime& GetNow() const;


		Drawing::Renderer& GetRenderer() const;
		Drawing::RenderSurface& GetRenderSurface();
		void Invalidate();
		/**
		 * Specifies the screen size.
		 */
		void DisplaySizeChanged(const Drawing::SizeI &size);
		
		void SetDefaultFont(const Drawing::FontPtr &font);
		Drawing::FontPtr& GetDefaultFont();
		

		const Drawing::PointI& GetCursorLocation() const;

		const std::shared_ptr<Cursor>& GetCursor() const;
		void SetCursor(const std::shared_ptr<Cursor> &cursor);
		/**
		 * Defines whether the cursor is to be drawn.
		 */
		void SetCursorEnabled(bool enabled);
		
		/**
		 * Specifies the Theme for Gui.
		 */
		void SetTheme(const Drawing::Theme &theme);
		Drawing::Theme& GetTheme();
	
		void Enable();
		void Disable();
		void Toggle(); // Toggles between Enabled and Disabled

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
		 * \return true, if the message was processed
		 */
		bool ProcessMouseMessage(const MouseMessage &mouse);
		/**
		 * Indicates a keyboard message to the open forms on.
		 *
		 * \return true, if the message was processed
		 */
		bool ProcessKeyboardMessage(const KeyboardMessage &keyboard);
		
		void Render();

		void RegisterHotkey(const Hotkey &hotkey);
		void UnregisterHotkey(const Hotkey &hotkey);
		
		static Application& Instance();
		static Application* InstancePtr();

		static bool HasBeenInitialized();

	private:
		static Application *instance;
		Application(std::unique_ptr<Drawing::Renderer> &&renderer);

		//copying prohibited
		Application(const Application&);
		void operator=(const Application&);

		void InjectTime();

		std::unique_ptr<Drawing::Renderer> renderer_;
		Drawing::RenderSurface renderSurface_;
		bool needsRedraw_;

		Drawing::FontPtr defaultFont_;
		
		Drawing::Theme defaultTheme_;
		Drawing::Theme currentTheme_;
	
		FormManager formManager_;
		
		Misc::DateTime now_;

		struct {
			Drawing::PointI Location;
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
