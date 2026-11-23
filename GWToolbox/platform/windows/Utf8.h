#pragma once

#include <string>
#include <string_view>

inline std::string ToUtf8(const std::wstring_view text)
{
    const auto length = static_cast<int>(text.size());
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, text.data(), length, nullptr, 0, nullptr, nullptr);
    std::string utf8(bytes, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), length, utf8.data(), bytes, nullptr, nullptr);
    return utf8;
}

inline std::wstring FromUtf8(const std::string_view text)
{
    const auto length = static_cast<int>(text.size());
    const int chars = MultiByteToWideChar(CP_UTF8, 0, text.data(), length, nullptr, 0);
    std::wstring wide(chars, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), length, wide.data(), chars);
    return wide;
}
