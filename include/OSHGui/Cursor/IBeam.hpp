/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_CURSOR_IBEAM_HPP
#define OSHGUI_CURSOR_IBEAM_HPP

#include "Cursor.hpp"

namespace OSHGui
{
	/**
	 * Textauswahl-Cursor
	 */
	class IBeamCursor : public Cursor
	{
	protected:
		virtual void Initialize() override;
	};
}

#endif