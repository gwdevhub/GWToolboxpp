#include "stdafx.h"

#include <atomic>
#include <Defines.h>
#include <GWToolbox.h>
#include <Logger.h>
#include <Modules/CrashHandler.h>
#include <Modules/Updater.h>
#include <MinHook.h>

namespace {
    HMODULE dllmodule;
    std::atomic_bool thread_running = false;
    std::atomic_bool is_detaching = false;
    std::atomic_bool terminate_requested = false;

    typedef UINT(WINAPI* GetUserDefaultLCID_t)();
    GetUserDefaultLCID_t GetUserDefaultLCID_Func = nullptr, GetUserDefaultLCID_Ret = nullptr;

    UINT OnGetUserDefaultLCID() {
        if(GetUserDefaultLCID_Func) MH_DisableHook(GetUserDefaultLCID_Func);
        GWToolbox::Initialize(dllmodule);
        return GetUserDefaultLCID_Ret();
    }
    void HookForInitialize() {
        const auto hTimeApi = GetModuleHandleA("kernel32.dll");
        GetUserDefaultLCID_Func = hTimeApi ? (GetUserDefaultLCID_t)GetProcAddress(hTimeApi, "GetUserDefaultLCID") : nullptr;
        ASSERT(GetUserDefaultLCID_Func);
        MH_Initialize();
        MH_CreateHook(GetUserDefaultLCID_Func, OnGetUserDefaultLCID, (void**)&GetUserDefaultLCID_Ret);
        MH_EnableHook(GetUserDefaultLCID_Func);
    }

    DWORD WINAPI MainLoopThread() noexcept
    {
        __try {
            if (Updater::CheckBeforeInitialize(dllmodule) && !terminate_requested) {
                HookForInitialize();
                GWToolbox::Initialize(dllmodule);
                if (terminate_requested) GWToolbox::SignalTerminate();
                GWToolbox::MainLoop(dllmodule);
            }
        } __except (EXCEPT_EXPRESSION_ENTRY) {
        }
        if(GetUserDefaultLCID_Func) MH_DisableHook(GetUserDefaultLCID_Func);
        thread_running = false;
        if (!is_detaching) {
            FreeLibraryAndExitThread(dllmodule, EXIT_SUCCESS);
        }
        return 0;
    }

    void StartMainLoop() {
        thread_running = true;
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
        else {
            thread_running = false;
        }
    }

}

extern "C" __declspec(dllexport) const char* GWToolboxVersion = GWTOOLBOXDLL_VERSION;

extern "C" __declspec(dllexport) void __cdecl Terminate()
{
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCWSTR>(dllmodule), &module)) return;
    terminate_requested = true;
    if (thread_running) {
        GWToolbox::SignalTerminate();
    }
    constexpr uint32_t timeout = 5000 / 16;
    for (auto i = 0u; i < timeout && thread_running; i++) {
        Sleep(16);
    }
    if (!is_detaching) {
        FreeLibraryAndExitThread(module, EXIT_SUCCESS);
    }
}


BOOL WINAPI DllMain(_In_ const HMODULE hDllHandle, _In_ const DWORD reason, _In_opt_ const LPVOID)
{
    DisableThreadLibraryCalls(hDllHandle);
    switch (reason) {
        case DLL_PROCESS_ATTACH: {
            dllmodule = hDllHandle;
            __try {
                StartMainLoop();
            } __except (EXCEPT_EXPRESSION_ENTRY) {
                return FALSE;
            }
        }
        break;
        case DLL_PROCESS_DETACH: {
            is_detaching = true;
            terminate_requested = true;
        }
        break;
        default:
            break;
    }
    return TRUE;
}
