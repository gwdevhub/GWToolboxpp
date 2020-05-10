#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN
#endif
#ifdef NOMINMAX
# define NOMINMAX
#endif
#include <Windows.h>
#include <Shlobj.h>
#include <shlwapi.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "Str.h"

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

bool PathGetAppDataDirectory(wchar_t *path, size_t length)
{
    assert(MAX_PATH <= length);

    HRESULT result = SHGetFolderPathW(
        nullptr,
        CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE,
        NULL,
        0,
        path);

    if (FAILED(result)) {
        fprintf(stderr, "SHGetFolderPathW failed (HRESULT:0x%X)\n", result);
        return false;
    }

    return true;
}

bool PathGetAppDataPath(wchar_t *path, size_t length, const wchar_t *suffix)
{
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
