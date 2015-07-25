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

namespace OSHGui
{
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
	class OSHGUI_EXPORT ComboBox : public Control
	{
	public:
		using Control::SetSize;

		/**
		 * Konstruktor der Klasse.
		 */
		ComboBox();
		
		/**
		 * Legt die Höhe und Breite des Steuerelements fest.
		 *
		 * \param size
		 */
		virtual void SetSize(const Drawing::SizeI &size) override;
		/**
		 * Legt die Schriftart des Texts im Steuerelement fest.
		 *
		 * \param font
		 */
		virtual void SetFont(const Drawing::FontPtr &font) override;
		/**
		 * Legt die Fordergrundfarbe des Steuerelements fest.
		 *
		 * \param color
		 */
		virtual void SetForeColor(const Drawing::Color &color) override;
		/**
		 * Legt die Hintergrundfarbe des Steuerelements fest.
		 *
		 * \param color
		 */
		virtual void SetBackColor(const Drawing::Color &color) override;
		/**
		 * Ruft ab, ob das Steuerelement den Fokus besitzt.
		 *
		 * \return isFocused
		 */
		virtual bool GetIsFocused() const override;
		/**
		 * Legt den Text fest. Falls autoSize = true, wird die Größe automatisch angepasst.
		 *
		 * \param text
		 */
		void SetText(const Misc::AnsiString &text);
		/**
		 * Ruft den Text ab.
		 *
		 * \return der Text
		 */
		const Misc::AnsiString& GetText() const;
		/**
		 * Gibt das Item an der Stelle index zurück.
		 *
		 * \param index
		 * \return das Item
		 */
		const Misc::AnsiString& GetItem(int index) const;
		/**
		 * Legt den ausgewählten Index fest.
		 *
		 * \param index
		 */
		void SetSelectedIndex(int index);
		/**
		 * Gibt den ausgewählten Index zurück.
		 *
		 * \return der ausgewählte Index
		 */
		int GetSelectedIndex() const;
		/**
		 * Legt das ausgewählte Item fest.
		 *
		 * \param item
		 */
		void SetSelectedItem(const Misc::AnsiString &item);
		/**
		 * Ruft das ausgewählte Item ab.
		 *
		 * \return das Item
		 */
		const Misc::AnsiString& GetSelectedItem() const;
		/**
		 * Gibt die Anzahl der Items zurück.
		 *
		 * \return Anzahl der Items
		 */
		int GetItemsCount() const;
		/**
		 * Legt die anzahl an Items fest, die ausgeklappt angezeigt werden.
		 *
		 * \param items
		 */
		void SetMaxShowItems(int items);
		/**
		 * Ruft die Anzahl der Items ab, die ausgeklappt angezeigt werden.
		 *
		 * \return items
		 */
		int GetMaxShowItems() const;
		/**
		 * Ruft das SelectedIndexEvent für das Steuerelement ab.
		 *
		 * \return selectedIndexEvent
		 */
		SelectedIndexChangedEvent& GetSelectedIndexChangedEvent();
		
		/**
		 * Fügt ein neues Item hinzu.
		 *
		 * \param text der Text des Items
		 */
		void AddItem(const Misc::AnsiString &text);
		/**
		 * Fügt ein neues Item am gewählten Index hinzu.
		 *
		 * \param index
		 * \param text der Text des Items
		 */
		void InsertItem(int index, const Misc::AnsiString &text);
		/**
		 * Löscht das Item am gewählten Index.
		 *
		 * \param index
		 */
		void RemoveItem(int index);
		/**
		 * Löscht alle Items.
		 */
		void Clear();
		/**
		 * Überprüft, ob sich der Punkt innerhalb des Steuerelements befindet.
		 *
		 * \param point
		 * \return ja / nein
		 */
		virtual bool Intersect(const Drawing::PointI &point) const override;
		/**
		 * Setzt den Eingabefokus auf das Steuerelement.
		 */
		virtual void Focus() override;
	
	protected:
		virtual void DrawSelf(Drawing::RenderContext &context) override;

	private:
		static const int DefaultMaxShowItems;

		void Expand();
		void Collapse();
		
		int maxShowItems_;
		bool droppedDown_;
		
		Drawing::Color dropDownColor_;
		
		class ComboBoxButton : public Button
		{
		public:
			using Button::SetSize;

			virtual void SetSize(const Drawing::SizeI &size) override;

			virtual bool Intersect(const Drawing::PointI &point) const override;

		protected:
			virtual void CalculateLabelLocation() override;

			virtual void DrawSelf(Drawing::RenderContext &context) override;
			virtual void PopulateGeometry() override;

			virtual bool OnKeyDown(const KeyboardMessage &keyboard) override;

		private:
			Drawing::SizeI realSize_;
			Drawing::PointI arrowAbsoluteLocation_;
		};

		ComboBoxButton *button_;
		ListBox *listBox_;
	};
}

#endif