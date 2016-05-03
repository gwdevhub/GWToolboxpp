/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_BUTTON_HPP
#define OSHGUI_BUTTON_HPP

#include "Control.hpp"
#include "Label.hpp"

namespace OSHGui {
	class OSHGUI_EXPORT Button : public Control {
	public:
		using Control::SetSize;

		Button(Control* parent);

		virtual void SetAutoSize(bool autoSize) override;

		virtual void SetSize(const Drawing::SizeI &size) override;

		virtual void SetLocation(const Drawing::PointI &location) override;

		void SetText(const Misc::UnicodeString &text);
		const Misc::UnicodeString& GetText() const;

		virtual void SetFont(const Drawing::FontPtr &font) override;

		virtual void SetForeColor(const Drawing::Color &color) override;
		
	protected:
		virtual void CalculateLabelLocation();
		
		virtual void PopulateGeometry();

		virtual bool OnKeyUp(const KeyboardMessage &keyboard) override;

		Label* label_;

	private:
		static const Drawing::SizeI DefaultSize;
		static const Drawing::PointI DefaultLabelOffset;
	};
}

#endif