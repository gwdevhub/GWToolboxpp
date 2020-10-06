#include "stdafx.h"

#include <GWCA/Utilities/Debug.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include <Defines.h>
#include <Logger.h>
#include <GuiUtils.h>

#include <Modules/Resources.h>

#define GWTOOLBOX_CHAN GW::Chat::Channel::CHANNEL_GWCA2
#define GWTOOLBOX_SENDER L"GWToolbox++"
#define GWTOOLBOX_SENDER_COL 0x00ccff
#define GWTOOLBOX_WARNING_COL 0xFFFF44
#define GWTOOLBOX_ERROR_COL 0xFF4444
#define GWTOOLBOX_INFO_COL 0xFFFFFF

namespace {
    FILE* logfile = nullptr;
    FILE* stdout_file = nullptr;
    FILE* stderr_file = nullptr;

    enum LogType : uint8_t {
        LogType_Info,
        LogType_Warning,
        LogType_Error
    };
}

static void GWCALogHandler(
    void *context,
    GW::LogLevel level,
    const char *msg,
    const char *file,
    unsigned int line,
    const char *function)
{
    UNREFERENCED_PARAMETER(context);
    UNREFERENCED_PARAMETER(level);
    UNREFERENCED_PARAMETER(file);
    UNREFERENCED_PARAMETER(line);
    UNREFERENCED_PARAMETER(function);

    Log::Log("[GWCA] %s", msg);
}

static void GWCAPanicHandler(
    void *context,
    const char *expr,
    const char *file,
    unsigned int line,
    const char *function)
{
    UNREFERENCED_PARAMETER(context);

    Log::Log("[GWCA] Fatal error (expr: '%s', file: '%s', line: %u, function: '%s'",
             expr, file, line, function);

    __try {
        __debugbreak();
    } __except(EXCEPT_EXPRESSION_ENTRY) {
    }
}

// === Setup and cleanup ====
void Log::InitializeLog() {
#ifdef GWTOOLBOX_DEBUG
    logfile = stdout;
    AllocConsole();
    freopen_s(&stdout_file, "CONOUT$", "w", stdout);
    freopen_s(&stderr_file, "CONOUT$", "w", stderr);
    SetConsoleTitle("GWTB++ Debug Console");
#else
    logfile = _wfreopen(Resources::GetPath(L"log.txt").c_str(), L"w", stdout);
#endif

    GW::RegisterLogHandler(GWCALogHandler, nullptr);
    GW::RegisterPanicHandler(GWCAPanicHandler, nullptr);
}

void Log::InitializeChat() {
    GW::Chat::SetSenderColor(GWTOOLBOX_CHAN, 0xFF000000 | GWTOOLBOX_SENDER_COL);
    GW::Chat::SetMessageColor(GWTOOLBOX_CHAN, 0xFF000000 | GWTOOLBOX_INFO_COL);
}

void Log::Terminate() {
    GW::RegisterLogHandler(nullptr, nullptr);
    GW::RegisterPanicHandler(nullptr, nullptr);

#ifdef GWTOOLBOX_DEBUG
    if (stdout_file)
        fclose(stdout_file);
    if (stderr_file)
        fclose(stderr_file);
    FreeConsole();
#else
    if (logfile) {
        fflush(logfile);
        fclose(logfile);
    }
#endif
}

// === File/console logging ===
static void PrintTimestamp() {
    time_t rawtime;
    time(&rawtime);
    
    struct tm timeinfo;
    localtime_s(&timeinfo, &rawtime);

    char buffer[16];
    strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);

    fprintf(logfile, "[%s] ", buffer);
}

void Log::Log(const char* msg, ...) {
    if (!logfile) return;
    PrintTimestamp();

    va_list args;
    va_start(args, msg);
    vfprintf(logfile, msg, args);
    va_end(args);
}

void Log::LogW(const wchar_t* msg, ...) {
    if (!logfile) return;
    PrintTimestamp();

    va_list args;
    va_start(args, msg);
    vfwprintf(logfile, msg, args);
    va_end(args);
}

// === Game chat logging ===
static void _chatlog(LogType log_type, const wchar_t* message) {
    uint32_t color;
    switch (log_type) {
    case LogType::LogType_Error:
        color = GWTOOLBOX_ERROR_COL;
        break;
    case LogType::LogType_Warning:
        color = GWTOOLBOX_WARNING_COL;
        break;
    default:
        color = GWTOOLBOX_INFO_COL;
        break;
    }
    size_t len = 5 + wcslen(GWTOOLBOX_SENDER) + 4 + 13 + wcslen(message) + 4 + 1;
    wchar_t* to_send = new wchar_t[len];
    swprintf(to_send, len - 1, L"<a=1>%s</a><c=#%6X>: %s</c>", GWTOOLBOX_SENDER, color, message);

    GW::GameThread::Enqueue([to_send]() {
        GW::Chat::WriteChat(GWTOOLBOX_CHAN, to_send);
        delete[] to_send;
        });

    const wchar_t* c = [](LogType log_type) -> const wchar_t* {
        switch (log_type) {
        case LogType::LogType_Info: return L"Info";
        case LogType::LogType_Warning: return L"Warning";
        case LogType::LogType_Error: return L"Error";
        default: return L"";
        }
    }(log_type);
    Log::LogW(L"[%s] %s\n", c, message);
}
static void _vchatlogW(LogType log_type, const wchar_t* format, va_list argv) {
    wchar_t buf1[256];
    vswprintf(buf1, 256, format, argv);
    _chatlog(log_type, buf1);
}

