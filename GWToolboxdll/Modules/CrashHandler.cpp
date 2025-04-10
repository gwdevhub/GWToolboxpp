#include "stdafx.h"

#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include <Modules/CrashHandler.h>
#include <Modules/Resources.h>
#include <GWToolbox.h>
#include <Defines.h>
#include <Utils/TextUtils.h>

namespace {
    char* tb_exception_message = nullptr;

    struct GWDebugInfo {
        size_t len;
        uint32_t log_file_name[0x82];
        char buffer[0x80001];
    };

    static_assert(sizeof(GWDebugInfo) == 0x80210, "struct GWDebugInfo has incorrect size");

    typedef void (__cdecl*HandleCrash_pt)(GWDebugInfo* details, uint32_t param_2, void* pExceptionPointers, char* exception_message, char* exception_file, uint32_t exception_line);
    HandleCrash_pt HandleCrash_Func = nullptr;
    HandleCrash_pt RetHandleCrash = nullptr;



    GWDebugInfo* gw_debug_info = nullptr;

    int failed(const char* failure_message)
    {
        wchar_t error_info[512];
        swprintf(error_info, _countof(error_info),
                 L"Guild Wars crashed!\n\n"
                 "GWToolbox tried to create a crash dump, but failed\n\n"
                 "%S\n"
                 "GetLastError code: %d\n\n"
                 "I don't really know what to do, sorry, contact the developers.\n",
                 failure_message, GetLastError());

        MessageBoxW(nullptr, error_info, L"GWToolbox++ crash dump error", 0);
        return 1;
    }

    void Cleanup()
    {
        if (HandleCrash_Func) {
            GW::Hook::RemoveHook(HandleCrash_Func);
            HandleCrash_Func = nullptr;
        }
    }

    void OnGWCrash(GWDebugInfo* details, const uint32_t param_2, EXCEPTION_POINTERS* pExceptionPointers, char* exception_message, char* exception_file, const uint32_t exception_line)
    {
        GW::Hook::EnterHook();
        if (!gw_debug_info) {
            gw_debug_info = details;
        }
        if (!pExceptionPointers) {
            CrashHandler::FatalAssert(exception_message, exception_file, exception_line);
        }
#ifdef _DEBUG
        __try {
            // Debug break here to catch stack trace in debug mode before dumping
            __debugbreak();
        } __except (EXCEPTION_CONTINUE_EXECUTION) {}
#endif
        // Assertion here will throw a GWToolbox exception if pExceptionPointers isn't found; this will give us the correct call stack for a GW Assertion failure in the subsequent crash dump.
        if (CrashHandler::Crash(pExceptionPointers)) {
            abort();
        }
        gw_debug_info = nullptr;
        RetHandleCrash(details, param_2, pExceptionPointers, exception_message, exception_file, exception_line);

        GW::Hook::LeaveHook();
        abort();
    }
}

void CrashHandler::GWCAPanicHandler(
    void*,
    const char* expr,
    const char* file,
    const unsigned int line,
    const char*)
{
    FatalAssert(expr, file, line);
}

void CrashHandler::FatalAssert(const char* expr, const char* file, const unsigned line)
{
    __try {
        const char* fmt = "Assertion Error: '%s' in '%s' line %u";
        const size_t len = snprintf(nullptr, 0, fmt, expr, file, line);
        tb_exception_message = new char[len + 1];
        snprintf(tb_exception_message, len + 1, fmt, expr, file, line);
#ifdef _DEBUG
        __debugbreak();
#endif
        throw std::runtime_error(tb_exception_message);
    } __except (EXCEPT_EXPRESSION_ENTRY) {}

    abort();
}

