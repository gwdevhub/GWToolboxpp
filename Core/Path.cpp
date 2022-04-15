#include "stdafx.h"

#include "Str.h"

namespace fs = std::filesystem;

void PathGetExeFullPath(wchar_t *path, size_t length)
{
    DWORD result = GetModuleFileNameW(NULL, path, length);
    if (result >= length)
        path[0] = 0;
}

void PathGetExeFileName(wchar_t *path, size_t length)
{
    PathGetExeFullPath(path, length);

    wchar_t *filename = PathFindFileNameW(path);
    if (filename != path)
        memmove(path, filename, StrBytesW(filename));
}

void PathRemoveFileName(wchar_t *path, size_t length, const wchar_t *src)
{
    assert(MAX_PATH <= length);

    if (path != src)
        StrCopyW(path, length, src);
    PathRemoveFileSpecW(path);
}

void PathRemovePath(wchar_t *path, size_t length, const wchar_t *src)
{
    assert(MAX_PATH <= length);

    if (path != src)
        StrCopyW(path, length, src);
    PathStripPathW(path);
}

void PathGetProgramDirectory(wchar_t *path, size_t length)
{
    PathGetExeFullPath(path, length);

    // Trim the filename from path to get the directory
    wchar_t *filename = PathFindFileNameW(path);
    if (filename != path)
        filename[0] = 0;
}

bool PathGetDocumentsDirectory(wchar_t *path, size_t length)
{
    assert(MAX_PATH <= length);

    HRESULT result = SHGetFolderPathW(nullptr, CSIDL_MYDOCUMENTS, NULL, 0, path);

    if (FAILED(result)) {
        fprintf(stderr, "SHGetFolderPathW failed (HRESULT:0x%lX)\n", result);
        return false;
    }

    return true;
}

bool PathGetDocumentsPath(wchar_t *path, size_t length, const wchar_t *suffix)
{
    if (!PathGetDocumentsDirectory(path, length)) {
        fprintf(stderr, "PathGetDocumentsDirectory failed\n");
        return false;
    }

    if (PathAppendW(path, suffix) != TRUE) {
        fprintf(stderr, "PathAppendW failed\n");
        return false;
    }

    return true;
}

bool PathGetAppDataDirectory(wchar_t* path, size_t length) {
    assert(MAX_PATH <= length);

    HRESULT result = SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, path);

    if (FAILED(result)) {
        fprintf(stderr, "SHGetFolderPathW failed (HRESULT:0x%lX)\n", result);
        return false;
    }

    return true;
}

bool PathGetAppDataPath(wchar_t* path, size_t length, const wchar_t* suffix) {
    if (!PathGetAppDataDirectory(path, length)) {
        fprintf(stderr, "PathGetAppDataDirectory failed\n");
        return false;
    }

    if (PathAppendW(path, suffix) != TRUE) {
        fprintf(stderr, "PathAppendW failed\n");
        return false;
    }

    return true;
}

bool PathCreateDirectory(const wchar_t *path)
{
    if (CreateDirectoryW(path, NULL)) {
        return true;
    }

    DWORD LastError = GetLastError();
    if (LastError == ERROR_ALREADY_EXISTS) {
        if (PathIsDirectoryW(path))
            return true;
        fprintf(stderr, "PathCreateDirectory failed (%lu)\n", LastError);
    }

    return false;
}

bool PathCompose(wchar_t *dest, size_t length, const wchar_t *left, const wchar_t *right)
{
    assert(MAX_PATH <= length);

    size_t left_size = StrBytesW(left);
    if (length < left_size) {
        fprintf(stderr, "Left string too long for the destination buffer\n");
        return false;
    }

    if (dest != left) {
        memmove(dest, left, left_size);
    }

    if (PathAppendW(dest, right) != TRUE) {
        fprintf(stderr, "PathAppendW failed\n");
        return false;
    }

    return true;
}

fs::path PathGetComputerName()
{
    constexpr auto INFO_BUFFER_SIZE = 32;
    WCHAR computername[INFO_BUFFER_SIZE];
    DWORD bufCharCount = INFO_BUFFER_SIZE;

    if (!GetComputerNameW(computername, &bufCharCount)) {
        fprintf(stderr, "Cannot get computer name.\n");
    }

    static fs::path computer = computername;

    return computer;
}

bool PathMoveDataAndCreateSymlink(bool create_symlink = true)
{
    wchar_t appdir[MAX_PATH];
    wchar_t docdir[MAX_PATH];

    if (!PathGetDocumentsPath(docdir, MAX_PATH, L"GWToolboxpp")) {
        fprintf(stderr, "Appdata directory not found.\n");
        return false;
    }

    if (!PathGetAppDataPath(appdir, MAX_PATH, L"GWToolboxpp")) {
        fprintf(stderr, "Documents directory not found.\n");
        return false;
    }

    fs::path docpath = docdir;
    fs::path apppath = appdir;

    if (!fs::exists(appdir) || fs::is_symlink(appdir)) {
        return true;
    }

    if (fs::is_symlink(appdir) && fs::is_directory(docpath) && fs::is_directory(docpath / PathGetComputerName())) {
        return true;
    }

    if (!fs::exists(docpath)) {
        fs::create_directory(docpath);
    }

    if (!fs::is_directory(docpath)) {
        fprintf(stderr, "%USERPROFILE%/GWToolboxxpp exists and is not a directory.\n");
        return false;
    }

    auto error = std::error_code{};

    if (fs::is_directory(apppath) && fs::is_directory(docpath)) {
        for (fs::path p : fs::directory_iterator(apppath)) {
            if (!fs::exists(docpath / p.filename())) {
                if (fs::is_directory(p)) {
                    fs::path dir = (docpath / PathGetComputerName() / p.filename()).parent_path();
                    if (!fs::exists(dir)) {
                        fs::create_directories(dir, error);
                    }
                    fs::rename(p, docpath / PathGetComputerName() / p.filename(), error);
                } else if (p.extension() != ".dll" && p.extension() != ".exe") {
                    fs::path dir = (docpath / PathGetComputerName() / p.filename()).parent_path();
                    if (fs::exists(dir)) fs::create_directories(dir);
                    fs::rename(p, docpath / PathGetComputerName() / p.filename(), error);
                } else {
                    fs::rename(p, docpath / p.filename(), error);
                }
            }
        }
    }

    if (fs::is_directory(apppath)) {
        fs::remove_all(apppath, error);
    }

    if (create_symlink) {
        fs::create_directory_symlink(docpath / PathGetComputerName(), apppath, error);

        if (error.value()) {
            fprintf(stderr, "Couldn't create symlink from appdir to docdir.\n");
            return false;
        }
    }

    return true;
}