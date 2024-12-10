#pragma once

#define ASSERT(expr) ((void)(!!(expr) || (Log::FatalAssert(#expr, __FILE__, (unsigned)__LINE__), 0)))
#define IM_ASSERT(expr) ASSERT(expr)
#include <GWCA/Managers/ChatMgr.h>

constexpr auto GWTOOLBOX_CHAN = GW::Chat::Channel::CHANNEL_GWCA2;
constexpr auto GWTOOLBOX_SENDER = L"GWToolbox++";
constexpr auto GWTOOLBOX_SENDER_COL = 0x00ccff;
constexpr auto GWTOOLBOX_WARNING_COL = 0xFFFF44;
constexpr auto GWTOOLBOX_ERROR_COL = 0xFF4444;
constexpr auto GWTOOLBOX_INFO_COL = 0xFFFFFF;

namespace Log {
    // === Setup and cleanup ====
    // in release redirects stdout and stderr to log file
    // in debug creates console
    bool InitializeLog();
    void InitializeChat();
    void Terminate();

    // === File/console logging ===
    // printf-style log
    void Log(const char* msg, ...);

    // printf-style wide-string log
    void LogW(const wchar_t* msg, ...);

    // flushes log file.
    //static void FlushFile() { fflush(logfile); }

    // === Game chat logging ===
    // Shows a message in chat in the form of a white chat message from toolbox
    void Info(const char* format, ...);
    // Shows a message in chat in the form of a white chat message from toolbox
    void InfoW(const wchar_t* format, ...);

    // Shows a temporary message in chat in the form of a white chat message from toolbox. This message will disappear on map change.
    void Flash(const char* format, ...);
    // Shows a temporary message in chat in the form of a white chat message from toolbox. This message will disappear on map change.
    void FlashW(const wchar_t* format, ...);

    // Shows a temporary message in chat in the form of a red chat message from toolbox. This message will disappear on map change.
    void Error(const char* format, ...);
    // Shows a temporary message in chat in the form of a red chat message from toolbox. This message will disappear on map change.
    void ErrorW(const wchar_t* format, ...);

    // Shows a temporary message in chat in the form of a yellow chat message from toolbox. This message will disappear on map change.
    void Warning(const char* format, ...);
    // Shows a temporary message in chat in the form of a yellow chat message from toolbox. This message will disappear on map change.
    void WarningW(const wchar_t* format, ...);

    void FatalAssert(const char* expr, const char* file, const unsigned line);
}
