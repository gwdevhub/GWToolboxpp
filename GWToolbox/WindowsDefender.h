#pragma once

bool IsPathExcludedFromDefender(const std::filesystem::path& path);

bool AddDefenderExclusion(const std::filesystem::path& path, const bool quiet, std::wstring& error);

// Looks for a recent Defender detection/ASR-block event naming dll_name.
// Returns true and sets `detail` to the matching event message when found.
bool FindRecentDefenderBlock(const std::wstring& dll_name, std::wstring& detail);
