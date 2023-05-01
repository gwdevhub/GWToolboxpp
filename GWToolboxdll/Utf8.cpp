#include "stdafx.h"

#include <Utf8.h>
#include <utf8proc.h>

utf8::string Unicode16ToUtf8(const wchar_t *str)
{
    utf8::string res;
    const int isize = WideCharToMultiByte(CP_UTF8, 0, str, -1, NULL, 0, NULL, NULL);
    if (isize < 0) return res;
    const auto size = static_cast<size_t>(isize);
    res.bytes = static_cast<char*>(malloc(size + 1));
    res.count = size;
    res.allocated = true;
    WideCharToMultiByte(CP_UTF8, 0, str, -1, res.bytes, isize + 1, NULL, NULL);
    return res;
}

utf8::string Unicode16ToUtf8(const wchar_t *start, const wchar_t *end)
{
    utf8::string res;
    const int isize = WideCharToMultiByte(CP_UTF8, 0, start, end - start, nullptr, 0,
                                    nullptr, nullptr);
    if (isize < 0) return res;
    const auto size = static_cast<size_t>(isize);
    res.bytes = static_cast<char*>(malloc(size + 1));
    res.count = size;
    res.allocated = true;
    WideCharToMultiByte(CP_UTF8, 0, start, end - start, res.bytes, isize + 1,
                        nullptr, nullptr);
    res.bytes[res.count] = 0;
    return res;
}

utf8::string Unicode16ToUtf8(char *buffer, size_t n_buffer, const wchar_t *start, const wchar_t *end)
{
    utf8::string res;
    const int isize = WideCharToMultiByte(CP_UTF8, 0, start, end - start, buffer,
                                          static_cast<int>(n_buffer), nullptr, nullptr);
    if (isize < 0) return res;
    const auto size = static_cast<size_t>(isize);
    res.bytes = buffer;
    res.count = size;
    if (size + 1 < n_buffer)
        res.bytes[size] = 0;
    return res;
}

size_t Utf8ToUnicode(const char *str, wchar_t *buffer, size_t count)
{
    const int iret = MultiByteToWideChar(CP_UTF8, 0, str, -1, buffer, static_cast<int>(count));
    if (iret < 0)
        return 0;
    return static_cast<size_t>(iret);
}

utf8::string Utf8Normalize(const char *str)
{
    constexpr int flags = UTF8PROC_NULLTERM | UTF8PROC_STABLE |
        UTF8PROC_COMPOSE | UTF8PROC_COMPAT | UTF8PROC_CASEFOLD | UTF8PROC_IGNORE | UTF8PROC_STRIPMARK;
    utf8::string res;
    utf8proc_map((utf8proc_uint8_t *)str, 0, (utf8proc_uint8_t **)&res.bytes, (utf8proc_option_t)flags);
    res.count = res.bytes ? strlen(res.bytes) : 0;
    res.allocated = true;
    return res;
}
