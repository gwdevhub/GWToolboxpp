#pragma once

bool IsPathExcludedFromDefender(const std::filesystem::path& path);

bool AddDefenderExclusion(const std::filesystem::path& path, const bool quiet, std::wstring& error);

// Looks for a recent Defender detection/ASR-block event naming one of dll_names.
// Returns true and sets `detail` to the matching event message when found.
bool FindRecentDefenderBlock(const std::vector<std::wstring>& dll_names, std::wstring& detail);
