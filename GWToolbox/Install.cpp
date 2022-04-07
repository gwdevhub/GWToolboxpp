#include "stdafx.h"
#include <filesystem>

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

    wchar_t install_location[MAX_PATH];
    if (!PathGetDocumentsPath(install_location, MAX_PATH, L"GWToolboxpp")) {
        fprintf(stderr, "PathGetDocumentsPath failed\n");
        return false;
    }

    wchar_t dll_path[MAX_PATH];
    if (!PathCompose(dll_path, MAX_PATH, install_location, L"GWToolboxdll.dll")) {
        fprintf(stderr, "PathCompose failed\n");
        return false;
    }

    wchar_t installer_path[MAX_PATH];
    if (!PathCompose(installer_path, MAX_PATH, install_location, L"GWToolbox.exe")) {
        fprintf(stderr, "PathCompose failed\n");
        return false;
    }

    DWORD dll_size;
    if (!GetFileSizeAsDword(dll_path, &dll_size)) {
        fprintf(stderr, "GetFileSizeAsDword failed\n");
        return false;
    }

    wchar_t uninstall[MAX_PATH + 64];
    wchar_t uninstall_quiet[MAX_PATH + 64];
    _snwprintf_s(uninstall, ARRAYSIZE(uninstall), L"%ls /uninstall", installer_path);
    _snwprintf_s(uninstall_quiet, ARRAYSIZE(uninstall_quiet), L"%ls /uninstall /quiet", installer_path);

    HKEY UninstallKey;
    if (!CreateUninstallKey(&UninstallKey)) {
        fprintf(stderr, "OpenUninstallKey failed\n");
        return false;
    }

    // current date as YYYYMMDD
    if (!RegWriteStr(UninstallKey, L"DisplayName", L"GWToolbox") ||
        !RegWriteStr(UninstallKey, L"DisplayIcon", installer_path) ||
        !RegWriteStr(UninstallKey, L"DisplayVersion", L"3.0") ||
        !RegWriteDWORD(UninstallKey, L"EstimatedSize", dll_size / 1000) ||
        !RegWriteStr(UninstallKey, L"InstallDate", time_buf) ||
        !RegWriteStr(UninstallKey, L"InstallLocation", install_location) ||
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

static fs::path GetComputer() {
    constexpr auto INFO_BUFFER_SIZE = 32;
    WCHAR computername[INFO_BUFFER_SIZE];
    DWORD bufCharCount = INFO_BUFFER_SIZE;

    if (!GetComputerNameW(computername, &bufCharCount)) {
        fprintf(stderr, "Cannot get computer name.\n");
    }

    static fs::path computer = computername;

    return computer;
}

bool MoveDataAndCreateSymlink() {
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

    if (fs::is_symlink(appdir) && fs::is_directory(docpath) && fs::is_directory(docpath / GetComputer())) {
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
                    fs::path dir = (docpath / GetComputer() / p.filename()).parent_path();
                    if (!fs::exists(dir)) {
                        fs::create_directories(dir, error);
                    }
                    fs::rename(p, docpath / GetComputer() / p.filename(), error);
                }
                else if (p.extension() == ".ini" || p.extension() == ".txt") {
                    fs::path dir = (docpath / GetComputer() / p.filename()).parent_path();
                    if (fs::exists(dir)) fs::create_directories(dir);
                    fs::rename(p, docpath / GetComputer() / p.filename(), error);
                }
                else {
                    fs::rename(p, docpath / p.filename(), error);
                }
            }
        }
    }

    if (fs::is_directory(apppath)) {
        fs::remove_all(apppath, error);
    }

    fs::create_directory_symlink(docpath, apppath, error);

    if (error.value()) {
        fprintf(stderr, "Couldn't create symlink from appdir to docdir.\n");
        return false;
    }

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
    wchar_t path[MAX_PATH];
    wchar_t temp[MAX_PATH];

    // Create %USERPROFILE%\Documents\GWToolboxpp\ 
    if (!PathGetDocumentsPath(path, MAX_PATH, L"GWToolboxpp\\")) {
        fprintf(stderr, "PathGetDocumentsPath failed\n");
        return false;
    }
    if (!PathCreateDirectory(path)) {
        fprintf(stderr, "PathCreateDirectory failed (path: '%ls')\n", path);
        return false;
    }

    // Create %USERPROFILE%\Documents\GWToolboxpp\<Computername>\logs
    if (!PathCompose(temp, MAX_PATH, path, (GetComputer() / L"logs").c_str())) {
        fprintf(stderr, "PathCompose failed ('%ls', '%ls')\n", path, L"logs");
        return false;
    }
    if (!fs::create_directories(temp)) {
        fprintf(stderr, "PathCreateDirectory failed (path: '%ls')\n", temp);
        return false;
    }

    // Create %USERPROFILE%\Documents\GWToolboxpp\<Computername>\crashes
    if (!PathCompose(temp, MAX_PATH, path, (GetComputer() / L"crashes").c_str())) {
        fprintf(stderr, "PathCompose failed ('%ls', '%ls')\n", path, L"crashes");
        return false;
    }
    if (!fs::create_directories(temp)) {
        fprintf(stderr, "PathCreateDirectory failed (path: '%ls')\n", temp);
        return false;
    }

    // Create %USERPROFILE%\Documents\GWToolboxpp\<Computername>\plugins
    if (!PathCompose(temp, MAX_PATH, path, (GetComputer() / L"plugins").c_str())) {
        fprintf(stderr, "PathCompose failed ('%ls', '%ls')\n", path, L"plugins");
        return false;
    }
    if (!fs::create_directories(temp)) {
        fprintf(stderr, "PathCreateDirectory failed (path: '%ls')\n", temp);
        return false;
    }

    // Create %USERPROFILE%\Documents\GWToolboxpp\<Computername>\data
    if (!PathCompose(temp, MAX_PATH, path, (GetComputer() / L"data").c_str())) {
        fprintf(stderr, "PathCompose failed ('%ls', '%ls')\n", path, L"data");
        return false;
    }
    if (!fs::create_directories(temp)) {
        fprintf(stderr, "PathCreateDirectory failed (path: '%ls')\n", temp);
        return false;
    }

    return true;
}

