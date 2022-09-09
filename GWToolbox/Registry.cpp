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
