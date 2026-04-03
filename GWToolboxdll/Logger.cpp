#include "stdafx.h"

#include <GWCA/Utilities/Debug.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include <Logger.h>

#include <Modules/CrashHandler.h>
// ReSharper disable once CppUnusedIncludeDirective
#include <Modules/Resources.h>
#include <Utils/TextUtils.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

namespace {
    FILE* logfile = nullptr;
    [[maybe_unused]] FILE* stdout_file = nullptr;
    [[maybe_unused]] FILE* stderr_file = nullptr;

    enum LogType : uint8_t {
        LogType_Info,
        LogType_Warning,
        LogType_Error
    };

    [[maybe_unused]] bool crash_dumped = false;

    bool log_transient = false;

    
// === Game chat logging ===
    void _chatlog(const LogType log_type, const wchar_t* message)
    {
        uint32_t color;
        switch (log_type) {
            case LogType_Error:
                color = GWTOOLBOX_ERROR_COL;
                break;
            case LogType_Warning:
                color = GWTOOLBOX_WARNING_COL;
                break;
            default:
                color = GWTOOLBOX_INFO_COL;
                break;
        }
        const size_t len = 5 + wcslen(GWTOOLBOX_SENDER) + 4 + 13 + wcslen(message) + 4 + 1;
        auto to_send = new wchar_t[len];
        ASSERT(swprintf(to_send, len, L"<a=1>%s</a><c=#%6X>: %s</c>", GWTOOLBOX_SENDER, color, message) != -1);

        GW::GameThread::Enqueue([to_send, add_to_log = log_transient] {
            WriteChat(GWTOOLBOX_CHAN, to_send, nullptr, add_to_log);
            delete[] to_send;
        });

        const wchar_t* c = [](const LogType log_type) -> const wchar_t* {
            switch (log_type) {
                case LogType_Info:
                    return L"Info";
                case LogType_Warning:
                    return L"Warning";
                case LogType_Error:
                    return L"Error";
                default:
                    return L"";
            }
        }(log_type);
        Log::LogW(L"[%s] %s\n", c, message);
    }

    void _vchatlogW(const LogType log_type, const wchar_t* format, const va_list argv)
    {
        wchar_t buf1[512];
        vswprintf(buf1, 512, format, argv);
        _chatlog(log_type, buf1);
    }

    void _vchatlog(const LogType log_type, const char* format, const va_list argv)
    {
        const size_t len = vsnprintf(nullptr, 0, format, argv);
        const auto buf = new char[len + 1];
        vsnprintf(buf, len + 1, format, argv);
        const std::wstring sbuf2 = TextUtils::StringToWString(buf);
        delete[] buf;
        _chatlog(log_type, sbuf2.c_str());
    }

    void PrintTimestamp()
    {
        // note - why not just use current_zone()->to_local(now()) ?
        // this has a small but non trivial perf cost - since the time zone conversion can depend on the time point (think daylight saving time starting or ending),
        // to_local has to do some logic to figure out how to convert every single time point independently
        // for logging purposes, we don't really care about accurate time zone conversions - in fact, monotonicity of timestamps is more convenient than having an accurate wall-clock time
        // consider that if DST changes mid session, we'd rather have some timestamps off by an hour than have gaps or non-monotonicity in timestamps
        // so just cache the timezone offset at first call and use it forever
        static const auto tzoffset = std::chrono::current_zone()->get_info(std::chrono::system_clock::now()).offset;
        std::print(logfile, "[{:%T}] ", std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() + tzoffset));
    }


    typedef void(__cdecl* LogWithArguments_pt)(uint32_t severity, const wchar_t* format, va_list argList);
    LogWithArguments_pt LogWithArguments_Func = 0,LogWithArguments_Ret = 0;

    void OnLogWithArguments(uint32_t severity, const wchar_t* format, va_list argList)
    {
        GW::Hook::EnterHook();
        _vchatlogW((LogType)severity, format, argList);
        //Log::Info(format, argList);
        // your hook logic here
        LogWithArguments_Ret(severity, format, argList);
        GW::Hook::LeaveHook();
    }

    void HookGWLogger() {
        if (LogWithArguments_Func) return;
        LogWithArguments_Func = (LogWithArguments_pt)GW::Scanner::ToFunctionStart(GW::Scanner::FindAssertion("Log.cpp", "argListPtr", 0, 0));
        if (!LogWithArguments_Func) return;
        GW::Hook::CreateHook((void**)&LogWithArguments_Func, OnLogWithArguments, (void**)&LogWithArguments_Ret);
        GW::Hook::EnableHooks(LogWithArguments_Func);

    }

}

static void GWCALogHandler(
    [[maybe_unused]] void* context,
    [[maybe_unused]] const GW::LogLevel level,
    const char* msg,
    [[maybe_unused]] const char* file,
    [[maybe_unused]] const unsigned int line,
    [[maybe_unused]] const char* function)
{
    Log::Log("[GWCA] %s", msg);
}

