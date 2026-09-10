#pragma once

bool IsPathExcludedFromDefender(const std::filesystem::path& path);

bool AddDefenderExceptions(const std::filesystem::path& exclusion_path,
                           const std::vector<std::filesystem::path>& controlled_folder_access_apps,
                           const bool quiet, std::wstring& error);

// Checks Defender state before injection and prompts to fix it when the folder isn't excluded or Controlled Folder Access blocks it; no-op when already fine or unreadable.
void EnsureDefenderReadiness(const std::filesystem::path& exclusion_path,
                             const std::vector<std::filesystem::path>& controlled_folder_access_apps);
