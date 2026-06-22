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
#include <Defender.h>
#include <Path.h>
#include <Utils/TextUtils.h>

namespace {
    char* tb_exception_message = nullptr;

    // If Defender quarantined/blocked the crash file in the last few seconds, surface the event text.
    std::wstring RecentDefenderBlock(const std::wstring& needle)
    {
        std::wstring detail;
        if (FindRecentDefenderBlock(needle, 15, detail))
            return L"\n\nWindows Defender reported a block moments ago:\n" + detail;
        return L"";
    }

    // Resolve the crashes folder without asserting; Resources::GetPath() would assert and re-enter the crash handler when Documents is blocked.
    std::wstring ResolveCrashFolder()
    {
        std::filesystem::path folder;
        if (!PathGetDocumentsPath(folder, L"GWToolboxpp"))
            return L"";
        if (std::filesystem::path computer; PathGetComputerName(computer))
            folder /= computer;
        return (folder / L"crashes").wstring();
    }

    struct GWDebugInfo {
        size_t len;
        uint32_t log_file_name[0x82];
        char buffer[0x80001];
    };

    static_assert(sizeof(GWDebugInfo) == 0x80210, "struct GWDebugInfo has incorrect size");

    using AppendStackTraceToCrashMessage_pt = void(__cdecl*)(GWDebugInfo*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
    AppendStackTraceToCrashMessage_pt AppendStackTraceToCrashMessage_Func = nullptr, AppendStackTraceToCrashMessage_Ret = nullptr;

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



        auto pContext = reinterpret_cast<PCONTEXT>(param_4);

        // Create EXCEPTION_POINTERS structure
        EXCEPTION_RECORD exceptionRecord = {0};
        EXCEPTION_POINTERS exceptionPointers = {nullptr};

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
        TerminateProcess(GetCurrentProcess(), 1);
        GW::Hook::LeaveHook();
    }

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

        MessageBoxW(nullptr, error_info, L"GWToolbox++ crash dump error", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL | MB_SETFOREGROUND);
        return 1;
    }

    void Cleanup()
    {
        if (AppendStackTraceToCrashMessage_Func) {
            GW::Hook::RemoveHook(AppendStackTraceToCrashMessage_Func);
            AppendStackTraceToCrashMessage_Func = nullptr;
        }
    }

    LONG WINAPI TopLevelExceptionFilter(EXCEPTION_POINTERS* pExceptionPointers)
    {
        // Handle the crash here - this runs BEFORE Windows Error Reporting
        CrashHandler::Crash(pExceptionPointers, nullptr);

        // Never returns, but if it did:
        return EXCEPTION_EXECUTE_HANDLER;
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
        auto fmt = "Assertion Error: '%s' in '%s' line %u";
        const size_t len = snprintf(nullptr, 0, fmt, expr, file, line);
        tb_exception_message = new char[len + 1];
        snprintf(tb_exception_message, len + 1, fmt, expr, file, line);

        throw std::runtime_error(tb_exception_message);
    } __except (EXCEPT_EXPRESSION_ENTRY) {
        // The Crash() function should have terminated the process
        // If we somehow get here, force termination
        TerminateProcess(GetCurrentProcess(), 1);
    }

    // Should never reach here
    TerminateProcess(GetCurrentProcess(), 1);
}

