#pragma once

#include <ctime>
#include <string>

namespace TextUtils {
    namespace Time {
        struct SystemTime {
            uint16_t year;
            uint16_t month;
            uint16_t day;
            uint16_t hour;
            uint16_t minute;
            uint16_t second;
            uint16_t millisecond;
        };

        std::tm SafeLocaltime(time_t timestamp);
        SystemTime GetCurrentSystemTime();
        std::string FilenameTimestamp();

#ifndef __EMSCRIPTEN__
        time_t FiletimeToTimeT(uint64_t filetime_100ns);
#endif
    }
}
