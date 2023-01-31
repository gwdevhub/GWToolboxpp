#include "stdafx.h"

#include <GWCA/Utilities/Scanner.h>

#include "KeyboardLanguageFix.h"

/*
* Tries to assign the GW keyboard layout address before the game has a chance to.
* Restores original keyboard layout once the en-us one has been loaded.
*/

void KeyboardLanguageFix::Initialize() {
    ToolboxModule::Initialize();

    HKL* address = *(HKL**)GW::Scanner::Find("\x81\xe6\xff\xff\xff\x7f\x85\xc0","xxxxxxxx",-0x4);
    if (!address) {
        Log::Error("Failed to find keyboard layout address");
        return;
    }
    Log::Info("Keyboard layout address %p, %p", address, *address);
    if (*address) {
        Log::Log("Failed to intercept keyboard layout override; Guild Wars has already set the keyboard language");
        return;
    }
    HKL loaded_languages[0xff];
    int keyboard_languages_count = GetKeyboardLayoutList(_countof(loaded_languages), loaded_languages);
    if (!keyboard_languages_count) {
        Log::Error("Failed to GetKeyboardLayoutList, GetLastError = %#08x", GetLastError());
        return;
    }
    // Ensure that en-US is available for this system. Side effect is that it adds the layout to the OS, which is naughty
    *address = LoadKeyboardLayoutA("00000409", 0);
    int current_keyboard_layout_count = GetKeyboardLayoutList(_countof(loaded_languages), loaded_languages);
    if (!current_keyboard_layout_count) {
        Log::Error("Failed to GetKeyboardLayoutList, GetLastError = %#08x", GetLastError());
        return;
    }
    // If the en US layout wasn't previously present, remove it.
    if (current_keyboard_layout_count != keyboard_languages_count) {
        if (!UnloadKeyboardLayout(*address)) {
            Log::Error("Failed to UnloadKeyboardLayout, GetLastError = %#08x", GetLastError());
            return;
        }
    }
}
