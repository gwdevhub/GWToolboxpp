#include "stdafx.h"

#include <Utf8.h>
#include "Utils/TextUtils_Encoding.h"

utf8::string Unicode16ToUtf8(const wchar_t* str)
{
    utf8::string res;
    if (!str) {
        return res;
    }

    std::string utf8_str = TextUtils::Encoding::WideToUtf8(str);
    if (utf8_str.empty()) {
        return res;
    }

    const auto size = utf8_str.size();
    res.bytes = static_cast<char*>(malloc(size + 1));
    if (res.bytes) {
        std::copy(utf8_str.begin(), utf8_str.end(), res.bytes);
        res.bytes[size] = 0;
        res.count = size;
        res.allocated = true;
    }
    return res;
}

utf8::string Unicode16ToUtf8(const wchar_t* start, const wchar_t* end)
{
    utf8::string res;
    if (!start || !end || start > end) {
        return res;
    }

    std::wstring_view wide_view(start, end - start);
    std::string utf8_str = TextUtils::Encoding::WideToUtf8(wide_view);
    if (utf8_str.empty()) {
        return res;
    }

    const auto size = utf8_str.size();
    res.bytes = static_cast<char*>(malloc(size + 1));
    if (res.bytes) {
        std::copy(utf8_str.begin(), utf8_str.end(), res.bytes);
        res.bytes[size] = 0;
        res.count = size;
        res.allocated = true;
    }
    return res;
}

utf8::string Unicode16ToUtf8(char* buffer, const size_t n_buffer, const wchar_t* start, const wchar_t* end)
{
    utf8::string res;
    if (!buffer || n_buffer == 0 || !start || !end || start > end) {
        return res;
    }

    std::wstring_view wide_view(start, end - start);
    std::string utf8_str = TextUtils::Encoding::WideToUtf8(wide_view);
    if (utf8_str.empty()) {
        return res;
    }

    const auto size = utf8_str.size();
    if (size >= n_buffer) {
        return res;
    }

    std::copy(utf8_str.begin(), utf8_str.end(), buffer);
    if (size + 1 < n_buffer) {
        buffer[size] = 0;
    }
    res.bytes = buffer;
    res.count = size;
    return res;
}

size_t Utf8ToUnicode(const char* str, wchar_t* buffer, const size_t count)
{
    if (!str || !buffer || count == 0) {
        return 0;
    }

    std::wstring wide_str = TextUtils::Encoding::Utf8ToWide(str);
    if (wide_str.empty()) {
        if (count > 0) {
            buffer[0] = 0;
        }
        return 1;
    }

    if (wide_str.size() + 1 > count) {
        return 0;
    }

    std::copy(wide_str.begin(), wide_str.end(), buffer);
    buffer[wide_str.size()] = 0;
    return wide_str.size() + 1;
}