void Log::FatalAssert(const char* expr, const char* file, const unsigned line)
{
    return CrashHandler::FatalAssert(expr, file, line);
}

BOOL WINAPI ConsoleCtrlHandler(const DWORD dwCtrlType)
{
    switch (dwCtrlType) {
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            // Returning would make the process exit!
            // Give GWToolbox WndProc time to handle the close message
            // If the application is still running after 10 seconds, windows forcefully terminates it
            Sleep(10000);

            return TRUE;
        default:
            break;
    }
    return FALSE;
}

// === Setup and cleanup ====
bool Log::InitializeLog()
{
    if (logfile)
        return true;
#ifdef _DEBUG
    logfile = stdout;
    AllocConsole();
    freopen_s(&stdout_file, "CONOUT$", "w", stdout);
    freopen_s(&stderr_file, "CONOUT$", "w", stderr);
    SetConsoleTitle("GWTB++ Debug Console");
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
#else
    Resources::EnsureFolderExists(Resources::GetComputerFolderPath());
    logfile = _wfreopen(Resources::GetPath(L"log.txt").c_str(), L"w", stdout);
    if (!logfile) {
        return false;
    }
#endif

    return true;
}

bool Log::InitializeGWCALog() {
    GW::RegisterLogHandler(GWCALogHandler, nullptr);
    return true;
}

void Log::InitializeChat()
{
    SetSenderColor(GWTOOLBOX_CHAN, 0xFF000000 | GWTOOLBOX_SENDER_COL);
    SetMessageColor(GWTOOLBOX_CHAN, 0xFF000000 | GWTOOLBOX_INFO_COL);
    #ifdef _DEBUG
    HookGWLogger();
    #endif
}

void Log::Terminate()
{
    GW::RegisterLogHandler(nullptr, nullptr);
    GW::RegisterPanicHandler(nullptr, nullptr);

#ifdef _DEBUG
    if (stdout_file) {
        fclose(stdout_file);
    }
    if (stderr_file) {
        fclose(stderr_file);
    }

    FreeConsole();
#else
    if (logfile) {
        fflush(logfile);
        fclose(logfile);
    }
#endif
    logfile = nullptr;
}

// === File/console logging ===


void Log::Log(const char* msg, ...)
{
    if (!logfile) {
        return;
    }
    PrintTimestamp();

    va_list args;
    va_start(args, msg);
    vfprintf(logfile, msg, args);
    va_end(args);
    if (msg[strlen(msg) - 1] != '\n') {
        fprintf(logfile, "\n");
    }
}

void Log::LogW(const wchar_t* msg, ...)
{
    if (!logfile) {
        return;
    }
    PrintTimestamp();

    va_list args;
    va_start(args, msg);
    vfwprintf(logfile, msg, args);
    va_end(args);
    if (msg[wcslen(msg) - 1] != '\n') {
        fprintf(logfile, "\n");
    }
}

void Log::FlushFile()
{
    if (logfile) {
        fflush(logfile);
    }
}

void Log::Flash(const char* format, ...)
{
    va_list vl;
    va_start(vl, format);
    log_transient = true;
    _vchatlog(LogType_Info, format, vl);
    log_transient = false;
    va_end(vl);
}
void Log::FlashW(const wchar_t* format, ...)
{
    va_list vl;
    va_start(vl, format);
    log_transient = true;
    _vchatlogW(LogType_Info, format, vl);
    log_transient = false;
    va_end(vl);
}

void Log::Info(const char* format, ...)
{
    va_list vl;
    va_start(vl, format);
    _vchatlog(LogType_Info, format, vl);
    va_end(vl);
}

void Log::InfoW(const wchar_t* format, ...)
{
    va_list vl;
    va_start(vl, format);
    _vchatlogW(LogType_Info, format, vl);
    va_end(vl);
}

void Log::Error(const char* format, ...)
{
    va_list vl;
    va_start(vl, format);
    log_transient = true;
    _vchatlog(LogType_Error, format, vl);
    log_transient = false;
    va_end(vl);
}

void Log::ErrorW(const wchar_t* format, ...)
{
    va_list vl;
    va_start(vl, format);
    log_transient = true;
    _vchatlogW(LogType_Error, format, vl);
    log_transient = false;
    va_end(vl);
}

void Log::Warning(const char* format, ...)
{
    va_list vl;
    va_start(vl, format);
    log_transient = true;
    _vchatlog(LogType_Warning, format, vl);
    log_transient = false;
    va_end(vl);
}

void Log::WarningW(const wchar_t* format, ...)
{
    va_list vl;
    va_start(vl, format);
    log_transient = true;
    _vchatlogW(LogType_Warning, format, vl);
    log_transient = false;
    va_end(vl);
}
