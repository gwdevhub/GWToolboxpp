#include "stdafx.h"
#include "GoalList.h"

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Serialization helpers
// ---------------------------------------------------------------------------
static std::string TriggerTypeName(GoalTrigger::Type t)
{
    switch (t) {
        case GoalTrigger::Type::MapEnter:         return "MapEnter";
        case GoalTrigger::Type::EnterExplorable:  return "EnterExplorable";
        case GoalTrigger::Type::ExitExplorable:   return "ExitExplorable";
        case GoalTrigger::Type::VanquishComplete: return "VanquishComplete";
        case GoalTrigger::Type::MissionComplete:  return "MissionComplete";
        case GoalTrigger::Type::MissionBonus:     return "MissionBonus";
        case GoalTrigger::Type::ReachLevel:       return "ReachLevel";
        case GoalTrigger::Type::ExitOutpost:      return "ExitOutpost";
        case GoalTrigger::Type::ReachTitleRank:   return "ReachTitleRank";
        default:                                  return "Manual";
    }
}

static GoalTrigger::Type TriggerTypeFromString(const std::string& s)
{
    if (s == "MapEnter")         return GoalTrigger::Type::MapEnter;
    if (s == "EnterExplorable")  return GoalTrigger::Type::EnterExplorable;
    if (s == "ExitExplorable")   return GoalTrigger::Type::ExitExplorable;
    if (s == "VanquishComplete") return GoalTrigger::Type::VanquishComplete;
    if (s == "MissionComplete")  return GoalTrigger::Type::MissionComplete;
    if (s == "MissionBonus")     return GoalTrigger::Type::MissionBonus;
    if (s == "ReachLevel")       return GoalTrigger::Type::ReachLevel;
    if (s == "ExitOutpost")      return GoalTrigger::Type::ExitOutpost;
    if (s == "ReachTitleRank")   return GoalTrigger::Type::ReachTitleRank;
    return GoalTrigger::Type::Manual;
}

// ---------------------------------------------------------------------------
void GoalList::ResetRunState()
{
    for (auto& g : goals) {
        g.completed = false;
        g.split     = {};
    }
}

bool GoalList::SaveToFile(const std::wstring& path) const
{
    json j;
    j["name"] = name;

    json jgoals = json::array();
    for (const auto& g : goals) {
        json jg;
        jg["label"]        = g.label;
        jg["trigger_type"] = TriggerTypeName(g.trigger.type);
        jg["map_id"]       = static_cast<int>(g.trigger.map_id);
        jg["level"]        = g.trigger.level;
        jg["title_id"]     = static_cast<uint32_t>(g.trigger.title_id);
        if (g.trigger.hard_mode) jg["hard_mode"] = true;
        jgoals.push_back(std::move(jg));
    }
    j["goals"] = std::move(jgoals);

    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << j.dump(2);
    return true;
}

bool GoalList::LoadFromFile(const std::wstring& path)
{
    std::ifstream f(path);
    if (!f.is_open()) return false;

    json j;
    try { j = json::parse(f); }
    catch (...) { return false; }

    name = j.value("name", "");
    goals.clear();

    for (const auto& jg : j.value("goals", json::array())) {
        GoalEntry g;
        g.label             = jg.value("label", "");
        g.trigger.type      = TriggerTypeFromString(jg.value("trigger_type", "Manual"));
        g.trigger.map_id    = static_cast<GW::Constants::MapID>(jg.value("map_id", 0));
        g.trigger.level     = jg.value("level", 0);
        g.trigger.title_id  = static_cast<GW::Constants::TitleID>(jg.value("title_id", 0xffu));
        g.trigger.hard_mode = jg.value("hard_mode", false);
        goals.push_back(std::move(g));
    }
    return true;
}

std::vector<std::pair<std::string, std::wstring>>
GoalList::ListSaved(const std::wstring& folder)
{
    std::vector<std::pair<std::string, std::wstring>> result;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
        if (entry.path().extension() != L".json") continue;
        if (entry.path().stem() == L"resume")     continue;
        result.emplace_back(entry.path().stem().string(), entry.path().wstring());
    }
    return result;
}
