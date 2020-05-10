#include <string.h>

#include "Str.h"

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
