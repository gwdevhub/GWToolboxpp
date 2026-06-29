#include "stdafx.h"
#include "GoalList.h"

#include <cctype>
#include <unordered_map>

// ---------------------------------------------------------------------------
// JSON DTOs for split-list files (glaze reflection requires external linkage).
// ---------------------------------------------------------------------------
namespace GoalListJson {
    struct SerializedTrigger {
        std::string trigger_type = "Manual";
        std::optional<int>      map_id;
        std::optional<uint32_t> param1;
        std::optional<uint32_t> param2;
        std::optional<std::vector<uint16_t>> pattern;
    };

    struct SerializedGoal {
        std::string label;
        std::string trigger_type = "Manual";
        int         map_id       = 0;
        int         level        = 0;
        uint32_t    title_id     = 0xffu;
        std::optional<bool>     hard_mode;
        std::optional<uint32_t> param1;
        std::optional<uint32_t> param2;
        std::optional<std::vector<uint16_t>> pattern;
        std::optional<bool> starts_immediately;
        std::optional<SerializedTrigger> start_trigger;
        std::optional<std::vector<SerializedTrigger>> extra_triggers;
        std::optional<int> auto_complete_previous;
        std::optional<bool> is_header;
        std::optional<int>  indent;
    };

    struct SerializedReference {
        std::string                name;
        std::optional<std::string> date;
        std::vector<double>        splits;
    };

    struct SerializedGoalList {
        std::string name;
        std::vector<SerializedGoal> goals;
        std::optional<SerializedReference> reference;
    };

    std::vector<uint16_t> EncodePattern(const std::wstring& pattern)
    {
        std::vector<uint16_t> out;
        out.reserve(pattern.size());
        for (wchar_t c : pattern) out.push_back(static_cast<uint16_t>(c));
        return out;
    }

    std::wstring DecodePattern(const std::vector<uint16_t>& pattern)
    {
        std::wstring out;
        out.reserve(pattern.size());
        for (uint16_t c : pattern) out += static_cast<wchar_t>(c);
        return out;
    }
}
using namespace GoalListJson;

// ---------------------------------------------------------------------------
// Serialization helpers
// ---------------------------------------------------------------------------
static std::string TriggerTypeName(GoalTrigger::Type t)
{
    switch (t) {
        case GoalTrigger::Type::MapEnter:                return "MapEnter";
        case GoalTrigger::Type::EnterExplorable:         return "EnterExplorable";
        case GoalTrigger::Type::ExitExplorable:          return "ExitExplorable";
        case GoalTrigger::Type::VanquishComplete:        return "VanquishComplete";
        case GoalTrigger::Type::MissionComplete:         return "MissionComplete";
        case GoalTrigger::Type::MissionBonus:            return "MissionBonus";
        case GoalTrigger::Type::ReachLevel:              return "ReachLevel";
        case GoalTrigger::Type::ExitOutpost:             return "ExitOutpost";
        case GoalTrigger::Type::ReachTitleRank:          return "ReachTitleRank";
        case GoalTrigger::Type::ObjectiveDone:           return "ObjectiveDone";
        case GoalTrigger::Type::DoorOpen:                return "DoorOpen";
        case GoalTrigger::Type::DoorClose:               return "DoorClose";
        case GoalTrigger::Type::AgentUpdateAllegiance:   return "AgentUpdateAllegiance";
        case GoalTrigger::Type::DoACompleteZone:         return "DoACompleteZone";
        case GoalTrigger::Type::DungeonReward:           return "DungeonReward";
        case GoalTrigger::Type::ServerMessage:           return "ServerMessage";
        case GoalTrigger::Type::DisplayDialogue:         return "DisplayDialogue";
        case GoalTrigger::Type::CountdownStart:          return "CountdownStart";
        case GoalTrigger::Type::ObjectiveStarted:        return "ObjectiveStarted";
        default:                                         return "Manual";
    }
}

