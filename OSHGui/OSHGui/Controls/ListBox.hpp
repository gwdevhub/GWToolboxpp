/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_LISTBOX_HPP
#define OSHGUI_LISTBOX_HPP

#include "Control.hpp"

namespace OSHGui {
	class ScrollBar;

	/**
	 * Tritt ein, wenn sich der Wert der SelectedIndex-Eigenschaft ändert.
	 */
	typedef Event<void(Control*)> SelectedIndexChangedEvent;
	typedef EventHandler<void(Control*)> SelectedIndexChangedEventHandler;

	/**
	 * Stellt ein Steuerlement zum Anzeigen einer Liste von Elementen dar.
	 */
	class OSHGUI_EXPORT ListBox : public Control {
	public:
		using Control::SetSize;

		/**
		 * Konstruktor der Klasse.
		 */
		ListBox(Control* parent);
		virtual ~ListBox();
		
		virtual void SetSize(const Drawing::SizeI &size) override;

		virtual void SetFont(const Drawing::FontPtr &font) override;

		void SetAutoScrollEnabled(bool autoScrollEnabled);
		bool GetAutoScrollEnabled() const;

		const Misc::UnicodeString& GetItem(int index) const;

		void SetSelectedIndex(int index);
		int GetSelectedIndex() const;

		void SetSelectedItem(const Misc::UnicodeString &item);
		const Misc::UnicodeString& GetSelectedItem() const;

		int GetItemsCount() const;

		SelectedIndexChangedEvent& GetSelectedIndexChangedEvent();

		void ExpandSizeToShowItems(int count);


		void AddItem(const Misc::UnicodeString &text);
		void InsertItem(int index, const Misc::UnicodeString &text);
		void RemoveItem(int index);
		void Clear();
	
	protected:
		virtual void PopulateGeometry() override;

		virtual void OnMouseLeave(const MouseMessage &mouse) override;
		virtual void OnMouseMove(const MouseMessage &mouse) override;
		virtual void OnMouseDown(const MouseMessage &mouse) override;
		virtual void OnMouseUp(const MouseMessage &mouse) override;
		virtual void OnMouseClick(const MouseMessage &mouse) override;
		virtual bool OnMouseScroll(const MouseMessage &mouse) override;
		virtual bool OnKeyDown(const KeyboardMessage &keyboard) override;
		virtual bool OnKeyPress(const KeyboardMessage &keyboard) override;

	private:
		static const Drawing::SizeI DefaultSize;
		static const Drawing::SizeI DefaultItemAreaPadding;
		static const int DefaultItemPadding;

		void CheckForScrollBar();
		void CheckForWidth();
		void ComputeItemHeight();

		void SetHoveredIndex(int index);
		
		int hoveredIndex_;
		int selectedIndex_;
		int firstVisibleItemIndex_;
		int itemHeight_;
		long maxVisibleItems_;
		bool autoScrollEnabled_;
		
		Drawing::RectangleI itemsRect_;
		Drawing::SizeI itemAreaSize_;
		
		std::vector<Misc::UnicodeString> items_;

		SelectedIndexChangedEvent selectedIndexChangedEvent_;

		ScrollBar *scrollBar_;
	};
}

#endif