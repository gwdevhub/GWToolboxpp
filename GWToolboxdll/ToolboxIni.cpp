#include <stdafx.h>

#include <ToolboxIni.h>

SI_Error ToolboxIni::LoadFile(const wchar_t* a_pwszFile) {
    const std::filesystem::path pFile = a_pwszFile;
    return LoadFile(pFile);
}
SI_Error ToolboxIni::LoadIfExists(const std::filesystem::path& a_pwszFile) {
    if (!std::filesystem::exists(a_pwszFile)) {
        Log::LogW(L"[ToolboxIni] %s doesn't exist", a_pwszFile.wstring().c_str());
        return SI_OK;
    }
    return LoadFile(a_pwszFile);
}
SI_Error ToolboxIni::LoadFile(const std::filesystem::path& a_pwszFile) {
    
    int res = -1;
    // 3 tries to load from disk
    for (int i = 0; i < 3 && res != SI_OK; i++) {
        res = CSimpleIni::LoadFile(a_pwszFile.wstring().c_str());
    }
    if (res == SI_OK) {
        Log::LogW(L"[ToolboxIni] LoadFile successful for %s", a_pwszFile.wstring().c_str());
        // Store location on disk on successful load
        location_on_disk = a_pwszFile;
    } else {
        Log::LogW(L"[ToolboxIni] LoadFile failed for %s", a_pwszFile.wstring().c_str());
    }
    return res;
}
