#include "stdafx.h"

#include <Defines.h>
#include <GWToolbox.h>
#include <Logger.h>
#include <Module.h>
#include <Modules/CrashHandler.h>
#include <Utils/TryCatch.h>

namespace {
    HMODULE dllmodule;
    volatile bool thread_running = false;
    volatile bool is_detaching = false;

    using GetUserDefaultLCID_t = LCID(WINAPI*)();
    // Gw.exe's import table slot for GetUserDefaultLCID, which the game calls from its own thread whenever it maps the
    // locale to a game language. Pointing the slot at OnGetUserDefaultLCID runs Initialize on the game thread.
    GetUserDefaultLCID_t* lcid_import = nullptr;
    GetUserDefaultLCID_t original_lcid = nullptr;

    // The import table slot of `function` from `dll` in the module's import directory, or null.
    template <typename Function>
    Function* FindImport(const HMODULE module, const char* dll, const char* function)
    {
        auto* const base = reinterpret_cast<uint8_t*>(module);
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
        const auto& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (!directory.VirtualAddress) return nullptr;
        for (auto* import = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + directory.VirtualAddress); import->Name; import++) {
            if (_stricmp(reinterpret_cast<const char*>(base + import->Name), dll) != 0 || !import->OriginalFirstThunk) continue;
            auto* names = reinterpret_cast<const IMAGE_THUNK_DATA*>(base + import->OriginalFirstThunk);
            auto* slots = reinterpret_cast<IMAGE_THUNK_DATA*>(base + import->FirstThunk);
            for (; names->u1.AddressOfData; names++, slots++) {
                if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
                if (strcmp(reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData)->Name, function) == 0) {
                    return reinterpret_cast<Function*>(&slots->u1.Function);
                }
            }
        }
        return nullptr;
    }

    void SetImport(GetUserDefaultLCID_t function)
    {
        DWORD protection;
        if (!VirtualProtect(lcid_import, sizeof(*lcid_import), PAGE_READWRITE, &protection)) return;
        InterlockedExchangePointer(reinterpret_cast<void**>(lcid_import), reinterpret_cast<void*>(function));
        VirtualProtect(lcid_import, sizeof(*lcid_import), protection, &protection);
    }

    LCID WINAPI OnGetUserDefaultLCID()
    {
        SetImport(original_lcid);
        static std::atomic_flag initialised;
        if (!initialised.test_and_set()) {
            GWToolbox::Initialize(dllmodule);
        }
        return original_lcid();
    }

    void HookForInitialize()
    {
        lcid_import = FindImport<GetUserDefaultLCID_t>(GetModuleHandleW(nullptr), "kernel32.dll", "GetUserDefaultLCID");
        if (!lcid_import) return; // the main loop initialises regardless
        original_lcid = *lcid_import;
        SetImport(OnGetUserDefaultLCID);
    }

    void UnhookForInitialize()
    {
        if (lcid_import && *lcid_import == OnGetUserDefaultLCID) SetImport(original_lcid);
    }

    // Do all your startup things here instead.
    DWORD WINAPI MainLoopThread() noexcept
    {
        ASSERT(!thread_running);
        thread_running = true;
        GWToolbox::MainLoop(dllmodule);
        UnhookForInitialize();
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
            const auto started = RunWithFaultFilter([] {
                // Add a hook for if GW isn't loaded yet...
                HookForInitialize();
                // ...but also call GWToolbox::Initialize inside the main loop too!
                StartMainLoop();
            }, CrashHandler::CrashFilter);
            if (!started) {
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
