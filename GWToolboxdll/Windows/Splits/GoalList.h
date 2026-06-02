#pragma once

#include <string>
#include <vector>

#include "GoalEntry.h"

// ---------------------------------------------------------------------------
// GoalList — a named, ordered collection of goals.
// ---------------------------------------------------------------------------
struct GoalList {
    std::string            name;
    std::vector<GoalEntry> goals;

    void ResetRunState();

    bool SaveToFile(const std::wstring& path) const;
    bool LoadFromFile(const std::wstring& path);

    static std::vector<std::pair<std::string, std::wstring>>
        ListSaved(const std::wstring& folder);
};
