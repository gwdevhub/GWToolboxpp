#include "Utf8.h"

#include <Windows.h>
#include <stdio.h>

string Unicode16ToUtf8(const wchar_t *str)
{
	string res = {0};
	int size = WideCharToMultiByte(CP_UTF8, 0, str, -1, NULL, 0, NULL, NULL);
	if (size < 0) return res;
	res.bytes = new char[size + 1];
	res.count = size;
	WideCharToMultiByte(CP_UTF8, 0, str, -1, res.bytes, size + 1, NULL, NULL);
	return res;
}

size_t Utf8ToUnicode(const char *str, wchar_t *buffer, size_t count)
{
	return MultiByteToWideChar(CP_UTF8, 0, str, -1, buffer, count);
}