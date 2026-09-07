// libc's wcs* assume a 4-byte wchar_t and we build -fshort-wchar; see README.md, "-fshort-wchar".

#include <wchar.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>

static_assert(sizeof(wchar_t) == 2, "GWCA's wasm build expects -fshort-wchar");

namespace {
    // wchar_t's signedness is implementation-defined; order by code unit, as UTF-16 requires.
    inline uint16_t unit(const wchar_t c) { return static_cast<uint16_t>(c); }
}

extern "C" {

size_t wcslen(const wchar_t* s)
{
    const wchar_t* p = s;
    while (*p)
        ++p;
    return static_cast<size_t>(p - s);
}

wchar_t* wcschr(const wchar_t* s, wchar_t c)
{
    for (;; ++s) {
        if (*s == c)
            return const_cast<wchar_t*>(s);
        if (!*s)
            return nullptr;
    }
}

wchar_t* wcsrchr(const wchar_t* s, wchar_t c)
{
    const wchar_t* found = nullptr;
    for (;; ++s) {
        if (*s == c)
            found = s;
        if (!*s)
            return const_cast<wchar_t*>(found);
    }
}

int wcscmp(const wchar_t* a, const wchar_t* b)
{
    while (*a && *a == *b) {
        ++a;
        ++b;
    }
    return unit(*a) < unit(*b) ? -1 : unit(*a) > unit(*b) ? 1 : 0;
}

int wcsncmp(const wchar_t* a, const wchar_t* b, size_t n)
{
    for (; n && *a && *a == *b; --n) {
        ++a;
        ++b;
    }
    if (!n)
        return 0;
    return unit(*a) < unit(*b) ? -1 : unit(*a) > unit(*b) ? 1 : 0;
}

int wmemcmp(const wchar_t* a, const wchar_t* b, size_t n)
{
    for (; n; --n, ++a, ++b) {
        if (*a != *b)
            return unit(*a) < unit(*b) ? -1 : 1;
    }
    return 0;
}

wchar_t* wcscpy(wchar_t* dst, const wchar_t* src)
{
    wchar_t* out = dst;
    while ((*out++ = *src++) != 0)
        ;
    return dst;
}

// Pads to n with NULs and does not terminate when src is longer, as the standard requires.
wchar_t* wcsncpy(wchar_t* dst, const wchar_t* src, size_t n)
{
    wchar_t* out = dst;
    for (; n && *src; --n)
        *out++ = *src++;
    for (; n; --n)
        *out++ = 0;
    return dst;
}

wchar_t* wcsstr(const wchar_t* haystack, const wchar_t* needle)
{
    if (!*needle)
        return const_cast<wchar_t*>(haystack);
    for (; *haystack; ++haystack) {
        const wchar_t* h = haystack;
        const wchar_t* n = needle;
        while (*n && *h == *n) {
            ++h;
            ++n;
        }
        if (!*n)
            return const_cast<wchar_t*>(haystack);
    }
    return nullptr;
}

} // extern "C"

namespace {
    // musl keeps all four wcsto*l in one archive member, so all four must be defined here
    // or that member is pulled in for a sibling and collides with ours.
    unsigned long long ParseWide(const wchar_t* s, wchar_t** end, int base, bool& negative, bool& overflow)
    {
        const wchar_t* p = s;
        while (*p == L' ' || (*p >= 0x9 && *p <= 0xd))
            ++p;

        negative = false;
        if (*p == L'-' || *p == L'+')
            negative = (*p++ == L'-');

        if ((base == 0 || base == 16) && p[0] == L'0' && (p[1] == L'x' || p[1] == L'X')) {
            base = 16;
            p += 2;
        }
        else if (base == 0) {
            base = (p[0] == L'0') ? 8 : 10;
        }

        const wchar_t* digits_at = p;
        unsigned long long acc = 0;
        overflow = false;
        for (;; ++p) {
            int d;
            if (*p >= L'0' && *p <= L'9') d = *p - L'0';
            else if (*p >= L'a' && *p <= L'z') d = *p - L'a' + 10;
            else if (*p >= L'A' && *p <= L'Z') d = *p - L'A' + 10;
            else break;
            if (d >= base)
                break;
            if (acc > (~0ull - static_cast<unsigned>(d)) / static_cast<unsigned>(base))
                overflow = true;
            acc = acc * static_cast<unsigned>(base) + static_cast<unsigned>(d);
        }

        if (end)
            *end = const_cast<wchar_t*>(p == digits_at ? s : p);
        return acc;
    }

