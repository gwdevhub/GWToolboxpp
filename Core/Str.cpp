#include "stdafx.h"

#include "Str.h"

int StrSprintf(std::string& out, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int written = StrVsprintf(out, fmt, args);
    va_end(args);
    return written;
}
int StrVsprintf(std::string& out, const char* fmt, va_list args) {
    int written = vsnprintf(nullptr, 0, fmt, args);
    if (written < 0)
        return written;
    out.resize(written + 1);
    written = vsnprintf(out.data(), out.capacity(), fmt, args);
    if (written < 0)
        return written;
    out.resize(written);
    return written;
}

int StrSwprintf(std::wstring& out, const wchar_t* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int written = StrVswprintf(out, fmt, args);
    va_end(args);
    return written;
}
int StrVswprintf(std::wstring& out, const wchar_t* fmt, va_list args) {
    int written = vswprintf(nullptr, 0, fmt, args);
    if (written < 0)
        return written;
    out.resize(written + 1);
    written = vswprintf(out.data(), out.capacity(), fmt, args);
    if (written < 0)
        return written;
    out.resize(written);
    return written;
}

void StrCopyA(char *dest, size_t size, const char *src)
{
    size_t i;
    for (i = 0; i < (size - 1); i++) {
        if (src[i] == 0)
            break;
        dest[i] = src[i];
    }
    dest[i] = 0;
}

void StrCopyW(wchar_t *dest, size_t size, const wchar_t *src)
{
    size_t i;
    for (i = 0; i < (size - 1); i++) {
        if (src[i] == 0)
            break;
        dest[i] = src[i];
    }
    dest[i] = 0;
}

void StrAppendA(char *dest, size_t size, const char *src)
{
    size_t start = strnlen(dest, size);
    size_t remaining = size - start;
    StrCopyA(&dest[start], remaining, src);
}

void StrAppendW(wchar_t *dest, size_t size, const wchar_t *src)
{
    size_t start = wcsnlen(dest, size);
    size_t remaining = size - start;
    StrCopyW(&dest[start], remaining, src);
}

size_t StrLenA(const char *str)
{
    return strlen(str);
}

size_t StrLenW(const wchar_t *str)
{
    return wcslen(str);
}

size_t StrBytesA(const char *str)
{
    return (StrLenA(str) + 1);
}

size_t StrBytesW(const wchar_t *str)
{
    return (StrLenW(str) + 2) * sizeof(wchar_t);
}
