/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "WindowsMessageThreaded.hpp"
#include "../Application.hpp"

namespace OSHGui
{
	namespace Input
	{
		void WindowsMessageThreaded::PopulateMessages()
		{
			auto &app = Application::Instance();

			while (!mouseMessages_.IsEmpty())
			{
				app.ProcessMouseMessage(mouseMessages_.Pop());
			}
			while (!keyboardMessages_.IsEmpty())
			{
				app.ProcessKeyboardMessage(keyboardMessages_.Pop());
			}
		}
		//---------------------------------------------------------------------------
		bool WindowsMessageThreaded::InjectMouseMessage(const MouseMessage &mouse)
		{
			mouseMessages_.Push(mouse);

			return false;
		}
		//---------------------------------------------------------------------------
		bool WindowsMessageThreaded::InjectKeyboardMessage(const KeyboardMessage &keyboard)
		{
			keyboardMessages_.Push(keyboard);

			return false;
		}
		//---------------------------------------------------------------------------
	}
}