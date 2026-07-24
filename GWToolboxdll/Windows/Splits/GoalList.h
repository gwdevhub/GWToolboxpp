#pragma once

#include <string>
#include <vector>

#include "GoalEntry.h"

// ---------------------------------------------------------------------------
// Reference splits embedded in a template — personal PB or an imported WR.
// ---------------------------------------------------------------------------
struct GoalReference {
    std::string         name;   // e.g. "Personal Best" or "WR - SomePlayer"
    std::string         date;   // YYYY-MM-DD when this reference was set
    std::vector<double> splits; // real-time cumulative split per non-header goal
};

// ---------------------------------------------------------------------------
// GoalList — a named, ordered collection of goals.
// ---------------------------------------------------------------------------
struct GoalList {
    bool                         is_preset = false;
    std::string                  name;
    std::vector<GoalEntry>       goals;
    std::optional<GoalReference> reference;

    void ResetRunState();
    // Suffixes " (1)", " (2)", ... onto goals that share a label, in list order;
    // strips the suffix back off when only one goal has that label.
    void RenumberDuplicateLabels();

    bool SaveToFile(const std::wstring& path) const;
    bool LoadFromFile(const std::wstring& path);

    static std::vector<std::pair<std::string, std::wstring>>
        ListSaved(const std::wstring& folder);
};
