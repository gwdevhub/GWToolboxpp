/*
* OldSchoolHack GUI
*
* by KN4CK3R http://www.oldschoolhack.me
*
* See license in OSHGui.hpp
*/

#ifndef OSHGUI_INPUT_INPUT_HPP
#define OSHGUI_INPUT_INPUT_HPP

#include "../Exports.hpp"

namespace OSHGui
{
	class MouseMessage;
	class KeyboardMessage;

	namespace Input
	{
		/**
		* Verwaltet den Input von DirectInput8.
		*/
		class OSHGUI_EXPORT Input
		{
		public:
			Input();
			virtual ~Input() { };

			/**
			* Legt fest, ob Mausnachrichten verarbeitet werden sollen.
			*
			* @param enable
			*/
			void SetMouseInputEnabled(bool enable);
			/**
			* Legt fest, ob Tastaturnachrichten verarbeitet werden sollen.
			*
			* @param enable
			*/
			void SetKeyboardInputEnabled(bool enable);

		protected:
			virtual bool InjectMouseMessage(const MouseMessage &mouse);
			virtual bool InjectKeyboardMessage(const KeyboardMessage &keyboard);

			bool enableMouseInput;
			bool enableKeyboardInput;
		};
	}
}

#endif