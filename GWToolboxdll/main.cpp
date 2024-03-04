#include "stdafx.h"

#include <GWCA/Utilities/Scanner.h>

#include <Defines.h>
#include <GWToolbox.h>
#include <Logger.h>
#include <Modules/CrashHandler.h>

namespace {
    HMODULE dllmodule;
    volatile bool thread_running = false;
    volatile bool is_detaching = false;
}

// Exported functions
extern "C" __declspec(dllexport) const char* GWToolboxVersion = GWTOOLBOXDLL_VERSION;

extern "C" __declspec(dllexport) void __cdecl Terminate()
{
    if (thread_running) {
        // Tell tb to close, then wait for the thread to finish.
        GWToolbox::SignalTerminate();
    }
    // Wait up to 5000 ms for toolbox to clean up after itself; after that, bomb out
    constexpr uint32_t timeout = 5000 / 16;
    for (auto i = 0u; i < timeout && thread_running; i++) {
        Sleep(16);
    }
    Sleep(16);
    if (!is_detaching) {
        FreeLibraryAndExitThread(dllmodule, EXIT_SUCCESS);
    }
}


// Do all your startup things here instead.
DWORD WINAPI Init() noexcept
{
    ASSERT(!thread_running);
    thread_running = true;
    __try {
        if (!Log::InitializeLog()) {
            MessageBoxA(nullptr, "Failed to create outgoing log file.\nThis could be due to a file permissions error or antivirus blocking.", "GWToolbox++ - Clientside Error Detected", 0);
            thread_running = false;
            if (!is_detaching) {
                FreeLibraryAndExitThread(dllmodule, EXIT_SUCCESS);
            }
            return 0;
        }
        GW::Scanner::Initialize();
        Log::Log("Creating toolbox thread\n");
        SafeThreadEntry(dllmodule);
    } __except (EXCEPT_EXPRESSION_ENTRY) { }
    thread_running = false;
    if (!is_detaching) {
        FreeLibraryAndExitThread(dllmodule, EXIT_SUCCESS);
    }
    return 0;
}

// DLL entry point, dont do things in this thread unless you know what you are doing.
BOOL WINAPI DllMain(_In_ const HMODULE hDllHandle, _In_ const DWORD reason, _In_opt_ const LPVOID)
{
    DisableThreadLibraryCalls(hDllHandle);
    switch (reason) {
        case DLL_PROCESS_ATTACH: {
            dllmodule = hDllHandle;
            __try {
                const HANDLE hThread = CreateThread(
                    nullptr,
                    0,
                    reinterpret_cast<LPTHREAD_START_ROUTINE>(Init),
                    nullptr,
                    0,
                    nullptr);

                if (hThread != nullptr) {
                    CloseHandle(hThread);
                }
            } __except (EXCEPT_EXPRESSION_ENTRY) { }
        }
        break;
        case DLL_PROCESS_DETACH: {
            is_detaching = true;
            Terminate();
        }
        break;
        default:
            break;
    }
    return TRUE;
}
