#include "stdafx.h"

#include <GWCA/Utilities/Scanner.h>

#include "KeyboardLanguageFix.h"

/*
* Tries to assign the GW keyboard layout address before the game has a chance to.
* Restores original keyboard layout once the en-us one has been loaded.
*/

void KeyboardLanguageFix::Initialize() {
    ToolboxModule::Initialize();

    const char* en_us_keyboard_name = "00000409";

    HKL* address = *(HKL**)GW::Scanner::Find("\x81\xe6\xff\xff\xff\x7f\x85\xc0","xxxxxxxx",-0x4);
    if (!address) {
        Log::Error("Failed to find keyboard layout address");
        return;
    }
    Log::Log("Keyboard layout address %p, %p", address, *address);
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
    char layout_name[KL_NAMELENGTH];
    for (int i = 0; i < keyboard_languages_count;i++) {
        if (!ActivateKeyboardLayout(loaded_languages[i], 0)) {
            Log::Error("Failed to ActivateKeyboardLayout, GetLastError = %#08x", GetLastError());
            return;
        }
        if (!GetKeyboardLayoutNameA(layout_name)) {
            Log::Error("Failed to GetKeyboardLayoutNameA, GetLastError = %#08x", GetLastError());
            return;
        }
        if (strcmp(layout_name, en_us_keyboard_name) == 0) {
            // Found en-US, but call LoadKeyboardLayoutA just in case windows has set a substitute language.
            *address = LoadKeyboardLayoutA(en_us_keyboard_name,0);
            return; 
        }
    }
    // Got this far; en-US isn't already loaded, but we don't want Windows to add it as a new language, so keep with the current one.
    *address = GetKeyboardLayout(0);
}
