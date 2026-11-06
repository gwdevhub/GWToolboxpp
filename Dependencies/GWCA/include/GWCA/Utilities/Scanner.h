#pragma once
#include <GWCA/Runtime.h>

#include "Export.h"

#include <cstdint>

namespace GW {
    enum ScannerSection : uint8_t {
        Section_TEXT = 0,
        Section_RDATA = 1,
        Section_DATA = 2,
        Section_Count = 3
    };
    struct ScannerSectionOffset {
        uintptr_t start = 0;
        uintptr_t end = 0;
    };
    // 32-bit pattern scanner for x86 programs. Credits to Zat & Midi12 @ unknowncheats.me.
    namespace Scanner {
        // Initializer to determine scan range.
        void InitializeByName(const char* moduleName = NULL);
        void InitializeForModule(HMODULE hModule);
        inline void Initialize(const char* name = nullptr) { InitializeByName(name); }
        inline void Initialize(HMODULE module) { InitializeForModule(module); }

        DWORD GetGameTlsIndex();

        // Find reference in GW memory to a specific assertion message
        uintptr_t FindAssertion(const char* assertion_file, const char* assertion_msg, uint32_t line_number, int offset);

        // Pattern find between a start and end address. If end is less than start, will scan backward.
        uintptr_t FindInRange(const char* pattern, const char* mask, int offset, DWORD start, DWORD end);

        uintptr_t FindUseOfStringA(const char* str, int offset = 0, ScannerSection section = ScannerSection::Section_TEXT);

        uintptr_t FindNthUseOfStringA(const char* str, size_t nth, int offset = 0, ScannerSection section = ScannerSection::Section_TEXT);

        uintptr_t FindUseOfStringW(const wchar_t* str, int offset = 0, ScannerSection section = ScannerSection::Section_TEXT);

        uintptr_t FindNthUseOfStringW(const wchar_t* str, size_t nth, int offset = 0, ScannerSection section = ScannerSection::Section_TEXT);
        inline uintptr_t FindUseOfString(const char* str, int offset = 0, ScannerSection section = Section_TEXT) { return FindUseOfStringA(str, offset, section); }
        inline uintptr_t FindUseOfString(const wchar_t* str, int offset = 0, ScannerSection section = Section_TEXT) { return FindUseOfStringW(str, offset, section); }
        inline uintptr_t FindNthUseOfString(const char* str, size_t nth, int offset = 0, ScannerSection section = Section_TEXT) { return FindNthUseOfStringA(str, nth, offset, section); }
        inline uintptr_t FindNthUseOfString(const wchar_t* str, size_t nth, int offset = 0, ScannerSection section = Section_TEXT) { return FindNthUseOfStringW(str, nth, offset, section); }

        // Actual pattern finder.
        uintptr_t Find(const char* pattern, const char* mask = 0, int offset = 0, ScannerSection section = ScannerSection::Section_TEXT);

        // The nth match of a pattern, 0-based: FindNth(p, m, 0, off) == Find(p, m, off).
        // Use when a pattern is deliberately ambiguous and it is a later hit you want.
        uintptr_t FindNth(const char* pattern, const char* mask, size_t nth, int offset = 0, ScannerSection section = ScannerSection::Section_TEXT);

        void GetSectionAddressRange(ScannerSection section, uintptr_t* start = nullptr, uintptr_t* end = nullptr);

        // Check if current address is a valid pointer (usually to a data variable in DATA)
        bool IsValidPtr(uintptr_t address, ScannerSection section = ScannerSection::Section_DATA);

        // Returns actual address of a function call given via CALL <near call> instruction e.g. *call_instruction_address = 0xE8 ?? ?? ?? 0xFF
        uintptr_t FunctionFromNearCall(uintptr_t call_instruction_address, bool check_valid_ptr = true);

