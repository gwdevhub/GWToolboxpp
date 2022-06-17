#include "stdafx.h"

#include "Str.h"
#include "Path.h"

namespace fs = std::filesystem;

static std::error_code errcode; // Not thread safe

bool PathGetExeFullPath(std::filesystem::path& out)
{
    wchar_t path[MAX_PATH];
    DWORD result = GetModuleFileNameW(NULL, path, sizeof(path));
    if (result >= sizeof(path)) {
        path[0] = 0;
        fwprintf(stderr, L"%S: GetModuleFileNameW failed (%lu)\n", __func__, GetLastError());
        return false;
    }
    out.assign(path);
    return true;
        
}

bool PathGetExeFileName(std::wstring& out)
{
    std::filesystem::path temp;
    if (!PathGetExeFullPath(temp)) {
        return false;
    }
    out.assign(temp.filename());
    return true;
}

bool PathGetProgramDirectory(std::filesystem::path& out)
{
    if (!PathGetExeFullPath(out)) {
        return false;
    }
    std::filesystem::path parent = out.parent_path();
    out = parent;
    return true;
}

bool PathGetDocumentsPath(fs::path& out, const wchar_t* suffix = nullptr)
{
    wchar_t temp[MAX_PATH];
    HRESULT result = SHGetFolderPathW(nullptr, CSIDL_MYDOCUMENTS, NULL, 0, temp);

    if (FAILED(result)) {
        fwprintf(stderr, L"%S: SHGetFolderPathW failed (HRESULT:0x%lX)\n", __func__, result);
        return false;
    }
    if (!temp[0]) {
        fwprintf(stderr, L"%S: SHGetFolderPathW returned empty path\n", __func__);
        return false;
    }
    fs::path p = temp;
    if (suffix && suffix[0]) {
        p = p.append(suffix);
    }
    out.assign(p);
    return true;
}

bool PathGetAppDataPath(fs::path& out, const wchar_t* suffix = nullptr) {

    wchar_t temp[MAX_PATH];

    HRESULT result = SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, temp);

    if (FAILED(result)) {
        fwprintf(stderr, L"%S: SHGetFolderPathW failed (HRESULT:0x%lX)\n", __func__, result);
        return false;
    }
    if (!temp[0]) {
        fwprintf(stderr, L"%S: SHGetFolderPathW returned empty path\n", __func__);
        return false;
    }
    fs::path p = temp;
    if (suffix && suffix[0]) {
        p = p.append(suffix);
    }
    out.assign(p);
    return true;
}

bool PathCreateDirectorySafe(const fs::path& path)
{
    bool result;
    if (!PathIsDirectorySafe(path, &result)) {
        return false;
    }
    if (result) {
        // Already a valid directory
        return true;
    }
    result = fs::create_directories(path, errcode);
    if (errcode.value()) {
        fwprintf(stderr, L"%S: create_directories failed for %s (%lu, %S)\n", __func__, path.wstring().c_str(), errcode.value(), errcode.message().c_str());
        return false;
    }
    if (!result) {
        fwprintf(stderr, L"%S: Failed to create directory %s\n", __func__, path.wstring().c_str());
        return false;
    }
    return true;
}

bool PathGetComputerName(fs::path& out)
{
    wchar_t temp[MAX_COMPUTERNAME_LENGTH + 1];
    int written = sizeof(temp);
    if (!GetComputerNameW(temp, (DWORD*)&written)) {
        fwprintf(stderr, L"%S: Cannot get computer name. (%lu)\n", __func__, GetLastError());
        return false;
    }
    if (!temp[0]) {
        fwprintf(stderr, L"%S: GetComputerNameW returned no valid computer name.\n", __func__);
        return false;
    }
    out.assign(temp);
    return true;
}

bool PathRecursiveRemove(const fs::path& from) {
    std::error_code errcode;
    bool result;
    fs::directory_iterator it;
    if (!PathExistsSafe(from, &result)) {
        return false;
    }
    if (!result) {
        // Doesn't exist anyway
        return true;
    }
    if (!PathIsDirectorySafe(from, &result)) {
        return false;
    }
    if (result) {
        // directory
        if (!PathDirectoryIteratorSafe(from, &it)) {
            return false;
        }
        for (const fs::path& p : it) {
            if (!PathRecursiveRemove(from / p.filename()))
                return false;
        }
    }
    result = fs::remove(from, errcode);
    if (errcode.value()) {
        fwprintf(stderr, L"%S: %s remove failed (%lu, %S)\n", __func__, from.wstring().c_str(), errcode.value(), errcode.message().c_str());
        return false;
    }
    if (!PathExistsSafe(from, &result)) {
        return false;
    }
    if (result) {
        fwprintf(stderr, L"%S: %s exists returned true after remove\n", __func__, from.wstring().c_str());
        return false;
    }
    return true;
}