    template <typename S, typename U>
    S ClampSigned(const unsigned long long acc, const bool negative, const bool overflow)
    {
        constexpr U max = static_cast<U>(~static_cast<U>(0) >> 1);
        if (overflow || acc > static_cast<unsigned long long>(max) + (negative ? 1u : 0u))
            return negative ? -static_cast<S>(max) - 1 : static_cast<S>(max);
        return negative ? -static_cast<S>(acc) : static_cast<S>(acc);
    }
}

extern "C" {

long wcstol(const wchar_t* s, wchar_t** end, int base)
{
    bool negative, overflow;
    const unsigned long long acc = ParseWide(s, end, base, negative, overflow);
    return ClampSigned<long, unsigned long>(acc, negative, overflow);
}

long long wcstoll(const wchar_t* s, wchar_t** end, int base)
{
    bool negative, overflow;
    const unsigned long long acc = ParseWide(s, end, base, negative, overflow);
    return ClampSigned<long long, unsigned long long>(acc, negative, overflow);
}

unsigned long wcstoul(const wchar_t* s, wchar_t** end, int base)
{
    bool negative, overflow;
    const unsigned long long acc = ParseWide(s, end, base, negative, overflow);
    if (overflow || acc > ~0ul)
        return ~0ul;
    return negative ? static_cast<unsigned long>(0) - static_cast<unsigned long>(acc)
                    : static_cast<unsigned long>(acc);
}

unsigned long long wcstoull(const wchar_t* s, wchar_t** end, int base)
{
    bool negative, overflow;
    const unsigned long long acc = ParseWide(s, end, base, negative, overflow);
    if (overflow)
        return ~0ull;
    return negative ? 0ull - acc : acc;
}

} // extern "C"

// -- swprintf ---------------------------------------------------------------
//
// musl's writes 4-byte characters, so against -fshort-wchar output it produces
// an empty or garbled buffer -- silently, like the wcs* above. That cost a real
// debugging session in the wasm test mod before it was spotted.
//
// Conversions are the C standard's. GWCA's wide format strings spell string and
// character arguments %ls/%hs and %lc/%hc, which mean the same to MSVC and to C,
// so the two builds cannot disagree. Bare %s/%c are read as char*/char per the
// standard -- MSVC would read them as wide, so do not write them.
//
// Floating point is deliberately absent -- nothing in GWCA formats a float into
// a wide string, and a wrong dtoa is worse than a refusal. It returns -1.

namespace {
    struct WideSink {
        wchar_t* dst;
        size_t cap;      // total capacity including the NUL; 0 means measure only
        size_t len;      // characters the full result needs, written or not

        void Put(const wchar_t c)
        {
            if (dst && cap && len + 1 < cap)
                dst[len] = c;
            ++len;
        }
        void Pad(const wchar_t c, int n)
        {
            for (; n > 0; --n)
                Put(c);
        }
    };

    struct Spec {
        bool left, zero, plus, space, alt;
        int width, precision;   // precision -1 when absent
    };

    void EmitPadded(WideSink& out, const Spec& s, const wchar_t* body, int body_len,
                    const wchar_t* prefix, int prefix_len)
    {
        const int total = body_len + prefix_len;
        const int pad = s.width > total ? s.width - total : 0;
        if (!s.left && !s.zero)
            out.Pad(L' ', pad);
        for (int i = 0; i < prefix_len; i++)
            out.Put(prefix[i]);
        // Zero padding goes after the sign/0x, never before it.
        if (!s.left && s.zero)
            out.Pad(L'0', pad);
        for (int i = 0; i < body_len; i++)
            out.Put(body[i]);
        if (s.left)
            out.Pad(L' ', pad);
    }

