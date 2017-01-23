/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_CHECKBOX_HPP
#define OSHGUI_CHECKBOX_HPP

#include "Control.hpp"

namespace OSHGui {
	class Label;

	typedef Event<void(Control*)> CheckedChangedEvent;
	typedef EventHandler<void(Control*)> CheckedChangedEventHandler;
	
	class OSHGUI_EXPORT CheckBox : public Control {
	public:

		CheckBox(Control* parent);

		virtual void SetChecked(bool checked);
		virtual bool GetChecked() const;

		void SetText(const Misc::UnicodeString &text);
		const Misc::UnicodeString& GetText() const;

		virtual void SetFont(const Drawing::FontPtr &font) override;

		virtual void SetForeColor(const Drawing::Color &color) override;

		CheckedChangedEvent& GetCheckedChangedEvent();

		virtual void CalculateAbsoluteLocation() override;
	
	protected:
		static const Drawing::PointI DefaultLabelOffset;
		static const int DefaultCheckBoxSize = 17;

		virtual void PopulateGeometry() override;

		virtual void OnMouseClick(const MouseMessage &mouse) override;
		virtual bool OnKeyUp(const KeyboardMessage &keyboard) override;

		bool checked_;
		Drawing::PointI checkBoxLocation_;
		Drawing::PointI textLocation_;
		Drawing::PointI checkBoxAbsoluteLocation_;
		
		CheckedChangedEvent checkedChangedEvent_;

		Label* label_;
	};
}

#endif
