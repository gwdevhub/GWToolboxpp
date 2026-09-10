#pragma once

#include <Logger.h>

#ifdef _DEBUG
#define PATH_LOG_ERROR(...) Log::Error(__VA_ARGS__)
#define PATH_LOG_WARNING(...) Log::Warning(__VA_ARGS__)
#define PATH_LOG_NOTICE(...) Log::Info(__VA_ARGS__)
#else
#define PATH_LOG_ERROR(...) Log::Log(__VA_ARGS__)
#define PATH_LOG_WARNING(...) Log::Log(__VA_ARGS__)
#define PATH_LOG_NOTICE(...) Log::Log(__VA_ARGS__)
#endif