    void EmitInteger(WideSink& out, Spec s, unsigned long long value, const int base,
                     const bool upper, const bool negative)
    {
        wchar_t digits[24];
        int n = 0;
        do {
            const unsigned d = static_cast<unsigned>(value % static_cast<unsigned>(base));
            digits[n++] = static_cast<wchar_t>(d < 10 ? L'0' + d : (upper ? L'A' : L'a') + (d - 10));
            value /= static_cast<unsigned>(base);
        } while (value);

        // An explicit precision is a minimum digit count, and it cancels '0'.
        if (s.precision >= 0) {
            s.zero = false;
            while (n < s.precision && n < 23)
                digits[n++] = L'0';
        }
        wchar_t body[24];
        for (int i = 0; i < n; i++)
            body[i] = digits[n - 1 - i];

        wchar_t prefix[3];
        int prefix_len = 0;
        if (negative)            prefix[prefix_len++] = L'-';
        else if (s.plus)         prefix[prefix_len++] = L'+';
        else if (s.space)        prefix[prefix_len++] = L' ';
        if (s.alt && base == 16) {
            prefix[prefix_len++] = L'0';
            prefix[prefix_len++] = upper ? L'X' : L'x';
        }
        EmitPadded(out, s, body, n, prefix, prefix_len);
    }

    int ReadInt(const wchar_t*& p)
    {
        int v = 0;
        while (*p >= L'0' && *p <= L'9')
            v = v * 10 + (*p++ - L'0');
        return v;
    }
}