LONG WINAPI CrashHandler::Crash(EXCEPTION_POINTERS* pExceptionPointers, const char* extra_info)
{
    // A crash while handling a crash (e.g. resolving the blocked crash folder asserts again) must not recurse.
    static volatile LONG crashing = 0;
    if (InterlockedExchange(&crashing, 1) != 0) {
        MessageBoxW(nullptr,
            L"Guild Wars crashed, and GWToolbox crashed again while trying to write the crash dump.\n\n"
            L"This almost always means something is blocking your Documents\\GWToolboxpp folder - "
            L"usually Windows Defender Controlled Folder Access or antivirus.\n\n"
            L"Allow Guild Wars through Controlled Folder Access, or add an exclusion for your "
            L"GWToolbox folder, then try again.",
            L"GWToolbox++ crash dump error", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL | MB_SETFOREGROUND | MB_TOPMOST);
        TerminateProcess(GetCurrentProcess(), 1);
        return EXCEPTION_EXECUTE_HANDLER;
    }

    // Disable WER right at the start of crash handling
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

    using SetProcessUserModeExceptionPolicy_t = BOOL(WINAPI *)(DWORD dwFlags);
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    if (hKernel32) {
        auto SetProcessUserModeExceptionPolicy = (SetProcessUserModeExceptionPolicy_t)GetProcAddress(hKernel32, "SetProcessUserModeExceptionPolicy");
        if (SetProcessUserModeExceptionPolicy) {
            SetProcessUserModeExceptionPolicy(0x1);
        }
    }


#ifndef _DEBUG
    if (!Updater::IsLatestVersion()) {
        const std::wstring error_message = L"YOU ARE NOT USING THE LATEST VERSION OF GWTOOLBOX++!\n\n"
            L"Please update to the latest version before reporting any issues.\n"
            L"No crash dump will be created because the issue may have already been fixed.";

        MessageBoxW(nullptr, error_message.c_str(), L"GWToolbox++ - Outdated Version", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL | MB_TOPMOST);
        TerminateProcess(GetCurrentProcess(), 1);
    }
    if (!PluginModule::GetPlugins().empty()) {
        const std::wstring error_message = L"YOU ARE USING PLUGINS!\n\n"
            L"Do not report issues that happen while you are using plugins.\n"
            L"No crash dump will be created because the issue may not come from Toolbox.";

        MessageBoxW(nullptr, error_message.c_str(), L"GWToolbox++ - Plugins used", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL | MB_TOPMOST);
        TerminateProcess(GetCurrentProcess(), 1);
    }
#endif

    const std::wstring crash_folder = ResolveCrashFolder();

    std::wstring ensure_folder_error;
    if (crash_folder.empty()) {
        std::wstring error =
            L"Guild Wars crashed!\n\n"
            L"GWToolbox couldn't find your Documents folder to write a crash dump.\n\n"
            L"This is usually Windows Defender Controlled Folder Access or antivirus blocking access - "
            L"allow Guild Wars through Controlled Folder Access, or add an exclusion for your GWToolbox folder.";
        error += RecentDefenderBlock(L"GWToolbox");
        MessageBoxW(nullptr, error.c_str(), L"GWToolbox++ crash dump error", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL | MB_TOPMOST);
        TerminateProcess(GetCurrentProcess(), 1);
    }
    if (!Resources::EnsureFolderExists(crash_folder.c_str(), ensure_folder_error)) {
        ensure_folder_error += RecentDefenderBlock(crash_folder);
        MessageBoxW(nullptr, ensure_folder_error.c_str(), L"GWToolbox++ crash dump error", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL | MB_TOPMOST);
        TerminateProcess(GetCurrentProcess(), 1);
    }



    const DWORD ProcessId = GetCurrentProcessId();
    const DWORD ThreadId = GetCurrentThreadId();

    SYSTEMTIME stLocalTime;
    GetLocalTime(&stLocalTime);
    wchar_t szFileName[MAX_PATH];
    const auto fn_print = swprintf(
        szFileName, MAX_PATH, L"%s\\%S%S-%04d%02d%02d-%02d%02d%02d-%ld-%ld.dmp", crash_folder.c_str(), GWTOOLBOXDLL_VERSION, GWTOOLBOXDLL_VERSION_BETA, stLocalTime.wYear, stLocalTime.wMonth, stLocalTime.wDay, stLocalTime.wHour, stLocalTime.wMinute,
        stLocalTime.wSecond, ProcessId, ThreadId
    );

    if (fn_print < 0) {
        MessageBoxW(nullptr, L"Failed to format crash file name", L"GWToolbox++ crash dump error", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL | MB_TOPMOST);
        TerminateProcess(GetCurrentProcess(), 1);
    }


    const HANDLE hFile = CreateFileW(szFileName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, nullptr, CREATE_ALWAYS, 0, nullptr);

    if (hFile == INVALID_HANDLE_VALUE) {
        const DWORD last_error = GetLastError();
        std::wstring error = std::format(
            L"Guild Wars crashed!\n\n"
            L"GWToolbox tried to create a crash file, but Windows refused to create it.\n\n"
            L"GetLastError: {} ({})\n\n"
            L"File: {}\n\n"
            L"{}",
            last_error, FormatWindowsError(last_error), szFileName, PathDiagnoseWritability(crash_folder)
        );
        error += RecentDefenderBlock(szFileName);
        MessageBoxW(nullptr, error.c_str(), L"GWToolbox++ crash dump error", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL | MB_TOPMOST);
        TerminateProcess(GetCurrentProcess(), 1);
    }

    MINIDUMP_USER_STREAM_INFORMATION* UserStreamParam = nullptr;
    MINIDUMP_EXCEPTION_INFORMATION* ExpParam = nullptr;

    if (!extra_info && tb_exception_message) {
        extra_info = tb_exception_message;
    }

    if (extra_info) {
        UserStreamParam = new MINIDUMP_USER_STREAM_INFORMATION();
        auto s = new MINIDUMP_USER_STREAM();
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
    const BOOL success = MiniDumpWriteDump(GetCurrentProcess(), ProcessId, hFile, static_cast<MINIDUMP_TYPE>(MiniDumpWithThreadInfo | MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithDataSegs), ExpParam, UserStreamParam, nullptr);

    DWORD lastError = GetLastError();
    CloseHandle(hFile);

    // Antivirus can let the write succeed then delete the file, so confirm it's really there and non-empty.
    bool file_present = false;
    {
        const HANDLE hVerify = CreateFileW(szFileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hVerify != INVALID_HANDLE_VALUE) {
            LARGE_INTEGER file_size{};
            file_present = GetFileSizeEx(hVerify, &file_size) && file_size.QuadPart > 0;
            CloseHandle(hVerify);
        }
    }

    const bool dump_ok = success && file_present;

    std::wstring error_info;

    if (!dump_ok) {
        if (!success) {
            error_info = std::format(
                L"Guild Wars crashed!\n\n"
                L"GWToolbox tried to create a crash dump, but MiniDumpWriteDump failed.\n\n"
                L"GetLastError: {} ({})\n\n"
                L"File: {}\n\n"
                L"{}",
                lastError, FormatWindowsError(lastError), szFileName, PathDiagnoseWritability(crash_folder)
            );
        }
        else {
            error_info = std::format(
                L"Guild Wars crashed!\n\n"
                L"GWToolbox wrote a crash dump, but the file is now empty or gone.\n\n"
                L"File: {}\n\n"
                L"{}",
                szFileName, PathDiagnoseWritability(crash_folder)
            );
        }
        error_info += RecentDefenderBlock(szFileName);
    }
    else {
        error_info = L"Guild Wars crashed!\n\n";

        if (tb_exception_message && *tb_exception_message) {
            error_info += std::format(L"{}\n\n", TextUtils::StringToWString(tb_exception_message));
        }
        error_info += std::format(
            L"GWToolbox created a crash dump for more info\n\n"
            L"Crash file created @ {}\n\n",
            szFileName
        );
    }

    if (tb_exception_message) {
        delete[] tb_exception_message;
        tb_exception_message = nullptr;
    }
    if (UserStreamParam) {
        delete UserStreamParam->UserStreamArray;
        delete UserStreamParam;
    }
    delete ExpParam;

    MessageBoxW(nullptr, error_info.c_str(), dump_ok ? L"GWToolbox++ crash dump created!" : L"GWToolbox++ crash dump failed!", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL | MB_SETFOREGROUND | MB_TOPMOST);

    #ifdef _DEBUG
    if (IsDebuggerPresent()) {
        __debugbreak();
    }
    abort();
    #else
    TerminateProcess(GetCurrentProcess(), 1);
    return EXCEPTION_EXECUTE_HANDLER;
    #endif
}

void CrashHandler::Terminate()
{
    ToolboxModule::Terminate();
    Cleanup();
}

void CrashHandler::Initialize()
{
    ToolboxModule::Initialize();
    // Disable WER
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

    using SetProcessUserModeExceptionPolicy_t = BOOL(WINAPI *)(DWORD dwFlags);
    using GetProcessUserModeExceptionPolicy_t = BOOL(WINAPI *)(LPDWORD lpFlags);

    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    if (hKernel32) {
        auto SetProcessUserModeExceptionPolicy = (SetProcessUserModeExceptionPolicy_t)GetProcAddress(hKernel32, "SetProcessUserModeExceptionPolicy");
        auto GetProcessUserModeExceptionPolicy = (GetProcessUserModeExceptionPolicy_t)GetProcAddress(hKernel32, "GetProcessUserModeExceptionPolicy");

        if (SetProcessUserModeExceptionPolicy && GetProcessUserModeExceptionPolicy) {
            DWORD dwFlags;
            if (GetProcessUserModeExceptionPolicy(&dwFlags)) {
                SetProcessUserModeExceptionPolicy(dwFlags & ~0x1); // Disable the filter callback
            }
        }
    }

    SetUnhandledExceptionFilter(TopLevelExceptionFilter);
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