bool PathSafeCopy(const fs::path& from, const fs::path& to, bool copy_if_target_is_newer) {
    bool result;
    fs::directory_iterator it;
    if (!PathExistsSafe(from, &result)) {
        return false;
    }
    if (!result) {
        fwprintf(stderr, L"%S: from doesn't exist %s\n", __func__, from.wstring().c_str());
        return false;
    }
    if (!PathIsDirectorySafe(from, &result)) {
        return false;
    }
    if (result) {
        // directory
        if (!PathDirectoryIteratorSafe(from, &it)) {
            return false;
        }
        for (const fs::path& p : it) {
            if (!PathSafeCopy(p, to / p.filename(), copy_if_target_is_newer))
                return false;
        }
        return true;
    }
    
    const fs::path& dir = to.parent_path();
    if (!PathCreateDirectorySafe(dir)) {
        return false;
    }
    auto copyOptions = fs::copy_options::recursive;
    if (copy_if_target_is_newer)
        copyOptions |= fs::copy_options::update_existing;
    fs::copy(from, to, copyOptions, errcode);
    // 17 = Already exists; this is ok.
    switch(errcode.value()) {
        case 0:
        case 17: // Already exists
        case 80: // Already exists
            break;
        default:
            fwprintf(stderr, L"%S: copy %s failed (%lu, %S)\n", __func__, from.wstring().c_str(), errcode.value(), errcode.message().c_str());
            return false;
    }
    if (!PathExistsSafe(to, &result)) {
        return false;
    }
    if (!result) {
        fwprintf(stderr, L"%S: to file doesn't exist after copy %s (%lu, %S)\n", __func__, from.wstring().c_str(), errcode.value(), errcode.message().c_str());
        return false;
    }
    return true;
}
bool PathExistsSafe(const fs::path& path, bool* out) {
    std::error_code errcode;
    *out = fs::exists(path, errcode);
    if (errcode.value()) {
        fwprintf(stderr, L"%S: exists failed %s (%lu, %S).\n", __func__, path.wstring().c_str(), errcode.value(), errcode.message().c_str());
        return false;
    }
    return true;
}
bool PathDirectoryIteratorSafe(const fs::path& path, fs::directory_iterator* out) {
    *out = fs::directory_iterator(path, errcode);
    if (errcode.value()) {
        fwprintf(stderr, L"%S: directory_iterator failed %s (%lu, %S)\n", __func__, path.wstring().c_str(), errcode.value(), errcode.message().c_str());
        return false;
    }
    return true;
}
bool PathIsDirectorySafe(const fs::path& path, bool* out) {
    std::error_code errcode;
    *out = fs::is_directory(path, errcode);
    switch (errcode.value()) {
    case 0:
        break;
    case 3:
    case 2:
        *out = false;
        return true; // 2 = The system cannot find the file specified
    default:
        fwprintf(stderr, L"%S: is_directory failed %s (%lu, %S).\n", __func__, path.wstring().c_str(), errcode.value(), errcode.message().c_str());
        return false;
    }
    return true;
}
bool PathIsSymlinkSafe(const fs::path& path, bool* out) {
    
    *out = fs::is_symlink(path, errcode);
    if (errcode.value()) {
        switch (errcode.value()) {
        case 0:
            break;
        case 3:
        case 2:
            *out = false;
            return true; // 2 = The system cannot find the file specified
        default:
            fwprintf(stderr, L"%S: is_symlink failed %s (%lu, %S).\n", __func__, path.wstring().c_str(), errcode.value(), errcode.message().c_str());
            return false;
        }
    }
    return true;
}
bool PathMigrateDataAndCreateSymlink(bool create_symlink)
{
    std::error_code errcode;
    fs::directory_iterator it;
    fs::path apppath;
    bool result;
    if (!PathGetAppDataPath(apppath, L"GWToolboxpp")) {
        return false;
    }
    fs::path docpath;
    if (!PathGetDocumentsPath(docpath, L"GWToolboxpp")) {
        return false;
    }
    fs::path computer_name_path;
    if (!PathGetComputerName(computer_name_path)) {
        return false;
    }
    docpath = docpath / computer_name_path;

    if (!PathExistsSafe(apppath, &result)) {
        return false;
    }
    if (!result) {
        // No legacy AppData folder for GWToolbox, nothing to do.
        goto createsymlink;
    }
    if (!PathIsDirectorySafe(apppath, &result)) {
        return false;
    }
    if (!result) {
        // AppData is not a directory, nothing to copy.
        goto createsymlink;
    }
    if (!PathIsDirectorySafe(apppath, &result)) {
        return false;
    }
    if (!result) {
        // AppData is not a directory, nothing to copy.
        goto createsymlink;
    }
    if (!PathIsSymlinkSafe(apppath, &result)) {
        return false;
    }
    if (result) {
        // AppData is already a symlink, nothing to copy.
        goto createsymlink;
    }


    // Copy over all files from AppData/Local to new path
    if (!PathDirectoryIteratorSafe(apppath, &it)) {
        return false;
    }
    fwprintf(stdout, L"Copying content from %s to %s...", apppath.wstring().c_str(), docpath.wstring().c_str());
    // @Cleanup: This bit can take a long time to do the manual copy. Think about a dialog window without writing a sea of code for it.
    for (const fs::path& p : it) {
        if (!PathSafeCopy(apppath / p.filename(), docpath / p.filename(), false))
            return false;
    }
    // At this point using PathSafeCopy we're confident that the files have been copied.
    if (!PathRecursiveRemove(apppath)) {
        return false;
    }
    createsymlink:
    if (create_symlink) {
        if (!PathIsSymlinkSafe(apppath, &result)) {
            return false;
        }
        if (!result) {
            fs::create_directory_symlink(docpath, apppath, errcode);

            if (errcode.value()) {
                fwprintf(stderr, L"%S: Couldn't create symlink from apppath to docpath (%lu, %S).\n", __func__, errcode.value(), errcode.message().c_str());
                return false;
            }
        }
    }

    return true;
}