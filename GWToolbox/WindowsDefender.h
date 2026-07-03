#pragma once

bool IsPathExcludedFromDefender(const std::filesystem::path& path);

// Adds a Defender folder exclusion and allows the given executables through Controlled Folder
// Access, in a single elevated PowerShell invocation (so at most one UAC prompt). Entries that
// already exist are skipped; `controlled_folder_access_apps` that don't exist on disk are ignored.
bool AddDefenderExceptions(const std::filesystem::path& exclusion_path,
                           const std::vector<std::filesystem::path>& controlled_folder_access_apps,
                           const bool quiet, std::wstring& error);

// Checks Defender state before injection and prompts to fix it when the folder isn't excluded or Controlled Folder Access blocks it; no-op when already fine or unreadable.
void EnsureDefenderReadiness(const std::filesystem::path& exclusion_path,
                             const std::vector<std::filesystem::path>& controlled_folder_access_apps);
