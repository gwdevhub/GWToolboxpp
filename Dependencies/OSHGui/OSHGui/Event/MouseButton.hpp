/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_EVENT_MOUSEBUTTON_HPP
#define OSHGUI_EVENT_MOUSEBUTTON_HPP

namespace OSHGui
{
	/**
	 * Gibt Konstanten an, die definieren, welche Maustaste gedrückt wurde.
	 */
	enum class MouseButton
	{
		None = 0,
		Left,
		Right,
		Middle,
		XButton1,
		XButton2
	};
}

#endif