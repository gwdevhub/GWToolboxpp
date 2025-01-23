#pragma once
#define GWCA_ASSERT(expr) ((void)(!!(expr) || (GW::FatalAssert(#expr, __FILE__, (unsigned)__LINE__, __FUNCTION__), 0)))
#ifdef _DEBUG
    #define GWCA_TRACE(fmt, ...) GW::LogMessage(GW::LEVEL_TRACE, __FILE__, (unsigned)__LINE__, __FUNCTION__, fmt, __VA_ARGS__)
    #define GWCA_DEBUG(fmt, ...) GW::LogMessage(GW::LEVEL_DEBUG, __FILE__, (unsigned)__LINE__, __FUNCTION__, fmt, __VA_ARGS__)
    #define GWCA_INFO(fmt, ...) GW::LogMessage(GW::LEVEL_INFO, __FILE__, (unsigned)__LINE__, __FUNCTION__, fmt, __VA_ARGS__)
    #define GWCA_WARN(fmt, ...) GW::LogMessage(GW::LEVEL_WARN, __FILE__, (unsigned)__LINE__, __FUNCTION__, fmt, __VA_ARGS__)
    #define GWCA_ERR(fmt, ...) GW::LogMessage(GW::LEVEL_ERR, __FILE__, (unsigned)__LINE__, __FUNCTION__, fmt, __VA_ARGS__)
    #define GWCA_CRITICAL(fmt, ...) GW::LogMessage(GW::LEVEL_CRITICAL, __FILE__, (unsigned)__LINE__, __FUNCTION__, fmt, __VA_ARGS__)
#else

    #define GWCA_TRACE(fmt, ...)
    #define GWCA_DEBUG(fmt, ...)
    #define GWCA_INFO(fmt, ...)
    #define GWCA_WARN(fmt, ...)
    #define GWCA_ERR(fmt, ...)
    #define GWCA_CRITICAL(fmt, ...)
#endif

#include <GWCA/Utilities/Export.h>

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

    typedef void (*LogHandler)(
        void *context,
        LogLevel level,
        const char *msg,
        const char *file,
        unsigned int line,
        const char *function);
    GWCA_API void RegisterLogHandler(LogHandler handler, void *context);

    typedef void (*PanicHandler)(
        void *context,
        const char *expr,
        const char *file,
        unsigned int line,
        const char *function);
    GWCA_API void RegisterPanicHandler(PanicHandler handler, void *context);

    GWCA_API void FatalAssert(
        const char *expr,
        const char *file,
        unsigned int line,
        const char *function);

    GWCA_API void __cdecl LogMessage(
        LogLevel level,
        const char *file,
        unsigned int line,
        const char *function,
        const char *fmt,
        ...);

    void __cdecl LogMessageV(
        LogLevel level,
        const char *file,
        unsigned int line,
        const char *function,
        const char *fmt,
        va_list args);
}