        uintptr_t ToFunctionStart(uintptr_t call_instruction_address, uint32_t scan_range = 0xff);

#if GWCA_WASM
        // Every scan returns an address, as on x86 -- here a tagged CODE OFFSET
        // (0x80000000 | offset) rather than a linear one. FunctionAtCodeOffset is the one
        // way back to a bare function index; ToFunctionStart and the arity checks take
        // either form. See Source/Platform/Wasm/Scanner.cpp.

        // Initialise from the module bytes -- a running module cannot read its own code section.
        bool InitializeFromWasm(const void* wasm_bytes, size_t length);
        inline bool Initialize(const void* bytes, size_t length) { return InitializeFromWasm(bytes, length); }
        bool IsInitialized();

        // Scans with no wasm equivalent record what they were asked for rather than failing silently.
        uint32_t GetUnportedScans();
        uint32_t GetUnportedScan(uint32_t index, char* buffer, uint32_t capacity);

        // Replaces *(uintptr_t*)address: LLVM puts the global in the load's memarg offset, not in a constant.
        uintptr_t GlobalFromFunction(uintptr_t func_index, size_t nth = 0);
        uint32_t GlobalsInFunction(uintptr_t func_index, uint32_t* buffer = nullptr, uint32_t capacity = 0);

        // The global read straight out of a load/store you already located. 0 if that offset is not a load/store.
        uintptr_t GlobalAtCodeOffset(uintptr_t code_offset);

        // The i32.const immediate at an offset you already located, decoded from LEB128 --
        // what a deref does on x86, which cannot work here: the code section is not in
        // linear memory, and the immediate is a varint rather than a raw word.
        uintptr_t ConstAtCodeOffset(uintptr_t code_offset);

        // A static passed as an i32.const rather than loaded from; restricted to .bss to exclude .data string constants.
        uintptr_t BssConstFromFunction(uintptr_t func_index, size_t nth = 0);

        // The one function referencing `str` with this exact type; 0 if none or ambiguous.
        // For an overload pair a shared string cannot separate and an ordinal must not: nth
        // orders by .text address on x86 but by function index here, and they disagree.
        uintptr_t FindUseOfStringWithSignature(const char* str, uint32_t params,
                                                        uint32_t results);

        // Replaces the Find+FunctionFromNearCall idiom: find where a constant is passed, take the callee.
        uintptr_t FindCallAfterConst(int32_t value, int window = 6);
        uintptr_t FindCallAfterConstWithArity(int32_t value, uint32_t nparams,
                                                       int window = 6);

        // Does the resolved index take `expected` wasm params? True when it matches or cannot be determined; drive it via the macro.
        bool CheckArity(uintptr_t func_index, uint32_t expected, const char* what);

        // Same, plus the RESULT count -- the only guard catching the return-type mismatch that makes call_indirect trap.
        bool CheckSignature(uintptr_t func_index, uint32_t expected_params,
                                     uint32_t expected_results, const char* what);

        // Resolve a code offset or bare index to a function index (0 if it does not resolve); GW::Hook uses it to find what it hooks.
        uintptr_t FunctionAtCodeOffset(uintptr_t address);

        // The function's start as a CODE OFFSET, which is what ToFunctionStart
        // used to give. Only for scanning on from it -- FindInRange bounds and
        // address arithmetic. ToFunctionStart now returns something callable,
        // and an offset is not callable any more than a callable is scannable.
        uintptr_t FunctionStartAddress(uintptr_t address, uint32_t scan_range = 0);

        // Records that `callable` is `func_index`, so a scan result that has been
        // made callable still resolves for CheckArity, GlobalFromFunction and
        // CreateHook. GW::Hook fills this in as it adopts.
        void NoteCallable(uintptr_t callable, uintptr_t func_index);
#endif
    }
}

