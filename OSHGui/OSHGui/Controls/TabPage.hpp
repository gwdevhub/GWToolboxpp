/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_TABPAGE_HPP
#define OSHGUI_TABPAGE_HPP

#include "Panel.hpp"
#include "TabControl.hpp"

namespace OSHGui
{
	/**
	 * Wird zum Gruppieren von Auflistungen von Steuerelementen verwendet.
	 */
	class OSHGUI_EXPORT TabPage : public Panel
	{
		friend TabControl;

	public:
		using Panel::SetSize;

		/**
		 * Konstruktor der Klasse.
		 */
		TabPage(Control* parent);
		
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
		void SetText(const Misc::UnicodeString &text);
		/**
		 * Ruft den Text ab.
		 *
		 * \return der Text
		 */
		const Misc::UnicodeString& GetText() const;

		inline Panel* GetContainer() { return containerPanel_; }

	protected:
		virtual void PopulateGeometry();

	private:
		Misc::UnicodeString text_;

		Panel *containerPanel_;
		TabControl::TabControlButton *button_;
	};
}

#endif