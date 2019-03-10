#pragma once

#include <stdint.h>

enum FileAttribute : uint32_t {
    FileAttribute_Directory = (1 << 1),
    FileAttribute_Hidden    = (1 << 2),
};

struct FileInfo {
    uint32_t   attributes;
    uint64_t   size;
    wchar_t    path[512];

    bool GetIsDirectory() { return (attributes & FileAttribute_Directory) != 0; }
    bool GetIsHidden()    { return (attributes & FileAttribute_Hidden) != 0; }
};

class FileIterator {
public:
    FileIterator(const wchar_t *path);
    ~FileIterator();

    bool GetNext(FileInfo *info);

private:
    wchar_t dirpath[512];
    HANDLE handle;

    static bool IsDotFile(const wchar_t *path);
    void InitFileInfo(FileInfo *info, struct _WIN32_FIND_DATAW *find_data);
};