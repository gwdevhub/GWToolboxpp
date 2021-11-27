#include "stdafx.h"

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/CharContext.h>

#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include <GuiUtils.h>

#include <Modules/CrashHandler.h>
#include <Modules/Resources.h>
#include <GWToolbox.h>
#include <Defines.h>

namespace {
    typedef void(__cdecl* AddToSentLog_pt)(wchar_t* message);
    AddToSentLog_pt AddToSentLog_Func = 0;
    AddToSentLog_pt RetAddToSentLog = 0;

    typedef void(__cdecl* ClearChatLog_pt)();
    ClearChatLog_pt ClearChatLog_Func = 0;
    typedef void(__cdecl* InitChatLog_pt)();
    InitChatLog_pt InitChatLog_Func = 0;

    struct GWDebugInfo {
        size_t len;
        uint32_t unk[0x82];
        char buffer[0x80001];
    } *guild_wars_log_info = 0;
    static_assert(sizeof(GWDebugInfo) == 0x80210, "struct GWDebugInfo has incorect size");

    typedef void(__cdecl* HandleCrash_pt)(GWDebugInfo* details, uint32_t param_2, void* pExceptionPointers, char* exception_message, char* exception_file, uint32_t exception_line);
    HandleCrash_pt HandleCrash_Func = 0;
    HandleCrash_pt RetHandleCrash = 0;

    void __cdecl OnGWCrash(GWDebugInfo* details, uint32_t param_2, EXCEPTION_POINTERS* pExceptionPointers, char* exception_message, char* exception_file, uint32_t exception_line) {
        GW::HookBase::EnterHook();
        guild_wars_log_info = details; // Used a bit later in TBMiniDumpWriteDump
        RetHandleCrash(details, param_2, pExceptionPointers, exception_message, exception_file, exception_line);
        guild_wars_log_info = 0;
        GW::HookBase::LeaveHook();
    }
}
HMODULE CrashHandler::TBLoadLibraryA(LPCSTR module) {
    if (strcmp(module, "DbgHelp.dll") == 0)
        return GWToolbox::GetDLLModule();
    return LoadLibraryA(module);
};
BOOL WINAPI TBMiniDumpWriteDump(HANDLE hProcess, DWORD ProcessId, HANDLE hFile, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam, PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam, PMINIDUMP_CALLBACK_INFORMATION CallbackParam)
{
    MINIDUMP_USER_STREAM_INFORMATION* extra_info_stream = 0;

    std::wstring crash_folder = Resources::GetPath(L"crashes");
    Resources::EnsureFolderExists(crash_folder.c_str());

    // Instead of writing to Crash.dmp, write to gwtoolboxpp/crashes
    SYSTEMTIME stLocalTime;
    GetLocalTime(&stLocalTime);
    wchar_t szFileName[MAX_PATH];
    StringCchPrintfW(szFileName, MAX_PATH, L"%s\\%S%S-%04d%02d%02d-%02d%02d%02d-%ld-%ld.dmp",
        crash_folder.c_str(), GWTOOLBOXDLL_VERSION, GWTOOLBOXDLL_VERSION_BETA,
        stLocalTime.wYear, stLocalTime.wMonth, stLocalTime.wDay,
        stLocalTime.wHour, stLocalTime.wMinute, stLocalTime.wSecond,
        ProcessId, GetCurrentThreadId());
    hFile = CreateFileW(szFileName, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);


    if (guild_wars_log_info) {
        char* extra_info = guild_wars_log_info->buffer;
        extra_info_stream = new MINIDUMP_USER_STREAM_INFORMATION();
        MINIDUMP_USER_STREAM* s = new MINIDUMP_USER_STREAM();
        s->Type = MINIDUMP_STREAM_TYPE::CommentStreamA;
        s->Buffer = extra_info;
        s->BufferSize = (strlen(extra_info) + 1) * sizeof(extra_info[0]);
        extra_info_stream->UserStreamCount = 1;
        extra_info_stream->UserStreamArray = s;
        // NB: Use and free our own ptrs to avoid issues (GW never uses this anyway but just in case)
        UserStreamParam = extra_info_stream;
    }
    BOOL success = MiniDumpWriteDump(hProcess, ProcessId, hFile, MiniDumpWithDataSegs, ExceptionParam, UserStreamParam, CallbackParam);
    wchar_t error_info[MAX_PATH];
    if (!success) {
        swprintf(error_info, _countof(error_info), L"Guild Wars crashed!\n\n"
            "GWToolbox tried to create a crash dump, but failed\n\n"
            "GetLastError code from MiniDumpWriteDump: %d\n\n"
            "I don't really know what to do, sorry, contact the developers.\n", GetLastError());
        MessageBoxW(0, error_info,L"GWToolbox++ crash dump error", 0);
    }
    else {
        swprintf(error_info, _countof(error_info), L"Guild Wars crashed!\n\n"
            "GWToolbox created a crash dump for more info\n\n"
            "Crash file created @ %s\n\n", szFileName);
        MessageBoxW(0, error_info, L"GWToolbox++ crash dump created!", 0);
        abort();
    }
    if (extra_info_stream) {
        delete extra_info_stream->UserStreamArray;
        delete extra_info_stream;
    }
    return success;
}
FARPROC CrashHandler::TBGetProcAddress(HMODULE hModule, LPCSTR func_name) {
    if (strcmp(func_name, "MiniDumpWriteDump") == 0)
        return (FARPROC)&MiniDumpWriteDump;
    return GetProcAddress(hModule, func_name);
}

