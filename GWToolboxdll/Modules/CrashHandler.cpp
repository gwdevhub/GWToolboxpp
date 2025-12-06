#include "stdafx.h"

#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include <Modules/CrashHandler.h>
#include <Modules/PluginModule.h>
#include <Modules/Resources.h>
#include <Modules/Updater.h>
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

    typedef void(__cdecl* AppendStackTraceToCrashMessage_pt)(GWDebugInfo*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
    AppendStackTraceToCrashMessage_pt AppendStackTraceToCrashMessage_Func = 0, AppendStackTraceToCrashMessage_Ret = 0;

    void OnAppendStackTraceToCrashMessage(GWDebugInfo* message_buffer, uint32_t param_1, uint32_t param_2, uint32_t param_3, uint32_t param_4, uint32_t param_5, uint32_t param_6)
    {
        GW::Hook::EnterHook();
        AppendStackTraceToCrashMessage_Ret(message_buffer, param_1, param_2, param_3, param_4, param_5, param_6);

        if (!tb_exception_message && message_buffer && message_buffer->buffer && *message_buffer->buffer) {
            const auto start_of_error = strstr(message_buffer->buffer, "*-->");
            const auto end_of_error = start_of_error ? strstr(&start_of_error[1], "*-->") : nullptr;

            if (end_of_error) {
                size_t length_of_error = end_of_error - start_of_error;
                tb_exception_message = new char[length_of_error + 1];
                strncpy(tb_exception_message, start_of_error, length_of_error);
                tb_exception_message[length_of_error] = 0;
            }
        }



        PCONTEXT pContext = reinterpret_cast<PCONTEXT>(param_4);

        // Create EXCEPTION_POINTERS structure
        EXCEPTION_RECORD exceptionRecord = {0};
        EXCEPTION_POINTERS exceptionPointers = {0};

        // Fill in exception record with info from CONTEXT
        exceptionRecord.ExceptionCode = EXCEPTION_BREAKPOINT; // Or appropriate code
        exceptionRecord.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
        exceptionRecord.ExceptionAddress = reinterpret_cast<PVOID>(pContext->Eip);
        exceptionRecord.NumberParameters = 0;

        // Set up exception pointers
        exceptionPointers.ExceptionRecord = &exceptionRecord;
        exceptionPointers.ContextRecord = pContext;

        EXCEPTION_POINTERS* pExceptionPointers = &exceptionPointers;
        // this function will create a minidump for us
        CrashHandler::Crash(pExceptionPointers, message_buffer->buffer);
        GW::Hook::LeaveHook();
    }

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
        if (AppendStackTraceToCrashMessage_Func) {
            GW::Hook::RemoveHook(AppendStackTraceToCrashMessage_Func);
            AppendStackTraceToCrashMessage_Func = nullptr;
        }
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
        throw std::runtime_error(tb_exception_message);
    } __except (EXCEPT_EXPRESSION_ENTRY) {}

    abort();
}

LONG WINAPI CrashHandler::Crash(EXCEPTION_POINTERS* pExceptionPointers, const char* extra_info)
{
#ifndef _DEBUG
    // Check if user is running the latest version
    if (!Updater::IsLatestVersion()) {
        const std::wstring error_message = L"YOU ARE NOT USING THE LATEST VERSION OF GWTOOLBOX++!\n\n"
                                     L"Please update to the latest version before reporting any issues.\n"
                                     L"No crash dump will be created because the issue may have already been fixed.";

        MessageBoxW(nullptr, error_message.c_str(), L"GWToolbox++ - Outdated Version", MB_OK | MB_ICONERROR);
        abort();
    }
    if (!PluginModule::GetPlugins().empty()) {
        const std::wstring error_message = L"YOU ARE USING PLUGINS!\n\n"
                                     L"Do not report issues that happen while you are using plugins.\n"
                                     L"No crash dump will be created because the issue may not come from Toolbox.";

        MessageBoxW(nullptr, error_message.c_str(), L"GWToolbox++ - Plugins used", MB_OK | MB_ICONERROR);
        abort();
    }
#endif
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
    if (extra_info) {
        UserStreamParam = new MINIDUMP_USER_STREAM_INFORMATION();
        const auto s = new MINIDUMP_USER_STREAM();
        s->Type = CommentStreamA;
        s->Buffer = (void*)extra_info;
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
#ifdef _DEBUG
    __debugbreak();
#endif
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

    AppendStackTraceToCrashMessage_Func = (AppendStackTraceToCrashMessage_pt)GW::Scanner::ToFunctionStart(GW::Scanner::FindUseOfString("%p  %08x %08x %08x %08x "), 0xfff);
    if (AppendStackTraceToCrashMessage_Func) {
        GW::Hook::CreateHook((void**)&AppendStackTraceToCrashMessage_Func, OnAppendStackTraceToCrashMessage, (void**)&AppendStackTraceToCrashMessage_Ret);
        GW::Hook::EnableHooks(AppendStackTraceToCrashMessage_Func);
    }
#ifdef _DEBUG
    ASSERT(AppendStackTraceToCrashMessage_Func);
#endif
}
