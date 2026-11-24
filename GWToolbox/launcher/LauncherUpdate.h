#pragma once

#include <filesystem>
#include <functional>
#include <string>

#include "DllDownload.h"

std::filesystem::path RunningLauncherPath();
std::filesystem::path StagedLauncherPath(const std::filesystem::path& current);
std::filesystem::path OldLauncherPath(const std::filesystem::path& current);
bool FetchLauncherRelease(DllRelease& release, std::string& error);
bool DownloadLauncherRelease(const DllRelease& release, const std::filesystem::path& staged, const std::function<void(int)>& progress, std::string& error);
bool InstallLauncherRelease(const std::filesystem::path& current, const std::filesystem::path& staged, std::string& error);
bool RestartLauncher(std::string& error);
void RemoveLauncherLeftovers(const std::filesystem::path& current);
