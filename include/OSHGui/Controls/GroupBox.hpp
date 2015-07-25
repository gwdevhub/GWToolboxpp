/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_GROUPBOX_HPP
#define OSHGUI_GROUPBOX_HPP

#include "Panel.hpp"

namespace OSHGui
{
	class Label;

	/**
	 * Stellt ein Steuerelement dar, dass einen Rahmen um eine Gruppe
	 * von Steuerlementen anzeigt, der eine Beschriftung enthalten kann.
	 */
	class OSHGUI_EXPORT GroupBox : public Panel
	{
	public:
		using Panel::SetSize;

		/**
		 * Konstruktor der Klasse.
		 */
		GroupBox();

		/**
		 * Legt die Höhe und Breite des Steuerelements fest.
		 *
		 * \param size
		 */
		virtual void SetSize(const Drawing::SizeI &size) override;
		/**
		 * Legt den Text fest.
		 *
		 * \param text
		 */
		void SetText(const Misc::AnsiString &text);
		/**
		 * Gibt den Text zurück.
		 *
		 * \return der Text
		 */
		const Misc::AnsiString& GetText() const;
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
		 * Gibt eine Liste der untergeordneten Steuerelemente zurück.
		 *
		 * \return parent
		 */
		virtual const std::deque<Control*>& GetControls() const override;
		
		/**
		 * Fügt ein untergeordnetes Steuerelement hinzu.
		 *
		 * \param control
		 */
		virtual void AddControl(Control *control) override;
		/**
		 * Entfernt ein untergeordnetes Steuerelement.
		 *
		 * \param control
		 */
		virtual void RemoveControl(Control *control) override;

		virtual void DrawSelf(Drawing::RenderContext &context) override;

	protected:
		virtual void PopulateGeometry() override;
		
	private:
		Label *captionLabel_;
		Panel *containerPanel_;
	};
}

#endif