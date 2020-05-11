#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN
#endif
#ifdef NOMINMAX
# define NOMINMAX
#endif
#include <Windows.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "Download.h"
#include "Install.h"
#include "Options.h"
#include "Path.h"

#define HKEY_USED_HIVE HKEY_CURRENT_USER

static bool OpenUninstallKey(HKEY hKey, PHKEY phkResult)
{
    LSTATUS status;
    DWORD Disposition;

    status = RegCreateKeyExW(
        hKey,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\GWToolbox",
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_SET_VALUE | KEY_READ,
        nullptr,
        phkResult,
        &Disposition);

    if (status != ERROR_SUCCESS)
    {
        fprintf(stderr, "RegCreateKeyExW failed: status:%d\n", status);
        phkResult = nullptr;
        return false;
    }

    return true;
}

static bool OpenSettingsKey(HKEY hKey, PHKEY phkResult)
{
    LSTATUS status;
    DWORD Disposition;

    status = RegCreateKeyExW(
        hKey,
        L"Software\\GWToolbox",
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_SET_VALUE | KEY_READ,
        nullptr,
        phkResult,
        &Disposition);

    if (status != ERROR_SUCCESS)
    {
        fprintf(stderr, "RegCreateKeyExW failed: status:%d\n", status);
        phkResult = nullptr;
        return false;
    }

    return true;
}

static bool RegWriteStr(HKEY hKey, LPCWSTR KeyName, LPCWSTR Value)
{
    size_t ValueSize = wcslen(Value) * 2;
    LSTATUS status = RegSetValueExW(
        hKey,
        KeyName,
        0,
        REG_SZ,
        reinterpret_cast<const BYTE *>(Value),
        ValueSize);

    if (status != ERROR_SUCCESS)
    {
        fprintf(stderr, "RegSetValueExW failed: status:%d\n", status);
        return false;
    }

    return true;
}

static bool RegWriteDWORD(HKEY hKey, LPCWSTR KeyName, DWORD Value)
{
    LSTATUS status = RegSetValueExW(
        hKey,
        KeyName,
        0,
        REG_DWORD,
        reinterpret_cast<const BYTE *>(&Value),
        sizeof(4));

    if (status != ERROR_SUCCESS)
    {
        fprintf(stderr, "RegSetValueExW failed: status:%d\n", status);
        return false;
    }

    return true;
}

static bool RegReadStr(HKEY hKey, LPCWSTR KeyName, LPWSTR Buffer, size_t BufferLength)
{
    assert(BufferLength > 0);

    DWORD cbData = static_cast<DWORD>(BufferLength - 1) * sizeof(WCHAR);
    LSTATUS status = RegGetValueW(
        hKey,
        L"",
        KeyName,
        RRF_RT_REG_SZ,
        nullptr,
        Buffer,
        &cbData);

    if (status != ERROR_SUCCESS)
    {
        fprintf(stderr, "RegGetValueW failed: status:%d\n", status);
        return false;
    }

    Buffer[cbData / sizeof(WCHAR)] = 0;
    return true;
}

static bool RegReadDWORD(HKEY hKey, LPCWSTR KeyName, PDWORD dwDword)
{
    DWORD cbData = sizeof(*dwDword);
    LSTATUS status = RegGetValueW(
        hKey,
        L"",
        KeyName,
        RRF_RT_REG_DWORD,
        nullptr,
        dwDword,
        &cbData);

    if (status != ERROR_SUCCESS)
    {
        fprintf(stderr, "RegGetValueW failed: status:%d\n", status);
        return false;
    }

    return true;
}

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

    *file_size = FileSize.QuadPart;
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

