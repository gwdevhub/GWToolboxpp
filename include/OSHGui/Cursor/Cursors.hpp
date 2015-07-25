/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_CURSOR_CURSORS_HPP
#define OSHGUI_CURSOR_CURSORS_HPP

#include <map>
#include <memory>
#include "Cursor.hpp"

namespace OSHGui
{
	/**
	 * Stellt eine Auflistung von Cursorn bereit.
	 */
	class Cursors
	{
	public:
		enum CursorType
		{
			/**
			 * der Standardcursor
			 */
			Default,
			/**
			 * der Textauswahlcursor
			 */
			IBeam,
			/**
			 * der Farbwahlcursor
			 */
			Pipette,
			/**
			 * der Handcursor
			 */
			Hand,
			/**
			 * der Kreiscursor
			 */
			Circle,
			/**
			 * der Wartecursor
			 */
			Wait,
			/**
			 * der Stiftcursor
			 */
			Pen
		};

		/**
		 * Ruft den angegebenen Cursor ab.
		 *
		 * \param cursorType der gewünschte Cursor
		 * \return cursor
		 */
		static const std::shared_ptr<Cursor>& Get(CursorType cursorType);

	private:
		static std::map<CursorType, std::shared_ptr<Cursor> > Cursors::cursors;
	};
}

#endif