#pragma once

#include <Windows/Splits/GoalEntry.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

struct GoalReference {
    std::vector<double> splits;
};

struct GoalList {
    bool is_preset = false;
    std::string name;
    std::vector<GoalEntry> goals;
    std::optional<GoalReference> reference;

    void ResetRunState();
    void RenumberDuplicateLabels();

    bool SaveToFile(const std::filesystem::path& path) const;
    bool LoadFromFile(const std::filesystem::path& path);

    static std::vector<std::pair<std::string, std::wstring>> ListSaved(const std::filesystem::path& folder);
    static std::wstring FileStem(std::string_view name);
};
