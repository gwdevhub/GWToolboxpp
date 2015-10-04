/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_CURSOR_CIRCLE_HPP
#define OSHGUI_CURSOR_CIRCLE_HPP

#include "Cursor.hpp"

namespace OSHGui
{
	/**
	 * Kreis-Cursor
	 */
	class CircleCursor : public Cursor
	{
	protected:
		virtual void Initialize() override;
	};
}

#endif