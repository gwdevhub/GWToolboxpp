/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include <array>
#include "DirectInput8.hpp"
#include "../Event/MouseMessage.hpp"
#include "../Event/KeyboardMessage.hpp"
#include "../Application.hpp"

namespace OSHGui
{
	namespace Input
	{
		const int STATE_PRESSED = 0x80;
		const int STATE_ON = 0x1;

		unsigned long long DirectInput8::InitialRepeatTime = 300;
		unsigned long long DirectInput8::HoldRepeatTime = 40;
		Key DirectInput8::DIKToKeyTable[] =
		{
			Key::None, /*0x00*/
			Key::Escape, /* DIK_ESCAPE */ /*0x01*/
			Key::D1, /* DIK_1 */ /*0x02*/
			Key::D2, /* DIK_2 */ /*0x03*/
			Key::D3, /* DIK_3 */ /*0x04*/
			Key::D4, /* DIK_4 */ /*0x05*/
			Key::D5, /* DIK_5 */ /*0x06*/
			Key::D6, /* DIK_6 */ /*0x07*/
			Key::D7, /* DIK_7 */ /*0x08*/
			Key::D8, /* DIK_8 */ /*0x09*/
			Key::D9, /* DIK_9 */ /*0x0A*/
			Key::D0, /* DIK_0 */ /*0x0B*/
			Key::OemMinus, /* DIK_MINUS */ /*0x0C*/
			Key::OemPlus, /* DIK_EQUALS */ /*0x0D*/
			Key::Back, /* DIK_BACK */ /*0x0E*/
			Key::Tab, /* DIK_TAB */ /*0x0F*/
			Key::Q, /* DIK_Q */ /*0x10*/
			Key::W, /* DIK_W */ /*0x11*/
			Key::E, /* DIK_E */ /*0x12*/
			Key::R, /* DIK_R */ /*0x13*/
			Key::T, /* DIK_T */ /*0x14*/
			Key::Z, /* DIK_Y */ /*0x15*/
			Key::U, /* DIK_U */ /*0x16*/
			Key::I, /* DIK_I */ /*0x17*/
			Key::O, /* DIK_O */ /*0x18*/
			Key::P, /* DIK_P */ /*0x19*/
			Key::Oem4, /* DIK_LBRACKET */ /*0x1A*/
			Key::Oem6, /* DIK_RBRACKET */ /*0x1B*/
			Key::Return, /* DIK_RETURN */ /*0x1C*/
			Key::LControlKey, /* DIK_LControl */ /*0x1D*/
			Key::A, /* DIK_A */ /*0x1E*/
			Key::S, /* DIK_S */ /*0x1F*/
			Key::D, /* DIK_D */ /*0x20*/
			Key::F, /* DIK_F */ /*0x21*/
			Key::G, /* DIK_G */ /*0x22*/
			Key::H, /* DIK_H */ /*0x23*/
			Key::J, /* DIK_J */ /*0x24*/
			Key::K, /* DIK_K */ /*0x25*/
			Key::L, /* DIK_L */ /*0x26*/
			Key::Oem1, /* DIK_SEMICOLON */ /*0x27*/
			Key::Oem7, /* DIK_APOSTROPHE */ /*0x28*/
			Key::Oem3, /* DIK_GRAVE */ /*0x29*/
			Key::LShiftKey, /* DIK_LSHIFT */ /*0x2A*/
			Key::OemBackslash, /* DIK_BACKSLASH */ /*0x2B*/
			Key::Y, /* DIK_Z */ /*0x2C*/
			Key::X, /* DIK_X */ /*0x2D*/
			Key::C, /* DIK_C */ /*0x2E*/
			Key::V, /* DIK_V */ /*0x2F*/
			Key::B, /* DIK_B */ /*0x30*/
			Key::N, /* DIK_N */ /*0x31*/
			Key::M, /* DIK_M */ /*0x32*/
			Key::OemComma, /* DIK_COMMA */ /*0x33*/
			Key::OemPeriod, /* DIK_PERIOD */ /*0x34*/
			Key::Oem2, /* DIK_SLASH */ /*0x35*/
			Key::RShiftKey, /* DIK_RSHIFT */ /*0x36*/
			Key::Multiply, /* DIK_MULTIPLY */ /*0x37*/
			Key::LMenu, /* DIK_LMENU */ /*0x38*/
			Key::Space, /* DIK_SPACE */ /*0x39*/
			Key::CapsLock, /* DIK_CAPITAL */ /*0x3A*/
			Key::F1, /* DIK_F1 */ /*0x3B*/
			Key::F2, /* DIK_F2 */ /*0x3C*/
			Key::F3, /* DIK_F3 */ /*0x3D*/
			Key::F4, /* DIK_F4 */ /*0x3E*/
			Key::F5, /* DIK_F5 */ /*0x3F*/
			Key::F6, /* DIK_F6 */ /*0x40*/
			Key::F7, /* DIK_F7 */ /*0x41*/
			Key::F8, /* DIK_F8 */ /*0x42*/
			Key::F9, /* DIK_F9 */ /*0x43*/
			Key::F10, /* DIK_F10 */ /*0x44*/
			Key::NumLock, /* DIK_NUMLOCK */ /*0x45*/
			Key::Scroll, /* DIK_SCROLL */ /*0x46*/
			Key::NumPad7, /* DIK_NUMPAD7 */ /*0x47*/
			Key::NumPad8, /* DIK_NUMPAD8 */ /*0x48*/
			Key::NumPad9, /* DIK_NUMPAD9 */ /*0x49*/
			Key::Subtract, /* DIK_SUBSTRACT */ /*0x4A*/
			Key::NumPad4, /* DIK_NUMPAD4 */ /*0x4B*/
			Key::NumPad5, /* DIK_NUMPAD5 */ /*0x4C*/
			Key::NumPad6, /* DIK_NUMPAD6 */ /*0x4D*/
			Key::Add, /* DIK_ADD */ /*0x4E*/
			Key::NumPad1, /* DIK_NUMPAD1 */ /*0x4F*/
			Key::NumPad2, /* DIK_NUMPAD2 */ /*0x50*/
			Key::NumPad3, /* DIK_NUMPAD3 */ /*0x51*/
			Key::NumPad0, /* DIK_NUMPAD0 */ /*0x52*/
			Key::Decimal,  /* DIK_DECIMAL */ /*0x53*/
			Key::None, /*0x54*/
			Key::None, /*0x55*/
			Key::Oem102, /* DIK_OEM_102 */ /*0x56*/
			Key::F11, /* DIK_F11 */ /*0x57*/
			Key::F12, /* DIK_F12 */ /*0x58*/
			Key::None, /*0x59*/
			Key::None, /*0x5A*/
			Key::None, /*0x5B*/
			Key::None, /*0x5C*/
			Key::None, /*0x5D*/
			Key::None, /*0x5E*/
			Key::None, /*0x5F*/
			Key::None, /*0x60*/
			Key::None, /*0x61*/
			Key::None, /*0x62*/
			Key::None, /*0x63*/
			Key::F13, /* DIK_F13 */ /*0x64*/
			Key::F14, /* DIK_F14 */ /*0x65*/
			Key::F15, /* DIK_F15 */ /*0x66*/
			Key::None, /*0x67*/
			Key::None, /*0x68*/
			Key::None, /*0x69*/
			Key::None, /*0x6A*/
			Key::None, /*0x6B*/
			Key::None, /*0x6C*/
			Key::None, /*0x6D*/
			Key::None, /*0x6E*/
			Key::None, /*0x6F*/
			Key::None, /* DIK_KANA */ /*0x70*/
			Key::None, /*0x71*/
			Key::None, /*0x72*/
			Key::None, /* DIK_ABNT_C1 */ /*0x73*/
			Key::None, /*0x74*/
			Key::None, /*0x75*/
			Key::None, /*0x76*/
			Key::None, /*0x77*/
			Key::None, /*0x78*/
			Key::IMEConvert, /* DIK_CONVERT */ /*0x79*/
			Key::None, /*0x7A*/
			Key::IMENonconvert, /* DIK_NOCONVERT */ /*0x7B*/
			Key::None, /*0x7C*/
			Key::None, /* DIK_YEN */ /*0x7D*/
			Key::None, /* DIK_ABNT_C2 */ /*0x7E*/
			Key::None, /*0x7F*/
			Key::None, /*0x80*/
			Key::None, /*0x81*/
			Key::None, /*0x82*/
			Key::None, /*0x83*/
			Key::None, /*0x84*/
			Key::None, /*0x85*/
			Key::None, /*0x86*/
			Key::None, /*0x87*/
			Key::None, /*0x88*/
			Key::None, /*0x89*/
			Key::None, /*0x8A*/
			Key::None, /*0x8B*/
			Key::None, /*0x8C*/
			Key::None, /* DIK_NUMPADEQUALS */ /*0x8D*/
			Key::None, /*0x8E*/
			Key::None, /*0x8F*/
			Key::MediaPreviousTrack, /* DIK_CIRCUMFLEX */ /*0x90*/
			Key::None, /* DIK_AT */ /*0x91*/
			Key::None, /* DIK_COLON */ /*0x92*/
			Key::None, /* DIK_UNDERLINE */ /*0x93*/
			Key::None, /* DIK_KANJI */ /*0x94*/
			Key::None, /* DIK_STOP */ /*0x95*/
			Key::None, /* DIK_AX */ /*0x96*/
			Key::None, /* DIK_UNLABELED */ /*0x97*/
			Key::None, /*0x98*/
			Key::MediaNextTrack, /* DIK_NEXTTRACK */ /*0x99*/
			Key::None, /*0x9A*/
			Key::None, /*0x9B*/
			Key::Return, /* DIK_NUMPADENTER */ /*0x9C*/
			Key::RControlKey, /* DIK_RCONTROL */ /*0x9D*/
			Key::None, /*0x9E*/
			Key::None, /*0x9F*/
			Key::VolumeMute, /* DIK_MUTE */ /*0xA0*/
			Key::None, /* DIK_CALCULATOR */ /*0xA1*/
			Key::MediaPlayPause, /* DIK_PLAYPAUSE */ /*0xA2*/
			Key::None, /*0xA3*/
			Key::MediaStop, /* DIK_MEDIASTOP */ /*0xA4*/
			Key::None, /*0xA5*/
			Key::None, /*0xA6*/
			Key::None, /*0xA7*/
			Key::None, /*0xA8*/
			Key::None, /*0xA9*/
			Key::None, /*0xAA*/
			Key::None, /*0xAB*/
			Key::None, /*0xAC*/
			Key::None, /*0xAD*/
			Key::VolumeDown, /* DIK_VOLUMEDOWN */ /*0xAE*/
			Key::None, /*0xAF*/
			Key::VolumeUp, /* DIK_VOLUMEUP */ /*0xB0*/
			Key::None, /*0xB1*/
			Key::BrowserHome, /* DIK_WEBHOME */ /*0xB2*/
			Key::None, /* DIK_NUMPADCOMMA */ /*0xB3*/
			Key::None, /*0xB4*/
			Key::Divide, /* DIK_DIVIDE */ /*0xB5*/
			Key::None, /*0xB6*/
			Key::None, /* DIK_SYSRQ */ /*0xB7*/
			Key::RMenu, /* DIK_RMENU */ /*0xB8*/
			Key::None, /*0xB9*/
			Key::None, /*0xBA*/
			Key::None, /*0xBB*/
			Key::None, /*0xBC*/
			Key::None, /*0xBD*/
			Key::None, /*0xBE*/
			Key::None, /*0xBF*/
			Key::None, /*0xC0*/
			Key::None, /*0xC1*/
			Key::None, /*0xC2*/
			Key::None, /*0xC3*/
			Key::None, /*0xC4*/
			Key::Pause, /* DIK_PAUSE */ /*0xC5*/
			Key::None, /*0xC6*/
			Key::Home, /* DIK_HOME */ /*0xC7*/
			Key::Up, /* DIK_UP */ /*0xC8*/
			Key::Prior, /* DIK_PRIOR */ /*0xC9*/
			Key::None, /*0xCA*/
			Key::Left, /* DIK_LEFT */ /*0xCB*/
			Key::None, /*0xCC*/
			Key::Right, /* DIK_RIGHT */ /*0xCD*/
			Key::None, /*0xCE*/
			Key::End, /* DIK_END */ /*0xCF*/
			Key::Down, /* DIK_DOWN */ /*0xD0*/
			Key::Next, /* DIK_NEXT */ /*0xD1*/
			Key::Insert, /* DIK_INSERT */ /*0xD2*/
			Key::Delete, /* DIK_DELETE */ /*0xD3*/
			Key::None, /*0xD4*/
			Key::None, /*0xD5*/
			Key::None, /*0xD6*/
			Key::None, /*0xD7*/
			Key::None, /*0xD8*/
			Key::None, /*0xD9*/
			Key::None, /*0xDA*/
			Key::LWin, /* DIK_LWIN */ /*0xDB*/
			Key::RWin, /* DIK_RWIN */ /*0xDC*/
			Key::None, /* DIK_APPS */ /*0xDD*/
			Key::None, /* DIK_POWER */ /*0xDE*/
			Key::Sleep, /* DIK_SLEEP */ /*0xDF*/
			Key::None, /*0xE0*/
			Key::None, /*0xE1*/
			Key::None, /*0xE2*/
			Key::None, /* DIK_WAKE */ /*0xE3*/
			Key::None, /*0xE4*/
			Key::BrowserSearch, /* DIK_WEBSEARCH */ /*0xE5*/
			Key::BrowserFavorites, /* DIK_WEBFAVORITES */ /*0xE6*/
			Key::BrowserRefresh, /* DIK_WEBREFRESH */ /*0xE7*/
			Key::BrowserStop, /* DIK_WEBSTOP */ /*0xE8*/
			Key::BrowserForward, /* DIK_WEBFORWARD */ /*0xE9*/
			Key::BrowserBack, /* DIK_WEBBACK */ /*0xEA*/
			Key::None, /* DIK_MYCOMPUTER */ /*0xEB*/
			Key::LaunchMail, /* DIK_MAIL */ /*0xEC*/
			Key::SelectMedia, /* DIK_MEDIASELECT */ /*0xED*/
		};
		//---------------------------------------------------------------------------
		DirectInput8::DirectInput8()
			: directInputInterface(nullptr),
			  mouseDevice(nullptr),
			  keyboardDevice(nullptr),
			  lastMouseButtonStates()
		{
			keyLayout = GetKeyboardLayout(0);
		}
		//---------------------------------------------------------------------------
		DirectInput8::~DirectInput8()
		{
			Release();
		}
		//---------------------------------------------------------------------------
		void DirectInput8::SetMouseClipArea(const Drawing::RectangleI &area)
		{
			mouseClipArea = area;
		}
		//---------------------------------------------------------------------------
		bool DirectInput8::Initialize(HINSTANCE instance, HWND target)
		{
			if (DirectInput8Create(instance, DIRECTINPUT_VERSION, IID_IDirectInput8A, (void**)&directInputInterface, nullptr) != DI_OK)
			{
				return false;
			}

			//mouse
			if (directInputInterface->CreateDevice(GUID_SysMouse, &mouseDevice, nullptr) != DI_OK)
			{
				Release();
				return false;
			}
			if (mouseDevice->SetDataFormat(&c_dfDIMouse2) != DI_OK)
			{
				Release();
				return false;
			}
			if (mouseDevice->SetCooperativeLevel(target, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE) != DI_OK)
			{
				Release();
				return false;
			}

			//keyboard
			if (directInputInterface->CreateDevice(GUID_SysKeyboard, &keyboardDevice, nullptr) != DI_OK)
			{
				Release();
				return false;
			}
			if (keyboardDevice->SetDataFormat(&c_dfDIKeyboard) != DI_OK)
			{
				Release();
				return false;
			}
			if (keyboardDevice->SetCooperativeLevel(target, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE) != DI_OK)
			{
				Release();
				return false;
			}

			return true;
		}
		//---------------------------------------------------------------------------
		void DirectInput8::Release()
		{
			if (mouseDevice)
			{
				mouseDevice->Unacquire();
				mouseDevice->Release();
				mouseDevice = nullptr;
			}
			if (keyboardDevice)
			{
				keyboardDevice->Unacquire();
				keyboardDevice->Release();
				keyboardDevice = nullptr;
			}
			if (directInputInterface)
			{
				directInputInterface->Release();
				directInputInterface = nullptr;
			}
		}
		//---------------------------------------------------------------------------
		bool DirectInput8::ProcessDevices()
		{
			if (enableMouseInput)
			{
				ProcessMouseDevice();
			}
			if (enableKeyboardInput)
			{
				ProcessKeyboardDevice();
			}

			return true;
		}
		//---------------------------------------------------------------------------
		void DirectInput8::ProcessMouseDevice()
		{
			DIMOUSESTATE2 mouseState;
			auto result = mouseDevice->GetDeviceState(sizeof(mouseState), &mouseState);
			if (result != DI_OK)
			{
				if (result == DIERR_NOTACQUIRED || result == DIERR_INPUTLOST)
				{
					mouseDevice->Acquire();
				}
				return;
			}

			if (mouseState.lX != 0 || mouseState.lY != 0)
			{
				mouseLocation.X += mouseState.lX;
				mouseLocation.Y += mouseState.lY;

				if (mouseLocation.X < mouseClipArea.GetLeft())
				{
					mouseLocation.X = mouseClipArea.GetLeft();
				}
				if (mouseLocation.X > mouseClipArea.GetRight())
				{
					mouseLocation.X = mouseClipArea.GetRight();
				}
				if (mouseLocation.Y < mouseClipArea.GetTop())
				{
					mouseLocation.Y = mouseClipArea.GetTop();
				}
				if (mouseLocation.Y > mouseClipArea.GetBottom())
				{
					mouseLocation.Y = mouseClipArea.GetBottom();
				}

				InjectMouseMessage(MouseMessage(MouseState::Move, MouseButton::None, mouseLocation, 0));
			}
			if (mouseState.lZ != 0)
			{
				InjectMouseMessage(MouseMessage(MouseState::Scroll, MouseButton::None, mouseLocation, mouseState.lZ));
			}
			for (auto i = 0; i < 5; ++i)
			{
				if (mouseState.rgbButtons[i] != lastMouseButtonStates[i])
				{
					InjectMouseMessage(MouseMessage((mouseState.rgbButtons[i] & 0x80) ? MouseState::Down : MouseState::Up, (MouseButton)((int)MouseButton::Left + i), mouseLocation, 0));
				}
				lastMouseButtonStates[i] = mouseState.rgbButtons[i];
			}
		}
		//---------------------------------------------------------------------------
		bool DirectInput8::IsKeyLocked(int key)
		{
			if (key == VK_INSERT || key == VK_NUMLOCK || key == VK_CAPITAL || key == VK_SCROLL)
			{
				int result = GetKeyState(key);

				// If the high-order bit is 1, the key is down; otherwise, it is up.
				// If the low-order bit is 1, the key is toggled. A key, such as the CAPS LOCK key,
				// is toggled if it is turned on. The key is off and untoggled if the low-order bit is 0.
				// A toggle key's indicator light (if any) on the keyboard will be on when the key is toggled,
				// and off when the key is untoggled.

				// Toggle keys (only low bit is of interest).
				if (key == VK_INSERT || key == VK_CAPITAL)
				{
					return (result & 1) != 0;
				}

				return (result & 0x8001) != 0;
			}

			throw;
		}
		//---------------------------------------------------------------------------
		int DirectInput8::TryConvertDIKey(BYTE dik, Misc::AnsiChar *chars)
		{
			auto vk = MapVirtualKeyExA(dik, MAPVK_VSC_TO_VK, keyLayout);
			if (vk == 0)
			{
				//Num special keys
				switch (dik)
				{
				case DIK_DIVIDE:
					vk = VK_DIVIDE;
					break;
				case DIK_MULTIPLY:
					vk = VK_MULTIPLY;
					break;
				case DIK_SUBTRACT:
					vk = VK_SUBTRACT;
					break;
				case DIK_ADD:
					vk = VK_ADD;
					break;
				case DIK_NUMPADENTER:
					vk = VK_RETURN;
					break;
				}
			}

			if (vk != 0)
			{
				//NumLock special keys
				static WORD NumpadConversion[][2] =
				{
					{ VK_DELETE, VK_DECIMAL },
					{ VK_INSERT, VK_NUMPAD0 },
					{ VK_END, VK_NUMPAD1 },
					{ VK_DOWN, VK_NUMPAD2 },
					{ VK_NEXT, VK_NUMPAD3 },
					{ VK_LEFT, VK_NUMPAD4 },
					{ VK_CLEAR, VK_NUMPAD5 },
					{ VK_RIGHT, VK_NUMPAD6 },
					{ VK_HOME, VK_NUMPAD7 },
					{ VK_UP, VK_NUMPAD8 },
					{ VK_PRIOR, VK_NUMPAD9 }
				};
				if (!fakeBuffer[VK_SHIFT] && IsKeyLocked(VK_NUMLOCK) && !IsKeyLocked(VK_CAPITAL))
				{
					for (auto i = 0; i < 11; i++)
					{
						if (vk == NumpadConversion[i][0])
						{
							vk = NumpadConversion[i][1];
							break;
						}
					}
				}

				auto byteCount = ToAsciiEx(vk, dik, fakeBuffer.data(), (LPWORD)chars, 0, keyLayout);
				if (byteCount <= 0)
				{
					return 0;
				}

				return byteCount;
			}

			return 0;
		}
		//---------------------------------------------------------------------------
		void DirectInput8::ProcessKeyboardDevice()
		{
			BYTE keyBuffer[256] = { };
			auto result = keyboardDevice->GetDeviceState(sizeof(keyBuffer), &keyBuffer);
			if (result != DI_OK)
			{
				if (result == DIERR_NOTACQUIRED || result == DIERR_INPUTLOST)
				{
					keyboardDevice->Acquire();
				}
				return;
			}

			fakeBuffer.fill(0);

			auto time = GetTickCount64();
			
			if (IsKeyLocked(VK_CAPITAL))
			{
				fakeBuffer[VK_CAPITAL] = STATE_ON;
			}
			if (keyBuffer[DIK_LSHIFT] & STATE_PRESSED || keyBuffer[DIK_RSHIFT] & STATE_PRESSED)
			{
				fakeBuffer[VK_SHIFT] = STATE_PRESSED;
			}
			if (keyBuffer[DIK_LCONTROL] & STATE_PRESSED || keyBuffer[DIK_RCONTROL] & STATE_PRESSED)
			{
				fakeBuffer[VK_CONTROL] = STATE_PRESSED;
			}
			if (keyBuffer[DIK_LMENU] & STATE_PRESSED)
			{
				fakeBuffer[VK_MENU] = STATE_PRESSED;
			}
			if (keyBuffer[DIK_RMENU] & STATE_PRESSED)
			{
				fakeBuffer[VK_MENU] = STATE_PRESSED;
				fakeBuffer[VK_CONTROL] = STATE_PRESSED;
			}

			for (auto i = 0u; i < 0xEF; ++i)
			{
				Misc::AnsiChar chars[3] = { };
				auto currentKey = DIKToKeyTable[i];
				if (currentKey != Key::None)
				{
					auto &keyState = keyStates[i];
					if (keyState.LastState != keyBuffer[i])
					{
						if (keyBuffer[i] & STATE_PRESSED)
						{
							keyState.LastTime = time;
							keyState.KeyHold = false;

							InjectKeyboardMessage(KeyboardMessage(KeyboardState::KeyDown, currentKey, '\0'));
							
							auto translated = TryConvertDIKey(i, chars);
							if (translated)
							{
								for (int j = 0; j < translated; ++j)
								{
									InjectKeyboardMessage(KeyboardMessage(KeyboardState::Character, currentKey, chars[j]));
								}
							}
						}
						else
						{
							InjectKeyboardMessage(KeyboardMessage(KeyboardState::KeyUp, currentKey, '\0'));
						}
						keyState.LastState = keyBuffer[i];
					}
					else
					{
						if (keyBuffer[i] & STATE_PRESSED)
						{
							if (keyState.KeyHold)
							{
								if (keyState.LastTime + HoldRepeatTime < time)
								{
									keyState.LastTime = time;

									InjectKeyboardMessage(KeyboardMessage(KeyboardState::KeyDown, currentKey, '\0'));

									auto translated = TryConvertDIKey(i, chars);
									if (translated)
									{
										for (int j = 0; j < translated; ++j)
										{
											InjectKeyboardMessage(KeyboardMessage(KeyboardState::Character, currentKey, chars[j]));
										}
									}
								}
							}
							else if (keyState.LastTime + InitialRepeatTime < time)
							{
								keyState.KeyHold = true;
								keyState.LastTime = time;

								InjectKeyboardMessage(KeyboardMessage(KeyboardState::KeyDown, currentKey, '\0'));

								auto translated = TryConvertDIKey(i, chars);
								if (translated)
								{
									for (int j = 0; j < translated; ++j)
									{
										InjectKeyboardMessage(KeyboardMessage(KeyboardState::Character, currentKey, chars[j]));
									}
								}
							}
						}
					}
				}
			}
		}
		//---------------------------------------------------------------------------
	}
}