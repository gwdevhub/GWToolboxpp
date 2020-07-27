#include "stdafx.h"

#include <GWCA/Utilities/Debug.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include <Defines.h>
#include <Logger.h>

#include <Modules/Resources.h>

#define CHAN_WARNING GW::Chat::Channel::CHANNEL_GWCA2
#define CHAN_INFO    GW::Chat::Channel::CHANNEL_EMOTE
#define CHAN_ERROR   GW::Chat::Channel::CHANNEL_GWCA3

namespace {
    FILE* logfile = nullptr;
    FILE* stdout_file = nullptr;
    FILE* stderr_file = nullptr;
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
    GW::Chat::SetMessageColor(CHAN_WARNING, 0xFFFFFF44); // warning
    GW::Chat::SetMessageColor(CHAN_INFO, 0xFFFFFFFF); // info
    GW::Chat::SetMessageColor(CHAN_ERROR, 0xFFFF4444); // error
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
static void _vchatlog(GW::Chat::Channel chan, const char* format, va_list argv) {
    char buf1[256];
    vsprintf_s(buf1, format, argv);

    char buf2[256];
    snprintf(buf2, 256, "<c=#00ccff>GWToolbox++</c>: %s", buf1);

    // @Fix: Visual Studio 2015 doesn't seem to accept to capture c-style arrays
    std::string sbuf2(buf2);
    GW::GameThread::Enqueue([chan, sbuf2]() {
        GW::Chat::WriteChat(chan, sbuf2.c_str());
        });
    

    const char* c = [](GW::Chat::Channel chan) -> const char* {
        switch (chan) {
        case CHAN_INFO: return "Info";
        case CHAN_WARNING: return "Warning";
        case CHAN_ERROR: return "Error";
        default: return "";
        }
    }(chan);
    Log::Log("[%s] %s\n", c, buf1);
}

void Log::Info(const char* format, ...) {
    va_list vl;
    va_start(vl, format);
    _vchatlog(CHAN_INFO, format, vl);
    va_end(vl);
}

void Log::Error(const char* format, ...) {
    va_list vl;
    va_start(vl, format);
    _vchatlog(CHAN_ERROR, format, vl);
    va_end(vl);
}

void Log::Warning(const char* format, ...) {
    va_list vl;
    va_start(vl, format);
    _vchatlog(CHAN_WARNING, format, vl);
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
        crash_folder.c_str(), GWTOOLBOX_VERSION,
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
