/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_CONTROLS_MESSAGEBOX_HPP
#define OSHGUI_CONTROLS_MESSAGEBOX_HPP

#include <functional>
#include <vector>
#include "Form.hpp"
#include "../Misc/Strings.hpp"

#ifdef _MSC_VER
#ifdef MessageBox
#undef MessageBox
#endif
#endif

namespace OSHGui
{
	/**
	 * Gibt Konstanten an, die definieren, welche Schaltflächen der MessageBox angezeigt werden.
	 */
	enum class MessageBoxButtons
	{
		/**
		 * Das Meldungsfeld enthält die Schaltfläche OK.
		 */
		OK,
		/**
		 * Das Meldungsfeld enthält die Schaltflächen OK und Abbrechen.
		 */
		OKCancel,
		/**
		 * Das Meldungsfeld enthält die Schaltflächen Abbrechen, Wiederholen und Ignorieren.
		 */
		AbortRetryIgnore,
		/**
		 * Das Meldungsfeld enthält die Schaltflächen Ja und Nein.
		 */
		YesNo,
		/**
		 * Das Meldungsfeld enthält die Schaltflächen Ja, Nein und Abbrechen.
		 */
		YesNoCancel,
		/**
		 * Das Meldungsfeld enthält die Schaltflächen Wiederholen und Abbrechen.
		 */
		RetryCancel
	};

	class OSHGUI_EXPORT MessageBox
	{
	public:
		static void Show(const Misc::AnsiString &text);
		static void Show(const Misc::AnsiString &text, const Misc::AnsiString &caption);
		static void Show(const Misc::AnsiString &text, const Misc::AnsiString &caption, MessageBoxButtons buttons);
		static void ShowDialog(const Misc::AnsiString &text, const std::function<void(DialogResult result)> &closeFunction);
		static void ShowDialog(const Misc::AnsiString &text, const Misc::AnsiString &caption, const std::function<void(DialogResult result)> &closeFunction);
		static void ShowDialog(const Misc::AnsiString &text, const Misc::AnsiString &caption, MessageBoxButtons buttons, const std::function<void(DialogResult result)> &closeFunction);

	private:
		class MessageBoxForm : public Form
		{
		private:
			void InitializeComponent(const Misc::AnsiString &text, const Misc::AnsiString &caption, MessageBoxButtons buttons);
			void AddButtons(const std::vector<Misc::AnsiString> &label, const std::vector<ClickEventHandler> &eventHandler);

		public:
			MessageBoxForm(const Misc::AnsiString &text, const Misc::AnsiString &caption, MessageBoxButtons buttons);
		};
	};
}

#endif