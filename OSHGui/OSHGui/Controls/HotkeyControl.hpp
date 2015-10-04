/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_HOTKEYCONTROL_HPP
#define OSHGUI_HOTKEYCONTROL_HPP

#include "Control.hpp"
#include <map>

namespace OSHGui
{
	/**
	 * Tritt ein, wenn sich der Wert der Hotkey/Modifier-Eigenschaft ändert.
	 */
	typedef Event<void(Control*)> HotkeyChangedEvent;
	typedef EventHandler<void(Control*)> HotkeyChangedEventHandler;

	class TextBox;
	
	/**
	 * Stellt ein Textfeld-Steuerelement dar.
	 */
	class OSHGUI_EXPORT HotkeyControl : public Control
	{
	public:
		using Control::SetSize;
	
		/**
		 * Konstruktor der Klasse.
		 */
		HotkeyControl();

		/**
		 * Legt die Höhe und Breite des Steuerelements fest.
		 *
		 * \param size
		 */
		virtual void SetSize(const Drawing::SizeI &size);
		/**
		 * Legt die Schriftart des Texts im Steuerelement fest.
		 *
		 * \param font
		 */
		virtual void SetFont(const Drawing::FontPtr &font) override;
		/**
		 * Legt die Fordergrundfarbe des Steuerelements fest.
		 *
		 * \param color
		 */
		virtual void SetForeColor(const Drawing::Color &color) override;
		/**
		 * Legt die Hintergrundfarbe des Steuerelements fest.
		 *
		 * \param color
		 */
		virtual void SetBackColor(const Drawing::Color &color) override;
		
		/**
		 * Legt den Hotkey fest.
		 *
		 * \param hotkey
		 */
		virtual void SetHotkey(Key hotkey);
		/**
		 * Ruft den Hotkey ab.
		 *
		 * \return hotkey
		 */
		virtual Key GetHotkey() const;
		/**
		 * Legt den HotkeyModifier fest.
		 *
		 * \param hotkey
		 */
		virtual void SetHotkeyModifier(Key modifier);
		/**
		 * Ruft den HotkeyModifier ab.
		 *
		 * \return hotkeyModifier
		 */
		virtual Key GetHotkeyModifier() const;

		/**
		 * Ruft das HotkeyChangedEvent für das Steuerelement ab.
		 *
		 * \return hotkeyChangedEvent
		 */
		HotkeyChangedEvent& GetHotkeyChangedEvent();
		
		/**
		 * Setzt den Hotkey und den HotkeyModifier auf None.
		 */
		void ClearHotkey();

		/**
		 * Berechnet die absolute Position des Steuerelements.
		 */
		virtual void CalculateAbsoluteLocation() override;
	
	protected:
		virtual void DrawSelf(Drawing::RenderContext &context) override;
		virtual void PopulateGeometry() override;

		virtual void OnMouseClick(const MouseMessage &mouse) override;
		virtual bool OnKeyDown(const KeyboardMessage &keyboard) override;
		virtual bool OnKeyPress(const KeyboardMessage &keyboard) override;
		virtual bool OnKeyUp(const KeyboardMessage &keyboard) override;

	private:
		void HotkeyToText();

		static const Drawing::SizeI DefaultSize;
		static std::map<Key, Misc::AnsiString> HotkeyNames;
		
		std::unique_ptr<TextBox> textBox_;
		
		Drawing::PointI clearButtonLocation_;

		Key hotkey_;
		Key modifier_;

		HotkeyChangedEvent hotkeyChangedEvent_;
	};
}

#endif