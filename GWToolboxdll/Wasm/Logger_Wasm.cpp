// Minimal Log:: implementation for the wasm build - native Logger.cpp/CrashHandler.cpp are Windows-only and out of scope; every entry point still needs to link, so this routes to the browser console via mod_log instead.

#include "stdafx.h"

#include <Logger.h>

#include <Utils/TextUtils_Encoding.h>

#include <cstdarg>
#include <cstdio>
#include <cwchar>

extern "C" {
    __attribute__((import_module("env"), import_name("mod_log")))
    void mod_log(const char* ptr, int len);
}

namespace {
    void LogLine(const std::string& line)
    {
        mod_log(line.c_str(), static_cast<int>(line.size()));
    }

    void LogFormatted(const char* prefix, const char* format, va_list args)
    {
        char buf[1024];
        const int n = std::vsnprintf(buf, sizeof(buf), format, args);
        LogLine(std::string(prefix) + (n > 0 ? std::string(buf, static_cast<size_t>(n) < sizeof(buf) ? n : sizeof(buf) - 1) : std::string("<format error>")));
    }

    void LogFormattedW(const char* prefix, const wchar_t* format, va_list args)
    {
        wchar_t wbuf[1024];
        const int n = std::vswprintf(wbuf, sizeof(wbuf) / sizeof(wchar_t), format, args);
        const std::wstring wide = n > 0 ? std::wstring(wbuf, static_cast<size_t>(n)) : L"<format error>";
        LogLine(std::string(prefix) + TextUtils::Encoding::WideToUtf8(wide));
    }
} // namespace

namespace Log {
    bool InitializeLog() { return true; }
    bool InitializeGWCALog() { return true; }
    void InitializeChat() { }
    void Terminate() { }
    void FlushFile() { }

    void Log(const char* msg, ...)
    {
        va_list args;
        va_start(args, msg);
        LogFormatted("", msg, args);
        va_end(args);
    }

    void LogW(const wchar_t* msg, ...)
    {
        va_list args;
        va_start(args, msg);
        LogFormattedW("", msg, args);
        va_end(args);
    }

    void Info(const char* format, ...)
    {
        va_list args;
        va_start(args, format);
        LogFormatted("[info] ", format, args);
        va_end(args);
    }

    void InfoW(const wchar_t* format, ...)
    {
        va_list args;
        va_start(args, format);
        LogFormattedW("[info] ", format, args);
        va_end(args);
    }

    void Flash(const char* format, ...)
    {
        va_list args;
        va_start(args, format);
        LogFormatted("[flash] ", format, args);
        va_end(args);
    }

    void FlashW(const wchar_t* format, ...)
    {
        va_list args;
        va_start(args, format);
        LogFormattedW("[flash] ", format, args);
        va_end(args);
    }

    void Error(const char* format, ...)
    {
        va_list args;
        va_start(args, format);
        LogFormatted("[error] ", format, args);
        va_end(args);
    }

    void ErrorW(const wchar_t* format, ...)
    {
        va_list args;
        va_start(args, format);
        LogFormattedW("[error] ", format, args);
        va_end(args);
    }

    void Warning(const char* format, ...)
    {
        va_list args;
        va_start(args, format);
        LogFormatted("[warning] ", format, args);
        va_end(args);
    }

    void WarningW(const wchar_t* format, ...)
    {
        va_list args;
        va_start(args, format);
        LogFormattedW("[warning] ", format, args);
        va_end(args);
    }

    void FatalAssert(const char* expr, const char* file, const unsigned line)
    {
        char buf[512];
        std::snprintf(buf, sizeof(buf), "[FATAL] assertion failed: %s (%s:%u)", expr, file, line);
        LogLine(buf);
        std::abort(); // no CrashHandler equivalent yet - abort rather than continue past a broken invariant
    }
} // namespace Log
