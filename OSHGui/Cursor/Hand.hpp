/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_CURSOR_HAND_HPP
#define OSHGUI_CURSOR_HAND_HPP

#include "Cursor.hpp"

namespace OSHGui
{
	/**
	 * Hand-Cursor
	 */
	class HandCursor : public Cursor
	{
	protected:
		virtual void Initialize() override;
	};
}

#endif