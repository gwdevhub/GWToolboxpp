#pragma once

bool OpenSettingsKey(PHKEY phkResult);

bool OpenUninstallKey(PHKEY phkResult);
bool CreateUninstallKey(PHKEY phkResult);

bool DeleteSettingsKey();
bool DeleteUninstallKey();

bool RegWriteStr(HKEY hKey, LPCWSTR KeyName, LPCWSTR Value);
bool RegWriteDWORD(HKEY hKey, LPCWSTR KeyName, DWORD Value);

bool RegReadStr(HKEY hKey, LPCWSTR KeyName, LPWSTR Buffer, size_t BufferLength);
bool RegReadDWORD(HKEY hKey, LPCWSTR KeyName, PDWORD dwDword);

bool RegReadRelease(char *buffer, size_t size);
bool RegWriteRelease(const char *buffer, size_t length);
