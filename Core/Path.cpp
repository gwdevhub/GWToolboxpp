#include "stdafx.h"

#include "Path.h"

namespace fs = std::filesystem;

static std::error_code errcode; // Not thread safe

bool PathGetExeFullPath(std::filesystem::path& out)
{
    wchar_t path[MAX_PATH];
    const auto result = GetModuleFileNameW(NULL, path, MAX_PATH);
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
    out = out.parent_path();
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
    int written = MAX_COMPUTERNAME_LENGTH + 1;
    if (!GetComputerNameW(temp, reinterpret_cast<DWORD*>(&written))) {
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
    std::error_code error_code;
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
        for (const fs::path p : it) {
            if (!PathRecursiveRemove(from / p.filename()))
                return false;
        }
    }
    result = fs::remove(from, error_code);
    if (error_code.value()) {
        fwprintf(stderr, L"%S: %s remove failed (%lu, %S)\n", __func__, from.wstring().c_str(), error_code.value(), error_code.message().c_str());
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
        return std::all_of(begin(it), end(it), [=](const fs::path& p) {
            return PathSafeCopy(p, to / p.filename(), copy_if_target_is_newer);
        });
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
