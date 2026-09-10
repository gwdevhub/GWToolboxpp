#pragma once

#include "Export.h"

#include <cstdint>
#include <string>
#include <vector>

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
        GWCA_API void Initialize(const char* moduleName = NULL);
        GWCA_API void Initialize(HMODULE hModule);

        GWCA_API DWORD GetGameTlsIndex();

        // Find reference in GW memory to a specific assertion message
        GWCA_API uintptr_t FindAssertion(const char* assertion_file, const char* assertion_msg, uint32_t line_number, int offset);

        // Pattern find between a start and end address. If end is less than start, will scan backward.
        GWCA_API uintptr_t FindInRange(const char* pattern, const char* mask, int offset, DWORD start, DWORD end);

        GWCA_API uintptr_t FindUseOfString(const char* str, int offset = 0, ScannerSection section = ScannerSection::Section_TEXT);

        GWCA_API uintptr_t FindNthUseOfString(const char* str, size_t nth, int offset = 0, ScannerSection section = ScannerSection::Section_TEXT);

        GWCA_API uintptr_t FindUseOfString(const wchar_t* str, int offset = 0, ScannerSection section = ScannerSection::Section_TEXT);

        GWCA_API uintptr_t FindNthUseOfString(const wchar_t* str, size_t nth, int offset = 0, ScannerSection section = ScannerSection::Section_TEXT);

        // Actual pattern finder.
        GWCA_API uintptr_t Find(const char* pattern, const char* mask = 0, int offset = 0, ScannerSection section = ScannerSection::Section_TEXT);

        // The nth match of a pattern, 0-based: FindNth(p, m, 0, off) == Find(p, m, off).
        // Use when a pattern is deliberately ambiguous and it is a later hit you want.
        GWCA_API uintptr_t FindNth(const char* pattern, const char* mask, size_t nth, int offset = 0, ScannerSection section = ScannerSection::Section_TEXT);

        GWCA_API void GetSectionAddressRange(ScannerSection section, uintptr_t* start = nullptr, uintptr_t* end = nullptr);

        // Check if current address is a valid pointer (usually to a data variable in DATA)
        GWCA_API bool IsValidPtr(uintptr_t address, ScannerSection section = ScannerSection::Section_DATA);

        // Returns actual address of a function call given via CALL <near call> instruction e.g. *call_instruction_address = 0xE8 ?? ?? ?? 0xFF
        GWCA_API uintptr_t FunctionFromNearCall(uintptr_t call_instruction_address, bool check_valid_ptr = true);

        GWCA_API uintptr_t ToFunctionStart(uintptr_t call_instruction_address, uint32_t scan_range = 0xff);

#if GWCA_WASM
        // Every scan returns an address, as on x86 -- here a tagged CODE OFFSET
        // (0x80000000 | offset) rather than a linear one. FunctionAtCodeOffset is the one
        // way back to a bare function index; ToFunctionStart and the arity checks take
        // either form. See Source/Platform/Wasm/Scanner.cpp.

        // Initialise from the module bytes -- a running module cannot read its own code section.
        GWCA_API bool Initialize(const void* wasm_bytes, size_t length);
        GWCA_API bool IsInitialized();

        // Scans with no wasm equivalent record what they were asked for rather than failing silently.
        GWCA_API const std::vector<std::string>& GetUnportedScans();

        // Replaces *(uintptr_t*)address: LLVM puts the global in the load's memarg offset, not in a constant.
        GWCA_API uintptr_t GlobalFromFunction(uintptr_t func_index, size_t nth = 0);
        GWCA_API std::vector<uint32_t> GlobalsInFunction(uintptr_t func_index);

        // The global read straight out of a load/store you already located. 0 if that offset is not a load/store.
        GWCA_API uintptr_t GlobalAtCodeOffset(uintptr_t code_offset);

        // The i32.const immediate at an offset you already located, decoded from LEB128 --
        // what a deref does on x86, which cannot work here: the code section is not in
        // linear memory, and the immediate is a varint rather than a raw word.
        GWCA_API uintptr_t ConstAtCodeOffset(uintptr_t code_offset);

        // A static passed as an i32.const rather than loaded from; restricted to .bss to exclude .data string constants.
        GWCA_API uintptr_t BssConstFromFunction(uintptr_t func_index, size_t nth = 0);

        // The one function referencing `str` with this exact type; 0 if none or ambiguous.
        // For an overload pair a shared string cannot separate and an ordinal must not: nth
        // orders by .text address on x86 but by function index here, and they disagree.
        GWCA_API uintptr_t FindUseOfStringWithSignature(const char* str, uint32_t params,
                                                        uint32_t results);

        // Replaces the Find+FunctionFromNearCall idiom: find where a constant is passed, take the callee.
        GWCA_API uintptr_t FindCallAfterConst(int32_t value, int window = 6);
        GWCA_API uintptr_t FindCallAfterConstWithArity(int32_t value, uint32_t nparams,
                                                       int window = 6);

        // Does the resolved index take `expected` wasm params? True when it matches or cannot be determined; drive it via the macro.
        GWCA_API bool CheckArity(uintptr_t func_index, uint32_t expected, const char* what);

        // Same, plus the RESULT count -- the only guard catching the return-type mismatch that makes call_indirect trap.
        GWCA_API bool CheckSignature(uintptr_t func_index, uint32_t expected_params,
                                     uint32_t expected_results, const char* what);

        // Resolve a code offset or bare index to a function index (0 if it does not resolve); GW::Hook uses it to find what it hooks.
        GWCA_API uintptr_t FunctionAtCodeOffset(uintptr_t address);

        // The function's start as a CODE OFFSET, which is what ToFunctionStart
        // used to give. Only for scanning on from it -- FindInRange bounds and
        // address arithmetic. ToFunctionStart now returns something callable,
        // and an offset is not callable any more than a callable is scannable.
        GWCA_API uintptr_t FunctionStartAddress(uintptr_t address, uint32_t scan_range = 0);

        // Records that `callable` is `func_index`, so a scan result that has been
        // made callable still resolves for CheckArity, GlobalFromFunction and
        // CreateHook. GW::Hook fills this in as it adopts.
        GWCA_API void NoteCallable(uintptr_t callable, uintptr_t func_index);
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
