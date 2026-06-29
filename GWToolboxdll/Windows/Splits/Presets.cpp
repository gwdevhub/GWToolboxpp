#include "stdafx.h"
#include "Presets.h"

#include <GWCA/Constants/AgentIDs.h>

using namespace GW::Constants;

// ---------------------------------------------------------------------------
// Constants (mirrored from ObjectiveTimerWindow.cpp)
// ---------------------------------------------------------------------------
namespace {

enum DoorID : uint32_t {
    Deep_room_1_first  = 12669,
    Deep_room_1_second = 11692,
    Deep_room_2_first  = 54552,
    Deep_room_2_second = 1760,
    Deep_room_3_first  = 45425,
    Deep_room_3_second = 48290,
    Deep_room_4_first  = 40330,
    Deep_room_4_second = 60114,
    Deep_room_5        = 29594,
    Deep_room_6        = 49742,
    Deep_room_7        = 55680,
    Deep_room_9        = 99887,
    Deep_room_11       = 28961,

    DoA_foundry_entrance_r1 = 39534,
    DoA_foundry_r1_r2       = 6356,
    DoA_foundry_r2_r3       = 45276,
    DoA_foundry_r3_r4       = 55421,
    DoA_foundry_r4_r5       = 49719,
    DoA_foundry_r5_bb       = 45667,
    DoA_foundry_behind_bb   = 1731,
    DoA_city_entrance       = 63939,
    DoA_city_wall           = 54727,
    DoA_city_jadoth         = 64556,
    DoA_veil_360_left       = 13005,
    DoA_veil_360_middle     = 11772,
    DoA_veil_360_right      = 28851,
    DoA_veil_derv           = 56510,
    DoA_veil_ranger         = 4753,
    DoA_veil_trench_necro   = 46650,
    DoA_veil_trench_mes     = 29594,
    DoA_veil_trench_ele     = 49742,
    DoA_veil_trench_monk    = 55680,
    DoA_veil_trench_gloom   = 28961,
    DoA_veil_to_gloom       = 3,
    DoA_gloom_to_foundry    = 17955,
    DoA_gloom_rift          = 47069,
};

enum DoA_ObjId : uint32_t {
    Foundry = 0x273F,
    Veil,   // 0x2740
    Gloom,  // 0x2741
    City    // 0x2742
};

// Kanaxai room dialogs (The Deep)
constexpr wchar_t kanaxai_dialog_r10[] = L"\x5339\xA7BA\xC67B\x5D81";
constexpr wchar_t kanaxai_dialog_r12[] = L"\x533A\xED06\x815D\x5FFB";
constexpr wchar_t kanaxai_dialog_r13[] = L"\x533B\xCAA6\xFDA9\x3277";
constexpr wchar_t kanaxai_dialog_r15[] = L"\x533D\x9EB1\x8BEE\x2637";

// ---------------------------------------------------------------------------
// Builder helpers
// ---------------------------------------------------------------------------

static GoalTrigger MakeTrigger(GoalTrigger::Type type, uint32_t param1 = 0, uint32_t param2 = 0)
{
    GoalTrigger t;
    t.type   = type;
    t.param1 = param1;
    t.param2 = param2;
    return t;
}

static GoalTrigger MakePatternTrigger(GoalTrigger::Type type, const wchar_t* pattern)
{
    GoalTrigger t;
    t.type    = type;
    t.pattern = pattern;
    return t;
}

// Build a GoalEntry with a single trigger.
static GoalEntry MakeGoal(const char* label, GoalTrigger trigger)
{
    GoalEntry g;
    g.label   = label;
    g.trigger = std::move(trigger);
    return g;
}

// Build a GoalEntry whose trigger fires on any of the supplied door IDs.
static GoalEntry MakeDoorGoal(const char* label, std::initializer_list<uint32_t> door_ids)
{
    GoalEntry g;
    g.label          = label;
    g.trigger        = MakeTrigger(GoalTrigger::Type::DoorOpen, *door_ids.begin());
    for (auto it = door_ids.begin() + 1; it != door_ids.end(); ++it)
        g.extra_triggers.push_back(MakeTrigger(GoalTrigger::Type::DoorOpen, *it));
    return g;
}

// Build a GoalEntry that fires on any of the supplied ServerMessage patterns.
static GoalEntry MakeServerMsgGoal(const char* label, std::initializer_list<const wchar_t*> patterns)
{
    GoalEntry g;
    g.label   = label;
    g.trigger = MakePatternTrigger(GoalTrigger::Type::ServerMessage, *patterns.begin());
    for (auto it = patterns.begin() + 1; it != patterns.end(); ++it)
        g.extra_triggers.push_back(MakePatternTrigger(GoalTrigger::Type::ServerMessage, *it));
    return g;
}

// ---------------------------------------------------------------------------
// Preset builders
// ---------------------------------------------------------------------------

GoalList BuildFoWPreset()
{
    GoalList list;
    list.is_preset = true;
    list.name = "Fissure of Woe";
    { GoalEntry hdr; hdr.is_header = true; hdr.label = "Fissure of Woe"; list.goals.push_back(std::move(hdr)); }
    auto addQ = [&](const char* label, uint32_t id) {
        GoalEntry g;
        g.label         = label;
        g.trigger       = MakeTrigger(GoalTrigger::Type::ObjectiveDone,    id);
        g.start_trigger = MakeTrigger(GoalTrigger::Type::ObjectiveStarted, id);
        list.goals.push_back(std::move(g));
    };
    addQ("ToC",            309);
    addQ("Wailing Lord",   310);
    addQ("Griffons",       311);
    addQ("Defend",         312);
    addQ("Forge",          313);
    addQ("Menzies",        314);
    addQ("Restore",        315);
    addQ("Khobay",         316);
    addQ("ToS",            317);
    addQ("Burning Forest", 318);
    addQ("The Hunt",       319);
    for (auto& g : list.goals) if (!g.is_header) g.indent = 1;
    return list;
}

GoalList BuildUWPreset()
{
    GoalList list;
    list.is_preset = true;
    list.name = "The Underworld";
    { GoalEntry hdr; hdr.is_header = true; hdr.label = "The Underworld"; list.goals.push_back(std::move(hdr)); }
    auto addQ = [&](const char* label, uint32_t id) {
        GoalEntry g;
        g.label         = label;
        g.trigger       = MakeTrigger(GoalTrigger::Type::ObjectiveDone,    id);
        g.start_trigger = MakeTrigger(GoalTrigger::Type::ObjectiveStarted, id);
        list.goals.push_back(std::move(g));
    };
    addQ("Chamber", 146);
    addQ("Restore",  147);
    addQ("Escort",   148);
    addQ("UWG",      149);
    addQ("Vale",     150);
    addQ("Waste",    151);
    addQ("Pits",     152);
    addQ("Planes",   153);
    addQ("Mnts",     154);
    addQ("Pools",    155);
    // Dhuum: allegiance fires when he activates, ObjectiveDone(157) on kill
    list.goals.push_back(MakeGoal("Dhuum", MakeTrigger(GoalTrigger::Type::ObjectiveDone, 157)));
    for (auto& g : list.goals) if (!g.is_header) g.indent = 1;
    return list;
}

GoalList BuildDeepPreset()
{
    GoalList list;
    list.is_preset = true;
    list.name = "The Deep";
    { GoalEntry hdr; hdr.is_header = true; hdr.label = "The Deep"; list.goals.push_back(std::move(hdr)); }

    // Rooms 1-4 run in parallel; each has 2 possible door IDs (spawn variant).
    // All start at t=0 simultaneously (same as OT's SetStarted() on construction).
    auto makeDoorGoalImmediate = [](const char* label, std::initializer_list<uint32_t> door_ids) {
        GoalEntry g = MakeDoorGoal(label, door_ids);
        g.starts_immediately = true;
        return g;
    };
    list.goals.push_back(makeDoorGoalImmediate("Room 1 | Soothing",  {Deep_room_1_first, Deep_room_1_second}));
    list.goals.push_back(makeDoorGoalImmediate("Room 2 | Death",     {Deep_room_2_first, Deep_room_2_second}));
    list.goals.push_back(makeDoorGoalImmediate("Room 3 | Surrender", {Deep_room_3_first, Deep_room_3_second}));
    list.goals.push_back(makeDoorGoalImmediate("Room 4 | Exposure",  {Deep_room_4_first, Deep_room_4_second}));

    // Rooms 5-7: sequential sections entered via single doors.
    list.goals.push_back(MakeGoal("Room 5 | Pain",     MakeTrigger(GoalTrigger::Type::DoorOpen, Deep_room_5)));
    list.goals.push_back(MakeGoal("Room 6 | Lethargy", MakeTrigger(GoalTrigger::Type::DoorOpen, Deep_room_6)));
    list.goals.push_back(MakeGoal("Room 7 | Depletion",MakeTrigger(GoalTrigger::Type::DoorOpen, Deep_room_7)));

    // Rooms 8-9 have no door boundary; split fires when Kanaxai's Room 10 dialog plays.
    list.goals.push_back(MakeGoal("Room 8-9 | Failure/Shadows",
        MakePatternTrigger(GoalTrigger::Type::DisplayDialogue, kanaxai_dialog_r10)));

    // Room 10 ends when Room 11's door opens.
    list.goals.push_back(MakeGoal("Room 10 | Scorpion",
        MakeTrigger(GoalTrigger::Type::DoorOpen, Deep_room_11)));

    // Remaining Kanaxai dialog-gated rooms.
    list.goals.push_back(MakeGoal("Room 11 | Fear",
        MakePatternTrigger(GoalTrigger::Type::DisplayDialogue, kanaxai_dialog_r12)));
    list.goals.push_back(MakeGoal("Room 12 | Depletion",
        MakePatternTrigger(GoalTrigger::Type::DisplayDialogue, kanaxai_dialog_r13)));
    list.goals.push_back(MakeGoal("Room 13-14 | Decay/Torment",
        MakePatternTrigger(GoalTrigger::Type::DisplayDialogue, kanaxai_dialog_r15)));

    // Kanaxai kill — EN and DE server messages both supported.
    list.goals.push_back(MakeServerMsgGoal("Room 15 | Kanaxai",
        {L"\x6D4D\x0\x0\x0\x0\x2810", L"\x6D4D\x0\x0\x0\x0\x1488"}));

    for (auto& g : list.goals) if (!g.is_header) g.indent = 1;
    return list;
}

GoalList BuildUrgozPreset()
{
    GoalList list;
    list.is_preset = true;
    list.name = "Urgoz's Warren";
    { GoalEntry hdr; hdr.is_header = true; hdr.label = "Urgoz's Warren"; list.goals.push_back(std::move(hdr)); }

    // Each zone's split fires when the door to the next zone opens.
    // Zone 1 starts at t=0 (SetStarted equivalent), all others start when their door opens.
    {
        GoalEntry g = MakeGoal("Zone 1 | Weakness", MakeTrigger(GoalTrigger::Type::DoorOpen, 45420));
        g.starts_immediately = true;
        list.goals.push_back(std::move(g));
    }
    list.goals.push_back(MakeGoal("Zone 2 | Life Drain",      MakeTrigger(GoalTrigger::Type::DoorOpen, 11692)));
    list.goals.push_back(MakeGoal("Zone 3 | Levers",          MakeTrigger(GoalTrigger::Type::DoorOpen, 54552)));
    list.goals.push_back(MakeGoal("Zone 4 | Bridge Wolves",   MakeTrigger(GoalTrigger::Type::DoorOpen, 1760)));
    list.goals.push_back(MakeGoal("Zone 5 | More Wolves",     MakeTrigger(GoalTrigger::Type::DoorOpen, 40330)));
    list.goals.push_back(MakeGoal("Zone 6 | Energy Drain",    MakeTrigger(GoalTrigger::Type::DoorOpen, 60114)));
    list.goals.push_back(MakeGoal("Zone 7 | Exhaustion",      MakeTrigger(GoalTrigger::Type::DoorOpen, 37191)));
    list.goals.push_back(MakeGoal("Zone 8 | Pillars",         MakeTrigger(GoalTrigger::Type::DoorOpen, 35500)));
    list.goals.push_back(MakeGoal("Zone 9 | Blood Drinkers",  MakeTrigger(GoalTrigger::Type::DoorOpen, 34278)));

    // Zone 10: three possible door IDs depending on final room spawn.
    list.goals.push_back(MakeDoorGoal("Zone 10 | Bridge", {15529, 45631, 53071}));

    // Urgoz kill — EN and DE server messages both supported.
    list.goals.push_back(MakeServerMsgGoal("Zone 11 | Urgoz",
        {L"\x6C9C\x0\x0\x0\x0\x2810", L"\x6C9C\x0\x0\x0\x0\x1488"}));

    for (auto& g : list.goals) if (!g.is_header) g.indent = 1;
    return list;
}

// Builds a DoA run starting from the given spawn point.
// Returns an empty list when the spawn is the Mallyx point (no preset).
// Sub-splits mirror ObjectiveTimerWindow's detailed objective breakdown.
GoalList BuildDoAPreset(GW::Vec2f spawn)
{
    constexpr GW::Vec2f mallyx_spawn{-3931.f, -6214.f};
    constexpr GW::Vec2f area_spawns[] = {
        {-10514.f, 15231.f}, // 0 = Foundry
        {-18575.f, -8833.f}, // 1 = City
        {364.f,   -10445.f}, // 2 = Veil
        {16034.f,   1244.f}, // 3 = Gloom
    };
    constexpr int n_areas = 4;

    double best_dist = GW::GetDistance(spawn, mallyx_spawn);
    int starting_area = -1;
    for (int i = 0; i < n_areas; ++i) {
        const double d = GW::GetDistance(spawn, area_spawns[i]);
        if (d < best_dist) { best_dist = d; starting_area = i; }
    }

    GoalList list;
    if (starting_area == -1) return list; // Mallyx run — no preset

    list.is_preset = true;
    list.name = "Domain of Anguish";
    { GoalEntry hdr; hdr.is_header = true; hdr.label = "Domain of Anguish"; list.goals.push_back(std::move(hdr)); }

    using T = GoalTrigger::Type;

    // Dialog prefixes (4 wchar_t each, prefix-matched by GoalEngine)
    constexpr wchar_t fury_dialog[]       = L"\x8101\x273D\x98DB\xB91A";
    constexpr wchar_t tendrils_dialog[]   = L"\x8101\x34C1\x9FA1\xED8F";
    constexpr wchar_t cave_start_dialog[] = L"\x8101\x5765\x9846\xA72B";
    constexpr wchar_t cave_end_dialog[]   = L"\x8101\x5767\xA547\xB2C2";
    constexpr wchar_t dark_start_dialog[] = L"\x8101\x273B\xB5DB\x8B13";

    // Helper: goal with both start and end triggers
    auto makeGoalSE = [](const char* label, GoalTrigger trig, GoalTrigger start) {
        GoalEntry g;
        g.label        = label;
        g.trigger      = std::move(trig);
        g.start_trigger = std::move(start);
        return g;
    };

    auto addFoundry = [&]() {
        // Room 1-4: door close (enter) to door open (exit)
        list.goals.push_back(makeGoalSE("Foundry | Room 1",
            MakeTrigger(T::DoorOpen,  DoA_foundry_r1_r2),
            MakeTrigger(T::DoorClose, DoA_foundry_entrance_r1)));
        list.goals.push_back(makeGoalSE("Foundry | Room 2",
            MakeTrigger(T::DoorOpen,  DoA_foundry_r2_r3),
            MakeTrigger(T::DoorClose, DoA_foundry_r1_r2)));
        list.goals.push_back(makeGoalSE("Foundry | Room 3",
            MakeTrigger(T::DoorOpen,  DoA_foundry_r3_r4),
            MakeTrigger(T::DoorClose, DoA_foundry_r2_r3)));
        list.goals.push_back(makeGoalSE("Foundry | Room 4",
            MakeTrigger(T::DoorOpen,  DoA_foundry_r4_r5),
            MakeTrigger(T::DoorClose, DoA_foundry_r3_r4)));
        // Black Beast: start when BB door opens, end on allegiance update (all 3 snakes same id)
        list.goals.push_back(makeGoalSE("Foundry | Black Beast",
            MakeTrigger(T::AgentUpdateAllegiance, 5221, 0x6E6F6E63),
            MakeTrigger(T::DoorOpen, DoA_foundry_r5_bb)));
        // Fury: start on dialog, end on area complete
        list.goals.push_back(makeGoalSE("Foundry | Fury",
            MakeTrigger(T::DoACompleteZone, Foundry),
            MakePatternTrigger(T::DisplayDialogue, fury_dialog)));
    };

    auto addCity = [&]() {
        list.goals.push_back(makeGoalSE("City | Outside",
            MakeTrigger(T::DoorOpen, DoA_city_wall),
            MakeTrigger(T::DoorOpen, DoA_city_entrance)));
        list.goals.push_back(makeGoalSE("City | Inside",
            MakeTrigger(T::DoACompleteZone, City),
            MakeTrigger(T::DoorOpen, DoA_city_wall)));
    };

    auto addVeil = [&]() {
        // Only Tendrils has a proper end event; 360/Underlords/Lords are start-only in OT
        list.goals.push_back(makeGoalSE("Veil | Tendrils",
            MakeTrigger(T::DoACompleteZone, Veil),
            MakePatternTrigger(T::DisplayDialogue, tendrils_dialog)));
    };

    auto addGloom = [&]() {
        list.goals.push_back(makeGoalSE("Gloom | Cave",
            MakePatternTrigger(T::DisplayDialogue, cave_end_dialog),
            MakePatternTrigger(T::DisplayDialogue, cave_start_dialog)));
        list.goals.push_back(makeGoalSE("Gloom | Darknesses",
            MakeTrigger(T::DoACompleteZone, Gloom),
            MakePatternTrigger(T::DisplayDialogue, dark_start_dialog)));
    };

    const std::function<void()> area_builders[] = {addFoundry, addCity, addVeil, addGloom};
    for (int i = 0; i < n_areas; ++i) {
        const size_t before = list.goals.size();
        area_builders[(starting_area + i) % n_areas]();
        // Mirrors OT's AddObjectiveAfterAll: entering a new area auto-completes any
        // not-yet-done goals from earlier areas (e.g. skipped sub-objectives).
        if (list.goals.size() > before)
            list.goals[before].auto_complete_previous = -1;
    }
    for (auto& g : list.goals) if (!g.is_header) g.indent = 1;
    return list;
}

// Builds a generic dungeon preset.
// level_map_ids must be the ordered list of level MapIDs (e.g. Level_1, Level_2, ...).
// Each intermediate level fires on MapEnter of the next level; the final level fires on DungeonReward.
GoalList BuildDungeonPreset(const char* name, const std::vector<MapID>& levels)
{
    GoalList list;
    list.is_preset = true;
    list.name = name;
    { GoalEntry hdr; hdr.is_header = true; hdr.label = name; list.goals.push_back(std::move(hdr)); }
    const int n = static_cast<int>(levels.size());
    for (int i = 0; i < n; ++i) {
        char label[32];
        if (n == 1)
            snprintf(label, sizeof(label), "Complete");
        else
            snprintf(label, sizeof(label), "Level %d", i + 1);

        if (i < n - 1) {
            // Intermediate level: fires when the next level loads.
            list.goals.push_back(MakeGoal(label,
                MakeTrigger(GoalTrigger::Type::MapEnter,
                            static_cast<uint32_t>(levels[static_cast<size_t>(i + 1)]))));
            list.goals.back().trigger.map_id = levels[static_cast<size_t>(i + 1)];
        } else {
            // Final level: fires on dungeon chest reward.
            list.goals.push_back(MakeGoal(label,
                MakeTrigger(GoalTrigger::Type::DungeonReward)));
        }
    }
    // First non-header level starts at t=0 (mirrors OT's objectives.front()->SetStarted()).
    for (auto& g : list.goals) if (!g.is_header) { g.starts_immediately = true; break; }
    for (auto& g : list.goals) if (!g.is_header) g.indent = 1;
    return list;
}

static GoalList BuildToPKPreset()
{
    // Each goal fires when the GvG countdown begins on that specific arena map.
    // The countdown fires once the match is won, signalling progression.
    GoalList list;
    list.is_preset = true;
    list.name = "Tomb of the Primeval Kings";
    { GoalEntry hdr; hdr.is_header = true; hdr.label = "Tomb of the Primeval Kings"; list.goals.push_back(std::move(hdr)); }
    auto add = [&](const char* label, MapID map_id) {
        GoalEntry g;
        g.label          = label;
        g.trigger.type   = GoalTrigger::Type::CountdownStart;
        g.trigger.param1 = static_cast<uint32_t>(map_id);
        list.goals.push_back(std::move(g));
    };
    add("Underworld PvP",  MapID::The_Underworld_PvP);
    add("Scarred Earth",   MapID::Scarred_Earth);
    add("The Courtyard",   MapID::The_Courtyard);
    add("Hall of Heroes",  MapID::The_Hall_of_Heroes);
    for (auto& g : list.goals) if (!g.is_header) g.indent = 1;
    return list;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

std::optional<GoalList> Presets::GetPresetForMap(MapID map_id, GW::Vec2f spawn)
{
    using namespace GW::Constants;

    switch (map_id) {
        // Elite areas
        case MapID::The_Fissure_of_Woe:  return BuildFoWPreset();
        case MapID::The_Underworld:      return BuildUWPreset();
        case MapID::The_Deep:            return BuildDeepPreset();
        case MapID::Urgozs_Warren:       return BuildUrgozPreset();
        case MapID::The_Underworld_PvP:  return BuildToPKPreset();
        case MapID::Domain_of_Anguish: {
            auto gl = BuildDoAPreset(spawn);
            if (gl.goals.empty()) return std::nullopt;
            return gl;
        }

        // Dungeons — 1 level
        case MapID::Ooze_Pit:
            return BuildDungeonPreset("Ooze Pit", {MapID::Ooze_Pit});
        case MapID::Fronis_Irontoes_Lair_mission:
            return BuildDungeonPreset("Fronis Irontoe's Lair", {MapID::Fronis_Irontoes_Lair_mission});
        case MapID::Secret_Lair_of_the_Snowmen:
            return BuildDungeonPreset("Secret Lair of the Snowmen", {MapID::Secret_Lair_of_the_Snowmen});
        case MapID::Slavers_Exile_Level_5:
            return BuildDungeonPreset("Slaver's Exile (Duncan)", {MapID::Slavers_Exile_Level_5});

        // Dungeons — 2 levels
        case MapID::Sepulchre_of_Dragrimmar_Level_1:
            return BuildDungeonPreset("Sepulchre of Dragrimmar",
                {MapID::Sepulchre_of_Dragrimmar_Level_1, MapID::Sepulchre_of_Dragrimmar_Level_2});
        case MapID::Bogroot_Growths_Level_1:
            return BuildDungeonPreset("Bogroot Growths",
                {MapID::Bogroot_Growths_Level_1, MapID::Bogroot_Growths_Level_2});
        case MapID::Arachnis_Haunt_Level_1:
            return BuildDungeonPreset("Arachni's Haunt",
                {MapID::Arachnis_Haunt_Level_1, MapID::Arachnis_Haunt_Level_2});

        // Dungeons — 3 levels
        case MapID::Catacombs_of_Kathandrax_Level_1:
            return BuildDungeonPreset("Catacombs of Kathandrax",
                {MapID::Catacombs_of_Kathandrax_Level_1,
                 MapID::Catacombs_of_Kathandrax_Level_2,
                 MapID::Catacombs_of_Kathandrax_Level_3});
        case MapID::Rragars_Menagerie_Level_1:
            return BuildDungeonPreset("Rragar's Menagerie",
                {MapID::Rragars_Menagerie_Level_1,
                 MapID::Rragars_Menagerie_Level_2,
                 MapID::Rragars_Menagerie_Level_3});
        case MapID::Cathedral_of_Flames_Level_1:
            return BuildDungeonPreset("Cathedral of Flames",
                {MapID::Cathedral_of_Flames_Level_1,
                 MapID::Cathedral_of_Flames_Level_2,
                 MapID::Catacombs_of_Kathandrax_Level_3}); // level 3 reuses Kathandrax map
        case MapID::Darkrime_Delves_Level_1:
            return BuildDungeonPreset("Darkrime Delves",
                {MapID::Darkrime_Delves_Level_1,
                 MapID::Darkrime_Delves_Level_2,
                 MapID::Darkrime_Delves_Level_3});
        case MapID::Ravens_Point_Level_1:
            return BuildDungeonPreset("Raven's Point",
                {MapID::Ravens_Point_Level_1,
                 MapID::Ravens_Point_Level_2,
                 MapID::Ravens_Point_Level_3});
        case MapID::Vloxen_Excavations_Level_1:
            return BuildDungeonPreset("Vloxen Excavations",
                {MapID::Vloxen_Excavations_Level_1,
                 MapID::Vloxen_Excavations_Level_2,
                 MapID::Vloxen_Excavations_Level_3});
        case MapID::Bloodstone_Caves_Level_1:
            return BuildDungeonPreset("Bloodstone Caves",
                {MapID::Bloodstone_Caves_Level_1,
                 MapID::Bloodstone_Caves_Level_2,
                 MapID::Bloodstone_Caves_Level_3});
        case MapID::Shards_of_Orr_Level_1:
            return BuildDungeonPreset("Shards of Orr",
                {MapID::Shards_of_Orr_Level_1,
                 MapID::Shards_of_Orr_Level_2,
                 MapID::Shards_of_Orr_Level_3});
        case MapID::Oolas_Lab_Level_1:
            return BuildDungeonPreset("Oola's Lab",
                {MapID::Oolas_Lab_Level_1,
                 MapID::Oolas_Lab_Level_2,
                 MapID::Oolas_Lab_Level_3});
        case MapID::Heart_of_the_Shiverpeaks_Level_1:
            return BuildDungeonPreset("Heart of the Shiverpeaks",
                {MapID::Heart_of_the_Shiverpeaks_Level_1,
                 MapID::Heart_of_the_Shiverpeaks_Level_2,
                 MapID::Heart_of_the_Shiverpeaks_Level_3});

        // Dungeons — 5 levels
        case MapID::Frostmaws_Burrows_Level_1:
            return BuildDungeonPreset("Frostmaw's Burrows",
                {MapID::Frostmaws_Burrows_Level_1,
                 MapID::Frostmaws_Burrows_Level_2,
                 MapID::Frostmaws_Burrows_Level_3,
                 MapID::Frostmaws_Burrows_Level_4,
                 MapID::Frostmaws_Burrows_Level_5});

        default: return std::nullopt;
    }
}
