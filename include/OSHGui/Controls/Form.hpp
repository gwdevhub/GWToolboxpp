/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_FORM_HPP
#define OSHGUI_FORM_HPP

#include "Control.hpp"

namespace OSHGui
{
	class Label;
	class Panel;

	/**
	 * Tritt ein, wenn die Form geschlossen werden soll.
	 */
	typedef Event<void(Control*, bool &canClose)> FormClosingEvent;
	typedef EventHandler<void(Control*, bool &canClose)> FormClosingEventHandler;

	/**
	 * Gibt Bezeichner an, die den Rückgabewert eines Dialogfelds angeben.
	 */
	enum class DialogResult
	{
		/**
		 * Der Rückgabewert des Dialogfelds ist Nothing.
		 */
		None,
		/**
		 * Der Rückgabewert des Dialogfelds ist OK (üblicherweise von der Schaltfläche OK gesendet).
		 */
		OK,
		/**
		 * Der Rückgabewert des Dialogfelds ist Cancel (üblicherweise von der Schaltfläche Abbrechen gesendet).
		 */
		Cancel,
		/**
		 * Der Rückgabewert des Dialogfelds ist Abort (üblicherweise von der Schaltfläche Abbrechen gesendet).
		 */
		Abort,
		/**
		 * Der Rückgabewert des Dialogfelds ist Retry (üblicherweise von der Schaltfläche Wiederholen gesendet).
		 */
		Retry,
		/**
		 * Der Rückgabewert des Dialogfelds ist Ignore (üblicherweise von der Schaltfläche Ignorieren gesendet).
		 */
		Ignore,
		/**
		 * Der Rückgabewert des Dialogfelds ist Yes (üblicherweise von der Schaltfläche Ja gesendet).
		 */
		Yes,
		/**
		 * Der Rückgabewert des Dialogfelds ist No (üblicherweise von der Schaltfläche Nein gesendet).
		 */
		No
	};

	/**
	 * Stellt ein Fenster dar, das die Benutzeroberfläche bildet.
	 */
	class OSHGUI_EXPORT Form : public Control
	{
		class CaptionBar;

	public:
		using Control::SetSize;

		/**
		 * Konstruktor der Klasse.
		 */
		Form();
		
		/**
		 * Ruft ab, ob die Form modal dargestellt wird.
		 *
		 * \return modal
		 */
		bool IsModal() const;
		/**
		 * Legt die Höhe und Breite des Steuerelements fest.
		 *
		 * \param size
		 */
		virtual void SetSize(const Drawing::SizeI &size) override;
		/**
		 * Legt den Text fest.
		 *
		 * \param text
		 */
		void SetText(const Misc::AnsiString &text);
		/**
		 * Gibt den Text zurück.
		 *
		 * \return der Text
		 */
		const Misc::AnsiString& GetText() const;
		/**
		 * Legt die Fordergrundfarbe des Steuerelements fest.
		 *
		 * \param color
		 */
		virtual void SetForeColor(const Drawing::Color &color) override;
		/**
		 * Gibt eine Liste der untergeordneten Steuerelemente zurück.
		 *
		 * \return parent
		 */
		virtual const std::deque<Control*>& GetControls() const override;
		/**
		 * Legt das DialogResult für das Fenster fest.
		 *
		 * \param result
		 */
		void SetDialogResult(DialogResult result);
		/**
		 * Ruft das DialogResult für das Fenster ab.
		 *
		 * \return dialogResult
		 */
		DialogResult GetDialogResult() const;
		/**
		 * Ruft das FormClosingEvent ab.
		 *
		 * \return formClosingEvent
		 */
		FormClosingEvent& GetFormClosingEvent();
		
		/**
		 * Zeigt die Form an.
		 *
		 * \param instance die aktuelle Instanz dieser Form
		 */
		void Show(const std::shared_ptr<Form> &instance);
		/**
		 * Zeigt die Form modal an.
		 *
		 * \param instance die aktuelle Instanz dieser Form
		 */
		void ShowDialog(const std::shared_ptr<Form> &instance);
		/**
		 * Zeigt die Form modal an.
		 *
		 * \param instance die aktuelle Instanz dieser Form
		 * \param closeFunction diese Funktion wird ausgeführt, wenn die Form geschlossen wird (kann 0 sein)
		 */
		void ShowDialog(const std::shared_ptr<Form> &instance, const std::function<void()> &closeFunction);
		/**
		 * Schließt die Form.
		 */
		void Close();
		/**
		 * Fügt ein untergeordnetes Steuerelement hinzu.
		 *
		 * \param control
		 */
		virtual void AddControl(Control *control) override;

		virtual void DrawSelf(Drawing::RenderContext &context) override;

	protected:
		virtual void PopulateGeometry() override;

	private:
		static const Drawing::PointI DefaultLocation;
		static const Drawing::SizeI DefaultSize;

		std::weak_ptr<Form> instance_;

		CaptionBar *captionBar_;
		Panel *containerPanel_;

		bool isModal_;
		FormClosingEvent formClosingEvent_;

		DialogResult dialogResult_;

		class CaptionBar : public Control
		{
			class CaptionBarButton : public Control
			{
			public:
				static const Drawing::SizeI DefaultSize;

				CaptionBarButton();

				virtual void CalculateAbsoluteLocation() override;

			protected:
				virtual void PopulateGeometry() override;

				virtual void OnMouseUp(const MouseMessage &mouse) override;

			private:
				static const Drawing::PointI DefaultCrossOffset;

				Drawing::PointI crossAbsoluteLocation_;
			};

		public:
			static const int DefaultCaptionBarHeight = 17;

			CaptionBar();

			virtual void SetSize(const Drawing::SizeI &size) override;
			void SetText(const Misc::AnsiString &text);
			const Misc::AnsiString& GetText() const;
			virtual void SetForeColor(const Drawing::Color &color) override;

		protected:
			virtual void DrawSelf(Drawing::RenderContext &context) override;

			virtual void OnMouseDown(const MouseMessage &mouse) override;
			virtual void OnMouseMove(const MouseMessage &mouse) override;
			virtual void OnMouseUp(const MouseMessage &mouse) override;

		private:
			static const int DefaultButtonPadding = 6;
			static const Drawing::PointI DefaultTitleOffset;

			bool drag_;
			Drawing::PointI dragStart_;

			Label *titleLabel_;
			CaptionBarButton *closeButton_;
		};
	};
}

#endif