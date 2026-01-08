#include "stdafx.h"

#include <Path.h>
#include "Download.h"
#include "Install.h"
#include "WindowsDefender.h"

namespace fs = std::filesystem;

bool GetFileSize(const wchar_t* path, uint64_t* file_size)
{
    const HANDLE hFile = CreateFileW(
        path,
        FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);

    if (hFile == INVALID_HANDLE_VALUE) {
        // @Enhancement:
        // Deal with access denied?
        fprintf(stderr, "CreateFileW failed (%lu)\n", GetLastError());
        return false;
    }

    LARGE_INTEGER FileSize;
    if (!GetFileSizeEx(hFile, &FileSize)) {
        fprintf(stderr, "GetFileSizeEx failed (%lu)\n", GetLastError());
        return false;
    }

    CloseHandle(hFile);

    *file_size = static_cast<uint64_t>(FileSize.QuadPart);
    return true;
}

fs::path GetInstallationDir()
{
    std::filesystem::path docpath;
    if (!PathGetDocumentsPath(docpath, L"GWToolboxpp")) {
        return {};
    }
    std::filesystem::path computer_name;
    if (!PathGetComputerName(computer_name)) {
        return {};
    }
    return docpath / computer_name;
}

fs::path EnsureInstallationDirectoryExist()
{
    const auto installation_dir = GetInstallationDir();
    if (!installation_dir.empty() && PathCreateDirectorySafe(installation_dir))
        return installation_dir;
    return {};
}

bool CopyInstaller()
{
    std::filesystem::path dest_path = GetInstallationDir();
    if (dest_path.empty())
        return false;
    dest_path = dest_path.parent_path() / L"GWToolbox.exe";

    std::filesystem::path source_path;
    if (!PathGetExeFullPath(source_path)) {
        return false;
    }

    if (source_path == dest_path) {
        return true;
    }

    if (!PathSafeCopy(source_path, dest_path, true)) {
        return false;
    }

    return true;
}

bool DeleteInstallationDirectory(std::wstring& error)
{
    // @Remark:
    // "SHFileOperationW" expect the path to be double-null terminated.
    //
    // Moreover, the path should be a full path otherwise, the folder won't be
    // moved to the recycle bin regardless of "FOF_ALLOWUNDO".

    const auto install_dir = GetInstallationDir();
    if (install_dir.empty())
        return error = L"Failed to GetInstallationDir() in DeleteInstallationDirectory()", false;
    const auto fspath = install_dir / "*";

    const auto str = fspath.wstring();
    SHFILEOPSTRUCTW FileOp = {nullptr};
    FileOp.wFunc = FO_DELETE;
    FileOp.pFrom = fspath.c_str();
    FileOp.fFlags = FOF_NO_UI | FOF_ALLOWUNDO;
    FileOp.fAnyOperationsAborted = FALSE;
    FileOp.lpszProgressTitle = L"GWToolbox uninstallation";

    const int iRet = SHFileOperationW(&FileOp);
    if (iRet != 0)
        return error = std::format(L"SHFileOperationW failed: {:#X}, GetLastError {:#X}", iRet, GetLastError()), false;
    if (FileOp.fAnyOperationsAborted)
        return error = std::format(L"SHFileOperationW failed: fAnyOperationsAborted"), false;
    return true;
}

bool Install(const bool quiet, std::wstring& error)
{
    if (IsInstalled())
        return true;
    fs::path install_path = EnsureInstallationDirectoryExist();
    if (install_path.empty())
        return error = L"EnsureInstallationDirectoryExist failed", false;

    if (!CopyInstaller())
        return error = L"CopyInstaller failed", false;

    if (!DownloadWindow::DownloadAllFiles(error))
        return false;
    const auto dll_path = install_path.parent_path() / "GWToolboxdll.dll";
    if (!exists(dll_path)) {
        return error = std::format(L"Application @ {} not found after installation; it may have been quarantined by anti virus software!\n\nExclude the {} directory in your anti virus settings and re-launch.", dll_path.wstring(),
                                   dll_path.parent_path().wstring()), false;
    }
    AddDefenderExclusion(install_path.parent_path(), quiet, error); // Silent fail, it'll show messages etc
    if (!IsInstalled()) {
        return error = std::format(L"IsInstalled() returned false after installation; it may have been quarantined by anti virus software!\n\nExclude the {} directory in your anti virus settings and re-launch.", dll_path.wstring(),
                                   dll_path.parent_path().wstring()), false;
    }
    if (!quiet) {
        MessageBoxW(nullptr, L"Installation successful", L"Installation", 0);
    }

    return true;
}

bool Uninstall(const bool quiet, std::wstring& error)
{
    bool DeleteAllFiles = true;
    if (quiet == false) {
        const int iRet = MessageBoxW(
            nullptr,
            L"Do you want to delete *all* possible files from installation folder? (Default: no)\n",
            L"Uninstallation",
            MB_YESNO);

        if (iRet != IDYES) {
            DeleteAllFiles = false;
        }
    }

    if (DeleteAllFiles) {
        // Delete all files
        DeleteInstallationDirectory(error);
    }

    if (quiet == false) {
        MessageBoxW(nullptr, L"Uninstallation successful", L"Uninstallation", 0);
    }

    return true;
}

bool IsInstalled()
{
    const auto install_path = GetInstallationDir();
    if (install_path.empty())
        return false;

    const auto dllpath = install_path.parent_path();
    return exists(dllpath / L"GWToolboxdll.dll")
           && exists(dllpath / L"GWToolbox.exe");
}
