#include "stdafx.h"
#include "Utf8.h"

#include <Windows.h>
#include <stdio.h>

#include <utf8proc.h>

utf8::string Unicode16ToUtf8(const wchar_t *str)
{
	utf8::string res = {0};
	int size = WideCharToMultiByte(CP_UTF8, 0, str, -1, NULL, 0, NULL, NULL);
	if (size < 0) return res;
	res.bytes = new char[size + 1];
	res.count = size;
	WideCharToMultiByte(CP_UTF8, 0, str, -1, res.bytes, size + 1, NULL, NULL);
	return res;
}

utf8::string Unicode16ToUtf8(const wchar_t *start, const wchar_t *end)
{
	utf8::string res = {0};
	int size = WideCharToMultiByte(CP_UTF8, 0, start, end - start, NULL, 0, NULL, NULL);
	if (size < 0) return res;
	res.bytes = new char[size + 1];
	res.count = size;
	WideCharToMultiByte(CP_UTF8, 0, start, end - start, res.bytes, size + 1, NULL, NULL);
	res.bytes[res.count] = 0;
	return res;
}

utf8::string Unicode16ToUtf8(char *buffer, size_t n_buffer, const wchar_t *start, const wchar_t *end)
{
	utf8::string res = {0};
	int size = WideCharToMultiByte(CP_UTF8, 0, start, end - start, buffer, n_buffer, NULL, NULL);
	if (size < 0) return res;
	res.bytes = buffer;
	res.count = size;
	if ((size_t)(size + 1) < n_buffer)
		res.bytes[size] = 0;
	return res;
}

size_t Utf8ToUnicode(const char *str, wchar_t *buffer, size_t count)
{
	return MultiByteToWideChar(CP_UTF8, 0, str, -1, buffer, count);
}

utf8::string Utf8Normalize(const char *str)
{
	const int flags = UTF8PROC_NULLTERM | UTF8PROC_STABLE |
		UTF8PROC_COMPOSE | UTF8PROC_COMPAT | UTF8PROC_CASEFOLD | UTF8PROC_IGNORE | UTF8PROC_STRIPMARK;
	utf8::string res = {0};
	utf8proc_map((utf8proc_uint8_t *)str, 0, (utf8proc_uint8_t **)&res.bytes, (utf8proc_option_t)flags);
	res.count = res.bytes ? strlen(res.bytes) : 0;
	return res;
}