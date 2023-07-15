#include "stdafx.h"

#include "File.h"

bool WriteEntireFile(const wchar_t *FilePath, const void *Content, size_t Length)
{
    HANDLE hFile = CreateFileW(
        FilePath,
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        0,
        nullptr);

    if (hFile == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Failed open file '%ls' (%lu)\n", FilePath, GetLastError());
        return false;
    }

    DWORD BytesWritten;
    const uint8_t *Buffer = reinterpret_cast<const uint8_t *>(Content);

    //
    // We don't early quit, because there is a difference between writing a empty
    // file and not writing the file at all. The file could already exist, in which
    // case it would be "cleared" and if it doesn't exist, it would be created.
    //
    while (Length != 0)
    {
        size_t BytesToWrite = std::min<size_t>(Length, (DWORD)-1);
        DWORD dwBytesToWrite = static_cast<DWORD>(BytesToWrite);

        BOOL Success = WriteFile(
            hFile,
            Buffer,
            dwBytesToWrite,
            &BytesWritten,
            nullptr);

        if (!Success || (BytesWritten != dwBytesToWrite)) {
            fprintf(stderr, "Failed to write %lu bytes to file '%ls' (%lu)\n",
                dwBytesToWrite, FilePath, GetLastError());
            CloseHandle(hFile);
            return false;
        }

        Length -= dwBytesToWrite;
        Buffer += dwBytesToWrite;
    }

    CloseHandle(hFile);
    return true;
}
