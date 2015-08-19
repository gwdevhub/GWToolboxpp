/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_PANEL_HPP
#define OSHGUI_PANEL_HPP

#include "Control.hpp"

namespace OSHGui
{
	/**
	 * Used to group collections of controls.
	 */
	class OSHGUI_EXPORT Panel : public Control
	{
	public:
		/**
		 * Constructor of the class.
		 */
		Panel();

	protected:
		virtual void PopulateGeometry() override;

	private:
		static const Drawing::SizeI DefaultSize;
	};
}

#endif