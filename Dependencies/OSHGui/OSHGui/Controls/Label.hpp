/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_LABEL_HPP
#define OSHGUI_LABEL_HPP

#include "Control.hpp"
#include "../Misc/TextHelper.hpp"

namespace OSHGui {
	/**
	 * Stellt ein Label-Steuerelement dar.
	 */
	class OSHGUI_EXPORT Label : public Control {
	public:
		Label(Control* parent);
		

		void SetText(const Misc::UnicodeString &text);
		const Misc::UnicodeString& GetText() const;

		virtual void SetFont(const Drawing::FontPtr &font) override;
		
	protected:
		virtual void PopulateGeometry() override;

		Misc::TextHelper textHelper_;
	};
}

#endif