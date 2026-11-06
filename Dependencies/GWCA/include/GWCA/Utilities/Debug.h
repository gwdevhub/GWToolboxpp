#pragma once
#include <GWCA/Runtime.h>
#ifdef _DEBUG
    // ##__VA_ARGS__ is a GNU extension swallowing the trailing comma when no extra args are passed; MSVC tolerates plain __VA_ARGS__.
    #define GWCA_TRACE(fmt, ...) GW::LogMessage(GW::LEVEL_TRACE, __FILE__, (unsigned)__LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
    #define GWCA_DEBUG(fmt, ...) GW::LogMessage(GW::LEVEL_DEBUG, __FILE__, (unsigned)__LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
    #define GWCA_INFO(fmt, ...) GW::LogMessage(GW::LEVEL_INFO, __FILE__, (unsigned)__LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
    #define GWCA_WARN(fmt, ...) GW::LogMessage(GW::LEVEL_WARN, __FILE__, (unsigned)__LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
    #define GWCA_ERR(fmt, ...) GW::LogMessage(GW::LEVEL_ERR, __FILE__, (unsigned)__LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
    #define GWCA_CRITICAL(fmt, ...) GW::LogMessage(GW::LEVEL_CRITICAL, __FILE__, (unsigned)__LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
    #define GWCA_ASSERT(expr) ((void)(!!(expr) || (GW::FatalAssert(#expr, __FILE__, (unsigned)__LINE__, __FUNCTION__), 0)))
#else
    #define GWCA_ASSERT(expr) ((void)(!!(expr) || (GW::FatalAssert("", "", __COUNTER__, ""), 0)))
    #define GWCA_TRACE(fmt, ...)
    #define GWCA_DEBUG(fmt, ...)
    #define GWCA_INFO(fmt, ...)
    #define GWCA_WARN(fmt, ...)
    #define GWCA_ERR(fmt, ...)
    #define GWCA_CRITICAL(fmt, ...)
#endif

#include <GWCA/Utilities/Export.h>
#include <cstdarg>

namespace GW
{
    enum LogLevel {
        LEVEL_TRACE,
        LEVEL_DEBUG,
        LEVEL_INFO,
        LEVEL_WARN,
        LEVEL_ERR,
        LEVEL_CRITICAL,
    };

    typedef void (__cdecl *LogHandler)(
        void *context,
        LogLevel level,
        const char *msg,
        const char *file,
        unsigned int line,
        const char *function);
    void RegisterLogHandler(LogHandler handler, void *context);

    typedef void (__cdecl *PanicHandler)(
        void *context,
        const char *expr,
        const char *file,
        unsigned int line,
        const char *function);
    void RegisterPanicHandler(PanicHandler handler, void *context);

    void FatalAssert(
        const char *expr,
        const char *file,
        unsigned int line,
        const char *function);

    void __cdecl LogMessageV(
        LogLevel level,
        const char *file,
        unsigned int line,
        const char *function,
        const char *fmt,
        va_list args);

    inline void __cdecl LogMessage(
        LogLevel level,
        const char *file,
        unsigned int line,
        const char *function,
        const char *fmt,
        ...)
    {
        va_list args;
        va_start(args, fmt);
        LogMessageV(level, file, line, function, fmt, args);
        va_end(args);
    }

}

namespace GW {
    extern "C" GWCA_API void __cdecl GW_RegisterLogHandler(LogHandler handler, void* context) noexcept;
    extern "C" GWCA_API void __cdecl GW_RegisterPanicHandler(PanicHandler handler, void* context) noexcept;
    extern "C" GWCA_API void __cdecl GW_FatalAssert(const char* expr, const char* file, unsigned int line, const char* function) noexcept;
    extern "C" GWCA_API void __cdecl GW_LogMessageV(LogLevel level, const char* file, unsigned int line, const char* function, const char* fmt, va_list args) noexcept;
}

#if !defined(GWCA_BUILD_EXPORTS)
namespace GW {
    inline void RegisterLogHandler(LogHandler handler, void* context)
    {
        return GWCA_CALL_C(GW_RegisterLogHandler, handler, context);
    }

    inline void RegisterPanicHandler(PanicHandler handler, void* context)
    {
        return GWCA_CALL_C(GW_RegisterPanicHandler, handler, context);
    }

    inline void FatalAssert(const char* expr, const char* file, unsigned int line, const char* function)
    {
        return GWCA_CALL_C(GW_FatalAssert, expr, file, line, function);
    }

    inline void LogMessageV(LogLevel level, const char* file, unsigned int line, const char* function, const char* fmt, va_list args)
    {
        return GWCA_CALL_C(GW_LogMessageV, level, file, line, function, fmt, args);
    }
}
#endif