extern "C" int vswprintf(wchar_t* dst, size_t n, const wchar_t* fmt, va_list ap)
{
    WideSink out{ dst, n, 0 };
    for (const wchar_t* p = fmt; *p; ++p) {
        if (*p != L'%') {
            out.Put(*p);
            continue;
        }
        ++p;
        if (*p == L'%') {
            out.Put(L'%');
            continue;
        }

        Spec s{ false, false, false, false, false, 0, -1 };
        for (;; ++p) {
            if (*p == L'-')      s.left = true;
            else if (*p == L'0') s.zero = true;
            else if (*p == L'+') s.plus = true;
            else if (*p == L' ') s.space = true;
            else if (*p == L'#') s.alt = true;
            else break;
        }
        if (*p == L'*') {
            s.width = va_arg(ap, int);
            if (s.width < 0) { s.left = true; s.width = -s.width; }
            ++p;
        }
        else {
            s.width = ReadInt(p);
        }
        if (*p == L'.') {
            ++p;
            s.precision = (*p == L'*') ? (++p, va_arg(ap, int)) : ReadInt(p);
            if (s.precision < 0)
                s.precision = -1;
        }

        // Width modifiers. For strings and characters only 'l' matters: it
        // selects wchar_t, and its absence selects char, as C specifies.
        bool wide = false;
        int longs = 0;
        for (;; ++p) {
            if (*p == L'l' || *p == L'w') { ++longs; wide = true; }
            else if (*p == L'h')          { }
            else if (*p == L'z' || *p == L'j' || *p == L't' || *p == L'L') { longs = 2; }
            else break;
        }

        switch (*p) {
        case L'd':
        case L'i': {
            const long long v = longs >= 2 ? va_arg(ap, long long) : va_arg(ap, int);
            EmitInteger(out, s, v < 0 ? 0ull - static_cast<unsigned long long>(v)
                                      : static_cast<unsigned long long>(v), 10, false, v < 0);
            break;
        }
        case L'u':
        case L'x':
        case L'X':
        case L'o': {
            const unsigned long long v = longs >= 2 ? va_arg(ap, unsigned long long)
                                                    : va_arg(ap, unsigned int);
            const int base = (*p == L'u') ? 10 : (*p == L'o') ? 8 : 16;
            EmitInteger(out, s, v, base, *p == L'X', false);
            break;
        }
        case L'p': {
            s.alt = true;
            s.zero = true;
            s.width = 8;
            EmitInteger(out, s, reinterpret_cast<uintptr_t>(va_arg(ap, void*)), 16, false, false);
            break;
        }
        case L'c':
        case L'C': {
            // %lc (and C's %C) is wchar_t; plain %c is char widened.
            const bool as_wide = wide || (*p == L'C');
            const wchar_t c = as_wide ? static_cast<wchar_t>(va_arg(ap, int))
                                      : static_cast<wchar_t>(static_cast<unsigned char>(va_arg(ap, int)));
            EmitPadded(out, s, &c, 1, nullptr, 0);
            break;
        }
        case L's':
        case L'S': {
            // %ls (and C's %S) is wchar_t*; plain %s is char*.
            const bool as_narrow = !(wide || (*p == L'S'));
            if (as_narrow) {
                const char* str = va_arg(ap, const char*);
                if (!str) str = "(null)";
                int len = 0;
                while (str[len] && (s.precision < 0 || len < s.precision))
                    ++len;
                const int pad = s.width > len ? s.width - len : 0;
                if (!s.left) out.Pad(L' ', pad);
                for (int i = 0; i < len; i++)
                    out.Put(static_cast<wchar_t>(static_cast<unsigned char>(str[i])));
                if (s.left) out.Pad(L' ', pad);
            }
            else {
                const wchar_t* str = va_arg(ap, const wchar_t*);
                if (!str) str = L"(null)";
                int len = 0;
                while (str[len] && (s.precision < 0 || len < s.precision))
                    ++len;
                EmitPadded(out, s, str, len, nullptr, 0);
            }
            break;
        }
        case L'f': case L'F': case L'e': case L'E': case L'g': case L'G': case L'a': case L'A':
            // Refused rather than approximated; see the note above.
            return -1;
        default:
            // Unknown conversion: emit it literally so the output shows the typo.
            out.Put(L'%');
            if (*p) out.Put(*p);
            break;
        }
        if (!*p)
            break;
    }

    // n == 0 measures: return what the result needs, write nothing. Everything
    // else follows C and MSVC -- NUL-terminate, and -1 if it did not all fit.
    if (!dst || !n)
        return static_cast<int>(out.len);
    if (out.len >= n) {
        dst[n - 1] = 0;
        return -1;
    }
    dst[out.len] = 0;
    return static_cast<int>(out.len);
}

extern "C" int swprintf(wchar_t* dst, size_t n, const wchar_t* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    const int r = vswprintf(dst, n, fmt, ap);
    va_end(ap);
    return r;
}

// Proves at runtime that the linker gave us OUR swprintf and not musl's.
//
// A static_assert cannot see this: both versions have the same signature, and
// picking the wrong one is a link-time outcome, not a compile-time one. The
// failure is silent -- musl writes 4-byte characters, so the buffer reads as an
// empty string rather than as anything obviously wrong -- which is exactly the
// kind of thing that only surfaces days later. So check the bytes.
extern "C" bool GWCA_WideCharSelfTest()
{
    wchar_t buf[32];
    // %ls and %06x together: the two this codebase most depends on, and the
    // pair that first exposed musl's version.
    if (swprintf(buf, 32, L"%ls-%06x", L"ab", 0x2ecu) != 9)
        return false;
    const wchar_t expect[] = { L'a', L'b', L'-', L'0', L'0', L'0', L'2', L'e', L'c', 0 };
    for (int i = 0; i < 10; i++) {
        if (buf[i] != expect[i])
            return false;
    }
    // Truncation must report -1, which ChatMgr's grow-and-retry depends on.
    if (swprintf(buf, 4, L"%ls", L"abcdef") != -1)
        return false;
    return true;
}
