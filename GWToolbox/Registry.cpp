#include "stdafx.h"

#include "Registry.h"

bool OpenSettingsKey(PHKEY phkResult)
{
    LSTATUS status;
    DWORD Disposition;

    LPCWSTR lpSubKey = L"Software\\GWToolbox";
    status = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        lpSubKey,
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_SET_VALUE | KEY_READ,
        nullptr,
        phkResult,
        &Disposition);

    if (status != ERROR_SUCCESS)
    {
        fprintf(stderr, "RegCreateKeyExW failed: {status:0x%lX, lpSubKey:'%ls'}\n", status, lpSubKey);
        phkResult = nullptr;
        return false;
    }

    return true;
}

bool OpenUninstallKey(PHKEY phkResult)
{
    LPCWSTR lpSubKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\GWToolbox";
    LSTATUS status = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        lpSubKey,
        0,
        KEY_READ,
        phkResult);

    if (status != ERROR_SUCCESS)
    {
        fprintf(stderr, "RegOpenKeyExW failed: {status:0x%lX, lpSubKey:'%ls'}\n", status, lpSubKey);
        phkResult = nullptr;
        return false;
    }

    return true;
}

bool CreateUninstallKey(PHKEY phkResult)
{
    LSTATUS status;
    DWORD Disposition;

    LPCWSTR lpSubKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\GWToolbox";
    status = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        lpSubKey,
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_SET_VALUE | KEY_READ,
        nullptr,
        phkResult,
        &Disposition);

    if (status != ERROR_SUCCESS)
    {
        fprintf(stderr, "RegCreateKeyExW failed: {status:0x%lX, lpSubKey:'%ls'}\n", status, lpSubKey);
        phkResult = nullptr;
        return false;
    }

    return true;
}

bool DeleteSettingsKey()
{
    LPCWSTR lpSubKey = L"Software\\GWToolbox";
    LSTATUS status = RegDeleteKeyW(
        HKEY_CURRENT_USER,
        lpSubKey);

    if (status != ERROR_SUCCESS)
    {
        fprintf(stderr, "RegDeleteKeyW failed: {status:0x%lX, lpSubKey:'%ls'}\n", status, lpSubKey);
        return false;
    }

    return true;
}

bool DeleteUninstallKey()
{
    LPCWSTR lpSubKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\GWToolbox";
    LSTATUS status = RegDeleteKeyW(
        HKEY_CURRENT_USER,
        lpSubKey);

    if (status != ERROR_SUCCESS)
    {
        fprintf(stderr, "RegDeleteKeyW failed: {status:0x%lX, lpSubKey:'%ls'}\n", status, lpSubKey);
        return false;
    }

    return true;
}

bool RegWriteStr(HKEY hKey, LPCWSTR KeyName, LPCWSTR Value)
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
        fprintf(stderr, "RegSetValueExW failed: {status:0x%lX, KeyName:'%ls', Value:'%ls'}\n",
                status, KeyName, Value);
        return false;
    }

    return true;
}

bool RegWriteDWORD(HKEY hKey, LPCWSTR KeyName, DWORD Value)
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
        fprintf(stderr, "RegSetValueExW failed: {status:0x%lX, KeyName:'%ls', Value:%lu}\n",
                status, KeyName, Value);
        return false;
    }

    return true;
}

bool RegReadStr(HKEY hKey, LPCWSTR KeyName, LPWSTR Buffer, size_t BufferLength)
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
        fprintf(stderr, "RegGetValueW failed: {status:0x%lX, KeyName:'%ls'}\n",
                status, KeyName);
        return false;
    }

    Buffer[cbData / sizeof(WCHAR)] = 0;
    return true;
}

bool RegReadDWORD(HKEY hKey, LPCWSTR KeyName, PDWORD dwDword)
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
        fprintf(stderr, "RegGetValueW failed: {status:0x%lX, KeyName:'%ls'}\n",
                status, KeyName);
        return false;
    }

    return true;
}

bool RegReadRelease(char *buffer, size_t size)
{
    HKEY hKey;
    wchar_t Release[256];

    if (!OpenSettingsKey(&hKey)) {
        fprintf(stderr, "OpenSettingsKey failed\n");
        return false;
    }

    if (!RegReadStr(hKey, L"release", Release, ARRAYSIZE(Release))) {
        fprintf(stderr, "Couldn't read the key 'release' from GWToolbox settings\n");
        RegCloseKey(hKey);
        return false;
    }

    RegCloseKey(hKey);

    size_t ReleaseLength = wcslen(Release);
    if (size < (ReleaseLength + 1)) {
        fprintf(stderr, "Buffer too small to read 'release' key\n");
        return false;
    }

    for (size_t i = 0; i < ReleaseLength; ++i)
    {
        if ((Release[i] & ~0x7F) != 0) {
            fprintf(stderr, "Key 'release' in GWToolbox settings is non-ascii\n");
            return false;
        }

        buffer[i] = static_cast<char>(Release[i]);
    }
    buffer[ReleaseLength] = 0;

    return true;
}

bool RegWriteRelease(const char *buffer, size_t length)
{
    HKEY hKey;
    wchar_t Release[256];

    if (ARRAYSIZE(Release) < (length + 1)) {
        fprintf(stderr, "Local buffer too small\n");
        return false;
    }

    for (size_t i = 0; i < length; i++)
        Release[i] = static_cast<wchar_t>(buffer[i]);
    Release[length] = 0;

    if (!OpenSettingsKey(&hKey)) {
        fprintf(stderr, "OpenSettingsKey failed\n");
        return false;
    }

    if (!RegWriteStr(hKey, L"release", Release)) {
        fprintf(stderr, "RegWriteStr failed\n");
        RegCloseKey(hKey);
        return false;
    }

    RegCloseKey(hKey);
    return true;
}
