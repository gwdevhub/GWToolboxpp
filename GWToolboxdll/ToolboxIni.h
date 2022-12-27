#pragma once

#include <SimpleIni.h>

class ToolboxIni : public CSimpleIni {
public:
    ToolboxIni(
        bool a_bIsUtf8 = false,
        bool a_bMultiKey = false,
        bool a_bMultiLine = false)
        : CSimpleIni(a_bIsUtf8, a_bMultiKey, a_bMultiLine)
    {
    }
    SI_Error LoadFile(const wchar_t* a_pwszFile);
    std::filesystem::path location_on_disk;
};
