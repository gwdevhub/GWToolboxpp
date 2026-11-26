#pragma once

#include <chrono>
#include <filesystem>
#include <optional>

#include "DllDownload.h"

std::filesystem::path FindInstalledDll();

std::filesystem::path LocalDllPath();

std::filesystem::path DefaultDllPath();

void RecordInstalledDll(const std::filesystem::path& dll);

std::optional<std::filesystem::path> savedDllPath();

void SetLocalDllOnly(bool enabled);

std::optional<DllRelease> CachedDllRelease(std::chrono::seconds max_age);

void StoreDllRelease(const DllRelease& release);
