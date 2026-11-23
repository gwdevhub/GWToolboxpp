#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

struct DllRelease {
    std::string version;
    std::string changelog;
    std::string url;
    std::uintmax_t size = 0;
    std::string sha256;
};

bool FetchDllRelease(DllRelease& release, std::string& error);
bool DownloadDllFile(const DllRelease& release, const std::filesystem::path& directory, const std::function<void(int)>& progress, std::string& error);
std::filesystem::path SuggestedInstallDirectory();
bool PrepareInstallDirectory(const std::filesystem::path& directory, std::string& error);
bool UseDll(const std::filesystem::path& dll, std::string& error);
