#pragma once
#include <GWCA/stdlib/detail/Support.h>
#include <GWCA/GWCA.h>

namespace GW {
    inline void Terminate()
    {
        Client::Clearing guard;
        Client::UnregisterAll();
        TerminateNative();
        Client::CancelTasks();
    }
}
