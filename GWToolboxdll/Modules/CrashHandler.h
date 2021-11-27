#pragma once

#include <GWCA/Utilities/Hook.h>
#include <GWCA/Packets/StoC.h>

#include <ToolboxModule.h>
#include <GWCA/Utilities/MemoryPatcher.h>

#pragma comment(linker, "/export:TBMiniDumpWriteDump=_TBMiniDumpWriteDump@28")
extern "C" DllExport BOOL WINAPI TBMiniDumpWriteDump(HANDLE hProcess,
    DWORD ProcessId,
    HANDLE hFile,
    MINIDUMP_TYPE DumpType,
    PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
    PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
    PMINIDUMP_CALLBACK_INFORMATION CallbackParam
);

class CrashHandler : public ToolboxModule {
    CrashHandler() {};
    ~CrashHandler() { Cleanup(); };
public:
    static CrashHandler& Instance() {
        static CrashHandler instance;
        return instance;
    }

    const char* Name() const override { return "Crash Handler"; }

    void Initialize() override;

    void Terminate() override;

    // Used for intercepting GetProcAddress(hModule,"MiniDumpWriteDump")
    static FARPROC TBGetProcAddress(HMODULE h, LPCSTR func_name);

    // Used for intercepting LoadLibraryA("DbgHelp.dll")
    static HMODULE TBLoadLibraryA(LPCSTR func_name);

private:
    // Only dump once per instance - GW likes to spit out 2/3 crash dumps after the initial one!
    bool crash_dumped = false;

    std::vector<GW::MemoryPatcher*> patches;

    char TBModuleName[MAX_PATH];
    char* TBCrashDumpName = "TBMiniDumpWriteDump";
    void GetDllName();
    void Cleanup();



};
