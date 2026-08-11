#include "stdafx.h"
#include "TextUtils_Time.h"
#include <chrono>
#include <mutex>

namespace TextUtils {
    namespace Time {
        std::tm SafeLocaltime(const time_t timestamp)
        {
#ifdef _WIN32
            std::tm result{};
            localtime_s(&result, &timestamp);
            return result;
#else
            std::tm result{};
            auto* local_tm = std::localtime(&timestamp);
            if (local_tm) {
                result = *local_tm;
            }
            return result;
#endif
        }

        SystemTime GetCurrentSystemTime()
        {
            auto now = std::chrono::system_clock::now();
            auto duration = now.time_since_epoch();
            auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration) % 1000;

            time_t time_t_now = std::chrono::system_clock::to_time_t(now);
            std::tm local_tm = SafeLocaltime(time_t_now);

            return SystemTime{
                static_cast<uint16_t>(local_tm.tm_year + 1900),
                static_cast<uint16_t>(local_tm.tm_mon + 1),
                static_cast<uint16_t>(local_tm.tm_mday),
                static_cast<uint16_t>(local_tm.tm_hour),
                static_cast<uint16_t>(local_tm.tm_min),
                static_cast<uint16_t>(local_tm.tm_sec),
                static_cast<uint16_t>(millis.count())
            };
        }

        std::string FilenameTimestamp()
        {
            auto now = GetCurrentSystemTime();
            char buf[32];
            snprintf(buf, sizeof(buf), "%04d-%02d-%02d_%02d-%02d-%02d",
                     now.year, now.month, now.day, now.hour, now.minute, now.second);
            return buf;
        }
    }
}
