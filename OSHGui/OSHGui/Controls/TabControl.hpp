/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_TABCONTROL_HPP
#define OSHGUI_TABCONTROL_HPP

#include <memory>
#include "Control.hpp"

namespace OSHGui {
	class Label;
	class TabPage;

	/**
	 * Tritt ein, wenn sich der Wert der SelectedIndex-Eigenschaft ändert.
	 */
	typedef Event<void(Control*)> SelectedIndexChangedEvent;
	typedef EventHandler<void(Control*)> SelectedIndexChangedEventHandler;

	/**
	 * Verwaltet eine Gruppe zusammengehöriger Registerkarten.
	 */
	 // TabControl is untested with the new OSHGui rework - bugs are likely
	class OSHGUI_EXPORT TabControl : public Control {
		friend TabPage;

	public:
		using Control::SetSize;

		/**
		 * Konstruktor der Klasse.
		 */
		TabControl(Control* parent);

		/**
		 * Legt die Höhe und Breite des Steuerelements fest.
		 *
		 * \param size
		 */
		virtual void SetSize(const Drawing::SizeI &size) override;
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
		 * Ruft die TabPage mit dem entsprechenden Namen ab.
		 *
		 * \param name der Name
		 * \return Zeiger zur TabPage
		 */
		TabPage* GetTabPage(const Misc::UnicodeString &name) const;
		/**
		 * Ruft die TabPage am entsprechenden Index ab.
		 *
		 * \param index
		 * \return Zeiger zur TabPage
		 */
		TabPage* GetTabPage(int index) const;
		/**
		 * Legt den Index der derzeit ausgewählten Registerkarte fest.
		 *
		 * \param index
		 */
		void SetSelectedIndex(int index);
		/**
		 * Ruft den Index der derzeit ausgewählten Registerkarte ab.
		 *
		 * \return index
		 */
		int GetSelectedIndex() const;
		/**
		 * Legt die derzeit ausgewählte Registerkarte fest.
		 *
		 * \param tabPage
		 */
		void SetSelectedTabPage(TabPage *tabPage);
		/**
		 * Ruft die derzeit ausgewählte Registerkarte ab.
		 *
		 * \return selectedTabPage
		 */
		TabPage* GetSelectedTabPage() const;
		/**
		 * Ruft das SelectedIndexEvent für das Steuerelement ab.
		 *
		 * \return selectedIndexEvent
		 */
		SelectedIndexChangedEvent& GetSelectedIndexChangedEvent();
		
		void AddTabPage(TabPage *tabPage);
		void RemoveTabPage(TabPage *tabPage);
		
		virtual void ApplyTheme(const Drawing::Theme &theme) override;

		virtual void CalculateAbsoluteLocation() override;

	private:
		static const Drawing::SizeI DefaultSize;

		void CalculateButtonLocationAndCount();

		struct TabPageButtonBinding;
		void SelectBinding(TabPageButtonBinding &binding);

		SelectedIndexChangedEvent selectedIndexChangedEvent_;

		class TabControlButton;
		struct TabPageButtonBinding {
			int Index;
			TabPage *TabPage;
			TabControlButton *Button;
		};

		class TabControlButton : public Control {
		public:
			TabControlButton(Control* parent, TabPageButtonBinding &binding);

			virtual void SetForeColor(const Drawing::Color &color) override;
			void SetText(const Misc::UnicodeString &text);
			void SetActive(bool active);

		protected:
			virtual void DrawSelf(Drawing::RenderContext &context) override;
			virtual void PopulateGeometry() override;

			virtual void OnMouseClick(const MouseMessage &mouse) override;

		private:
			static const Drawing::PointI DefaultLabelOffset;

			TabPageButtonBinding &binding_;
			std::unique_ptr<Label> label_;

			bool active_;
		};

		TabPageButtonBinding *selected_;
		std::vector<std::unique_ptr<TabPageButtonBinding>> bindings_;

		class TabControlSwitchButton : public Control {
		public:
			enum class TabControlSwitchButtonDirection {
				Left,
				Right
			};

			static const Drawing::SizeI DefaultSize;

			TabControlSwitchButton(Control* parent, TabControlSwitchButtonDirection direction);

		protected:
			virtual void PopulateGeometry() override;

		private:
			TabControlSwitchButtonDirection direction_;
		};

		int startIndex_;
		int maxIndex_;
		TabControlSwitchButton *lastSwitchButton_;
		TabControlSwitchButton *nextSwitchButton_;
	};
}

#endif