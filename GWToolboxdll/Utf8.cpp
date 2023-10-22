#include "stdafx.h"

#include <Utf8.h>

utf8::string Unicode16ToUtf8(const wchar_t* str)
{
    utf8::string res;
    const auto isize = WideCharToMultiByte(CP_UTF8, 0, str, -1, nullptr, 0, nullptr, nullptr);
    if (isize < 0) {
        return res;
    }
    const auto size = static_cast<size_t>(isize);
    res.bytes = static_cast<char*>(malloc(size + 1));
    res.count = size;
    res.allocated = true;
    WideCharToMultiByte(CP_UTF8, 0, str, -1, res.bytes, isize + 1, nullptr, nullptr);
    return res;
}

utf8::string Unicode16ToUtf8(const wchar_t* start, const wchar_t* end)
{
    utf8::string res;
    const auto isize = WideCharToMultiByte(CP_UTF8, 0, start, end - start, nullptr, 0,
                                           nullptr, nullptr);
    if (isize < 0) {
        return res;
    }
    const auto size = static_cast<size_t>(isize);
    res.bytes = static_cast<char*>(malloc(size + 1));
    res.count = size;
    res.allocated = true;
    WideCharToMultiByte(CP_UTF8, 0, start, end - start, res.bytes, isize + 1,
                        nullptr, nullptr);
    res.bytes[res.count] = 0;
    return res;
}

utf8::string Unicode16ToUtf8(char* buffer, const size_t n_buffer, const wchar_t* start, const wchar_t* end)
{
    utf8::string res;
    const auto isize = WideCharToMultiByte(CP_UTF8, 0, start, end - start, buffer,
                                           static_cast<int>(n_buffer), nullptr, nullptr);
    if (isize < 0) {
        return res;
    }
    const auto size = static_cast<size_t>(isize);
    res.bytes = buffer;
    res.count = size;
    if (size + 1 < n_buffer) {
        res.bytes[size] = 0;
    }
    return res;
}

size_t Utf8ToUnicode(const char* str, wchar_t* buffer, const size_t count)
{
    const auto iret = MultiByteToWideChar(CP_UTF8, 0, str, -1, buffer, static_cast<int>(count));
    if (iret < 0) {
        return 0;
    }
    return static_cast<size_t>(iret);
}
