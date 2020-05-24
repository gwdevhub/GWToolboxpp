#include <string.h>
#include <wchar.h>

#include <TbWindows.h>

#include "FileIterator.h"

FileIterator::FileIterator(const wchar_t *path)
    : handle(NULL)
{
    wcsncpy(dirpath, path, _countof(dirpath));
}

FileIterator::~FileIterator()
{
    if (handle != NULL && handle != INVALID_HANDLE_VALUE)
        FindClose(handle);
}

bool FileIterator::IsDotFile(const wchar_t *path)
{
    if (wcscmp(path, L".") && wcscmp(path, L".."))
        return false;
    else
        return true;
}

void FileIterator::InitFileInfo(FileInfo *info, struct _WIN32_FIND_DATAW *find_data)
{
    info->attributes = 0;
    if (find_data->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        info->attributes |= FileAttribute_Directory;
    if (find_data->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
        info->attributes |= FileAttribute_Hidden;

    info->size = ((uint64_t)find_data->nFileSizeHigh << 32) | find_data->nFileSizeLow;
    swprintf(info->path, _countof(info->path), L"%s\\%s", dirpath, find_data->cFileName);
}

bool FileIterator::GetNext(FileInfo *info)
{
    WIN32_FIND_DATAW find_data;

    if (handle == NULL) {
        wchar_t pattern[512];
        swprintf(pattern, 512, L"%s\\*", dirpath);
        handle = FindFirstFileW(pattern, &find_data);
        if (handle == INVALID_HANDLE_VALUE) {
            handle = NULL;
            return false;
        }

        if (!IsDotFile(find_data.cFileName)) {
            InitFileInfo(info, &find_data);
            return true;
        }
    }

    for (;;) {
        if (!FindNextFileW(handle, &find_data))
            return false;
        if (!IsDotFile(find_data.cFileName))
            break;
    }

    InitFileInfo(info, &find_data);
    return true;
}
