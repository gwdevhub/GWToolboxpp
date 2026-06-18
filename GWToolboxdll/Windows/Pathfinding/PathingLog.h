#pragma once

#include <Logger.h>

// Pathing failures (unreachable markers, missing/mismatched DAT data, malformed
// portal connection data, etc.) and one-shot notices (e.g. "loaded N portal
// connections") are expected during normal play and must not spam the in-game
// chat. In debug builds they surface via Log::Error/Warning/Info so developers
// see them; in release they go to the log file only.
//
// Note: this is distinct from PATH_LOG_INFO, which guards high-volume per-frame
// pathfinding chatter and is compiled out entirely unless PATHING_VERBOSE is set.
#ifdef _DEBUG
#define PATH_LOG_ERROR(...) Log::Error(__VA_ARGS__)
#define PATH_LOG_WARNING(...) Log::Warning(__VA_ARGS__)
#define PATH_LOG_NOTICE(...) Log::Info(__VA_ARGS__)
#else
#define PATH_LOG_ERROR(...) Log::Log(__VA_ARGS__)
#define PATH_LOG_WARNING(...) Log::Log(__VA_ARGS__)
#define PATH_LOG_NOTICE(...) Log::Log(__VA_ARGS__)
#endif
