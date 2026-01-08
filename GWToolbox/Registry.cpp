#include "stdafx.h"

#include "Registry.h"

#include <uWebsockets/HttpErrors.h>

namespace {
    const auto lpSubKey = L"Software\\GWToolbox";
}

bool RegReadDWORD(const LPCWSTR KeyName, const PDWORD Value, std::wstring& error)
{
    HKEY hKey;
    if (!OpenSettingsKey(&hKey, error))
        return false;
    DWORD cbData = sizeof(*Value);
    const LSTATUS status = RegGetValueW(
        hKey,
        L"",
        KeyName,
        RRF_RT_REG_DWORD,
        nullptr,
        Value,
        &cbData);

    RegCloseKey(hKey);
    if (status != ERROR_SUCCESS)
        return error = std::format(L"RegReadDWORD failed: (status:{:#X}, lpSubKey:'{}')", status, KeyName), false;

    return true;
}

bool RegWriteDWORD(const LPCWSTR KeyName, const DWORD Value, std::wstring& error)
{
    HKEY hKey;
    if (!OpenSettingsKey(&hKey, error))
        return false;
    const LSTATUS status = RegSetValueExW(
        hKey,
        KeyName,
        0,
        REG_DWORD,
        reinterpret_cast<const BYTE*>(&Value),
        sizeof(4));
    RegCloseKey(hKey);
    if (status != ERROR_SUCCESS) {
        return error = std::format(L"RegSetValueExW failed: (status:{:#X}, lpSubKey:'{}')", status, KeyName), false;
    }

    return true;
}

bool RegReadStr(const LPCWSTR KeyName, LPWSTR Buffer, const size_t BufferLength, std::wstring& error)
{
    HKEY hKey;
    if (!OpenSettingsKey(&hKey, error))
        return false;
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

    RegCloseKey(hKey);
    if (status != ERROR_SUCCESS)
        return error = std::format(L"RegReadStr failed: (status:{:#X}, lpSubKey:'{}')", status, KeyName), false;
    Buffer[cbData / sizeof(WCHAR)] = 0;
    return true;
}

bool RegWriteStr(const LPCWSTR KeyName, const LPCWSTR Value, std::wstring& error)
{
    HKEY hKey;
    if (!OpenSettingsKey(&hKey, error))
        return false;
    const size_t ValueSize = wcslen(Value) * 2;
    const LSTATUS status = RegSetValueExW(
        hKey,
        KeyName,
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(Value),
        ValueSize);
    RegCloseKey(hKey);
    if (status != ERROR_SUCCESS) {
        return error = std::format(L"RegWriteStr failed: (status:{:#X}, lpSubKey:'{}')", status, KeyName), false;
    }

    return true;
}

bool OpenSettingsKey(PHKEY phkResult, std::wstring& error)
{
    DWORD Disposition;
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

    if (status != ERROR_SUCCESS)
        return error = std::format(L"RegCreateKeyExW failed: (status:{:#X}, lpSubKey:'{}')", status, lpSubKey), false;
    return true;
}

bool DeleteSettingsKey(std::wstring& error)
{
    const LSTATUS status = RegDeleteKeyW(
        HKEY_CURRENT_USER,
        lpSubKey);

    if (status != ERROR_SUCCESS)
        return error = std::format(L"RegDeleteKeyW failed: (status:{:#X}, lpSubKey:'{}')", status, lpSubKey), false;

    return true;
}
