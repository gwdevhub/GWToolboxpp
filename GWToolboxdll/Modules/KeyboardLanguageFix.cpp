#include "stdafx.h"

#include <GWCA/Utilities/Scanner.h>

#include "KeyboardLanguageFix.h"

/*
* Tries to assign the GW keyboard layout address before the game has a chance to.
* Restores original keyboard layout once the en-us one has been loaded.
* 
* THE ISSUE:
* In Windows versions after Windows 8, LoadKeyboardLayoutA installs the keyboard language before using it.
* 
* This is a problem for users because pressing Win + SPACE when you have more than 1 language will de-focus whatever you're doing and show you a stupid little language picker.
* Players often like to use this shortcut for stuff, but can't do that anymore because GW has installed an extra language, forcing the switcher to do its thing.
* 
* Once the en-US keyboard layout has been sneakily installed by Windows, the only way to remove it again is by going into settings, installing the rest of the en-US language pack, then uninstalling the lot!
* 
* See: https://superuser.com/questions/1680608/how-to-get-rid-of-us-language-in-windows-11
* 
* THE SOLUTION:
* We don't actually want GW to install a language if its not found - load it if you have it, otherwise keep using the one that the OS is using instead.
* 
* SIDE EFFECTS:
* Labels on the chat tabs will be wrong - GW still thinks we're now using en-US, so SHIFT + " on a en-UK keyboard layout would actually open Guild Chat (SHIFT + @)
* 
*/

void KeyboardLanguageFix::Initialize()
{
    ToolboxModule::Initialize();

    const auto en_us_keyboard_name = "00000409";

    HKL* address = *(HKL**)GW::Scanner::Find("\x81\xe6\xff\xff\xff\x7f\x85\xc0", "xxxxxxxx", -0x4);
#ifdef _DEBUG
    ASSERT(address && "Failed to find keyboard layout address");
#endif
    if (!address) {
        Log::Error("Failed to find keyboard layout address");
        return;
    }
    Log::Log("Keyboard layout address %p, %p", address, *address);
    if (*address) {
        Log::Log("Failed to intercept keyboard layout override; Guild Wars has already set the keyboard language");
        return;
    }
    // Try to find out if en-US is already installed on the OS via GetKeyboardLayoutList
    HKL loaded_languages[0xff];
    const auto keyboard_languages_count = GetKeyboardLayoutList(_countof(loaded_languages), loaded_languages);
    if (!keyboard_languages_count) {
        Log::Error("Failed to GetKeyboardLayoutList, GetLastError = %#08x", GetLastError());
        return;
    }
    char layout_name[KL_NAMELENGTH];
    for (auto i = 0; i < keyboard_languages_count; i++) {
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
            *address = LoadKeyboardLayoutA(en_us_keyboard_name, 0);
            return;
        }
    }
    // Got this far; en-US isn't already installed, but we don't want Windows to add it as a new language, so keep with the current one.
    *address = GetKeyboardLayout(0);
}
