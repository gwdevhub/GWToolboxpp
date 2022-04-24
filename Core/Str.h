#pragma once

int SnprintfStr(std::string& out, const char* fmt, ...);
int SnprintfWstr(std::wstring& out, const wchar_t* fmt, ...);

void StrCopyA(char *dest, size_t size, const char *src);
void StrCopyW(wchar_t *dest, size_t size, const wchar_t *src);

void StrAppendA(char *dest, size_t size, const char *src);
void StrAppendW(wchar_t *dest, size_t size, const wchar_t *src);

size_t StrLenA(const char *str);
size_t StrLenW(const wchar_t *str);

size_t StrBytesA(const char *str);
size_t StrBytesW(const wchar_t *str);
