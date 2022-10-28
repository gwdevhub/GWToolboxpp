#include "stdafx.h"

#include <GWCA/Utilities/Scanner.h>

#include <Defines.h>
#include <GWToolbox.h>
#include <Logger.h>
#include <Modules/CrashHandler.h>

namespace {
    std::thread GWToolboxThread;
}

// Do all your startup things here instead.
DWORD WINAPI Init(HMODULE hModule) noexcept {
    ASSERT(!GWToolboxThread.joinable());
    GWToolboxThread = std::thread([hModule]() {
        __try {
            if (!Log::InitializeLog()) {
                MessageBoxA(0, "Failed to create outgoing log file.\nThis could be due to a file permissions error or antivirus blocking.", "GWToolbox++ - Clientside Error Detected", 0);
                FreeLibraryAndExitThread(hModule, EXIT_SUCCESS);
            }
            GW::Scanner::Initialize();
            Log::Log("Creating toolbox thread\n");
            SafeThreadEntry(hModule);
        }
        __except (EXCEPT_EXPRESSION_ENTRY) {
        }
        });
    ASSERT(GWToolboxThread.joinable());
    GWToolboxThread.detach();
    return 0;
}

// Exported functions
extern "C" __declspec(dllexport) const char* GWToolboxVersion = GWTOOLBOXDLL_VERSION;
extern "C" __declspec(dllexport) void __cdecl Terminate() {
    // Tell tb to close, then wait for the thread to finish.
    GWToolbox::Instance().StartSelfDestruct();
    if (GWToolboxThread.joinable())
        GWToolboxThread.join();
}

// DLL entry point, dont do things in this thread unless you know what you are doing.
BOOL WINAPI DllMain(_In_ HMODULE _HDllHandle, _In_ DWORD _Reason, _In_opt_ LPVOID _Reserved){
    UNREFERENCED_PARAMETER(_Reserved);
    switch (_Reason) {
    case DLL_PROCESS_ATTACH: {
        Init(_HDllHandle);
    } break;
    case DLL_PROCESS_DETACH: {
        Terminate();
    }break;
    }
    return TRUE;
}
