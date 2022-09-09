#include "stdafx.h"

#include <Path.h>
#include "Download.h"
#include "Install.h"
#include "EmbeddedResource.h"

namespace fs = std::filesystem;

static bool GetFileSize(const wchar_t *path, uint64_t *file_size)
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

static bool EnsureInstallationDirectoryExist(void)
{
    wchar_t temp[MAX_PATH];

    fs::path docpath;
    if (!PathGetDocumentsPath(docpath, L"GWToolboxpp")) {
        return false;
    }
    std::filesystem::path computer_name;
    if (!PathGetComputerName(computer_name)) {
        return false;
    }
    docpath = docpath / computer_name; // %USERPROFILE%\Documents\GWToolboxpp\<Computername>

    // Create %USERPROFILE%\Documents\GWToolboxpp\<Computername>\crashes
    const fs::path crashes = docpath / L"crashes";
    if (!PathCreateDirectorySafe(crashes)) {
        return false;
    }
    // Create %USERPROFILE%\Documents\GWToolboxpp\<Computername>\logs
    const fs::path logs = docpath / L"logs";
    if (!PathCreateDirectorySafe(logs)) {
        return false;
    }
    // Create %USERPROFILE%\Documents\GWToolboxpp\<Computername>\plugins
    const fs::path plugins = docpath / L"plugins";
    if (!PathCreateDirectorySafe(plugins)) {
        return false;
    }
    // Create %USERPROFILE%\Documents\GWToolboxpp\<Computername>\data
    const fs::path data = docpath / L"data";
    if (!PathCreateDirectorySafe(data)) {
        return false;
    }
    return true;
}

static bool CopyInstaller(void)
{
    std::filesystem::path dest_path;

    if (!PathGetDocumentsPath(dest_path, L"GWToolboxpp\\GWToolbox.exe")) {
        return false;
    }
    std::filesystem::path source_path;
    if (!PathGetExeFullPath(source_path)) {
        return false;
    }

    if (source_path == dest_path)
        return true;

    if (!PathSafeCopy(source_path, dest_path, true)) {
        return false;
    }

    return true;
}

static bool DeleteInstallationDirectory(void)
{
    // @Remark:
    // "SHFileOperationW" expect the path to be double-null terminated.
    //
    // Moreover, the path should be a full path otherwise, the folder won't be
    // moved to the recycle bin regardless of "FOF_ALLOWUNDO".

    wchar_t path[MAX_PATH + 2];
    std::filesystem::path fspath;
    if (!PathGetDocumentsPath(fspath, L"GWToolboxpp\\*")) {
        return false;
    }
    wcscpy(path, fspath.wstring().c_str());

    const size_t n_path = wcslen(path);
    path[n_path + 1] = 0;

    SHFILEOPSTRUCTW FileOp = {0};
    FileOp.wFunc = FO_DELETE;
    FileOp.pFrom = path;
    FileOp.fFlags = FOF_NO_UI | FOF_ALLOWUNDO;
    FileOp.fAnyOperationsAborted = FALSE;
    FileOp.lpszProgressTitle = L"GWToolbox uninstallation";

    const int iRet = SHFileOperationW(&FileOp);
    if (iRet != 0) {
        fprintf(stderr, "SHFileOperationW failed: 0x%X, GetLastError 0x%X\n", iRet, GetLastError());
        return false;
    }
    if (FileOp.fAnyOperationsAborted) {
        fprintf(stderr, "SHFileOperationW failed: fAnyOperationsAborted\n");
        return false;
    }

    return true;
}

bool DumpFont()
{
    fs::path docpath;
    if (!PathGetDocumentsPath(docpath, L"GWToolboxpp")) {
        return false;
    }
    fs::path computername;
    if (!PathGetComputerName(computername)) {
        return false;
    }
    const fs::path target = docpath / computername / "Font.ttf";
    if (fs::exists(target)) {
        return true;
    }

    const auto font = EmbeddedResource(IDR_BINARY2, L"Binary");
    if (!font.GetResourceData()) {
        return false;
    }

    std::ofstream f(target.c_str(), std::ios::out | std::ios::binary);
    f.write(static_cast<char*>(font.GetResourceData()), font.GetResourceSize());
    f.close();

    return true;
}

bool DumpD3dx9()
{
    fs::path docpath;
    if (!PathGetDocumentsPath(docpath, L"GWToolboxpp")) {
        return false;
    }
    const fs::path target = docpath / "d3dx9_43.dll";
    if (fs::exists(target)) {
        if (fs::file_size(target) == 2001304u) {
            return true;
        }
        fs::remove(target);
    }

    const auto dll = EmbeddedResource(IDR_BINARY1, L"Binary");
    if (!dll.GetResourceData() || dll.GetResourceSize() != 2001304u) {
        return false;
    }

    std::ofstream f(target.c_str(), std::ios::out | std::ios::binary);
    f.write(static_cast<char*>(dll.GetResourceData()), dll.GetResourceSize());
    f.close();

    return true;
}

bool Install(bool quiet)
{
    if (IsInstalled())
        return true;

    if (!EnsureInstallationDirectoryExist()) {
        fprintf(stderr, "EnsureInstallationDirectoryExist failed\n");
        return false;
    }

    if (!DumpFont()) {
        fprintf(stderr, "Couldn't unpack Font.ttf\n");
        return false;
    }

    if (!DumpD3dx9()) {
        fprintf(stderr, "Couldn't unpack d3dx9_43.dll\n");
        return false;
    }

    if (!CopyInstaller()) {
        fprintf(stderr, "CopyInstaller failed\n");
        return false;
    }

    if (!DownloadWindow::DownloadAllFiles()) {
        fprintf(stderr, "DownloadWindow::DownloadAllFiles failed\n");
        return false;
    }

    if (!quiet) {
        MessageBoxW(0, L"Installation successful", L"Installation", 0);
    }

    return true;
}

bool Uninstall(bool quiet)
{
    bool DeleteAllFiles = true;
    if (quiet == false) {
        const int iRet = MessageBoxW(
            nullptr,
            L"Do you want to delete *all* possible files from installation folder? (Default: no)\n",
            L"Uninstallation",
            MB_YESNO);

        if (iRet != IDYES)
            DeleteAllFiles = false;
    }

    if (DeleteAllFiles) {
        // Delete all files
        DeleteInstallationDirectory();
    }

    if (quiet == false) {
        MessageBoxW(nullptr, L"Uninstallation successful", L"Uninstallation", 0);
    }

    return true;
}

bool IsInstalled()
{
    fs::path dllpath;
    fs::path computername;
    if (!PathGetDocumentsPath(dllpath, L"GWToolboxpp")) {
        return false;
    }
    if (!PathGetComputerName(computername)) {
        return false;
    }
    const fs::path computerpath = dllpath / computername;

    if (!fs::exists(dllpath / L"GWToolboxdll.dll")) return false;
    if (!fs::exists(dllpath / L"GWToolbox.exe")) return false;
    if (!fs::exists(dllpath / L"d3dx9_43.dll")) return false;
    if (!fs::exists(computerpath / L"Font.ttf")) return false;

    return true;
}
