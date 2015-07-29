/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_INPUT_DIRECTINPUT8_HPP
#define OSHGUI_INPUT_DIRECTINPUT8_HPP

#include <array>
#include <dinput.h>
#include "Input.hpp"
#include "../Drawing/Rectangle.hpp"
#include "../Misc/Strings.hpp"
#include "../Event/Key.hpp"

namespace OSHGui
{
	namespace Input
	{
		/**
		 * Verwaltet den Input von DirectInput8.
		 */
		class OSHGUI_EXPORT DirectInput8 : public Input
		{
		public:
			DirectInput8();
			~DirectInput8();

			bool Initialize(HINSTANCE instance, HWND target);
			void Release();

			void SetMouseClipArea(const Drawing::RectangleI &area);

			/**
			 * Ruft den Status der Geräte ab und erzeugt Eingabe Nachrichten.
			 *
			 * \param message
			 */
			bool ProcessDevices();

		private:
			void ProcessMouseDevice();
			void ProcessKeyboardDevice();

			static bool IsKeyLocked(int key);

			int TryConvertDIKey(BYTE dik, Misc::AnsiChar *chars);

			IDirectInput8 *directInputInterface;
			IDirectInputDevice8A *keyboardDevice;
			IDirectInputDevice8A *mouseDevice;

			Drawing::RectangleI mouseClipArea;
			Drawing::PointI mouseLocation;
			BYTE lastMouseButtonStates[5];

			static unsigned long long InitialRepeatTime;
			static unsigned long long HoldRepeatTime;
			static Key DIKToKeyTable[0xEF];
			
			HKL keyLayout;
			struct KeyState
			{
			public:
				bool KeyHold;
				BYTE LastState;
				unsigned long long LastTime;
			};
			KeyState keyStates[0xEF];
			std::array<BYTE, 256> fakeBuffer;
		};
	}
}

#endif