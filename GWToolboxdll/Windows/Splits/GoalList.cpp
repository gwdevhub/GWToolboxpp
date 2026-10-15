#include "stdafx.h"

#include "GoalList.h"

#include <Modules/Resources.h>
#include <Utils/TextUtils.h>

#include <cctype>
#include <ranges>
#include <unordered_map>

// glaze reflection need external linkage.
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
        std::optional<SerializedTrigger> start_trigger;
        std::optional<std::vector<SerializedTrigger>> extra_start_triggers;
        std::optional<std::vector<SerializedTrigger>> extra_triggers;
        std::optional<int> auto_complete_previous;
        std::optional<bool> is_header;
        std::optional<int>  indent;
        std::optional<uint8_t> display_style;
    };

    struct SerializedReference {
        std::vector<double> splits;
    };

    struct SerializedGoalList {
        std::string name;
        std::vector<SerializedGoal> goals;
        std::optional<SerializedReference> reference;
        std::optional<bool> is_preset;
    };

    // Name only: file stem can differ once sanitised.
    struct SerializedListName {
        std::string name;
    };

    std::vector<uint16_t> EncodePattern(const std::wstring& pattern)
    {
        return {pattern.begin(), pattern.end()};
    }

    std::wstring DecodePattern(const std::vector<uint16_t>& pattern)
    {
        return {pattern.begin(), pattern.end()};
    }
}
using namespace GoalListJson;

namespace {
    // Manual absent on purpose: fallback both ways.
    struct TriggerTypeNameEntry { GoalTrigger::Type type; const char* name; };
    constexpr TriggerTypeNameEntry kTriggerTypeNames[] = {
        { GoalTrigger::Type::MapEnter,              "MapEnter" },
        { GoalTrigger::Type::EnterExplorable,       "EnterExplorable" },
        { GoalTrigger::Type::ExitExplorable,        "ExitExplorable" },
        { GoalTrigger::Type::VanquishComplete,      "VanquishComplete" },
        { GoalTrigger::Type::MissionComplete,       "MissionComplete" },
        { GoalTrigger::Type::MissionBonus,          "MissionBonus" },
        { GoalTrigger::Type::ReachLevel,            "ReachLevel" },
        { GoalTrigger::Type::ExitOutpost,           "ExitOutpost" },
        { GoalTrigger::Type::ReachTitleRank,        "ReachTitleRank" },
        { GoalTrigger::Type::ObjectiveDone,         "ObjectiveDone" },
        { GoalTrigger::Type::DoorOpen,              "DoorOpen" },
        { GoalTrigger::Type::DoorClose,             "DoorClose" },
        { GoalTrigger::Type::AgentUpdateAllegiance, "AgentUpdateAllegiance" },
        { GoalTrigger::Type::DoACompleteZone,       "DoACompleteZone" },
        { GoalTrigger::Type::DungeonReward,         "DungeonReward" },
        { GoalTrigger::Type::ServerMessage,         "ServerMessage" },
        { GoalTrigger::Type::DisplayDialogue,       "DisplayDialogue" },
        { GoalTrigger::Type::CountdownStart,        "CountdownStart" },
        { GoalTrigger::Type::ObjectiveStarted,      "ObjectiveStarted" },
        { GoalTrigger::Type::QuestPickup,           "QuestPickup" },
        { GoalTrigger::Type::QuestComplete,         "QuestComplete" },
        { GoalTrigger::Type::SkillLearnt,           "SkillLearnt" },
        { GoalTrigger::Type::MobKill,               "MobKill" },
        { GoalTrigger::Type::EnterOutpost,          "EnterOutpost" },
    };
}

static std::string TriggerTypeName(GoalTrigger::Type t)
{
    for (const auto& e : kTriggerTypeNames)
        if (e.type == t) return e.name;
    return "Manual";
}

