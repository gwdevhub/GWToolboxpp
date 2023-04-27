#pragma once

namespace utf8 {
    struct string {
        char  *bytes = nullptr;
        size_t count = 0;
        bool allocated = false;
        
        string() = default;
        ~string()
        {
            if (allocated)
                free(bytes);
            bytes = nullptr;
            count = 0;
        }

        string(const string& s) = delete;
        string& operator=(const string& s) = delete;

        string(string&& s) noexcept
        {
            bytes = s.bytes;
            count = s.count;
            s.bytes = nullptr;
            s.count = 0;
        }

        string& operator=(string&& s) noexcept
        {
            bytes = s.bytes;
            count = s.count;
            s.bytes = nullptr;
            s.count = 0;
            return *this;
        }
    };
}

// encode a unicode16 to utf8 using a allocated buffer (malloc).
utf8::string Unicode16ToUtf8(const wchar_t *str);
utf8::string Unicode16ToUtf8(const wchar_t *start, const wchar_t *end);

// encode a unicode16 to utf8 using the provided buffer.
utf8::string Unicode16ToUtf8(char *buffer, size_t n_buffer, const wchar_t *start, const wchar_t *end);

size_t Utf8ToUnicode(const char *str, wchar_t *buffer, size_t count);

// Custom normalization that returns a allocated buffer (malloc) and that do:
//  - change to lower case
//  - remove accent
//  - remove non-printable character (e.g. zero-width spaces)
utf8::string Utf8Normalize(const char *str);