static bool CopyInstaller(void)
{
    wchar_t dest_path[MAX_PATH];
    wchar_t source_path[MAX_PATH];

    if (!PathGetDocumentsPath(dest_path, MAX_PATH, L"GWToolboxpp\\GWToolbox.exe")) {
        fprintf(stderr, "PathGetDocumentsPath failed\n");
        return false;
    }

    PathGetExeFullPath(source_path, MAX_PATH);

    if (wcsncmp(dest_path, source_path, MAX_PATH) == 0)
        return true;

    if (CopyFileW(source_path, dest_path, FALSE) != TRUE) {
        fprintf(stderr, "CopyFileW failed (%lu)\n", GetLastError());
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
    if (!PathGetDocumentsPath(path, MAX_PATH, L"GWToolboxpp\\*")) {
        fprintf(stderr, "PathGetDocumentsPath failed\n");
        return false;
    }

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
        fprintf(stderr, "SHFileOperationW failed: 0x%Xd\n", iRet);
        return false;
    }

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

    if (!MoveDataAndCreateSymlink()) {
        fprintf(stderr, "MoveDataAndCreateSymlink failed\n");
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

bool GetInstallationLocation(wchar_t *path, size_t length)
{
    HKEY UninstallKey;
    if (!OpenUninstallKey(&UninstallKey)) {
        fprintf(stderr, "OpenUninstallKey failed\n");
        return false;
    }

    if (!RegReadStr(UninstallKey, L"InstallLocation", path, length)) {
        fprintf(stderr, "RegReadStr failed\n");
        RegCloseKey(UninstallKey);
        return false;
    }

    RegCloseKey(UninstallKey);
    return true;
}