// Discard a scan result whose resolved function takes the wrong wasm parameter count. No-op on x86.
#if GWCA_WASM
// Discards with `= 0` not `= nullptr`, so it takes a raw uintptr_t scan result as well as a typed function pointer.
#define GWCA_SCAN_CHECK_ARITY(func, expected) \
    do { if (!GW::Scanner::CheckArity((uintptr_t)(func), (uint32_t)(expected), #func)) (func) = 0; } while (0)
// Prefer over GWCA_SCAN_CHECK_ARITY when the result count is known: 0 for void, 1 otherwise.
#define GWCA_SCAN_CHECK_SIG(func, params, results) \
    do { if (!GW::Scanner::CheckSignature((uintptr_t)(func), (uint32_t)(params), \
                                          (uint32_t)(results), #func)) (func) = 0; } while (0)
#else
#define GWCA_SCAN_CHECK_ARITY(func, expected) ((void)0)
#define GWCA_SCAN_CHECK_SIG(func, params, results) ((void)0)
#endif

namespace GW::Scanner {
    extern "C" GWCA_API void __cdecl GW_Scanner_InitializeByName(const char* moduleName) noexcept;
    extern "C" GWCA_API void __cdecl GW_Scanner_InitializeForModule(HMODULE hModule) noexcept;
    extern "C" GWCA_API DWORD __cdecl GW_Scanner_GetGameTlsIndex() noexcept;
    extern "C" GWCA_API uintptr_t __cdecl GW_Scanner_FindAssertion(const char* assertion_file, const char* assertion_msg, uint32_t line_number, int offset) noexcept;
    extern "C" GWCA_API uintptr_t __cdecl GW_Scanner_FindInRange(const char* pattern, const char* mask, int offset, DWORD start, DWORD end) noexcept;
    extern "C" GWCA_API uintptr_t __cdecl GW_Scanner_FindUseOfStringA(const char* str, int offset, ScannerSection section) noexcept;
    extern "C" GWCA_API uintptr_t __cdecl GW_Scanner_FindNthUseOfStringA(const char* str, size_t nth, int offset, ScannerSection section) noexcept;
    extern "C" GWCA_API uintptr_t __cdecl GW_Scanner_FindUseOfStringW(const wchar_t* str, int offset, ScannerSection section) noexcept;
    extern "C" GWCA_API uintptr_t __cdecl GW_Scanner_FindNthUseOfStringW(const wchar_t* str, size_t nth, int offset, ScannerSection section) noexcept;
    extern "C" GWCA_API uintptr_t __cdecl GW_Scanner_Find(const char* pattern, const char* mask, int offset, ScannerSection section) noexcept;
    extern "C" GWCA_API uintptr_t __cdecl GW_Scanner_FindNth(const char* pattern, const char* mask, size_t nth, int offset, ScannerSection section) noexcept;
    extern "C" GWCA_API void __cdecl GW_Scanner_GetSectionAddressRange(ScannerSection section, uintptr_t* start, uintptr_t* end) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Scanner_IsValidPtr(uintptr_t address, ScannerSection section) noexcept;
    extern "C" GWCA_API uintptr_t __cdecl GW_Scanner_FunctionFromNearCall(uintptr_t call_instruction_address, bool check_valid_ptr) noexcept;
    extern "C" GWCA_API uintptr_t __cdecl GW_Scanner_ToFunctionStart(uintptr_t call_instruction_address, uint32_t scan_range) noexcept;
#if (GWCA_WASM)
    extern "C" GWCA_API bool __cdecl GW_Scanner_InitializeFromWasm(const void* wasm_bytes, size_t length) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Scanner_IsInitialized() noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_Scanner_GetUnportedScans() noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_Scanner_GetUnportedScan(uint32_t index, char* buffer, uint32_t capacity) noexcept;
    extern "C" GWCA_API uintptr_t __cdecl GW_Scanner_GlobalFromFunction(uintptr_t func_index, size_t nth) noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_Scanner_GlobalsInFunction(uintptr_t func_index, uint32_t* buffer, uint32_t capacity) noexcept;
    extern "C" GWCA_API uintptr_t __cdecl GW_Scanner_GlobalAtCodeOffset(uintptr_t code_offset) noexcept;
    extern "C" GWCA_API uintptr_t __cdecl GW_Scanner_ConstAtCodeOffset(uintptr_t code_offset) noexcept;
    extern "C" GWCA_API uintptr_t __cdecl GW_Scanner_BssConstFromFunction(uintptr_t func_index, size_t nth) noexcept;
    extern "C" GWCA_API uintptr_t __cdecl GW_Scanner_FindUseOfStringWithSignature(const char* str, uint32_t params, uint32_t results) noexcept;
    extern "C" GWCA_API uintptr_t __cdecl GW_Scanner_FindCallAfterConst(int32_t value, int window) noexcept;
    extern "C" GWCA_API uintptr_t __cdecl GW_Scanner_FindCallAfterConstWithArity(int32_t value, uint32_t nparams, int window) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Scanner_CheckArity(uintptr_t func_index, uint32_t expected, const char* what) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Scanner_CheckSignature(uintptr_t func_index, uint32_t expected_params, uint32_t expected_results, const char* what) noexcept;
    extern "C" GWCA_API uintptr_t __cdecl GW_Scanner_FunctionAtCodeOffset(uintptr_t address) noexcept;
    extern "C" GWCA_API uintptr_t __cdecl GW_Scanner_FunctionStartAddress(uintptr_t address, uint32_t scan_range) noexcept;
    extern "C" GWCA_API void __cdecl GW_Scanner_NoteCallable(uintptr_t callable, uintptr_t func_index) noexcept;
#endif
}

#if !defined(GWCA_BUILD_EXPORTS)
namespace GW::Scanner {
    inline void InitializeByName(const char* moduleName)
    {
        return GWCA_CALL_C(GW_Scanner_InitializeByName, moduleName);
    }

    inline void InitializeForModule(HMODULE hModule)
    {
        return GWCA_CALL_C(GW_Scanner_InitializeForModule, hModule);
    }

    inline DWORD GetGameTlsIndex()
    {
        return GWCA_CALL_C(GW_Scanner_GetGameTlsIndex);
    }

    inline uintptr_t FindAssertion(const char* assertion_file, const char* assertion_msg, uint32_t line_number, int offset)
    {
        return GWCA_CALL_C(GW_Scanner_FindAssertion, assertion_file, assertion_msg, line_number, offset);
    }

    inline uintptr_t FindInRange(const char* pattern, const char* mask, int offset, DWORD start, DWORD end)
    {
        return GWCA_CALL_C(GW_Scanner_FindInRange, pattern, mask, offset, start, end);
    }

    inline uintptr_t FindUseOfStringA(const char* str, int offset, ScannerSection section)
    {
        return GWCA_CALL_C(GW_Scanner_FindUseOfStringA, str, offset, section);
    }

    inline uintptr_t FindNthUseOfStringA(const char* str, size_t nth, int offset, ScannerSection section)
    {
        return GWCA_CALL_C(GW_Scanner_FindNthUseOfStringA, str, nth, offset, section);
    }

    inline uintptr_t FindUseOfStringW(const wchar_t* str, int offset, ScannerSection section)
    {
        return GWCA_CALL_C(GW_Scanner_FindUseOfStringW, str, offset, section);
    }

    inline uintptr_t FindNthUseOfStringW(const wchar_t* str, size_t nth, int offset, ScannerSection section)
    {
        return GWCA_CALL_C(GW_Scanner_FindNthUseOfStringW, str, nth, offset, section);
    }

    inline uintptr_t Find(const char* pattern, const char* mask, int offset, ScannerSection section)
    {
        return GWCA_CALL_C(GW_Scanner_Find, pattern, mask, offset, section);
    }

    inline uintptr_t FindNth(const char* pattern, const char* mask, size_t nth, int offset, ScannerSection section)
    {
        return GWCA_CALL_C(GW_Scanner_FindNth, pattern, mask, nth, offset, section);
    }

    inline void GetSectionAddressRange(ScannerSection section, uintptr_t* start, uintptr_t* end)
    {
        return GWCA_CALL_C(GW_Scanner_GetSectionAddressRange, section, start, end);
    }

    inline bool IsValidPtr(uintptr_t address, ScannerSection section)
    {
        return GWCA_CALL_C(GW_Scanner_IsValidPtr, address, section);
    }

    inline uintptr_t FunctionFromNearCall(uintptr_t call_instruction_address, bool check_valid_ptr)
    {
        return GWCA_CALL_C(GW_Scanner_FunctionFromNearCall, call_instruction_address, check_valid_ptr);
    }

    inline uintptr_t ToFunctionStart(uintptr_t call_instruction_address, uint32_t scan_range)
    {
        return GWCA_CALL_C(GW_Scanner_ToFunctionStart, call_instruction_address, scan_range);
    }
#if (GWCA_WASM)
    inline bool InitializeFromWasm(const void* wasm_bytes, size_t length)
    {
        return GWCA_CALL_C(GW_Scanner_InitializeFromWasm, wasm_bytes, length);
    }

    inline bool IsInitialized()
    {
        return GWCA_CALL_C(GW_Scanner_IsInitialized);
    }

    inline uint32_t GetUnportedScans()
    {
        return GWCA_CALL_C(GW_Scanner_GetUnportedScans);
    }

    inline uint32_t GetUnportedScan(uint32_t index, char* buffer, uint32_t capacity)
    {
        return GWCA_CALL_C(GW_Scanner_GetUnportedScan, index, buffer, capacity);
    }

    inline uintptr_t GlobalFromFunction(uintptr_t func_index, size_t nth)
    {
        return GWCA_CALL_C(GW_Scanner_GlobalFromFunction, func_index, nth);
    }

    inline uint32_t GlobalsInFunction(uintptr_t func_index, uint32_t* buffer, uint32_t capacity)
    {
        return GWCA_CALL_C(GW_Scanner_GlobalsInFunction, func_index, buffer, capacity);
    }

    inline uintptr_t GlobalAtCodeOffset(uintptr_t code_offset)
    {
        return GWCA_CALL_C(GW_Scanner_GlobalAtCodeOffset, code_offset);
    }

    inline uintptr_t ConstAtCodeOffset(uintptr_t code_offset)
    {
        return GWCA_CALL_C(GW_Scanner_ConstAtCodeOffset, code_offset);
    }

    inline uintptr_t BssConstFromFunction(uintptr_t func_index, size_t nth)
    {
        return GWCA_CALL_C(GW_Scanner_BssConstFromFunction, func_index, nth);
    }

    inline uintptr_t FindUseOfStringWithSignature(const char* str, uint32_t params, uint32_t results)
    {
        return GWCA_CALL_C(GW_Scanner_FindUseOfStringWithSignature, str, params, results);
    }

    inline uintptr_t FindCallAfterConst(int32_t value, int window)
    {
        return GWCA_CALL_C(GW_Scanner_FindCallAfterConst, value, window);
    }

    inline uintptr_t FindCallAfterConstWithArity(int32_t value, uint32_t nparams, int window)
    {
        return GWCA_CALL_C(GW_Scanner_FindCallAfterConstWithArity, value, nparams, window);
    }

    inline bool CheckArity(uintptr_t func_index, uint32_t expected, const char* what)
    {
        return GWCA_CALL_C(GW_Scanner_CheckArity, func_index, expected, what);
    }

    inline bool CheckSignature(uintptr_t func_index, uint32_t expected_params, uint32_t expected_results, const char* what)
    {
        return GWCA_CALL_C(GW_Scanner_CheckSignature, func_index, expected_params, expected_results, what);
    }

    inline uintptr_t FunctionAtCodeOffset(uintptr_t address)
    {
        return GWCA_CALL_C(GW_Scanner_FunctionAtCodeOffset, address);
    }

    inline uintptr_t FunctionStartAddress(uintptr_t address, uint32_t scan_range)
    {
        return GWCA_CALL_C(GW_Scanner_FunctionStartAddress, address, scan_range);
    }

    inline void NoteCallable(uintptr_t callable, uintptr_t func_index)
    {
        return GWCA_CALL_C(GW_Scanner_NoteCallable, callable, func_index);
    }
#endif
}
#endif
