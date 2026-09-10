#include "stdafx.h"

#include "File.h"

bool WriteEntireFile(const wchar_t* FilePath, const void* Content, size_t Length)
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
    auto Buffer = reinterpret_cast<const uint8_t*>(Content);

    // No early quit on an empty Content: writing nothing still clears/creates the file,
    // which isn't the same as not writing at all.
    while (Length != 0) {
        const size_t BytesToWrite = std::min<size_t>(Length, static_cast<DWORD>(-1));
        const DWORD dwBytesToWrite = BytesToWrite;

        const BOOL Success = WriteFile(
            hFile,
            Buffer,
            dwBytesToWrite,
            &BytesWritten,
            nullptr);

        if (!Success || BytesWritten != dwBytesToWrite) {
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
