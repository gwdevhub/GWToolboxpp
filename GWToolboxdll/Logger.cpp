#include "stdafx.h"

#include <condition_variable>
#include <share.h> // _SH_DENYWR for _wfsopen (shared-read log.txt in Debug)

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

#include <Defines.h>

namespace {
    FILE* logfile = nullptr;
    FILE* logfile2 = nullptr; // Debug: a second sink (log.txt on disk) so the harness can read it while the console stays
    [[maybe_unused]] FILE* stdout_file = nullptr;
    [[maybe_unused]] FILE* stderr_file = nullptr;

    enum LogType : uint8_t {
        LogType_Info,
        LogType_Warning,
        LogType_Error
    };

    [[maybe_unused]] bool crash_dumped = false;

#ifdef _DEBUG
    // The console is written from its own thread so QuickEdit stays usable: selecting text puts the
    // console in Select mode, which blocks the write - on the game thread that is a frozen client.
    // log.txt is still written synchronously, so a crash cannot lose lines that matter.
    std::mutex console_mutex;
    std::condition_variable console_cv;
    std::deque<std::string> console_queue;
    std::thread console_thread;
    bool console_stop = false;

    void ConsoleWriter()
    {
        for (;;) {
            std::deque<std::string> batch;
            {
                std::unique_lock lock(console_mutex);
                console_cv.wait(lock, [] { return console_stop || !console_queue.empty(); });
                if (console_stop && console_queue.empty()) return;
                batch.swap(console_queue);
            }
            for (const auto& line : batch) fputs(line.c_str(), stdout);
            fflush(stdout);
        }
    }

    void EmitConsole(std::string text)
    {
        if (!console_thread.joinable()) return;
        {
            std::lock_guard lock(console_mutex);
            // A wedged console must not grow this without bound; drop the oldest instead.
            if (console_queue.size() > 4096) console_queue.pop_front();
            console_queue.push_back(std::move(text));
        }
        console_cv.notify_one();
    }
#else
    void EmitConsole(const std::string&) {}
#endif

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
        auto to_send = new std::wstring();
        to_send->assign(std::format(L"<a=1>{}</a><c=#{:X}>: <quote>{}", GWTOOLBOX_SENDER, color, message));

        GW::GameThread::Enqueue([to_send, add_to_log = log_transient] {
            WriteChat(GWTOOLBOX_CHAN, to_send->c_str(), nullptr, add_to_log);
            delete to_send;
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
        const std::wstring buf = TextUtils::VStrPrintfW(format, argv);
        if (!buf.empty()) _chatlog(log_type, buf.c_str());
    }

    void _vchatlog(const LogType log_type, const char* format, const va_list argv)
    {
        const std::string buf = TextUtils::VStrPrintf(format, argv);
        if (!buf.empty()) _chatlog(log_type, TextUtils::StringToWString(buf).c_str());
    }
    std::string Timestamp()
    {
        const auto now = std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now());
        const auto ms = now.time_since_epoch().count() % 1000;
        return std::format("[{}] ", TextUtils::TimeToString(std::chrono::system_clock::to_time_t(now), true, static_cast<int>(ms)));
    }

    // In Debug `logfile` is the console and gets the async path; in Release it is log.txt itself.
    void Emit(const std::string& line)
    {
#ifdef _DEBUG
        EmitConsole(line);
#else
        if (logfile) fputs(line.c_str(), logfile);
#endif
        if (logfile2) fputs(line.c_str(), logfile2);
    }


    typedef void(__cdecl* LogWithArguments_pt)(uint32_t severity, const wchar_t* format, va_list argList);
    LogWithArguments_pt LogWithArguments_Func = 0,LogWithArguments_Ret = 0;

    void OnLogWithArguments(uint32_t severity, const wchar_t* format, va_list argList)
    {
        GW::Hook::EnterHook();
        if (format && !wcsstr(format, L"Invalid tag name")) {
            Emit(TextUtils::WStringToString(TextUtils::VStrPrintfW(format, argList)));
        }
        LogWithArguments_Ret(severity, format, argList);
        GW::Hook::LeaveHook();
    }

    void HookGWLogger() {
        if (LogWithArguments_Func) return;
        LogWithArguments_Func = (LogWithArguments_pt)GW::Scanner::ToFunctionStart(GW::Scanner::FindAssertion("Log.cpp", "argListPtr", 0, 0));
        DEBUG_ASSERT(LogWithArguments_Func);
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
    // QuickEdit on so the console can be selected and copied. Select mode still blocks the write,
    // but ConsoleWriter absorbs that instead of the game thread.
    if (const HANDLE conin = CreateFileA("CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr); conin != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(conin, &mode)) {
            SetConsoleMode(conin, mode | ENABLE_EXTENDED_FLAGS | ENABLE_QUICK_EDIT_MODE);
        }
        CloseHandle(conin);
    }
    console_thread = std::thread(ConsoleWriter);
    // Debug also writes to log.txt on disk (the harness reads it), keeping the console as the primary
    // sink. _wfsopen with _SH_DENYWR allows other processes to READ the file while we hold it open --
    // _wfopen_s opens it exclusively, which would block the harness host from tailing it.
    Resources::EnsureFolderExists(Resources::GetComputerFolderPath());
    logfile2 = _wfsopen(Resources::GetPath(L"log.txt").c_str(), L"w", _SH_DENYWR);
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
    if (console_thread.joinable()) {
        {
            std::lock_guard lock(console_mutex);
            console_stop = true;
        }
        console_cv.notify_one();
        console_thread.join();
    }
    if (stdout_file) {
        fclose(stdout_file);
    }
    if (stderr_file) {
        fclose(stderr_file);
    }
    if (logfile2) {
        fflush(logfile2);
        fclose(logfile2);
    }
    FreeConsole();
#else
    if (logfile) {
        fflush(logfile);
        fclose(logfile);
    }
#endif
    logfile = nullptr;
    logfile2 = nullptr;
}

// === File/console logging ===


void Log::Log(const char* msg, ...)
{
    if (!logfile && !logfile2) {
        return;
    }
    va_list args; va_start(args, msg); const std::string body = TextUtils::VStrPrintf(msg, args); va_end(args);
    Emit(Timestamp() + body + (msg[strlen(msg) - 1] != '\n' ? "\n" : ""));
}

void Log::LogW(const wchar_t* msg, ...)
{
    if (!logfile && !logfile2) {
        return;
    }
    va_list args; va_start(args, msg); const std::wstring body = TextUtils::VStrPrintfW(msg, args); va_end(args);
    Emit(Timestamp() + TextUtils::WStringToString(body) + (msg[wcslen(msg) - 1] != '\n' ? "\n" : ""));
}

void Log::FlushFile()
{
#ifndef _DEBUG
    if (logfile) fflush(logfile);
#endif
    if (logfile2) fflush(logfile2);
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