static void _vchatlog(LogType log_type, const char* format, va_list argv) {
    char buf1[256];
    vsnprintf(buf1, 256, format, argv);
    std::wstring sbuf2 = GuiUtils::StringToWString(buf1);
    _chatlog(log_type, sbuf2.c_str());
}

void Log::Info(const char* format, ...) {
    va_list vl;
    va_start(vl, format);
    _vchatlog(LogType::LogType_Info, format, vl);
    va_end(vl);
}
void Log::InfoW(const wchar_t* format, ...) {
    va_list vl;
    va_start(vl, format);
    _vchatlogW(LogType::LogType_Info, format, vl);
    va_end(vl);
}
void Log::Error(const char* format, ...) {
    va_list vl;
    va_start(vl, format);
    _vchatlog(LogType::LogType_Error, format, vl);
    va_end(vl);
}
void Log::ErrorW(const wchar_t* format, ...) {
    va_list vl;
    va_start(vl, format);
    _vchatlogW(LogType::LogType_Error, format, vl);
    va_end(vl);
}

void Log::Warning(const char* format, ...) {
    va_list vl;
    va_start(vl, format);
    _vchatlog(LogType::LogType_Warning, format, vl);
    va_end(vl);
}
void Log::WarningW(const wchar_t* format, ...) {
    va_list vl;
    va_start(vl, format);
    _vchatlogW(LogType::LogType_Warning, format, vl);
    va_end(vl);
}

// === Crash Dump ===
LONG WINAPI Log::GenerateDump(EXCEPTION_POINTERS* pExceptionPointers) {
    BOOL bMiniDumpSuccessful;
    wchar_t szFileName[MAX_PATH];
    HANDLE hDumpFile;
    SYSTEMTIME stLocalTime;
    MINIDUMP_EXCEPTION_INFORMATION ExpParam;

    GetLocalTime(&stLocalTime);


    std::wstring crash_folder = Resources::GetPath(L"crashes");
    Resources::EnsureFolderExists(crash_folder.c_str());

    StringCchPrintfW(szFileName, MAX_PATH, L"%s\\%S-%04d%02d%02d-%02d%02d%02d-%ld-%ld.dmp", 
        crash_folder.c_str(), GWTOOLBOXDLL_VERSION,
        stLocalTime.wYear, stLocalTime.wMonth, stLocalTime.wDay,
        stLocalTime.wHour, stLocalTime.wMinute, stLocalTime.wSecond,
        GetCurrentProcessId(), GetCurrentThreadId());
    hDumpFile = CreateFileW(szFileName, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

    ExpParam.ThreadId = GetCurrentThreadId();
    ExpParam.ExceptionPointers = pExceptionPointers;
    ExpParam.ClientPointers = TRUE;

    //MINIDUMP_TYPE flags = static_cast<MINIDUMP_TYPE>(MiniDumpWithDataSegs | MiniDumpWithPrivateReadWriteMemory);
    bMiniDumpSuccessful = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
        hDumpFile, MiniDumpWithDataSegs, &ExpParam, NULL, NULL);
    CloseHandle(hDumpFile);
    if (bMiniDumpSuccessful) {
        wchar_t buf[MAX_PATH];
        swprintf(buf, MAX_PATH, L"GWToolbox crashed, oops\n\n"
            "A dump file has been created in:\n\n%s\n\n"
            "Please send this file to the GWToolbox++ developers.\n"
            "Thank you and sorry for the inconvenience.", szFileName);
        MessageBoxW(0, buf,L"GWToolbox++ Crash!", 0);
    } else {
        MessageBoxW(0,
            L"GWToolbox crashed, oops\n\n"
            "Error creating the dump file\n"
            "I don't really know what to do, sorry, contact the developers.\n",
            L"GWToolbox++ Crash!", 0);
    }
    abort();
}

void Log::FatalAssert(const char *expr, const char *file, unsigned line)
{
    UNREFERENCED_PARAMETER(expr);
    UNREFERENCED_PARAMETER(file);
    UNREFERENCED_PARAMETER(line);
    __try {
        __debugbreak();
    } __except(EXCEPT_EXPRESSION_ENTRY) {
    }

    abort();
}
