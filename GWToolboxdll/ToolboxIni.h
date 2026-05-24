#pragma once

#include <Utils/FastIni.h>

class ToolboxIni : public FastIni {
public:
    ToolboxIni(const bool a_bIsUtf8 = false, const bool a_bMultiKey = false, const bool a_bMultiLine = false) : FastIni(a_bIsUtf8, a_bMultiKey, a_bMultiLine) {}
    

    // Returns SI_OK if file doesn't exist or was read successfully.
    SI_Error LoadIfExists(const std::filesystem::path& a_pwszFile);
    // Returns SI_OK if file exists and was read successfully
    SI_Error LoadFile(const std::filesystem::path& a_pwszFile);
    SI_Error LoadFile(const wchar_t* a_pwszFile);
    std::filesystem::path location_on_disk;
};
