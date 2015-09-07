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
	 * Occurs when the form is to be closed.
	 */
	typedef Event<void(Control*, bool &canClose)> FormClosingEvent;
	typedef EventHandler<void(Control*, bool &canClose)> FormClosingEventHandler;

	/**
	 * Specifies identifiers that indicate the return value of a dialog box.
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
	 * Represents a window that forms the user interface.
	 */
	class OSHGUI_EXPORT Form : public Control
	{
		class CaptionBar;

	public:
		using Control::SetSize;

		/**
		 * Constructor of the class.
		 */
		Form();
		
		/**
		 * Gets whether the form is displayed modally.
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
		 * Shows the form.
		 *
		 * \param instance die aktuelle Instanz dieser Form
		 */
		void Show(const std::shared_ptr<Form> &instance);
		/**
		 * Displays the form to modal.
		 *
		 * \param instance die aktuelle Instanz dieser Form
		 */
		void ShowDialog(const std::shared_ptr<Form> &instance);
		/**
		 * Displays the form to modal.
		 *
		 * \param instance the current instance of this form
		 * \param closeFunction this function is performed when the mold is closed (can be null)
		 */
		void ShowDialog(const std::shared_ptr<Form> &instance, const std::function<void()> &closeFunction);
		/**
		 * Schließt die Form.
		 */
		void Close();
		/**
		 * Adds a child control.
		 *
		 * \param control
		 */
		virtual void AddControl(Control *control) override;

		virtual void DrawSelf(Drawing::RenderContext &context) override;

	protected:
		virtual void PopulateGeometry() override;

		CaptionBar *captionBar_;
		Panel *containerPanel_;

	private:
		static const Drawing::PointI DefaultLocation;
		static const Drawing::SizeI DefaultSize;

		std::weak_ptr<Form> instance_;

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