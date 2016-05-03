/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_GROUPBOX_HPP
#define OSHGUI_GROUPBOX_HPP

#include "Panel.hpp"

namespace OSHGui {
	class Label;

	/**
	 * Stellt ein Steuerelement dar, dass einen Rahmen um eine Gruppe
	 * von Steuerlementen anzeigt, der eine Beschriftung enthalten kann.
	 */
	class OSHGUI_EXPORT GroupBox : public Panel {
	public:
		using Panel::SetSize;

		/**
		 * Konstruktor der Klasse.
		 */
		GroupBox(Control* parent);

		virtual void SetSize(const Drawing::SizeI &size) override;

		void SetText(const Misc::UnicodeString &text);
		const Misc::UnicodeString& GetText() const;

		inline Panel* GetContainer() { return containerPanel_; }

		virtual void SetFont(const Drawing::FontPtr &font) override;

		virtual void SetForeColor(const Drawing::Color &color) override;

		virtual const std::deque<Control*>& GetControls() const override;
		virtual void AddControl(Control *control) override;
		virtual void RemoveControl(Control *control) override;

	protected:
		virtual void PopulateGeometry() override;
		
	private:
		Label *captionLabel_;
		Panel *containerPanel_;
	};
}

#endif