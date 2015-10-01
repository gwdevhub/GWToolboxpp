/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_INPUT_WINDOWSMESSAGE_HPP
#define OSHGUI_INPUT_WINDOWSMESSAGE_HPP

#include <Windows.h>
#include "Input.hpp"

namespace OSHGui
{
	class MouseMessage;
	class KeyboardMessage;

	namespace Input
	{
		/**
		 * Verwaltet den Input unter Windows.
		 */
		class OSHGUI_EXPORT WindowsMessage : public Input
		{
		public:
			WindowsMessage();

			/**
			 * Wandelt eine Windows Message in ein Event um.
			 *
			 * \param message
			 */
			bool ProcessMessage(LPMSG message);

		private:
			static const int SystemDefaultCharSize = 2;

			int ImeWmCharsToIgnore_;
		};
	}
}

#endif