static GoalTrigger::Type TriggerTypeFromString(const std::string& s)
{
    for (const auto& e : kTriggerTypeNames)
        if (s == e.name) return e.type;
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

void GoalList::ResetRunState()
{
    for (auto& g : goals) {
        g.status           = GoalStatus::NotStarted;
        g.split            = {};
        g.start_real_time  = -1.0;
        g.start_game_time  = -1.0;
        g.trigger_progress = 0;
    }
}

void GoalList::RenumberDuplicateLabels()
{
    // Strip old " (N)" first so renumber start clean.
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

bool GoalList::SaveToFile(const std::filesystem::path& path) const
{
    SerializedGoalList j;
    j.name = name;
    if (is_preset) j.is_preset = true;
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
        // starts_immediately never saved: only valid same tick preset built.
        if (g.auto_complete_previous != 0) jg.auto_complete_previous = g.auto_complete_previous;
        if (g.is_header)  jg.is_header = true;
        if (g.indent != 0) jg.indent   = g.indent;
        if (g.display_style != GoalEntry::DisplayStyle::Splits)
            jg.display_style = static_cast<uint8_t>(g.display_style);
        if (g.start_trigger.has_value()) jg.start_trigger = ToSerialized(g.start_trigger.value());
        if (!g.extra_start_triggers.empty())
            jg.extra_start_triggers = g.extra_start_triggers | std::views::transform(ToSerialized) | std::ranges::to<std::vector>();
        if (!g.extra_triggers.empty())
            jg.extra_triggers = g.extra_triggers | std::views::transform(ToSerialized) | std::ranges::to<std::vector>();
        j.goals.push_back(std::move(jg));
    }

    if (reference.has_value() && !reference->splits.empty()) {
        SerializedReference jref;
        jref.splits = reference->splits;
        j.reference = std::move(jref);
    }

    return Resources::WriteFile(path, glz::write<glz::opts{.prettify = true}>(j).value_or(std::string{}));
}

bool GoalList::LoadFromFile(const std::filesystem::path& path)
{
    std::string content;
    if (!Resources::ReadFile(path, content)) return false;

    SerializedGoalList j;
    constexpr glz::opts opts{.error_on_unknown_keys = false};
    if (glz::read<opts>(j, content)) {
        Log::ErrorW(L"Splits: failed to parse %s", path.wstring().c_str());
        return false;
    }

    name = j.name;
    is_preset = j.is_preset.value_or(false);
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
        g.auto_complete_previous  = jg.auto_complete_previous.value_or(0);
        g.is_header               = jg.is_header.value_or(false);
        g.indent                  = jg.indent.value_or(0);
        g.display_style           = static_cast<GoalEntry::DisplayStyle>(jg.display_style.value_or(0));
        if (jg.start_trigger) g.start_trigger = FromSerialized(*jg.start_trigger);
        if (jg.extra_start_triggers)
            g.extra_start_triggers = *jg.extra_start_triggers | std::views::transform(FromSerialized) | std::ranges::to<std::vector>();
        if (jg.extra_triggers)
            g.extra_triggers = *jg.extra_triggers | std::views::transform(FromSerialized) | std::ranges::to<std::vector>();
        goals.push_back(std::move(g));
    }

    if (j.reference.has_value()) {
        GoalReference ref;
        ref.splits = j.reference->splits;
        reference  = std::move(ref);
    } else {
        reference.reset();
    }

    return true;
}

std::wstring GoalList::FileStem(const std::string_view name)
{
    // UTF-8 overload: also handles CON/NUL and trailing dots/spaces.
    const std::string safe = TextUtils::SanitiseFilename(name);
    return safe.empty() ? L"_" : TextUtils::StringToWString(safe);
}

std::vector<std::pair<std::string, std::wstring>>
GoalList::ListSaved(const std::filesystem::path& folder)
{
    std::vector<std::pair<std::string, std::wstring>> result;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
        if (entry.path().extension() != L".json") continue;
        if (entry.path().stem() == L"resume")     continue;
        std::string name;
        if (std::string content; Resources::ReadFile(entry.path(), content)) {
            SerializedListName j;
            constexpr glz::opts opts{.error_on_unknown_keys = false};
            if (!glz::read<opts>(j, content)) name = std::move(j.name);
        }
        if (name.empty()) name = TextUtils::WStringToString(entry.path().stem().wstring());
        result.emplace_back(std::move(name), entry.path().wstring());
    }
    return result;
}
