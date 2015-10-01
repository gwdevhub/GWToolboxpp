/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_EVENT_KEYBOARDMESSAGE_HPP
#define OSHGUI_EVENT_KEYBOARDMESSAGE_HPP

#include "Key.hpp"
#include "../Misc/Strings.hpp"

namespace OSHGui
{
	enum class KeyboardState
	{
		/**
		 * Unbekannt
		 */
		Unknown,
		/**
		 * Taste wurde gedrückt
		 */
		KeyDown,
		/**
		 * enthält das Zeichen der gedrückten Taste
		 */
		Character,
		/**
		 * Taste wurde losgelassen
		 */
		KeyUp
	};

	/**
	 * Tastaturevent
	 */
	class OSHGUI_EXPORT KeyboardMessage
	{
	public:
		/**
		 * Konstruktor der Klasse.
		 */
		KeyboardMessage()
			: state_(KeyboardState::Unknown),
			  keyData_(Key::None),
			  keyChar_('\0')
		{

		}

		KeyboardMessage(KeyboardState state, Key keyData, Misc::AnsiChar keyChar)
			: state_(state),
			  keyData_(keyData),
			  keyChar_(keyChar)
		{

		}

		Key GetKeyData() const
		{
			return keyData_;
		}

		Key GetKeyCode() const
		{
			return keyData_ & Key::KeyCode;
		}

		Key GetModifier() const
		{
			return keyData_ & Key::Modifiers;
		}

		KeyboardState GetState() const
		{
			return state_;
		}

		Misc::AnsiChar GetKeyChar() const
		{
			return keyChar_;
		}

		/**
		 * Prüft, ob das Zeichen alphanumerisch ist.
		 *
		 * \return ja / nein
		 */
		bool IsAlphaNumeric() const
		{
			return Misc::String::IsLetterOrDigit(keyChar_)
				|| Misc::String::IsPunctuation(keyChar_)
				|| Misc::String::IsSeperator(keyChar_)
				|| Misc::String::IsSymbol(keyChar_);
		}

	private:
		KeyboardState state_;
		Misc::AnsiChar keyChar_;
		Key keyData_;

	};
}

#endif