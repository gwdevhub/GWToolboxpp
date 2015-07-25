/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_CURSOR_CURSOR_HPP
#define OSHGUI_CURSOR_CURSOR_HPP

#include <memory>
#include "../Exports.hpp"
#include "../Drawing/Point.hpp"
#include "../Drawing/GeometryBuffer.hpp"

namespace OSHGui
{
	class Cursors;

	/**
	 * Standard-Cursor
	 */
	class OSHGUI_EXPORT Cursor
	{
		friend class Cursors;

	public:
		/**
		 * Konstruktor der Klasse
		 */
		Cursor();
		virtual ~Cursor();
	
		virtual Drawing::GeometryBufferPtr GetGeometry();
		
	protected:
		virtual void Initialize();

		Drawing::GeometryBufferPtr geometry_;
	};

	typedef std::shared_ptr<Cursor> CursorPtr;
}

#endif