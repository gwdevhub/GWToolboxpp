#include "stdafx.h"

#include <Path.h>

#include "Download.h"
#include "Install.h"
#include "Registry.h"
#include "Settings.h"

namespace fs = std::filesystem;

static bool GetFileSize(const wchar_t *path, uint64_t *file_size)
{
    HANDLE hFile = CreateFileW(
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

#define DWORD_MAX ((DWORD)-1)
static bool GetFileSizeAsDword(const wchar_t *path, DWORD *file_size)
{
    uint64_t FileSize;
    if (!GetFileSize(path, &FileSize)) {
        return false;
    }

    if (FileSize >= DWORD_MAX) {
        fprintf(stderr, "File '%ls' size (%llu) is too big for a DWORD (%lu)\n",
            path, FileSize, DWORD_MAX);
        return false;
    }

    *file_size = static_cast<DWORD>(FileSize);
    return true;
}

static bool InstallUninstallKey()
{
    wchar_t time_buf[32];
    struct tm local_time;
    time_t raw_time = time(nullptr);
    localtime_s(&local_time , &raw_time);
    wcsftime(time_buf, ARRAYSIZE(time_buf), L"%Y%m%d", &local_time);

    fs::path install_location;
    if (!PathGetDocumentsPath(install_location, L"GWToolboxpp")) {
        return false;
    }
    fs::path dll_path = install_location / L"GWToolboxdll.dll";
    fs::path installer_path = install_location / L"GWToolbox.exe";

    DWORD dll_size;
    if (!GetFileSizeAsDword(dll_path.wstring().c_str(), &dll_size)) {
        return false;
    }

    wchar_t uninstall[MAX_PATH + 64];
    wchar_t uninstall_quiet[MAX_PATH + 64];
    _snwprintf_s(uninstall, ARRAYSIZE(uninstall), L"%s /uninstall", installer_path.wstring().c_str());
    _snwprintf_s(uninstall_quiet, ARRAYSIZE(uninstall_quiet), L"%s /uninstall /quiet", installer_path.wstring().c_str());

    HKEY UninstallKey;
    if (!CreateUninstallKey(&UninstallKey)) {
        fprintf(stderr, "OpenUninstallKey failed\n");
        return false;
    }

    // current date as YYYYMMDD
    if (!RegWriteStr(UninstallKey, L"DisplayName", L"GWToolbox") ||
        !RegWriteStr(UninstallKey, L"DisplayIcon", installer_path.wstring().c_str()) ||
        !RegWriteStr(UninstallKey, L"DisplayVersion", L"3.0") ||
        !RegWriteDWORD(UninstallKey, L"EstimatedSize", dll_size / 1000) ||
        !RegWriteStr(UninstallKey, L"InstallDate", time_buf) ||
        !RegWriteStr(UninstallKey, L"InstallLocation", install_location.wstring().c_str()) ||
        !RegWriteDWORD(UninstallKey, L"NoModify", 1) ||
        !RegWriteDWORD(UninstallKey, L"NoRepair", 1) ||
        !RegWriteStr(UninstallKey, L"UninstallString", uninstall) ||
        !RegWriteStr(UninstallKey, L"QuietUninstallString", uninstall_quiet) ||
        !RegWriteStr(UninstallKey, L"URLInfoAbout", L"http://www.gwtoolbox.com/") ||
        !RegWriteStr(UninstallKey, L"URLUpdateInfo", L"http://www.gwtoolbox.com/history"))
    {
        RegCloseKey(UninstallKey);
        return false;
    }

    RegCloseKey(UninstallKey);
    return true;
}

static bool InstallSettingsKey()
{
    HKEY SettingsKey;
    if (!OpenSettingsKey(&SettingsKey)) {
        fprintf(stderr, "OpenUninstallKey failed\n");
        return false;
    }

    if (!RegWriteDWORD(SettingsKey, L"noupdate", 0) ||
        !RegWriteDWORD(SettingsKey, L"asadmin", 0))
        // Add the key to get crash dump
    {
        RegCloseKey(SettingsKey);
        return false;
    }

    RegCloseKey(SettingsKey);
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
    fs::path crashes = docpath / L"crashes";
    if (!PathCreateDirectorySafe(crashes)) {
        return false;
    }
    // Create %USERPROFILE%\Documents\GWToolboxpp\<Computername>\logs
    fs::path logs = docpath / L"logs";
    if (!PathCreateDirectorySafe(logs)) {
        return false;
    }
    // Create %USERPROFILE%\Documents\GWToolboxpp\<Computername>\plugins
    fs::path plugins = docpath / L"plugins";
    if (!PathCreateDirectorySafe(plugins)) {
        return false;
    }
    // Create %USERPROFILE%\Documents\GWToolboxpp\<Computername>\data
    fs::path data = docpath / L"data";
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

    size_t n_path = wcslen(path);
    path[n_path + 1] = 0;

    SHFILEOPSTRUCTW FileOp = {0};
    FileOp.wFunc = FO_DELETE;
    FileOp.pFrom = path;
    FileOp.fFlags = FOF_NO_UI | FOF_ALLOWUNDO;
    FileOp.fAnyOperationsAborted = FALSE;
    FileOp.lpszProgressTitle = L"GWToolbox uninstallation";

    int iRet = SHFileOperationW(&FileOp);
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

bool Install(bool quiet)
{
    if (IsInstalled())
        return true;

    if (!PathMigrateDataAndCreateSymlink(false)) {
        fwprintf(stderr, L"PathMigrateDataAndCreateSymlink failed\n");
        return false;
    }

    if (!EnsureInstallationDirectoryExist()) {
        fprintf(stderr, "EnsureInstallationDirectoryExist failed\n");
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

    if (!InstallUninstallKey()) {
        fprintf(stderr, "InstallUninstallKey failed\n");
        return false;
    }

    if (!InstallSettingsKey()) {
        fprintf(stderr, "InstallSettingKeys failed\n");
        return false;
    }

    if (!quiet) {
        MessageBoxW(0, L"Installation successful", L"Installation", 0);
    }

    return true;
}

bool Uninstall(bool quiet)
{
    if (!DeleteSettingsKey()) {
        fprintf(stderr, "DeleteSettingsKey failed\n");
        return false;
    }

    if (!DeleteUninstallKey()) {
        fprintf(stderr, "DeleteUninstallKey failed\n");
        return false;
    }

    bool DeleteAllFiles = true;
    if (quiet == false) {
        int iRet = MessageBoxW(
            0,
            L"Do you want to delete *all* possible files from installation folder? (Default: yes)\n",
            L"Uninstallation",
            MB_YESNO);

        if (iRet == IDNO)
            DeleteAllFiles = false;
    }

    if (DeleteAllFiles) {
        // Delete all files
        DeleteInstallationDirectory();
    }

    if (quiet == false) {
        MessageBoxW(0, L"Uninstallation successful", L"Uninstallation", 0);
    }

    return true;
}

bool IsInstalled()
{
    HKEY UninstallKey;
    if (!OpenUninstallKey(&UninstallKey)) {
        return false;
    }

    RegCloseKey(UninstallKey);
    return true;
}

bool GetInstallationLocation(fs::path& out)
{
    HKEY UninstallKey;
    if (!OpenUninstallKey(&UninstallKey)) {
        return false;
    }
    wchar_t temp[MAX_PATH];
    if (!RegReadStr(UninstallKey, L"InstallLocation", temp, sizeof(temp))) {
        RegCloseKey(UninstallKey);
        return false;
    }

    RegCloseKey(UninstallKey);
    out.assign(temp);
    return true;
}
