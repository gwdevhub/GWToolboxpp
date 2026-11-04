#include "stdafx.h"

#include "SystemInfo.h"
#include "Module.h"

#include <format>

namespace {
    constexpr auto arm64_emulation_description = L"ARM64 host (running via CPU emulation)";

    USHORT GetNativeProcessorMachine()
    {
        static const auto machine = []() -> USHORT {
            using IsWow64Process2_t = BOOL(WINAPI*)(HANDLE, USHORT*, USHORT*);
            const auto is_wow64_process2 = GetModuleFunctionW<IsWow64Process2_t>(L"kernel32.dll", "IsWow64Process2");
            auto process_machine = USHORT{IMAGE_FILE_MACHINE_UNKNOWN};
            auto native_machine = USHORT{IMAGE_FILE_MACHINE_UNKNOWN};
            if (is_wow64_process2 && is_wow64_process2(GetCurrentProcess(), &process_machine, &native_machine)) return native_machine;

            auto system_info = SYSTEM_INFO{};
            GetNativeSystemInfo(&system_info);
            switch (system_info.wProcessorArchitecture) {
                case PROCESSOR_ARCHITECTURE_INTEL: return IMAGE_FILE_MACHINE_I386;
                case PROCESSOR_ARCHITECTURE_AMD64: return IMAGE_FILE_MACHINE_AMD64;
                case PROCESSOR_ARCHITECTURE_ARM64: return IMAGE_FILE_MACHINE_ARM64;
                default: return IMAGE_FILE_MACHINE_UNKNOWN;
            }
        }();
        return machine;
    }
}

bool GetEmulationDiagnostics(std::wstring& out)
{
    static const auto emulated = GetNativeProcessorMachine() == IMAGE_FILE_MACHINE_ARM64;
    if (!emulated) return false;
    out = arm64_emulation_description;
    return true;
}

const std::wstring& GetProcessorDescription()
{
    static const auto description = []() -> std::wstring {
        switch (GetNativeProcessorMachine()) {
            case IMAGE_FILE_MACHINE_I386: return L"x86 host";
            case IMAGE_FILE_MACHINE_AMD64: return L"x64 host";
            case IMAGE_FILE_MACHINE_ARM64: return arm64_emulation_description;
            default: return L"Unknown architecture";
        }
    }();
    return description;
}

bool GetWineDiagnostics(std::wstring& out, const bool include_launcher_environment)
{
    struct Diagnostics {
        std::wstring system;
        std::wstring launcher;
    };
    static const auto diagnostics = [] {
        auto result = Diagnostics{};
        const auto get_version = GetModuleFunctionW<const char*(__cdecl*)()>(L"ntdll.dll", "wine_get_version");
        if (!get_version) return result;

        const auto get_host = GetModuleFunctionW<void(__cdecl*)(const char**, const char**)>(L"ntdll.dll", "wine_get_host_version");
        const char* sysname = nullptr;
        const char* release = nullptr;
        if (get_host) get_host(&sysname, &release);

        wchar_t buf[256];
        swprintf(buf, _countof(buf), L"Wine %S on %S %S", get_version(), sysname ? sysname : "unknown", release ? release : "");
        result.system = result.launcher = buf;
        if (GetEnvironmentVariableW(L"STEAM_COMPAT_DATA_PATH", nullptr, 0) || GetEnvironmentVariableW(L"SteamAppId", nullptr, 0))
            result.launcher += L"; launched via Steam/Proton (sandboxed prefix - known to break injection)";
        else if (GetEnvironmentVariableW(L"LUTRIS_GAME_UUID", nullptr, 0) || GetEnvironmentVariableW(L"LUTRIS_PGA_DB", nullptr, 0))
            result.launcher += L"; launched via Lutris (sandboxed prefix - known to break injection)";
        return result;
    }();
    if (diagnostics.system.empty()) return false;
    out = include_launcher_environment ? diagnostics.launcher : diagnostics.system;
    return true;
}

const std::wstring& GetOperatingSystemDescription()
{
    static const auto description = []() -> std::wstring {
        std::wstring wine;
        if (GetWineDiagnostics(wine, false)) return wine;

        using RtlGetVersionFunc = LONG(WINAPI*)(OSVERSIONINFOEXW*);
        const auto get_version = GetModuleFunctionW<RtlGetVersionFunc>(L"ntdll.dll", "RtlGetVersion");
        auto version = OSVERSIONINFOEXW{};
        version.dwOSVersionInfoSize = sizeof(version);
        if (get_version && get_version(&version) == 0) {
            auto name = std::wstring(L"Windows");
            if (version.dwMajorVersion == 10 && version.dwMinorVersion == 0 && version.wProductType == VER_NT_WORKSTATION) {
                name = version.dwBuildNumber >= 22000 ? L"Windows 11" : L"Windows 10";
            }
            return std::format(L"{} ({}.{} build {})", name, version.dwMajorVersion, version.dwMinorVersion, version.dwBuildNumber);
        }
        return L"Windows (version unavailable)";
    }();
    return description;
}
