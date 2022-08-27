#pragma once

#define ASSERT(expr) ((void)(!!(expr) || (Log::FatalAssert(#expr, __FILE__, (unsigned)__LINE__), 0)))
#define IM_ASSERT(expr) ASSERT(expr)

#define GWTOOLBOX_CHAN GW::Chat::Channel::CHANNEL_GWCA2
#define GWTOOLBOX_SENDER L"GWToolbox++"
#define GWTOOLBOX_SENDER_COL 0x00ccff
#define GWTOOLBOX_WARNING_COL 0xFFFF44
#define GWTOOLBOX_ERROR_COL 0xFF4444
#define GWTOOLBOX_INFO_COL 0xFFFFFF

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
    // Shows to the user in the form of a white chat message from toolbox
    void Info(const char* format, ...);
    void InfoW(const wchar_t* format, ...);

    // Shows to the user in the form of a red chat message from toolbox
    void Error(const char* format, ...);
    void ErrorW(const wchar_t* format, ...);

    // Shows to the user in the form of a yellow chat message from toolbox
    void Warning(const char* format, ...);
    void WarningW(const wchar_t* format, ...);

    void FatalAssert(const char *expr, const char *file, unsigned int line);
};
