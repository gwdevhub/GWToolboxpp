#include "stdafx.h"

#include <Defines.h>
#include <GWToolbox.h>
#include <Logger.h>
#include <Modules/CrashHandler.h>
#include <MinHook.h>

namespace {
    HMODULE dllmodule;
    volatile bool thread_running = false;
    volatile bool is_detaching = false;

    typedef UINT(WINAPI* GetUserDefaultLCID_t)();
    GetUserDefaultLCID_t GetUserDefaultLCID_Func = nullptr, GetUserDefaultLCID_Ret = nullptr;

    UINT OnGetUserDefaultLCID() {
        MH_DisableHook(GetUserDefaultLCID_Func);
        GWToolbox::Initialize(dllmodule);
        return GetUserDefaultLCID_Ret();
    }
    /**
    * We can't call GWToolbox::Initialize inside DllMain - it tries to call LoadLibrary later on and can cause deadlocks!
    * We know Guild Wars calls GetUserDefaultLCID on load - hook into this to initialize Toolbox instead
    * This allows us to intercept early calls like login and keyboard language
    */
    void HookForInitialize() {
        const auto hTimeApi = GetModuleHandleA("kernel32.dll");
        GetUserDefaultLCID_Func = hTimeApi ? (GetUserDefaultLCID_t)GetProcAddress(hTimeApi, "GetUserDefaultLCID") : nullptr;
        ASSERT(GetUserDefaultLCID_Func);
        MH_Initialize();
        MH_CreateHook(GetUserDefaultLCID_Func, OnGetUserDefaultLCID, (void**)&GetUserDefaultLCID_Ret);
        MH_EnableHook(GetUserDefaultLCID_Func);
    }

    // Do all your startup things here instead.
    DWORD WINAPI MainLoopThread() noexcept
    {
        ASSERT(!thread_running);
        thread_running = true;
        GWToolbox::MainLoop(dllmodule);
        thread_running = false;
        if (!is_detaching) {
            FreeLibraryAndExitThread(dllmodule, EXIT_SUCCESS);
        }
        return 0;
    }

    void StartMainLoop() {
        const HANDLE hThread = CreateThread(
            nullptr,
            0,
            reinterpret_cast<LPTHREAD_START_ROUTINE>(MainLoopThread),
            nullptr,
            0,
            nullptr);

        if (hThread != nullptr) {
            CloseHandle(hThread);
        }
    }

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


// DLL entry point, dont do things in this thread unless you know what you are doing.
BOOL WINAPI DllMain(_In_ const HMODULE hDllHandle, _In_ const DWORD reason, _In_opt_ const LPVOID)
{
    DisableThreadLibraryCalls(hDllHandle);
    switch (reason) {
        case DLL_PROCESS_ATTACH: {
            dllmodule = hDllHandle;
            __try {
                // Add a hook for if GW isn't loaded yet...
                HookForInitialize();
                // ...but also call GWToolbox::Initialize inside the main loop too!
                StartMainLoop();
            } __except (EXCEPT_EXPRESSION_ENTRY) {
                return FALSE;
            }
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
