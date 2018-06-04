#ifndef UTF8_H_INC
#define UTF8_H_INC

#include <stddef.h>

struct Utf8
{
	char  *bytes = nullptr;
	size_t size;
	size_t length;
	size_t allocated;

	bool init(const wchar_t *str, size_t length);

	Utf8(const wchar_t *str);
	Utf8(const wchar_t *str, size_t length);

	~Utf8();

	const char *c_str() { return bytes; }
};

size_t Utf8ToUnicode(const char *str, wchar_t *buffer, size_t count);

#endif // UTF8_H_INC