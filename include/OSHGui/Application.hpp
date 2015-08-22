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
	 * Stellt Methoden und Eigenschaften für die Verwaltung einer
	 * Anwendung zur Verfügung, z.B. Methoden zum Starten und Beenden einer
	 * Anwendung sowie für das Abrufen von Informationen zu einer Anwendung.
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
		 * \param renderer Instanz des verwendeten Renderers
		 */
		static void Initialize(std::unique_ptr<Drawing::Renderer> &&renderer);
		
		/**
		 * Gets whether the GUI is activated.
		 *
		 * return isEnabled
		 */
		const bool IsEnabled() const;
		/**
		 * Ruft die aktuelle Uhrzeit ab.
		 *
		 * \return DateTime::Now
		 */
		const Misc::DateTime& GetNow() const;

		/**
		 * Ruft den verwendeten Renderer ab.
		 *
		 * \return renderer
		 */
		Drawing::Renderer& GetRenderer() const;
		/**
		 * Ruft das RenderSurface der Gui ab.
		 *
		 * \return GuiRenderSurface
		 */
		GuiRenderSurface& GetRenderSurface();
		/**
		 * Legt die Display-Größe fest.
		 *
		 * @param size
		 */
		void DisplaySizeChanged(const Drawing::SizeF &size);
		
		/**
		 * Legt die Standardschrift für das Gui fest.
		 *
		 * \param font Standardschrift
		 */
		void SetDefaultFont(const Drawing::FontPtr &font);
		/**
		 * Ruft die Standardschrift für das Gui ab.
		 *
		 * \return Standardschrift
		 */
		Drawing::FontPtr& GetDefaultFont();
		
		/**
		 * Ruft die aktuelle Mausposition ab.
		 *
		 * \return cursorLocation
		 */
		const Drawing::PointF& GetCursorLocation() const;
		/**
		 * Ruft den Cursor ab.
		 *
		 * \return cursor
		 */
		const std::shared_ptr<Cursor>& GetCursor() const;
		/**
		 * Legt den Cursor fest.
		 *
		 * \param cursor
		 */
		void SetCursor(const std::shared_ptr<Cursor> &cursor);
		/**
		 * Legt fest, ob der Cursor gezeichnet werden soll.
		 *
		 * \param enabled
		 */
		void SetCursorEnabled(bool enabled);
		
		/**
		 * Legt das Theme für das Gui fest.
		 *
		 * \param theme Theme
		 */
		void SetTheme(const Drawing::Theme &theme);
		/**
		 * Ruft das Theme für das Gui ab.
		 *
		 * \return Theme
		 */
		const Drawing::Theme& GetTheme() const;
	
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
		 * \return true, falls die Nachricht verarbeitet wurde
		 */
		bool ProcessMouseMessage(const MouseMessage &mouse);
		/**
		 * Gibt eine KeyboardMessage an die geöffneten Formen weiter.
		 *
		 * \param keyboard
		 * \return true, falls die Nachricht verarbeitet wurde
		 */
		bool ProcessKeyboardMessage(const KeyboardMessage &keyboard);
		
		/**
		 * Zeichnet die geöffneten Formen.
		 */
		void Render();

		/**
		 * Registriert einen neuen Hotkey.
		 *
		 * \param hotkey
		 */
		void RegisterHotkey(const Hotkey &hotkey);
		/**
		 * Entfernt einen Hotkey.
		 *
		 * \param hotkey
		 */
		void UnregisterHotkey(const Hotkey &hotkey);
		
		/**
		 * Ruft die aktuelle Instanz der Application ab.
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
