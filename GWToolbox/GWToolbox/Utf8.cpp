#include "Utf8.h"

#include <uchar.h>
#include <wchar.h>
#include <string.h>
#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

static size_t compute_required_size(const wchar_t *str, size_t max_len);

Utf8::~Utf8()
{
	if (bytes) delete bytes;
}

bool Utf8::init(const wchar_t *str, size_t len)
{
	// we alway put a null character
	size_t required_size = compute_required_size(str, len);
	if (!required_size) return false;

	bytes = new char[required_size + 1];
	if (!bytes) return false;

	allocated = required_size + 1;
	length = len;
	size = required_size;
	
	WideCharToMultiByte(CP_UTF8, 0, str, len, bytes, required_size, NULL, NULL);
	bytes[size] = 0;
	return true;
}

Utf8::Utf8(const wchar_t *str)
{
	size_t len = wcslen(str);
	if (!init(str, len)) {
		bytes = nullptr;
	}
}

Utf8::Utf8(const wchar_t *str, size_t length)
{
	if (!init(str, length)) {
		bytes = nullptr;
	}
}

static size_t compute_required_size(const wchar_t *str, size_t max_len)
{
	int res = WideCharToMultiByte(CP_UTF8, 0, str, max_len, NULL, 0, NULL, NULL);
	if (!res) return 0;
	return res;
}