LONG WINAPI CrashHandler::Crash(EXCEPTION_POINTERS* pExceptionPointers)
{
    const std::wstring crash_folder = Resources::GetPath(L"crashes");

    const DWORD ProcessId = GetCurrentProcessId();
    const DWORD ThreadId = GetCurrentThreadId();

    // Instead of writing to Crash.dmp, write to gwtoolboxpp/crashes
    SYSTEMTIME stLocalTime;
    GetLocalTime(&stLocalTime);
    wchar_t szFileName[MAX_PATH];
    const auto fn_print = swprintf(szFileName, MAX_PATH, L"%s\\%S%S-%04d%02d%02d-%02d%02d%02d-%ld-%ld.dmp",
                                   crash_folder.c_str(), GWTOOLBOXDLL_VERSION, GWTOOLBOXDLL_VERSION_BETA, stLocalTime.wYear, stLocalTime.wMonth,
                                   stLocalTime.wDay, stLocalTime.wHour, stLocalTime.wMinute, stLocalTime.wSecond, ProcessId, ThreadId);

    MINIDUMP_USER_STREAM_INFORMATION* UserStreamParam = nullptr;
    char* extra_info = nullptr;

    MINIDUMP_EXCEPTION_INFORMATION* ExpParam = nullptr;
    const HANDLE hFile = CreateFileW(szFileName, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_WRITE | FILE_SHARE_READ, nullptr,
        CREATE_ALWAYS, 0, nullptr);
    if (!Resources::EnsureFolderExists(crash_folder.c_str())) {
        return failed("Failed to create crash directory");
    }
    if (fn_print < 0) {
        return failed("Failed to swprintf crash file name");
    }
    if (hFile == INVALID_HANDLE_VALUE) {
        return failed("Failed to CreateFileW crash file");
    }
    if (!Resources::EnsureFolderExists(crash_folder.c_str())) {
        return failed("Failed to create crash directory");
    }

    if (fn_print < 0) {
        return failed("Failed to swprintf crash file name");
    }
    if (gw_debug_info) {
        extra_info = gw_debug_info->buffer;
    }
    else if (tb_exception_message) {
        extra_info = tb_exception_message;
    }
    if (extra_info) {
        UserStreamParam = new MINIDUMP_USER_STREAM_INFORMATION();
        const auto s = new MINIDUMP_USER_STREAM();
        s->Type = CommentStreamA;
        s->Buffer = extra_info;
        s->BufferSize = static_cast<ULONG>(strlen(extra_info) + 1);
        UserStreamParam->UserStreamCount = 1;
        UserStreamParam->UserStreamArray = s;
    }
    if (pExceptionPointers) {
        ExpParam = new MINIDUMP_EXCEPTION_INFORMATION;
        ExpParam->ThreadId = ThreadId;
        ExpParam->ExceptionPointers = pExceptionPointers;
        ExpParam->ClientPointers = false;
    }
    const BOOL success = MiniDumpWriteDump(
        GetCurrentProcess(), ProcessId, hFile,
        static_cast<MINIDUMP_TYPE>(MiniDumpWithThreadInfo | MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithDataSegs),
        ExpParam, UserStreamParam, nullptr);
    CloseHandle(hFile);

    std::wstring error_info = L"Guild Wars crashed!\n\n";

    if (tb_exception_message && *tb_exception_message) {
        error_info += std::format(L"{}\n\n", TextUtils::StringToWString(tb_exception_message));
    }
    error_info += std::format(
        L"GWToolbox created a crash dump for more info\n\n"
        L"Crash file created @ {}\n\n",
        szFileName
    );

    if (tb_exception_message) 
        delete[] tb_exception_message;
    tb_exception_message = nullptr;
    if (UserStreamParam) {
        delete UserStreamParam->UserStreamArray;
        delete UserStreamParam;
    }
    delete ExpParam;
    if (!success) {
        return failed("Failed to create MiniDumpWriteDump");
    }

    MessageBoxW(nullptr, error_info.c_str(), L"GWToolbox++ crash dump created!", 0);
    abort();
}

void CrashHandler::Terminate()
{
    ToolboxModule::Terminate();
    Cleanup();
}

void CrashHandler::Initialize()
{
    ToolboxModule::Initialize();
    GW::RegisterPanicHandler(GWCAPanicHandler, nullptr);
    HandleCrash_Func = (HandleCrash_pt)GW::Scanner::Find("\x68\x00\x00\x08\x00\xff\x75\x1c", "xxxxxxxx", -0x4C);
    if (HandleCrash_Func) {
        GW::Hook::CreateHook((void**)&HandleCrash_Func, OnGWCrash, (void**)&RetHandleCrash);
        GW::Hook::EnableHooks(HandleCrash_Func);
    }
#ifdef _DEBUG
    ASSERT(HandleCrash_Func);
#endif
}
