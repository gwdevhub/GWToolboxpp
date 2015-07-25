/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_LINKLABEL_HPP
#define OSHGUI_LINKLABEL_HPP

#include "Label.hpp"

namespace OSHGui
{
	/**
	 * Stellt ein Label-Steuerelement dar, das Links anzeigen kann.
	 */
	class OSHGUI_EXPORT LinkLabel : public Label
	{
	public:
		/**
		 * Konstruktor der Klasse.
		 */
		LinkLabel();

		virtual bool Intersect(const Drawing::PointI &point) const override;

	protected:
		virtual void PopulateGeometry() override;
	};
}

#endif