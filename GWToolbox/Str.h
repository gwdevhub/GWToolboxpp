#pragma once

void StrCopyA(char *dest, size_t size, const char *src);
void StrCopyW(wchar_t *dest, size_t size, const wchar_t *src);

size_t StrLenA(const char *str);
size_t StrLenW(const wchar_t *str);

size_t StrBytesA(const char *str);
size_t StrBytesW(const wchar_t *str);