static GoalTrigger::Type TriggerTypeFromString(const std::string& s)
{
    if (s == "MapEnter")               return GoalTrigger::Type::MapEnter;
    if (s == "EnterExplorable")        return GoalTrigger::Type::EnterExplorable;
    if (s == "ExitExplorable")         return GoalTrigger::Type::ExitExplorable;
    if (s == "VanquishComplete")       return GoalTrigger::Type::VanquishComplete;
    if (s == "MissionComplete")        return GoalTrigger::Type::MissionComplete;
    if (s == "MissionBonus")           return GoalTrigger::Type::MissionBonus;
    if (s == "ReachLevel")             return GoalTrigger::Type::ReachLevel;
    if (s == "ExitOutpost")            return GoalTrigger::Type::ExitOutpost;
    if (s == "ReachTitleRank")         return GoalTrigger::Type::ReachTitleRank;
    if (s == "ObjectiveDone")          return GoalTrigger::Type::ObjectiveDone;
    if (s == "DoorOpen")               return GoalTrigger::Type::DoorOpen;
    if (s == "DoorClose")              return GoalTrigger::Type::DoorClose;
    if (s == "AgentUpdateAllegiance")  return GoalTrigger::Type::AgentUpdateAllegiance;
    if (s == "DoACompleteZone")        return GoalTrigger::Type::DoACompleteZone;
    if (s == "DungeonReward")          return GoalTrigger::Type::DungeonReward;
    if (s == "ServerMessage")          return GoalTrigger::Type::ServerMessage;
    if (s == "DisplayDialogue")        return GoalTrigger::Type::DisplayDialogue;
    if (s == "CountdownStart")         return GoalTrigger::Type::CountdownStart;
    if (s == "ObjectiveStarted")        return GoalTrigger::Type::ObjectiveStarted;
    return GoalTrigger::Type::Manual;
}

static SerializedTrigger ToSerialized(const GoalTrigger& t)
{
    SerializedTrigger jt;
    jt.trigger_type = TriggerTypeName(t.type);
    if (t.map_id != GW::Constants::MapID::None) jt.map_id = static_cast<int>(t.map_id);
    if (t.param1) jt.param1 = t.param1;
    if (t.param2) jt.param2 = t.param2;
    if (!t.pattern.empty()) jt.pattern = EncodePattern(t.pattern);
    return jt;
}

static GoalTrigger FromSerialized(const SerializedTrigger& jt)
{
    GoalTrigger t;
    t.type    = TriggerTypeFromString(jt.trigger_type);
    t.map_id  = static_cast<GW::Constants::MapID>(jt.map_id.value_or(0));
    t.param1  = jt.param1.value_or(0u);
    t.param2  = jt.param2.value_or(0u);
    if (jt.pattern) t.pattern = DecodePattern(*jt.pattern);
    return t;
}

// ---------------------------------------------------------------------------
void GoalList::ResetRunState()
{
    for (auto& g : goals) {
        g.status          = GoalStatus::NotStarted;
        g.split           = {};
        g.start_real_time = -1.0;
        g.start_game_time = -1.0;
    }
}

void GoalList::RenumberDuplicateLabels()
{
    // Strips a trailing " (N)" suffix this function previously added, so re-running it
    // (e.g. after a goal is deleted) renumbers from a clean base label each time.
    auto strip_suffix = [](const std::string& label) -> std::string {
        const size_t open = label.rfind(" (");
        if (open == std::string::npos || label.back() != ')') return label;
        const size_t digits_begin = open + 2;
        if (digits_begin >= label.size() - 1) return label;
        for (size_t i = digits_begin; i < label.size() - 1; ++i)
            if (!std::isdigit(static_cast<unsigned char>(label[i]))) return label;
        return label.substr(0, open);
    };

    std::vector<std::string> base_labels(goals.size());
    std::unordered_map<std::string, int> counts;
    for (size_t i = 0; i < goals.size(); ++i) {
        if (goals[i].is_header) continue;
        base_labels[i] = strip_suffix(goals[i].label);
        ++counts[base_labels[i]];
    }

    std::unordered_map<std::string, int> seen;
    for (size_t i = 0; i < goals.size(); ++i) {
        if (goals[i].is_header) continue;
        const std::string& base = base_labels[i];
        if (counts[base] <= 1) {
            goals[i].label = base;
        } else {
            goals[i].label = base + " (" + std::to_string(++seen[base]) + ")";
        }
    }
}

