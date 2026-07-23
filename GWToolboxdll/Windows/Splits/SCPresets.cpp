#include "stdafx.h"
#include "SCPresets.h"

#include <Modules/Resources.h>

namespace SCPresets {

using T = GoalTrigger::Type;
using MapID = GW::Constants::MapID;

const EliteCheckpoint kFow[11] = {
    { .name = "ToC",            .type = T::ObjectiveDone, .param1 = 309, .start_on_objective_started = true },
    { .name = "Wailing Lord",   .type = T::ObjectiveDone, .param1 = 310, .start_on_objective_started = true },
    { .name = "Griffons",       .type = T::ObjectiveDone, .param1 = 311, .start_on_objective_started = true },
    { .name = "Defend",         .type = T::ObjectiveDone, .param1 = 312, .start_on_objective_started = true },
    { .name = "Forge",          .type = T::ObjectiveDone, .param1 = 313, .start_on_objective_started = true },
    { .name = "Menzies",        .type = T::ObjectiveDone, .param1 = 314, .start_on_objective_started = true },
    { .name = "Restore",        .type = T::ObjectiveDone, .param1 = 315, .start_on_objective_started = true },
    { .name = "Khobay",         .type = T::ObjectiveDone, .param1 = 316, .start_on_objective_started = true },
    { .name = "ToS",            .type = T::ObjectiveDone, .param1 = 317, .start_on_objective_started = true },
    { .name = "Burning Forest", .type = T::ObjectiveDone, .param1 = 318, .start_on_objective_started = true },
    { .name = "The Hunt",       .type = T::ObjectiveDone, .param1 = 319, .start_on_objective_started = true },
};

const EliteCheckpoint kUw[11] = {
    { .name = "Chamber", .type = T::ObjectiveDone, .param1 = 146, .start_on_objective_started = true },
    { .name = "Restore", .type = T::ObjectiveDone, .param1 = 147, .start_on_objective_started = true },
    { .name = "Escort",  .type = T::ObjectiveDone, .param1 = 148, .start_on_objective_started = true },
    { .name = "UWG",     .type = T::ObjectiveDone, .param1 = 149, .start_on_objective_started = true },
    { .name = "Vale",    .type = T::ObjectiveDone, .param1 = 150, .start_on_objective_started = true },
    { .name = "Waste",   .type = T::ObjectiveDone, .param1 = 151, .start_on_objective_started = true },
    { .name = "Pits",    .type = T::ObjectiveDone, .param1 = 152, .start_on_objective_started = true },
    { .name = "Planes",  .type = T::ObjectiveDone, .param1 = 153, .start_on_objective_started = true },
    { .name = "Mnts",    .type = T::ObjectiveDone, .param1 = 154, .start_on_objective_started = true },
    { .name = "Pools",   .type = T::ObjectiveDone, .param1 = 155, .start_on_objective_started = true },
    { .name = "Dhuum",   .type = T::ObjectiveDone, .param1 = 157 },
};

const EliteCheckpoint kUrgoz[11] = {
    { .name = "Zone 1 | Weakness",       .type = T::DoorOpen, .param1 = 45420, .starts_on_area_entry = true },
    { .name = "Zone 2 | Life Drain",     .type = T::DoorOpen, .param1 = 11692 },
    { .name = "Zone 3 | Levers",         .type = T::DoorOpen, .param1 = 54552 },
    { .name = "Zone 4 | Bridge Wolves",  .type = T::DoorOpen, .param1 = 1760  },
    { .name = "Zone 5 | More Wolves",    .type = T::DoorOpen, .param1 = 40330 },
    { .name = "Zone 6 | Energy Drain",   .type = T::DoorOpen, .param1 = 60114 },
    { .name = "Zone 7 | Exhaustion",     .type = T::DoorOpen, .param1 = 37191 },
    { .name = "Zone 8 | Pillars",        .type = T::DoorOpen, .param1 = 35500 },
    { .name = "Zone 9 | Blood Drinkers", .type = T::DoorOpen, .param1 = 34278 },
    { .name = "Zone 10 | Bridge",        .type = T::DoorOpen, .param1 = 15529,
      .extra_param1_a = 45631, .extra_param1_b = 53071 },
    { .name = "Zone 11 | Urgoz",         .type = T::ServerMessage,
      .pattern = L"\x6C9C\x0\x0\x0\x0\x2810", .extra_pattern_a = L"\x6C9C\x0\x0\x0\x0\x1488" },
};

const EliteCheckpoint kDeep[13] = {
    { .name = "Room 1 | Soothing",          .type = T::DoorOpen, .param1 = 12669, .extra_param1_a = 11692, .starts_on_area_entry = true },
    { .name = "Room 2 | Death",             .type = T::DoorOpen, .param1 = 54552, .extra_param1_a = 1760,  .starts_on_area_entry = true },
    { .name = "Room 3 | Surrender",         .type = T::DoorOpen, .param1 = 45425, .extra_param1_a = 48290, .starts_on_area_entry = true },
    { .name = "Room 4 | Exposure",          .type = T::DoorOpen, .param1 = 40330, .extra_param1_a = 60114, .starts_on_area_entry = true },
    { .name = "Room 5 | Pain",              .type = T::DoorOpen, .param1 = 29594 },
    { .name = "Room 6 | Lethargy",          .type = T::DoorOpen, .param1 = 49742 },
    { .name = "Room 7 | Depletion",         .type = T::DoorOpen, .param1 = 55680 },
    { .name = "Room 8-9 | Failure/Shadows", .type = T::DisplayDialogue, .pattern = L"\x5339\xA7BA\xC67B\x5D81" },
    { .name = "Room 10 | Scorpion",         .type = T::DoorOpen, .param1 = 28961 },
    { .name = "Room 11 | Fear",             .type = T::DisplayDialogue, .pattern = L"\x533A\xED06\x815D\x5FFB" },
    { .name = "Room 12 | Depletion",        .type = T::DisplayDialogue, .pattern = L"\x533B\xCAA6\xFDA9\x3277" },
    { .name = "Room 13-14 | Decay/Torment", .type = T::DisplayDialogue, .pattern = L"\x533D\x9EB1\x8BEE\x2637" },
    { .name = "Room 15 | Kanaxai",          .type = T::ServerMessage,
      .pattern = L"\x6D4D\x0\x0\x0\x0\x2810", .extra_pattern_a = L"\x6D4D\x0\x0\x0\x0\x1488" },
};

const EliteArea kEliteAreas[4] = {
    { "Fissure of Woe", kFow,   std::size(kFow),   MapID::The_Fissure_of_Woe },
    { "Underworld",     kUw,    std::size(kUw),    MapID::The_Underworld     },
    { "Urgoz's Warren", kUrgoz, std::size(kUrgoz), MapID::Urgozs_Warren      },
    { "The Deep",       kDeep,  std::size(kDeep),  MapID::The_Deep           },
};

// clang-format off
static const MapID kOozePitLevels[]        = { MapID::Ooze_Pit };
static const MapID kFronisLevels[]         = { MapID::Fronis_Irontoes_Lair_mission };
static const MapID kSnowmenLevels[]        = { MapID::Secret_Lair_of_the_Snowmen };
static const MapID kSepulchreLevels[]      = { MapID::Sepulchre_of_Dragrimmar_Level_1, MapID::Sepulchre_of_Dragrimmar_Level_2 };
static const MapID kBogrootLevels[]        = { MapID::Bogroot_Growths_Level_1, MapID::Bogroot_Growths_Level_2 };
static const MapID kArachniLevels[]        = { MapID::Arachnis_Haunt_Level_1, MapID::Arachnis_Haunt_Level_2 };
static const MapID kCatacombsLevels[]      = { MapID::Catacombs_of_Kathandrax_Level_1, MapID::Catacombs_of_Kathandrax_Level_2, MapID::Catacombs_of_Kathandrax_Level_3 };
static const MapID kRragarsLevels[]        = { MapID::Rragars_Menagerie_Level_1, MapID::Rragars_Menagerie_Level_2, MapID::Rragars_Menagerie_Level_3 };
static const MapID kCathedralLevels[]      = { MapID::Cathedral_of_Flames_Level_1, MapID::Cathedral_of_Flames_Level_2, MapID::Cathedral_of_Flames_Level_3 };
static const MapID kDarkrimeLevels[]       = { MapID::Darkrime_Delves_Level_1, MapID::Darkrime_Delves_Level_2, MapID::Darkrime_Delves_Level_3 };
static const MapID kRavensPointLevels[]    = { MapID::Ravens_Point_Level_1, MapID::Ravens_Point_Level_2, MapID::Ravens_Point_Level_3 };
static const MapID kVloxenLevels[]         = { MapID::Vloxen_Excavations_Level_1, MapID::Vloxen_Excavations_Level_2, MapID::Vloxen_Excavations_Level_3 };
static const MapID kBloodstoneLevels[]     = { MapID::Bloodstone_Caves_Level_1, MapID::Bloodstone_Caves_Level_2, MapID::Bloodstone_Caves_Level_3 };
static const MapID kShardsOfOrrLevels[]    = { MapID::Shards_of_Orr_Level_1, MapID::Shards_of_Orr_Level_2, MapID::Shards_of_Orr_Level_3 };
static const MapID kOolasLabLevels[]       = { MapID::Oolas_Lab_Level_1, MapID::Oolas_Lab_Level_2, MapID::Oolas_Lab_Level_3 };
static const MapID kHeartShiverLevels[]    = { MapID::Heart_of_the_Shiverpeaks_Level_1, MapID::Heart_of_the_Shiverpeaks_Level_2, MapID::Heart_of_the_Shiverpeaks_Level_3 };
static const MapID kForsakenLevels[]       = { MapID::Forsaken_Tunnels_Level1, MapID::Forsaken_Tunnels_Level2, MapID::Forsaken_Tunnels_Level3 };
static const MapID kForsakenPreLevels[]    = { MapID::Forsaken_Tunnels_Presearing_Level1, MapID::Forsaken_Tunnels_Presearing_Level2, MapID::Forsaken_Tunnels_Presearing_Level3 };
static const MapID kFrostmawsLevels[]      = { MapID::Frostmaws_Burrows_Level_1, MapID::Frostmaws_Burrows_Level_2, MapID::Frostmaws_Burrows_Level_3, MapID::Frostmaws_Burrows_Level_4, MapID::Frostmaws_Burrows_Level_5 };
static const MapID kSlaversExileLevels[]   = { MapID::Slavers_Exile_Level_5 };
// clang-format on

const Dungeon kDungeons[20] = {
    { kOozePitLevels,     std::size(kOozePitLevels) },
    { kFronisLevels,      std::size(kFronisLevels) },
    { kSnowmenLevels,     std::size(kSnowmenLevels) },
    { kSepulchreLevels,   std::size(kSepulchreLevels) },
    { kBogrootLevels,     std::size(kBogrootLevels) },
    { kArachniLevels,     std::size(kArachniLevels) },
    { kCatacombsLevels,   std::size(kCatacombsLevels) },
    { kRragarsLevels,     std::size(kRragarsLevels) },
    { kCathedralLevels,   std::size(kCathedralLevels) },
    { kDarkrimeLevels,    std::size(kDarkrimeLevels) },
    { kRavensPointLevels, std::size(kRavensPointLevels) },
    { kVloxenLevels,      std::size(kVloxenLevels) },
    { kBloodstoneLevels,  std::size(kBloodstoneLevels) },
    { kShardsOfOrrLevels, std::size(kShardsOfOrrLevels) },
    { kOolasLabLevels,    std::size(kOolasLabLevels) },
    { kHeartShiverLevels, std::size(kHeartShiverLevels) },
    { kForsakenLevels,    std::size(kForsakenLevels) },
    { kForsakenPreLevels, std::size(kForsakenPreLevels) },
    { kFrostmawsLevels,   std::size(kFrostmawsLevels) },
    { kSlaversExileLevels, std::size(kSlaversExileLevels) },
};

GoalEntry BuildCheckpointGoal(const EliteCheckpoint& c, GW::Constants::MapID area_map_id)
{
    GoalEntry g;
    g.label          = c.name;
    g.trigger.type   = c.type;
    g.trigger.param1 = c.param1;
    if (c.pattern) g.trigger.pattern = c.pattern;
    if (c.extra_param1_a) {
        GoalTrigger alt; alt.type = c.type; alt.param1 = c.extra_param1_a;
        g.extra_triggers.push_back(alt);
    }
    if (c.extra_param1_b) {
        GoalTrigger alt; alt.type = c.type; alt.param1 = c.extra_param1_b;
        g.extra_triggers.push_back(alt);
    }
    if (c.extra_pattern_a) {
        GoalTrigger alt; alt.type = c.type; alt.pattern = c.extra_pattern_a;
        g.extra_triggers.push_back(alt);
    }
    // Real map-entry gated start (not an unconditional "starts_immediately") — see the
    // struct's doc comment for why that distinction actually matters here.
    if (c.starts_on_area_entry) {
        g.start_trigger = GoalTrigger{};
        g.start_trigger->type   = GoalTrigger::Type::MapEnter;
        g.start_trigger->map_id = area_map_id;
    }
    if (c.start_on_objective_started) {
        g.start_trigger = GoalTrigger{};
        g.start_trigger->type   = GoalTrigger::Type::ObjectiveStarted;
        g.start_trigger->param1 = c.param1;
    }
    return g;
}

GoalList BuildDungeonPresetList(const Dungeon& dungeon)
{
    GoalList list;
    list.is_preset = true;
    list.name      = Resources::GetMapName(dungeon.levels[0])->string();

    if (dungeon.level_count <= 1) {
        // Nothing to break down — same flat single goal as Manual's picker.
        GoalEntry g;
        g.label          = list.name;
        g.trigger.type   = GoalTrigger::Type::DungeonReward;
        g.trigger.map_id = dungeon.levels[0];
        list.goals.push_back(std::move(g));
        return list;
    }

    GoalEntry hdr;
    hdr.is_header      = true;
    hdr.label          = list.name;
    hdr.trigger.map_id = dungeon.levels[0]; // read by ApplyTimerPolicy's autostart/autofail, not the engine
    list.goals.push_back(std::move(hdr));

    for (size_t i = 0; i < dungeon.level_count; ++i) {
        GoalEntry g;
        char label[32];
        snprintf(label, sizeof(label), "Level %zu", i + 1);
        g.label   = label;
        g.indent  = 1;
        if (i + 1 < dungeon.level_count) {
            // This level's completion = the next level's own entry (matches OT's
            // AddObjectiveAfterAll chaining: starting the next objective auto-completes
            // every one before it).
            g.trigger.type   = GoalTrigger::Type::MapEnter;
            g.trigger.map_id = dungeon.levels[i + 1];
        } else {
            // Final level: the actual dungeon chest. map_id is this level's own map (not a
            // "next" map like the MapEnter branch above) — matchesPendingTrigger() ignores it
            // for Pass 2 completion, but GoalEngine's auto-fail-on-rezone check needs it to know
            // which map this goal is active on, same as Mission/Bonus/VQ goals.
            g.trigger.type   = GoalTrigger::Type::DungeonReward;
            g.trigger.map_id = dungeon.levels[i];
        }
        if (i == 0) {
            // Real map-entry gated start, not an unconditional one — matches OT's
            // objectives.front()->SetStarted(), which itself only ever runs once the
            // ObjectiveSet has actually been created by loading into this dungeon.
            g.start_trigger = GoalTrigger{};
            g.start_trigger->type   = GoalTrigger::Type::MapEnter;
            g.start_trigger->map_id = dungeon.levels[0];
        }
        list.goals.push_back(std::move(g));
    }
    return list;
}

GoalList BuildEliteAreaPresetList(const EliteArea& area)
{
    GoalList list;
    list.is_preset = true;
    list.name      = area.label;

    GoalEntry hdr;
    hdr.is_header      = true;
    hdr.label          = area.label;
    hdr.trigger.map_id = area.map_id; // read by ApplyTimerPolicy's autostart/autofail, not the engine
    list.goals.push_back(std::move(hdr));

    for (size_t i = 0; i < area.count; ++i) {
        GoalEntry g = BuildCheckpointGoal(area.checkpoints[i], area.map_id);
        g.indent    = 1;
        list.goals.push_back(std::move(g));
    }
    return list;
}

std::optional<GoalList> BuildPresetForMap(GW::Constants::MapID map_id)
{
    for (const auto& dungeon : kDungeons) {
        for (size_t i = 0; i < dungeon.level_count; ++i) {
            if (dungeon.levels[i] == map_id) return BuildDungeonPresetList(dungeon);
        }
    }
    for (const auto& area : kEliteAreas) {
        if (area.map_id == map_id) return BuildEliteAreaPresetList(area);
    }
    for (const auto level : kToPKLevels) {
        if (level == map_id) return BuildToPKPresetList();
    }
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// Domain of Anguish
// ---------------------------------------------------------------------------
// All zone-word/door/dialogue constants below are copied verbatim from
// ObjectiveTimerWindow.cpp's own DoA_ObjId/DoorID enums and AddDoAObjectiveSet — not
// independently re-derived. Unlike the elite areas above, every row here gets its own real
// explicit start_trigger matching OT's own AddStartEvent calls 1:1 (using the new
// extra_start_triggers for OT's OR'd/multi-door starts) rather than relying on relay — DoA's
// zone order rotates per run, and relying on "starts when the previous row completes" would
// misdate a zone's actual start to whenever the *previous* zone finished, which can be a real
// time gap (return to the hub, walk to the next entrance) rather than the same instant.
namespace {
    // DoACompleteZone's param1 ("zone message word") — OT's DoA_ObjId enum.
    constexpr uint32_t kDoAFoundry = 0x273F;
    constexpr uint32_t kDoAVeil    = 0x2740;
    constexpr uint32_t kDoAGloom   = 0x2741;
    constexpr uint32_t kDoACity    = 0x2742;

    constexpr uint32_t kDoAFoundryEntranceR1 = 39534;
    constexpr uint32_t kDoAFoundryR1R2       = 6356;
    constexpr uint32_t kDoAFoundryR2R3       = 45276;
    constexpr uint32_t kDoAFoundryR3R4       = 55421;
    constexpr uint32_t kDoAFoundryR4R5       = 49719;
    constexpr uint32_t kDoAFoundryR5Bb       = 45667;
    constexpr uint32_t kDoACityEntrance      = 63939;
    constexpr uint32_t kDoACityWall          = 54727;
    constexpr uint32_t kDoAVeil360Left       = 13005;
    constexpr uint32_t kDoAVeil360Middle     = 11772;
    constexpr uint32_t kDoAVeil360Right      = 28851;
    constexpr uint32_t kDoAVeilDerv          = 56510;
    constexpr uint32_t kDoAVeilRanger        = 4753;
    constexpr uint32_t kDoAVeilTrenchNecro   = 46650;
    constexpr uint32_t kDoAVeilTrenchMes     = 29594;
    constexpr uint32_t kDoAVeilTrenchEle     = 49742;
    constexpr uint32_t kDoAVeilTrenchMonk    = 55680;
    constexpr uint32_t kDoAVeilTrenchGloom   = 28961;

    constexpr uint32_t kDoABlackBeastModelId = 5221;
    constexpr uint32_t kDoAAllyFlag          = 0x6E6F6E63;

    constexpr wchar_t kDoAFuryDialogue[]       = L"\x8101\x273D\x98DB\xB91A";
    constexpr wchar_t kDoACaveStartDialogue[]  = L"\x8101\x5765\x9846\xA72B";
    constexpr wchar_t kDoACaveEndDialogue[]    = L"\x8101\x5767\xA547\xB2C2";
    constexpr wchar_t kDoADarknessesDialogue[] = L"\x8101\x273B\xB5DB\x8B13";
    constexpr wchar_t kDoATendrilsDialogue[]   = L"\x8101\x34C1\x9FA1\xED8F\x1BE4";

    // Nearest-neighbor rotation detection — matches AddDoAObjectiveSet's own starting_area
    // lambda exactly. Domain of Anguish's map/file_id is shared with the separate solo "Mallyx
    // the Unfathomable" challenge; if the Mallyx spawn is the closest of all 5 candidates,
    // this isn't a DoA run at all.
    constexpr GW::Vec2f kDoAMallyxSpawn(-3931, -6214);
    constexpr GW::Vec2f kDoAAreaSpawns[4] = {
        {-10514, 15231}, // Foundry
        {-18575, -8833}, // City
        {364,   -10445}, // Veil
        {16034,  1244},  // Gloom
    };

    int DetectDoAStartingZone(const GW::Vec2f spawn)
    {
        double best_dist     = GW::GetDistance(spawn, kDoAMallyxSpawn);
        int    starting_area = -1;
        for (int i = 0; i < 4; ++i) {
            const float dist = GW::GetDistance(spawn, kDoAAreaSpawns[i]);
            if (best_dist > dist) {
                best_dist     = dist;
                starting_area = i;
            }
        }
        return starting_area; // -1 = Mallyx, not DoA
    }

    // Builds one DoA sub-objective with its own real start/end, matching OT's per-objective
    // AddStartEvent/AddEndEvent pairs directly. start_extra/end_extra are alternate doors (OR
    // semantics via extra_start_triggers/extra_triggers) for OT's multi-door starts/ends.
    GoalEntry MakeDoAGoal(const char* label,
                          GoalTrigger::Type start_type, uint32_t start_param1,
                          std::initializer_list<uint32_t> start_extra,
                          const wchar_t* start_pattern,
                          GoalTrigger::Type end_type, uint32_t end_param1,
                          std::initializer_list<uint32_t> end_extra = {},
                          const wchar_t* end_pattern = nullptr, uint32_t end_param2 = 0)
    {
        GoalEntry g;
        g.label          = label;
        g.indent         = 1;
        g.trigger.type   = end_type;
        g.trigger.param1 = end_param1;
        g.trigger.param2 = end_param2;
        if (end_pattern) g.trigger.pattern = end_pattern;
        for (const uint32_t extra : end_extra) {
            GoalTrigger alt;
            alt.type   = end_type;
            alt.param1 = extra;
            g.extra_triggers.push_back(alt);
        }
        g.start_trigger        = GoalTrigger{};
        g.start_trigger->type  = start_type;
        g.start_trigger->param1 = start_param1;
        if (start_pattern) g.start_trigger->pattern = start_pattern;
        for (const uint32_t extra : start_extra) {
            GoalTrigger alt;
            alt.type   = start_type;
            alt.param1 = extra;
            g.extra_start_triggers.push_back(alt);
        }
        return g;
    }

    std::vector<GoalEntry> BuildDoAFoundryChildren()
    {
        std::vector<GoalEntry> goals;
        goals.push_back(MakeDoAGoal("Room 1", T::DoorClose, kDoAFoundryEntranceR1, {}, nullptr,
                                     T::DoorOpen, kDoAFoundryR1R2));
        goals.push_back(MakeDoAGoal("Room 2", T::DoorClose, kDoAFoundryR1R2, {}, nullptr,
                                     T::DoorOpen, kDoAFoundryR2R3));
        goals.push_back(MakeDoAGoal("Room 3", T::DoorClose, kDoAFoundryR2R3, {}, nullptr,
                                     T::DoorOpen, kDoAFoundryR3R4));
        goals.push_back(MakeDoAGoal("Room 4", T::DoorClose, kDoAFoundryR3R4, {}, nullptr,
                                     T::DoorOpen, kDoAFoundryR4R5));
        goals.push_back(MakeDoAGoal("Black Beast", T::DoorOpen, kDoAFoundryR5Bb, {}, nullptr,
                                     T::AgentUpdateAllegiance, kDoABlackBeastModelId, {}, nullptr, kDoAAllyFlag));
        goals.push_back(MakeDoAGoal("Fury", T::DisplayDialogue, 0, {}, kDoAFuryDialogue,
                                     T::DoACompleteZone, kDoAFoundry));
        return goals;
    }

    std::vector<GoalEntry> BuildDoACityChildren()
    {
        std::vector<GoalEntry> goals;
        goals.push_back(MakeDoAGoal("Outside", T::DoorOpen, kDoACityEntrance, {}, nullptr,
                                     T::DoorOpen, kDoACityWall));
        goals.push_back(MakeDoAGoal("Inside", T::DoorOpen, kDoACityWall, {}, nullptr,
                                     T::DoACompleteZone, kDoACity));
        return goals;
    }

    std::vector<GoalEntry> BuildDoAVeilChildren()
    {
        std::vector<GoalEntry> goals;
        // "360"/"Underlords"/"Lords" have no explicit end event in OT's own code (informational
        // start-only tracking) — each one's completion here borrows the next row's own real
        // start condition, landing on the same real-world moment rather than never completing.
        goals.push_back(MakeDoAGoal("360", T::DoorOpen, kDoAVeil360Left,
                                     {kDoAVeil360Middle, kDoAVeil360Right}, nullptr,
                                     T::DoorOpen, kDoAVeilRanger, {kDoAVeilDerv}));
        goals.push_back(MakeDoAGoal("Underlords", T::DoorOpen, kDoAVeilRanger, {kDoAVeilDerv}, nullptr,
                                     T::DoorOpen, kDoAVeilTrenchGloom,
                                     {kDoAVeilTrenchMonk, kDoAVeilTrenchEle, kDoAVeilTrenchMes, kDoAVeilTrenchNecro}));
        goals.push_back(MakeDoAGoal("Lords", T::DoorOpen, kDoAVeilTrenchGloom,
                                     {kDoAVeilTrenchMonk, kDoAVeilTrenchEle, kDoAVeilTrenchMes, kDoAVeilTrenchNecro},
                                     nullptr,
                                     T::DisplayDialogue, 0, {}, kDoATendrilsDialogue));
        goals.push_back(MakeDoAGoal("Tendrils", T::DisplayDialogue, 0, {}, kDoATendrilsDialogue,
                                     T::DoACompleteZone, kDoAVeil));
        return goals;
    }

    std::vector<GoalEntry> BuildDoAGloomChildren()
    {
        std::vector<GoalEntry> goals;
        goals.push_back(MakeDoAGoal("Cave", T::DisplayDialogue, 0, {}, kDoACaveStartDialogue,
                                     T::DisplayDialogue, 0, {}, kDoACaveEndDialogue));
        goals.push_back(MakeDoAGoal("Darknesses", T::DisplayDialogue, 0, {}, kDoADarknessesDialogue,
                                     T::DoACompleteZone, kDoAGloom));
        return goals;
    }
} // namespace

std::optional<GoalList> BuildDoAPresetList(const GW::Vec2f spawn)
{
    const int starting_zone = DetectDoAStartingZone(spawn);
    if (starting_zone == -1) return std::nullopt; // Mallyx, not DoA

    struct ZoneBlock {
        const char* label;
        std::vector<GoalEntry> (*build)();
    };
    static const ZoneBlock kZones[4] = {
        {"Foundry", BuildDoAFoundryChildren},
        {"City",    BuildDoACityChildren},
        {"Veil",    BuildDoAVeilChildren},
        {"Gloom",   BuildDoAGloomChildren},
    };

    GoalList list;
    list.is_preset = true;
    list.name      = Resources::GetMapName(GW::Constants::MapID::Domain_of_Anguish)->string();

    GoalEntry hdr;
    hdr.is_header      = true;
    hdr.label          = list.name;
    hdr.trigger.map_id = GW::Constants::MapID::Domain_of_Anguish;
    list.goals.push_back(std::move(hdr));

    for (int i = 0; i < 4; ++i) {
        const ZoneBlock& zone = kZones[(starting_zone + i) % 4];

        GoalEntry zone_hdr;
        zone_hdr.is_header      = true;
        zone_hdr.label          = zone.label;
        zone_hdr.trigger.map_id = GW::Constants::MapID::Domain_of_Anguish;
        list.goals.push_back(std::move(zone_hdr));

        for (auto& g : zone.build()) list.goals.push_back(std::move(g));
    }
    return list;
}

// ---------------------------------------------------------------------------
// Tomb of the Primeval Kings
// ---------------------------------------------------------------------------
const MapID kToPKLevels[4] = {
    MapID::The_Underworld_PvP,
    MapID::Scarred_Earth,
    MapID::The_Courtyard,
    MapID::The_Hall_of_Heroes,
};

GoalList BuildToPKPresetList()
{
    GoalList list;
    list.is_preset = true;
    list.name      = Resources::GetMapName(MapID::Tomb_of_the_Primeval_Kings)->string();

    GoalEntry hdr;
    hdr.is_header      = true;
    hdr.label          = list.name;
    hdr.trigger.map_id = kToPKLevels[0]; // entry map, read by ApplyTimerPolicy's autostart fallback
    list.goals.push_back(std::move(hdr));

    for (size_t i = 0; i < 4; ++i) {
        GoalEntry g;
        g.label          = Resources::GetMapName(kToPKLevels[i])->string();
        g.indent         = 1;
        g.trigger.type   = T::CountdownStart;
        g.trigger.param1 = static_cast<uint32_t>(kToPKLevels[i]); // matched via matchesPendingTrigger
        g.trigger.map_id = kToPKLevels[i]; // read by GoalEngine's auto-fail rezone check, not Pass 2
        g.start_trigger  = GoalTrigger{};
        // Only the entry map is explorable-ambiguous (shared with a non-ToPK outpost use
        // elsewhere) — the other 3 are only ever reached mid-run, so plain MapEnter is
        // unambiguous there, matching OT's own gate (only the outer AddObjectiveSet(map_id)
        // switch checks instance type, only for The_Underworld_PvP).
        g.start_trigger->type   = (i == 0) ? T::EnterExplorable : T::MapEnter;
        g.start_trigger->map_id = kToPKLevels[i];
        list.goals.push_back(std::move(g));
    }
    return list;
}

} // namespace SCPresets
