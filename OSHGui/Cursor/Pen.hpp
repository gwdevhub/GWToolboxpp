/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_CURSOR_PEN_HPP
#define OSHGUI_CURSOR_PEN_HPP

#include "Cursor.hpp"

namespace OSHGui
{
	/**
	 * Stift-Cursor
	 */
	class PenCursor : public Cursor
	{
	protected:
		virtual void Initialize() override;
	};
}

#endif