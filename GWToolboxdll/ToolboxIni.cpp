#include <stdafx.h>

#include <ToolboxIni.h>

SI_Error ToolboxIni::LoadFile(const wchar_t* a_pwszFile) {
    // Store location on disk on successful load
    auto res = CSimpleIni::LoadFile(a_pwszFile);
    if (res == SI_OK)
        location_on_disk = a_pwszFile;
    return res;
}
