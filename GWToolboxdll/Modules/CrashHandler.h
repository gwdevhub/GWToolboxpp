#pragma once

#include <GWCA/Utilities/Hook.h>
#include <GWCA/Packets/StoC.h>

#include <ToolboxModule.h>
#include <GWCA/Utilities/MemoryPatcher.h>

class CrashHandler : public ToolboxModule {
    CrashHandler() {};
    ~CrashHandler() { Cleanup(); };
public:
    static CrashHandler& Instance() {
        static CrashHandler instance;
        return instance;
    }

    const char* Name() const override { return "Crash Handler"; }
    bool HasSettings() override { return false; }

    void Initialize() override;

    void Terminate() override;

    struct GWDebugInfo {
        size_t len;
        uint32_t log_file_name[0x82];
        char buffer[0x80001];
    };
    static_assert(sizeof(GWDebugInfo) == 0x80210, "struct GWDebugInfo has incorect size");

    static LONG WINAPI Crash(EXCEPTION_POINTERS* pExceptionPointers = 0);
    static void FatalAssert(const char* expr, const char* file, unsigned line);
    static void GWCAPanicHandler(
        void*,
        const char* expr,
        const char* file,
        unsigned int line,
        const char* function);

    static void OnGWCrash(GWDebugInfo*, uint32_t, EXCEPTION_POINTERS*, char*, char*, uint32_t);

private:

    typedef void(__cdecl* HandleCrash_pt)(GWDebugInfo* details, uint32_t param_2, void* pExceptionPointers, char* exception_message, char* exception_file, uint32_t exception_line);
    HandleCrash_pt HandleCrash_Func = 0;
    HandleCrash_pt RetHandleCrash = 0;

    GWDebugInfo* gw_debug_info = 0;
    char* tb_exception_message = 0;

    void Cleanup();
};
