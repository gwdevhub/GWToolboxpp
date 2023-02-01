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
    Log::Log("Keyboard layout address %p, %p", address, *address);
    if (*address) {
        Log::Log("Failed to intercept keyboard layout override; Guild Wars has already set the keyboard language");
        return;
    }
    *address = GetKeyboardLayout(0);
}
