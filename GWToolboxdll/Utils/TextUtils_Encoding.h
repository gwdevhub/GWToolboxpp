#pragma once

#include <string>
#include <string_view>

namespace TextUtils {
    namespace Encoding {
        std::wstring Utf8ToWide(std::string_view utf8_str);
        std::string WideToUtf8(std::wstring_view wide_str);

        bool IsValidUtf8(const char* str, size_t len);
    }
}
