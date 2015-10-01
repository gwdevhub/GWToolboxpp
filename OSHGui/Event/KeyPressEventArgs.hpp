/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_EVENT_KEYPRESSEVENTARGS_HPP
#define OSHGUI_EVENT_KEYPRESSEVENTARGS_HPP

#include "../Misc/Strings.hpp"
#include "KeyboardMessage.hpp"

namespace OSHGui
{
	/**
	 * Stellt Daten für das KeyPressEvent bereit.
	 */
	class OSHGUI_EXPORT KeyPressEventArgs
	{
	public:
		Misc::AnsiChar KeyChar;
		bool Handled;
		
	public:
		/**
		 * Konstruktor der Klasse
		 */
		KeyPressEventArgs(const KeyboardMessage &keyboardEvent)
			: KeyChar(keyboardEvent.GetKeyChar()),
			  Handled(false)
		{

		}
	};
}

#endif