#include "stdafx.h"

#include "SystemInfo.h"

#include <format>

bool GetWineDiagnostics(std::wstring& out, const bool include_launcher_environment)
{
    const auto ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return false;
    const auto get_version = reinterpret_cast<const char*(__cdecl*)()>(GetProcAddress(ntdll, "wine_get_version"));
    if (!get_version) return false;

    const auto get_host = reinterpret_cast<void(__cdecl*)(const char**, const char**)>(GetProcAddress(ntdll, "wine_get_host_version"));
    const char* sysname = nullptr;
    const char* release = nullptr;
    if (get_host) get_host(&sysname, &release);

    wchar_t buf[256];
    swprintf(buf, _countof(buf), L"Wine %S on %S %S", get_version(), sysname ? sysname : "unknown", release ? release : "");
    out = buf;

    if (include_launcher_environment) {
        if (GetEnvironmentVariableW(L"STEAM_COMPAT_DATA_PATH", nullptr, 0) || GetEnvironmentVariableW(L"SteamAppId", nullptr, 0))
            out += L"; launched via Steam/Proton (sandboxed prefix - known to break injection)";
        else if (GetEnvironmentVariableW(L"LUTRIS_GAME_UUID", nullptr, 0) || GetEnvironmentVariableW(L"LUTRIS_PGA_DB", nullptr, 0))
            out += L"; launched via Lutris (sandboxed prefix - known to break injection)";
    }
    return true;
}

std::wstring GetOperatingSystemDescription()
{
    std::wstring wine;
    if (GetWineDiagnostics(wine, false)) return wine;

    const auto ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll) {
        using RtlGetVersionFunc = LONG(WINAPI*)(OSVERSIONINFOEXW*);
        const auto get_version = reinterpret_cast<RtlGetVersionFunc>(GetProcAddress(ntdll, "RtlGetVersion"));
        auto version = OSVERSIONINFOEXW{};
        version.dwOSVersionInfoSize = sizeof(version);
        if (get_version && get_version(&version) == 0) {
            auto name = std::wstring(L"Windows");
            if (version.dwMajorVersion == 10 && version.dwMinorVersion == 0 && version.wProductType == VER_NT_WORKSTATION) {
                name = version.dwBuildNumber >= 22000 ? L"Windows 11" : L"Windows 10";
            }
            return std::format(L"{} ({}.{} build {})", name, version.dwMajorVersion, version.dwMinorVersion, version.dwBuildNumber);
        }
    }
    return L"Windows (version unavailable)";
}
