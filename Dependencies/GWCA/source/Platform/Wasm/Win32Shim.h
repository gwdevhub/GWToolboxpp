#pragma once

// Minimal stand-ins for the Win32 types that leak into GWCA's public headers -- not an emulation layer.

#include <cstdint>
#include <cstddef>
#include <cstdlib>

using DWORD = uint32_t;
using WORD = uint16_t;
using BYTE = uint8_t;
using BOOL = int;
using LONG = int32_t;
using ULONG = uint32_t;
using UINT = unsigned int;
using LPVOID = void*;
using LPCVOID = const void*;
using HANDLE = void*;
using HMODULE = void*;
using HWND = void*;
using HINSTANCE = void*;
using LPARAM = intptr_t;
using WPARAM = uintptr_t;
using LRESULT = intptr_t;
using LPCSTR = const char*;
using LPCWSTR = const wchar_t*;
using LPSTR = char*;
using LPWSTR = wchar_t*;

#define WINAPI
#define CALLBACK
#define APIENTRY

// wasm32 has one calling convention. clang parses these and then warns
// -Wignored-attributes on every typedef carrying one, which is most of the
// scanned-function typedefs in the public headers -- define them away instead.
#define __fastcall
#define __stdcall
#define __thiscall

#ifndef TRUE
# define TRUE 1
#endif
#ifndef FALSE
# define FALSE 0
#endif
#ifndef MAX_PATH
# define MAX_PATH 260
#endif

// FILETIME passes through chat timestamps and packet handlers, so these mirror Win32's layout exactly.
struct FILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
};

struct SYSTEMTIME {
    WORD wYear, wMonth, wDayOfWeek, wDay;
    WORD wHour, wMinute, wSecond, wMilliseconds;
};

// MSVC extension used throughout the managers.
#ifndef _countof
# define _countof(a) (sizeof(a) / sizeof((a)[0]))
#endif

// SAL annotations: purely MSVC static-analysis markers, no semantics.
#define _In_
#define _In_opt_
#define _Out_
#define _Inout_

// Genuine no-ops, not stubs: the module is single-threaded, so there is no contention to guard.
struct CRITICAL_SECTION { int unused; };
inline void InitializeCriticalSection(CRITICAL_SECTION*) {}
inline void DeleteCriticalSection(CRITICAL_SECTION*) {}
inline void EnterCriticalSection(CRITICAL_SECTION*) {}
inline void LeaveCriticalSection(CRITICAL_SECTION*) {}

// Cooperative Sleep via JSPI -- a spin would deadlock the single-threaded module; the host supplies env.emscripten_sleep.
extern "C" {
    __attribute__((import_module("env"), import_name("emscripten_sleep")))
    void __gwca_wasm_sleep(unsigned int ms);
}
inline void Sleep(DWORD ms) { __gwca_wasm_sleep(static_cast<unsigned int>(ms)); }

// Real conversions, not stubs: FILETIME is 100ns ticks since 1601, and GWCA reads chat timestamps through these.
inline BOOL FileTimeToSystemTime(const FILETIME* ft, SYSTEMTIME* st)
{
    if (!ft || !st) return FALSE;
    uint64_t ticks = ((uint64_t)ft->dwHighDateTime << 32) | ft->dwLowDateTime;
    uint64_t secs = ticks / 10000000ULL;
    st->wMilliseconds = (WORD)((ticks / 10000ULL) % 1000);
    uint64_t days = secs / 86400ULL;
    uint32_t rem = (uint32_t)(secs % 86400ULL);
    st->wHour = (WORD)(rem / 3600);
    st->wMinute = (WORD)((rem % 3600) / 60);
    st->wSecond = (WORD)(rem % 60);
    st->wDayOfWeek = (WORD)((days + 1) % 7);          // 1601-01-01 was a Monday
    int y = 1601;
    for (;;) {
        bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
        uint32_t len = leap ? 366 : 365;
        if (days < len) break;
        days -= len; ++y;
    }
    bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    static const uint32_t md[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int mo = 0;
    for (; mo < 12; ++mo) {
        uint32_t len = md[mo] + ((mo == 1 && leap) ? 1u : 0u);
        if (days < len) break;
        days -= len;
    }
    st->wYear = (WORD)y;
    st->wMonth = (WORD)(mo + 1);
    st->wDay = (WORD)(days + 1);
    return TRUE;
}

// No timezone database in the module, so local == UTC; only used to display timestamps.
inline BOOL FileTimeToLocalFileTime(const FILETIME* in, FILETIME* out)
{
    if (!in || !out) return FALSE;
    *out = *in;
    return TRUE;
}

// Window message constants -- the host feeds equivalent events through the WebView, so the codes stay meaningful.
#define WM_MOUSEMOVE   0x0200
#define WM_LBUTTONDOWN 0x0201
#define WM_LBUTTONUP   0x0202
#define WM_RBUTTONDOWN 0x0204
#define WM_RBUTTONUP   0x0205
#define WM_KEYDOWN     0x0100
#define WM_KEYUP       0x0101
#define WM_CHAR        0x0102
#define SW_SHOWNORMAL  1

// One WebView, always focused -- there is no other window for focus to be anywhere else.
inline HWND GetFocus() { return nullptr; }

// Linear memory is always writable, but that is no licence to patch CODE: it is immutable and needs the build-time detour pass.
#define PAGE_READWRITE          0x04
#define PAGE_EXECUTE_READWRITE  0x40

inline BOOL VirtualProtect(LPVOID, size_t, DWORD, DWORD* old)
{
    if (old) *old = PAGE_READWRITE;
    return TRUE;
}

// Chat command tokenising: whitespace separates and quotes group; Win32's backslash-escape quirks are not reproduced.
inline LPWSTR* CommandLineToArgvW(LPCWSTR cmdline, int* argc)
{
    if (argc) *argc = 0;
    if (!cmdline) return nullptr;
    size_t len = 0;
    while (cmdline[len]) ++len;

    size_t max_args = len / 2 + 2;
    size_t bytes = max_args * sizeof(wchar_t*) + (len + 1) * sizeof(wchar_t);
    void* block = malloc(bytes);
    if (!block) return nullptr;

    LPWSTR* out = (LPWSTR*)block;
    wchar_t* buf = (wchar_t*)((char*)block + max_args * sizeof(wchar_t*));
    int n = 0;
    size_t i = 0, w = 0;
    while (i < len) {
        while (i < len && (cmdline[i] == L' ' || cmdline[i] == L'\t')) ++i;
        if (i >= len) break;
        out[n++] = &buf[w];
        bool quoted = false;
        while (i < len && (quoted || (cmdline[i] != L' ' && cmdline[i] != L'\t'))) {
            if (cmdline[i] == L'"') { quoted = !quoted; ++i; continue; }
            buf[w++] = cmdline[i++];
        }
        buf[w++] = 0;
    }
    if (argc) *argc = n;
    return out;
}

inline void LocalFree(void* p) { free(p); }

// _ReturnAddress() is used by GWCA's diagnostics; clang has the builtin.
#ifndef _ReturnAddress
# define _ReturnAddress() __builtin_return_address(0)
#endif
