/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_RADIOBUTTON_HPP
#define OSHGUI_RADIOBUTTON_HPP

#include "CheckBox.hpp"

namespace OSHGui
{
	/**
	 * Ermöglicht Benutzern bei Verwendung weiterer RadioButton-Steuerelemete
	 * die Auswahl einer Option aus mehreren Auswahlmöglichkeiten.
	 */
	class OSHGUI_EXPORT RadioButton : public CheckBox
	{
	public:
		/**
		 * Konstruktor der Klasse.
		 */
		RadioButton();
		
		/**
		 * Legt den checked-Status fest und passt automatisch
		 * andere RadioButtons in der gleichen Gruppe an.
		 *
		 * \param checked
		 */
		virtual void SetChecked(bool checked) override;
		
	protected:
		void SetCheckedInternal(bool checked);

		virtual void PopulateGeometry() override;

		virtual void OnMouseClick(const MouseMessage &mouse) override;
		virtual bool OnKeyUp(const KeyboardMessage &keyboard) override;
	};
}

#endif