bool GoalList::SaveToFile(const std::wstring& path) const
{
    SerializedGoalList j;
    j.name = name;
    j.goals.reserve(goals.size());

    for (const auto& g : goals) {
        SerializedGoal jg;
        jg.label        = g.label;
        jg.trigger_type = TriggerTypeName(g.trigger.type);
        jg.map_id       = static_cast<int>(g.trigger.map_id);
        jg.level        = g.trigger.level;
        jg.title_id     = static_cast<uint32_t>(g.trigger.title_id);
        if (g.trigger.hard_mode) jg.hard_mode = true;
        if (g.trigger.param1)    jg.param1 = g.trigger.param1;
        if (g.trigger.param2)    jg.param2 = g.trigger.param2;
        if (!g.trigger.pattern.empty()) jg.pattern = EncodePattern(g.trigger.pattern);
        if (g.starts_immediately) jg.starts_immediately = true;
        if (g.auto_complete_previous != 0) jg.auto_complete_previous = g.auto_complete_previous;
        if (g.is_header)  jg.is_header = true;
        if (g.indent != 0) jg.indent   = g.indent;
        if (g.start_trigger.has_value()) jg.start_trigger = ToSerialized(g.start_trigger.value());
        if (!g.extra_triggers.empty()) {
            std::vector<SerializedTrigger> jextras;
            jextras.reserve(g.extra_triggers.size());
            for (const auto& et : g.extra_triggers) jextras.push_back(ToSerialized(et));
            jg.extra_triggers = std::move(jextras);
        }
        j.goals.push_back(std::move(jg));
    }

    if (reference.has_value() && !reference->splits.empty()) {
        SerializedReference jref;
        jref.name   = reference->name;
        if (!reference->date.empty()) jref.date = reference->date;
        jref.splits = reference->splits;
        j.reference = std::move(jref);
    }

    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << glz::write<glz::opts{.prettify = true}>(j).value_or(std::string{});
    return true;
}

bool GoalList::LoadFromFile(const std::wstring& path)
{
    std::ifstream f(path);
    if (!f.is_open()) return false;

    std::stringstream ss;
    ss << f.rdbuf();

    SerializedGoalList j;
    constexpr glz::opts opts{.error_on_unknown_keys = false};
    if (glz::read<opts>(j, ss.str())) return false;

    name = j.name;
    goals.clear();
    goals.reserve(j.goals.size());

    for (const auto& jg : j.goals) {
        GoalEntry g;
        g.label             = jg.label;
        g.trigger.type      = TriggerTypeFromString(jg.trigger_type);
        g.trigger.map_id    = static_cast<GW::Constants::MapID>(jg.map_id);
        g.trigger.level     = jg.level;
        g.trigger.title_id  = static_cast<GW::Constants::TitleID>(jg.title_id);
        g.trigger.hard_mode = jg.hard_mode.value_or(false);
        g.trigger.param1    = jg.param1.value_or(0u);
        g.trigger.param2    = jg.param2.value_or(0u);
        if (jg.pattern) g.trigger.pattern = DecodePattern(*jg.pattern);
        g.starts_immediately      = jg.starts_immediately.value_or(false);
        g.auto_complete_previous  = jg.auto_complete_previous.value_or(0);
        g.is_header               = jg.is_header.value_or(false);
        g.indent                  = jg.indent.value_or(0);
        if (jg.start_trigger) g.start_trigger = FromSerialized(*jg.start_trigger);
        if (jg.extra_triggers) {
            g.extra_triggers.reserve(jg.extra_triggers->size());
            for (const auto& je : *jg.extra_triggers) g.extra_triggers.push_back(FromSerialized(je));
        }
        goals.push_back(std::move(g));
    }

    if (j.reference.has_value()) {
        GoalReference ref;
        ref.name   = j.reference->name;
        ref.date   = j.reference->date.value_or("");
        ref.splits = j.reference->splits;
        reference  = std::move(ref);
    } else {
        reference.reset();
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