static bool InstallUninstallKey(HKEY hKey)
{
    wchar_t time_buf[32];
    struct tm local_time;
    time_t raw_time = time(nullptr);
    localtime_s(&local_time , &raw_time);
    wcsftime(time_buf, ARRAYSIZE(time_buf), L"%Y%m%d", &local_time);

    wchar_t install_location[MAX_PATH];
    if (!PathGetAppDataPath(install_location, MAX_PATH, L"GWToolboxpp")) {
        fprintf(stderr, "PathGetAppDataPath failed\n");
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
    if (!OpenUninstallKey(hKey, &UninstallKey)) {
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

static bool InstallSettingsKey(HKEY hKey)
{
    HKEY SettingsKey;
    if (!OpenSettingsKey(hKey, &SettingsKey)) {
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

static bool DeleteUninstallKey(HKEY hKey)
{
    LSTATUS status = RegDeleteKeyW(
        hKey,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\GWToolbox");

    if (status != ERROR_SUCCESS)
    {
        fprintf(stderr, "RegDeleteKeyW failed: status:%d\n", status);
        return false;
    }

    return true;
}

static bool DeleteSettingsKey(HKEY hKey)
{
    LSTATUS status = RegDeleteKeyW(
        hKey,
        L"Software\\GWToolbox");

    if (status != ERROR_SUCCESS)
    {
        fprintf(stderr, "RegDeleteKeyW failed: status:%d\n", status);
        return false;
    }

    return true;
}

static bool EnsureInstallationDirectoryExist(void)
{
    wchar_t path[MAX_PATH];
    wchar_t temp[MAX_PATH];

    // Create %localappdata%\GWToolboxpp
    if (!PathGetAppDataPath(path, MAX_PATH, L"GWToolboxpp\\")) {
        fprintf(stderr, "PathGetAppDataPath failed\n");
        return false;
    }
    if (!PathCreateDirectory(path)) {
        fprintf(stderr, "PathCreateDirectory failed (path: '%ls')\n", path);
        return false;
    }

    // Create %localappdata%\GWToolboxpp\logs
    if (!PathCompose(temp, MAX_PATH, path, L"logs")) {
        fprintf(stderr, "PathCompose failed ('%ls', '%ls')\n", path, L"logs");
        return false;
    }
    if (!PathCreateDirectory(temp)) {
        fprintf(stderr, "PathCreateDirectory failed (path: '%ls')\n", temp);
        return false;
    }

    // Create %localappdata%\GWToolboxpp\crashes
    if (!PathCompose(temp, MAX_PATH, path, L"crashes")) {
        fprintf(stderr, "PathCompose failed ('%ls', '%ls')\n", path, L"crashes");
        return false;
    }
    if (!PathCreateDirectory(temp)) {
        fprintf(stderr, "PathCreateDirectory failed (path: '%ls')\n", temp);
        return false;
    }

    // Create %localappdata%\GWToolboxpp\plugins
    if (!PathCompose(temp, MAX_PATH, path, L"plugins")) {
        fprintf(stderr, "PathCompose failed ('%ls', '%ls')\n", path, L"plugins");
        return false;
    }
    if (!PathCreateDirectory(temp)) {
        fprintf(stderr, "PathCreateDirectory failed (path: '%ls')\n", temp);
        return false;
    }

    // Create %localappdata%\GWToolboxpp\data
    if (!PathCompose(temp, MAX_PATH, path, L"data")) {
        fprintf(stderr, "PathCompose failed ('%ls', '%ls')\n", path, L"data");
        return false;
    }
    if (!PathCreateDirectory(temp)) {
        fprintf(stderr, "PathCreateDirectory failed (path: '%ls')\n", temp);
        return false;
    }

    return true;
}

static bool CopyInstaller(void)
{
    wchar_t dest_path[MAX_PATH];
    wchar_t source_path[MAX_PATH];

    if (!PathGetAppDataPath(dest_path, MAX_PATH, L"GWToolboxpp\\GWToolbox.exe")) {
        fprintf(stderr, "PathGetAppDataPath failed\n");
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

static bool DeleteInstaller(void)
{
    wchar_t path[MAX_PATH];
    if (!PathGetAppDataPath(path, MAX_PATH, L"GWToolboxpp\\GWToolbox.exe")) {
        fprintf(stderr, "PathGetAppDataPath failed\n");
        return false;
    }

    if (DeleteFileW(path) != TRUE) {
        fprintf(stderr, "DeleteFileW failed (%lu)\n", GetLastError());
        return false;
    }

    return true;
}

bool Install(bool quiet)
{
    if (!EnsureInstallationDirectoryExist()) {
        fprintf(stderr, "EnsureInstallationDirectoryExist failed\n");
        return false;
    }

    if (!CopyInstaller()) {
        fprintf(stderr, "CopyInstaller failed\n");
        return false;        
    }

    if (!DownloadFiles()) {
        fprintf(stderr, "DownloadFiles failed\n");
        return false;
    }

    if (!InstallUninstallKey(HKEY_USED_HIVE)) {
        fprintf(stderr, "InstallUninstallKey failed\n");
        return false;
    }

    if (!InstallSettingsKey(HKEY_USED_HIVE)) {
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
    if (!DeleteSettingsKey(HKEY_USED_HIVE)) {
        fprintf(stderr, "DeleteSettingsKey failed\n");
        return false;
    }

    if (!DeleteUninstallKey(HKEY_USED_HIVE)) {
        fprintf(stderr, "DeleteUninstallKey failed\n");
        return false;
    }

    // @Remark:
    // This is likely to fail. (i.e. if we are using GWToolbox.exe from the file)
    DeleteInstaller();

    if (!quiet) {
        MessageBoxW(0, L"Uninstallation successful", L"Uninstallation", 0);
    }

    return true;
}

bool IsInstalled()
{
    HKEY UninstallKey;
    if (!OpenUninstallKey(HKEY_USED_HIVE, &UninstallKey)) {
        return false;
    }

    RegCloseKey(UninstallKey);
    return true;
}

bool GetInstallationLocation(wchar_t *path, size_t length)
{
    HKEY UninstallKey;
    if (!OpenUninstallKey(HKEY_USED_HIVE, &UninstallKey)) {
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

void ParseRegistrySettings()
{
    HKEY SettingsKey;
    if (!OpenSettingsKey(HKEY_USED_HIVE, &SettingsKey)) {
        fprintf(stderr, "OpenUninstallKey failed\n");
        return;
    }

    DWORD asadmin;
    if (RegReadDWORD(SettingsKey, L"asadmin", &asadmin)) {
        options.asadmin = (asadmin != 0);
    }

    DWORD noupdate;
    if (RegReadDWORD(SettingsKey, L"noupdate", &noupdate)) {
        options.noupdate = (noupdate != 0);
    }

    RegCloseKey(SettingsKey);
}
