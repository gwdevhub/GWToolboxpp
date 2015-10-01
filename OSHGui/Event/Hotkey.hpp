/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_EVENT_HOTKEY_HPP
#define OSHGUI_EVENT_HOTKEY_HPP

#include "../Exports.hpp"
#include "../Misc/Exceptions.hpp"
#include "Key.hpp"

namespace OSHGui
{
	/**
	 * Stellt die Methode dar, die ein Ereignis behandelt.
	 */
	class OSHGUI_EXPORT Hotkey
	{
	public:
		typedef std::function<void()> Handler;

		/**
		 * Konstruktor der Klasse.
		 *
		 * \param key
		 * \param handler die aufzurufende Funktion
		 */
		Hotkey(Key key, const Handler &handler)
			: key_(key),
			  modifier_(Key::None),
			  handler_(handler)
		{

		}

		/**
		 * Konstruktor der Klasse.
		 *
		 * \param key
		 * \param modifier Strg / Alt / Shift
		 * \param handler die aufzurufende Funktion
		 */
		Hotkey(Key key, Key modifier, const Handler &handler)
			: key_(key),
			  modifier_(modifier),
			  handler_(handler)
		{
			if (modifier_ == key_ || !(modifier_ == Key::None || modifier_ == Key::Control || modifier_ == Key::Alt || modifier_ == Key::Shift))
			{
				#ifndef OSHGUI_DONTUSEEXCEPTIONS
				throw Misc::ArgumentException("modifier");
				#endif
				throw 1;
			}
		}

		/**
		 * Ruft die Taste des Hotkeys ab.
		 *
		 * \return die Taste
		 */
		Key GetKey() const
		{
			return key_;
		}
		/**
		 * Ruft den Modifier des Hotkeys ab.
		 *
		 * \return der Modifier
		 */
		Key GetModifier() const
		{
			return modifier_;
		}

		bool operator==(const Hotkey &hotkey)
		{
			return key_ == hotkey.key_ && modifier_ == hotkey.modifier_;
		}

		void operator()()
		{
			if (handler_)
			{
				handler_();
			}
		}

	private:
		Key key_;
		Key modifier_;
		Handler handler_;
	};
}

#endif