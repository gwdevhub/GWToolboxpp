/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_COMBOBOX_HPP
#define OSHGUI_COMBOBOX_HPP

#include "Control.hpp"
#include "Button.hpp"

namespace OSHGui {
	class ListBox;
	class ScrollBar;

	/**
	 * Tritt ein, wenn sich der Wert der SelectedIndex-Eigenschaft ändert.
	 */
	typedef Event<void(Control*)> SelectedIndexChangedEvent;
	typedef EventHandler<void(Control*)> SelectedIndexChangedEventHandler;

	/**
	 * Stellt ein Kombinationsfeld-Steuerelement dar.
	 */
	class OSHGUI_EXPORT ComboBox : public Control {
	public:
		using Control::SetSize;

		/**
		 * Konstruktor der Klasse.
		 */
		ComboBox(Control* parent);

		virtual void SetSize(const Drawing::SizeI &size) override;
		virtual void SetAutoSize(bool b) override;

		virtual void SetFont(const Drawing::FontPtr &font) override;

		virtual void SetForeColor(const Drawing::Color &color) override;

		virtual void SetBackColor(const Drawing::Color &color) override;

		virtual bool GetIsFocused() const override;

		void SetText(const Misc::UnicodeString &text);

		const Misc::UnicodeString& GetText() const;

		const Misc::UnicodeString& GetItem(int index) const;

		void SetSelectedIndex(int index);

		int GetSelectedIndex() const;

		void SetSelectedItem(const Misc::UnicodeString &item);

		const Misc::UnicodeString& GetSelectedItem() const;

		int GetItemsCount() const;

		void SetMaxShowItems(int items);

		int GetMaxShowItems() const;

		SelectedIndexChangedEvent& GetSelectedIndexChangedEvent();
		
		void AddItem(const Misc::UnicodeString &text);

		void InsertItem(int index, const Misc::UnicodeString &text);

		void RemoveItem(int index);

		void Clear();

		virtual bool Intersect(const Drawing::PointI &point) const override;

		virtual void Focus() override;

	private:
		static const int DefaultMaxShowItems;

		void Expand();
		void Collapse();
		
		int maxShowItems_;
		bool droppedDown_;
		
		Drawing::Color dropDownColor_;
		
		class ComboBoxButton : public Button {
		public:
			using Button::SetSize;

			ComboBoxButton(Control* parent) : Button(parent) {}

			virtual void SetSize(const Drawing::SizeI &size) override;

			virtual bool Intersect(const Drawing::PointI &point) const override;

		protected:
			virtual void CalculateLabelLocation() override;

			virtual void PopulateGeometry() override;

			virtual bool OnKeyDown(const KeyboardMessage &keyboard) override;

		private:
			Drawing::SizeI realSize_;
			Drawing::PointI arrowAbsoluteLocation_;
		};

		protected:
		ComboBoxButton *button_;
		ListBox *listBox_;
	};
}

#endif