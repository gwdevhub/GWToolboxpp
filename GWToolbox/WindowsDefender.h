#pragma once

bool IsPathExcludedFromDefender(const std::filesystem::path& path);

bool AddDefenderExclusion(const std::filesystem::path& path, bool quiet);
