/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "Cursors.hpp"
#include "Cursor.hpp"
#include "IBeam.hpp"
#include "Pipette.hpp"
#include "Hand.hpp"
#include "Circle.hpp"
#include "Wait.hpp"
#include "Pen.hpp"

namespace OSHGui
{
	std::map<Cursors::CursorType, std::shared_ptr<Cursor> > Cursors::cursors;

	//---------------------------------------------------------------------------
	//Runtime-Functions
	//---------------------------------------------------------------------------
	const std::shared_ptr<Cursor>& Cursors::Get(Cursors::CursorType cursorType)
	{
		auto it = cursors.find(cursorType);
		if (it == cursors.end())
		{
			std::shared_ptr<Cursor> cursor;
			switch (cursorType)
			{
				case Cursors::IBeam:
					cursor = std::make_shared<IBeamCursor>();
					break;
				case Cursors::Pipette:
					cursor = std::make_shared<PipetteCursor>();
					break;
				case Cursors::Hand:
					cursor = std::make_shared<HandCursor>();
					break;
				case Cursors::Circle:
					cursor = std::make_shared<CircleCursor>();
					break;
				case Cursors::Wait:
					cursor = std::make_shared<WaitCursor>();
					break;
				case Cursors::Pen:
					cursor = std::make_shared<PenCursor>();
					break;
				default:
					cursor = std::make_shared<Cursor>();
					break;
			}
			cursor->Initialize();

			cursors[cursorType] = cursor;
			return cursors[cursorType];
		}
		return (*it).second;
	}
	//---------------------------------------------------------------------------
}