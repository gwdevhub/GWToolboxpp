#pragma once

#include <Windows.h>
#include <stdint.h>
#include <GWCA\Utilities\Scanner.h>

class FileScanner
{
public:
    static bool CreateFromPath(const wchar_t *path, FileScanner *result);

    FileScanner();
    FileScanner(const void *FileMapping, uintptr_t ImageBase, PIMAGE_SECTION_HEADER Sections, size_t Count);
    FileScanner& operator=(FileScanner&& other) noexcept;
    FileScanner& operator=(const FileScanner& other) noexcept;
    ~FileScanner();

    void GetSectionAddressRange(GW::ScannerSection section, uintptr_t* start, uintptr_t* end) const;
    uintptr_t FindAssertion(const char* assertion_file, const char* assertion_msg, uint32_t line_number, int offset);
    uintptr_t FindInRange(const char* pattern, const char* mask, int offset, uint32_t start, uint32_t end);
    uintptr_t Find(const char* pattern, const char* mask, int offset, GW::ScannerSection section = GW::ScannerSection::Section_TEXT);

    GW::ScannerSectionOffset sections[GW::ScannerSection::Section_Count] = {};
private:
    const void *FileMapping = nullptr;
    uintptr_t ImageBase = 0;
};
