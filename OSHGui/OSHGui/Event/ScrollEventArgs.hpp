/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_EVENT_SCROLLEVENTARGS_HPP
#define OSHGUI_EVENT_SCROLLEVENTARGS_HPP

namespace OSHGui
{
	/**
	 * Stellt Daten für das ScrollEvent bereit.
	 */
	class OSHGUI_EXPORT ScrollEventArgs
	{
	public:
		int NewValue;
		int OldValue;
		
	public:
		/**
		 * Konstruktor der Klasse
		 *
		 * \param newValue der neue Scroll-Wert
		 * \param oldValue der alte Scroll-Wert
		 */
		ScrollEventArgs(int newValue, int oldValue)
			: NewValue(newValue),
			  OldValue(oldValue)
		{
			
		}
	};
}

#endif