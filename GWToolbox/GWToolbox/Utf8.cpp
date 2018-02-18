#include "Utf8.h"

#include <uchar.h>
#include <wchar.h>
#include <string.h>
#include <stdlib.h>

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
	wcstombs(bytes, str, size);
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
	char buffer[4 /* MB_CUR_MAX */];
	size_t total = 0;
	for (size_t i = 0; i < max_len; i++) {
		int len = wctomb(buffer, str[i]);
		if (len < 0) return 0;
		total += len;
	}
	return total;
}