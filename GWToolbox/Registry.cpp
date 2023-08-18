#include "stdafx.h"

#include "Registry.h"

bool OpenSettingsKey(PHKEY phkResult)
{
    DWORD Disposition;

    const auto lpSubKey = L"Software\\GWToolbox";
    const LSTATUS status = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        lpSubKey,
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_SET_VALUE | KEY_READ,
        nullptr,
        phkResult,
        &Disposition);

    if (status != ERROR_SUCCESS) {
        fprintf(stderr, "RegCreateKeyExW failed: {status:0x%lX, lpSubKey:'%ls'}\n", status, lpSubKey);
        phkResult = nullptr;
        return false;
    }

    return true;
}

bool DeleteSettingsKey()
{
    const auto lpSubKey = L"Software\\GWToolbox";
    const LSTATUS status = RegDeleteKeyW(
        HKEY_CURRENT_USER,
        lpSubKey);

    if (status != ERROR_SUCCESS) {
        fprintf(stderr, "RegDeleteKeyW failed: {status:0x%lX, lpSubKey:'%ls'}\n", status, lpSubKey);
        return false;
    }

    return true;
}

bool RegWriteStr(HKEY hKey, const LPCWSTR KeyName, const LPCWSTR Value)
{
    const size_t ValueSize = wcslen(Value) * 2;
    const LSTATUS status = RegSetValueExW(
        hKey,
        KeyName,
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(Value),
        ValueSize);

    if (status != ERROR_SUCCESS) {
        fprintf(stderr, "RegSetValueExW failed: {status:0x%lX, KeyName:'%ls', Value:'%ls'}\n",
                status, KeyName, Value);
        return false;
    }

    return true;
}

bool RegWriteDWORD(HKEY hKey, const LPCWSTR KeyName, const DWORD Value)
{
    const LSTATUS status = RegSetValueExW(
        hKey,
        KeyName,
        0,
        REG_DWORD,
        reinterpret_cast<const BYTE*>(&Value),
        sizeof(4));

    if (status != ERROR_SUCCESS) {
        fprintf(stderr, "RegSetValueExW failed: {status:0x%lX, KeyName:'%ls', Value:%lu}\n",
                status, KeyName, Value);
        return false;
    }

    return true;
}

bool RegReadStr(HKEY hKey, const LPCWSTR KeyName, LPWSTR Buffer, const size_t BufferLength)
{
    assert(BufferLength > 0);

    DWORD cbData = static_cast<DWORD>(BufferLength - 1) * sizeof(WCHAR);
    const LSTATUS status = RegGetValueW(
        hKey,
        L"",
        KeyName,
        RRF_RT_REG_SZ,
        nullptr,
        Buffer,
        &cbData);

    if (status != ERROR_SUCCESS) {
        fprintf(stderr, "RegGetValueW failed: {status:0x%lX, KeyName:'%ls'}\n",
                status, KeyName);
        return false;
    }

    Buffer[cbData / sizeof(WCHAR)] = 0;
    return true;
}

bool RegReadDWORD(HKEY hKey, const LPCWSTR KeyName, PDWORD dwDword)
{
    DWORD cbData = sizeof(*dwDword);
    const LSTATUS status = RegGetValueW(
        hKey,
        L"",
        KeyName,
        RRF_RT_REG_DWORD,
        nullptr,
        dwDword,
        &cbData);

    if (status != ERROR_SUCCESS) {
        fprintf(stderr, "RegGetValueW failed: {status:0x%lX, KeyName:'%ls'}\n",
                status, KeyName);
        return false;
    }

    return true;
}
