#ifndef UTF8_H_INC
#define UTF8_H_INC

#include <stddef.h>

struct string {
	char  *bytes;
	size_t count;
};

static inline void free(string& s) {
	if (s.bytes) delete s.bytes;
}

string Unicode16ToUtf8(const wchar_t *str);

#endif // UTF8_H_INC