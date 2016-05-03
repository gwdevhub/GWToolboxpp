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

namespace OSHGui {
	namespace Input {
		WindowsMessage::WindowsMessage()
			: ImeWmCharsToIgnore_(0) {

		}
		//---------------------------------------------------------------------------
		bool WindowsMessage::ProcessMessage(LPMSG message) {
			switch (message->message) {
			case WM_MOUSEMOVE:
			case WM_LBUTTONDOWN:
			case WM_LBUTTONUP:
			case WM_LBUTTONDBLCLK:
			case WM_RBUTTONDOWN:
			case WM_RBUTTONUP:
			case WM_RBUTTONDBLCLK:
			case WM_MOUSEWHEEL:
				return ProcessMouseMessage(message);

			case WM_KEYDOWN:
			case WM_SYSKEYDOWN:
			case WM_KEYUP:
			case WM_SYSKEYUP:
			case WM_CHAR:
			case WM_SYSCHAR:
			case WM_IME_CHAR:
			case WM_XBUTTONDOWN:
			case WM_XBUTTONUP:
			case WM_MBUTTONDOWN:
			case WM_MBUTTONUP:
				return ProcessKeyboardMessage(message);

			default: return false;
			}
		}
		bool WindowsMessage::ProcessMouseMessage(LPMSG message) {
			if (!enableMouseInput) return false;

#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

			static Drawing::PointI lastMouseLocation;

			MouseState state = MouseState::Unknown;
			MouseButton button = MouseButton::None;
			Drawing::PointI location(GET_X_LPARAM(message->lParam), GET_Y_LPARAM(message->lParam));
			int delta = 0;

			switch (message->message) {
			case WM_MOUSEMOVE:
				state = MouseState::Move;
				break;
			case WM_LBUTTONDOWN:
				state = MouseState::Down;
				button = MouseButton::Left;
				break;
			case WM_LBUTTONUP:
				state = MouseState::Up;
				button = MouseButton::Left;
				break;
			case WM_LBUTTONDBLCLK:
				state = MouseState::Down;
				button = MouseButton::Left;
				break;
			case WM_RBUTTONDOWN:
				state = MouseState::Down;
				button = MouseButton::Right;
				break;
			case WM_RBUTTONUP:
				state = MouseState::Up;
				button = MouseButton::Right;
				break;
			case WM_RBUTTONDBLCLK:
				state = MouseState::Down;
				button = MouseButton::Right;
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

		bool WindowsMessage::ProcessKeyboardMessage(LPMSG message) {
			if (!enableKeyboardInput) return false;
			KeyboardState state = KeyboardState::Unknown;
			Misc::AnsiChar keyChar = '\0';
			Key keyData = Key::None;

			if (message->message == WM_CHAR || message->message == WM_SYSCHAR) {
				if (ImeWmCharsToIgnore_ > 0) {
					--ImeWmCharsToIgnore_;
					return false;
				} else {
					state = KeyboardState::Character;
					keyChar = (Misc::AnsiChar)message->wParam;
				}
			} else if (message->message == WM_IME_CHAR) {
				int charSize = SystemDefaultCharSize;
			} else {
				Key modifier = Key::None;
				if (GetKeyState(static_cast<int>(Key::ControlKey)) < 0)
					modifier |= Key::Control;
				if (GetKeyState(static_cast<int>(Key::ShiftKey)) < 0)
					modifier |= Key::Shift;
				if (GetKeyState(static_cast<int>(Key::Menu)) < 0)
					modifier |= Key::Alt;

				switch (message->message) {
				case WM_KEYDOWN:
				case WM_SYSKEYDOWN:
				case WM_XBUTTONDOWN:
				case WM_MBUTTONDOWN:
					state = KeyboardState::KeyDown;
					break;
				case WM_KEYUP:
				case WM_SYSKEYUP:
				case WM_XBUTTONUP:
				case WM_MBUTTONUP:
					state = KeyboardState::KeyUp;
					break;
				default:
					break;
				}

				switch (message->message) {
				case WM_KEYDOWN:
				case WM_SYSKEYDOWN:
				case WM_KEYUP:
				case WM_SYSKEYUP:
					keyData = (Key)message->wParam | modifier;
					break;
				case WM_XBUTTONDOWN:
				case WM_MBUTTONDOWN:
					keyData = (Key)LOWORD(message->wParam);
					if (LOWORD(message->wParam) == MK_MBUTTON) keyData = Key::MButton | modifier;
					if (LOWORD(message->wParam) == MK_XBUTTON1) keyData = Key::XButton1 | modifier;
					if (LOWORD(message->wParam) == MK_XBUTTON2) keyData = Key::XButton2 | modifier;
					break;
				case WM_XBUTTONUP:
				case WM_MBUTTONUP:
					// This is wrong, but there's no correct easy way to do it
					// See HotkeyPanel for Toolbox's solution to this
					// and msdn page on WM_XBUTTONUP message
					keyData = Key::None;
					break;
				}
			}

			if (state != KeyboardState::Unknown) {
				return InjectKeyboardMessage(KeyboardMessage(state, keyData, keyChar));
			} else {
				return false;
			}
		}
	}
}
