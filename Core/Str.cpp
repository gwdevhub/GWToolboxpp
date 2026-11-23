#include "stdafx.h"

#include "Str.h"

int StrSprintf(std::string& out, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    const int written = StrVsprintf(out, fmt, args);
    va_end(args);
    return written;
}

int StrVsprintf(std::string& out, const char* fmt, va_list args)
{
    int written = vsnprintf(nullptr, 0, fmt, args);
    if (written < 0) {
        return written;
    }
    out.resize(written + 1);
    written = vsnprintf(out.data(), out.capacity(), fmt, args);
    if (written < 0) {
        return written;
    }
    out.resize(written);
    return written;
}

int StrSwprintf(std::wstring& out, const wchar_t* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    const int written = StrVswprintf(out, fmt, args);
    va_end(args);
    return written;
}

// Only MSVC's vswprintf reports the needed length for a null buffer; the standard one returns -1, so grow until it fits.
int StrVswprintf(std::wstring& out, const wchar_t* fmt, va_list args)
{
    constexpr size_t max_length = 1 << 20;
    out.resize(256);
    while (true) {
        va_list attempt;
        va_copy(attempt, args);
        const int written = vswprintf(out.data(), out.size(), fmt, attempt);
        va_end(attempt);
        if (written >= 0) {
            out.resize(written);
            return written;
        }
        if (out.size() >= max_length) {
            out.clear();
            return written;
        }
        out.resize(out.size() * 2);
    }
}

void StrCopyA(char* dest, const size_t size, const char* src)
{
    size_t i;
    for (i = 0; i < size - 1; i++) {
        if (src[i] == 0) {
            break;
        }
        dest[i] = src[i];
    }
    dest[i] = 0;
}

void StrCopyW(wchar_t* dest, const size_t size, const wchar_t* src)
{
    size_t i;
    for (i = 0; i < size - 1; i++) {
        if (src[i] == 0) {
            break;
        }
        dest[i] = src[i];
    }
    dest[i] = 0;
}

void StrAppendA(char* dest, const size_t size, const char* src)
{
    const size_t start = strnlen(dest, size);
    const size_t remaining = size - start;
    StrCopyA(&dest[start], remaining, src);
}

void StrAppendW(wchar_t* dest, const size_t size, const wchar_t* src)
{
    const size_t start = wcsnlen(dest, size);
    const size_t remaining = size - start;
    StrCopyW(&dest[start], remaining, src);
}

size_t StrLenA(const char* str)
{
    return strlen(str);
}

size_t StrLenW(const wchar_t* str)
{
    return wcslen(str);
}

size_t StrBytesA(const char* str)
{
    return StrLenA(str) + 1;
}

size_t StrBytesW(const wchar_t* str)
{
    return (StrLenW(str) + 2) * sizeof(wchar_t);
}
