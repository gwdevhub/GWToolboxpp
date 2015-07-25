/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "WindowsMessage.hpp"
#include "../Event/MouseMessage.hpp"
#include "../Event/KeyboardMessage.hpp"
#include "../Application.hpp"

namespace OSHGui
{
	namespace Input
	{
		WindowsMessage::WindowsMessage()
			: ImeWmCharsToIgnore_(0)
		{

		}
		//---------------------------------------------------------------------------
		bool WindowsMessage::ProcessMessage(LPMSG message)
		{
			switch (message->message)
			{
				case WM_MOUSEMOVE:
				case WM_LBUTTONDOWN:
				case WM_LBUTTONUP:
				case WM_RBUTTONDOWN:
				case WM_RBUTTONUP:
				case WM_MOUSEWHEEL:
				{
					if (enableMouseInput)
					{
						#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
						#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

						static Drawing::PointI lastMouseLocation;

						MouseState state = MouseState::Unknown;
						MouseButton button = MouseButton::None;
						Drawing::PointI location(GET_X_LPARAM(message->lParam), GET_Y_LPARAM(message->lParam));
						int delta = 0;
						
						switch (message->message)
						{
							case WM_MOUSEMOVE:
								state = MouseState::Move;
								break;
							case WM_LBUTTONDOWN:
								SetCapture(message->hwnd);
								state = MouseState::Down;
								button = MouseButton::Left;
								break;
							case WM_LBUTTONUP:
								ReleaseCapture();
								state = MouseState::Up;
								button = MouseButton::Left;
								break;
							case WM_RBUTTONDOWN:
								SetCapture(message->hwnd);
								state = MouseState::Down;
								button = MouseButton::Right;
								break;
							case WM_RBUTTONUP:
								ReleaseCapture();
								state = MouseState::Up;
								button = MouseButton::Right;
								break;
							case WM_MBUTTONDOWN:
								SetCapture(message->hwnd);
								state = MouseState::Down;
								button = MouseButton::Middle;
								break;
							case WM_MBUTTONUP:
								ReleaseCapture();
								state = MouseState::Up;
								button = MouseButton::Middle;
								break;
							case WM_MOUSEWHEEL:
								state = MouseState::Scroll;
								location = lastMouseLocation; //not valid when scrolling
								delta = -((short)HIWORD(message->wParam) / 120) * 4/*number of lines to scroll*/;
								break;
						}

						lastMouseLocation = location;

						return InjectMouseMessage(MouseMessage(state, button, location, delta));
					}

					break;
				}
				case WM_KEYDOWN:
				case WM_SYSKEYDOWN:
				case WM_KEYUP:
				case WM_SYSKEYUP:
				case WM_CHAR:
				case WM_SYSCHAR:
				case WM_IME_CHAR:
				{
					if (enableKeyboardInput)
					{
						KeyboardState state = KeyboardState::Unknown;
						Misc::AnsiChar keyChar = '\0';
						Key keyData = Key::None;

						if (message->message == WM_CHAR || message->message == WM_SYSCHAR)
						{
							if (ImeWmCharsToIgnore_ > 0)
							{
								--ImeWmCharsToIgnore_;
								return false;
							}
							else
							{
								state = KeyboardState::Character;
								keyChar = (Misc::AnsiChar)message->wParam;
							}
						}
						else if (message->message == WM_IME_CHAR)
						{
							int charSize = SystemDefaultCharSize;
						}
						else
						{
							Key modifier = Key::None;
							if (GetKeyState(static_cast<int>(Key::ControlKey)) < 0)
								modifier |= Key::Control;
							if (GetKeyState(static_cast<int>(Key::ShiftKey)) < 0)
								modifier |= Key::Shift;
							if (GetKeyState(static_cast<int>(Key::Menu)) < 0)
								modifier |= Key::Alt;

							state = message->message == WM_KEYDOWN || message->message == WM_SYSKEYDOWN ? KeyboardState::KeyDown : KeyboardState::KeyUp;

							keyData = (Key)message->wParam | modifier;
						}

						if (state != KeyboardState::Unknown)
						{
							return InjectKeyboardMessage(KeyboardMessage(state, keyData, keyChar));
						}
					}

					break;
				}
			}

			return false;
		}
		//---------------------------------------------------------------------------
	}
}