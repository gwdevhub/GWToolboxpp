#pragma once

#include "Export.h"

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
    // class PatternScanner
    // 32 bit pattern scanner for x86 programs.
    // Credits to Zat & Midi12 @ unknowncheats.me for the functionality of this class.
    namespace Scanner {
        // Initializer to determine scan range.
        GWCA_API void Initialize(const char* moduleName = NULL);
        GWCA_API void Initialize(HMODULE hModule);

        // Find reference in GW memory to a specific assertion message
        GWCA_API uintptr_t FindAssertion(const char* assertion_file, const char* assertion_msg, uint32_t line_number, int offset);

        // Pattern find between a start and end address. If end is less than start, will scan backward.
        GWCA_API uintptr_t FindInRange(const char* pattern, const char* mask, int offset, DWORD start, DWORD end);

        // Actual pattern finder.
        GWCA_API uintptr_t Find(const char* pattern, const char* mask = 0, int offset = 0, ScannerSection section = ScannerSection::Section_TEXT);

        GWCA_API void GetSectionAddressRange(ScannerSection section, uintptr_t* start = nullptr, uintptr_t* end = nullptr);

        // Check if current address is a valid pointer (usually to a data variable in DATA)
        GWCA_API bool IsValidPtr(uintptr_t address, ScannerSection section = ScannerSection::Section_DATA);

        // Returns actual address of a function call given via CALL <near call> instruction e.g. *call_instruction_address = 0xE8 ?? ?? ?? 0xFF
        GWCA_API uintptr_t FunctionFromNearCall(uintptr_t call_instruction_address, bool check_valid_ptr = true);

        GWCA_API uintptr_t ToFunctionStart(uintptr_t call_instruction_address, uint32_t scan_range = 0xff);
    }
}
