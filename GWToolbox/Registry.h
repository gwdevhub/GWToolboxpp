#pragma once

bool OpenSettingsKey(PHKEY phkResult, std::wstring& error);
bool DeleteSettingsKey(std::wstring& error);

bool RegWriteStr(LPCWSTR KeyName, LPCWSTR Value, std::wstring& error);
bool RegReadDWORD(LPCWSTR KeyName, PDWORD Value, std::wstring& error);
bool RegReadStr(LPCWSTR KeyName, LPWSTR Buffer, size_t BufferLength, std::wstring& error);
bool RegWriteDWORD(LPCWSTR KeyName, DWORD Value, std::wstring& error);
