#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct Asset {
    std::string name{};
    size_t size = 0;
    std::string browser_download_url{};
    std::optional<std::string> digest{};
};

struct Release {
    std::string tag_name{};
    std::optional<std::string> body{};
    std::vector<Asset> assets{};
};

struct FoundAsset {
    const Release* release = nullptr;
    const Asset* asset = nullptr;
};

bool FetchReleases(std::vector<Release>& releases, std::string& error);
FoundAsset FindNewestAsset(const std::vector<Release>& releases, const std::vector<std::string_view>& names);
std::string Sha256FromDigest(const std::optional<std::string>& digest);
std::string VersionFromTag(const std::string& tag);
bool FileMatchesRelease(const std::filesystem::path& file, std::uintmax_t size, const std::string& sha256);
bool DownloadVerified(const std::string& url, std::uintmax_t size, const std::string& sha256, const std::function<void(int)>& progress, std::string& content, std::string& error);
