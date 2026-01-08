#pragma once

bool IsPathExcludedFromDefender(const std::filesystem::path& path);

bool AddDefenderExclusion(const std::filesystem::path& path, const bool quiet, std::wstring& error);
