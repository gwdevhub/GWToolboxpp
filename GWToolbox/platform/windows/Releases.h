#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

bool FetchReleaseListing(std::string& listing, std::string& error);
bool FileMatchesRelease(const std::filesystem::path& file, std::uintmax_t size, const std::string& sha256);
bool DownloadVerified(const std::string& url, std::uintmax_t size, const std::string& sha256, const std::function<void(int)>& progress, std::string& content, std::string& error);