void CrashHandler::Cleanup() {
    for (auto p : patches) {
        delete p;
    }
    patches.clear();
    if (HandleCrash_Func) {
        GW::Hook::RemoveHook(HandleCrash_Func);
        HandleCrash_Func = 0;
    }
}

void CrashHandler::Terminate() {
    ToolboxModule::Terminate();
    Cleanup();
}
void CrashHandler::Initialize() {
    ToolboxModule::Initialize();
    Log::Log("Rerouting GW crash handling through GWToolbox\n");

#pragma warning( push )
#pragma warning( disable : 4838 )
#pragma warning( disable : 4242 )
#pragma warning( disable : 4244 )
#pragma warning( disable : 4365 )

    // Find and redirect LoadLibraryA
    uintptr_t var_const_addr = GW::Scanner::Find("DbgHelp.dll", "xxxxxxxxxxx", 0, GW::Scanner::Section::RDATA);
    char pattern1[] = "\x68????\xff\x15";
    pattern1[1] = var_const_addr;
    pattern1[2] = var_const_addr >> 8;
    pattern1[3] = var_const_addr >> 16;
    pattern1[4] = var_const_addr >> 24;
    char replace[] = "????";
    ASSERT(GetModuleFileNameA(GWToolbox::GetDLLModule(), TBModuleName, _countof(TBModuleName)));
    std::string fn = TBModuleName;
    std::string base_filename = fn.substr(fn.find_last_of("/\\") + 1);
    strcpy(TBModuleName, base_filename.c_str());
    uintptr_t repl_ptr = (uintptr_t)TBModuleName;
    replace[0] = repl_ptr;
    replace[1] = repl_ptr >> 8;
    replace[2] = repl_ptr >> 16;
    replace[3] = repl_ptr >> 24;
    uintptr_t patch_addr = GW::Scanner::Find(pattern1, "xxxxxxx", 1);
    if (patch_addr) {
        GW::MemoryPatcher* p = new GW::MemoryPatcher;
        p->SetPatch(patch_addr, replace, strlen(replace));
        patches.push_back(p);
    }

    // Find and redirect GetProcAddress
    var_const_addr = GW::Scanner::Find("MiniDumpWriteDump", "xxxxxxxxxxxxxxxxx", 0, GW::Scanner::Section::RDATA);
    char pattern[] = "\x68????\x56\xff\x15";
    pattern[1] = var_const_addr;
    pattern[2] = var_const_addr >> 8;
    pattern[3] = var_const_addr >> 16;
    pattern[4] = var_const_addr >> 24;
    repl_ptr = (uintptr_t)TBCrashDumpName;
    replace[0] = repl_ptr;
    replace[1] = repl_ptr >> 8;
    replace[2] = repl_ptr >> 16;
    replace[3] = repl_ptr >> 24;
    patch_addr = GW::Scanner::Find(pattern, "xxxxxxx", 1);
    if (patch_addr) {
        GW::MemoryPatcher* p = new GW::MemoryPatcher;
        p->SetPatch(patch_addr, replace, strlen(replace));
        patches.push_back(p);
    }
#pragma warning( pop )
    for (auto p : patches) {
        p->TogglePatch(true);
    }
    HandleCrash_Func = (HandleCrash_pt)GW::Scanner::Find("\x68\x00\x00\x08\x00\xff\x75\x1c", "xxxxxxxx", -0x4C);
    if (HandleCrash_Func) {
        GW::Hook::CreateHook(HandleCrash_Func, OnGWCrash, (void**)&RetHandleCrash);
        GW::Hook::EnableHooks(HandleCrash_Func);
    }
}