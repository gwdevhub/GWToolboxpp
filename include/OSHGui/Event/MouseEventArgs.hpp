/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_EVENT_MOUSEEVENTARGS_HPP
#define OSHGUI_EVENT_MOUSEEVENTARGS_HPP

#include "../Drawing/Point.hpp"
#include "MouseMessage.hpp"

namespace OSHGui
{
	/**
	 * Stellt Daten für MouseEvents bereit.
	 */
	class OSHGUI_EXPORT MouseEventArgs
	{
	public:
		MouseState State;
		Drawing::PointI Location;
		
	public:
		/**
		 * Konstruktor der Klasse
		 */
		MouseEventArgs(const MouseMessage &mouseEvent)
			: State(mouseEvent.GetState()),
			  Location(mouseEvent.GetLocation())
		{
			
		}
	};
}

#endif