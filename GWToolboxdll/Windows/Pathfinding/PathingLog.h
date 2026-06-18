#pragma once

#include <Logger.h>

// Pathing failures (unreachable markers, missing/mismatched DAT data, malformed
// portal connection data, etc.) are expected during normal play and must not
// spam the in-game chat. In debug builds they surface via Log::Error/Log::Warning
// so developers see them; in release they go to the log file only.
#ifdef _DEBUG
#define PATH_LOG_ERROR(...) Log::Error(__VA_ARGS__)
#define PATH_LOG_WARNING(...) Log::Warning(__VA_ARGS__)
#else
#define PATH_LOG_ERROR(...) Log::Log(__VA_ARGS__)
#define PATH_LOG_WARNING(...) Log::Log(__VA_ARGS__)
#endif
