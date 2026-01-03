#include "stdafx.h"

#include <GWCA/Constants/Maps.h>
#include <GWCA/Constants/Skills.h>

#include <GWCA/Utilities/MemoryPatcher.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/GameEntities/Quest.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Pathing.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Context/MapContext.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/QuestMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/PlayerMgr.h>

#include <Widgets/WorldMapWidget.h>
#include <Widgets/Minimap/Minimap.h>
#include <Modules/GwDatTextureModule.h>
#include <Modules/Resources.h>
#include <Windows/CompletionWindow.h>
#include <Windows/TravelWindow.h>

#include <Utils/GuiUtils.h>
#include <Utils/ToolboxUtils.h>

#include "Defines.h"

#include <ImGuiAddons.h>
#include <Modules/QuestModule.h>
#include <GWCA/Managers/AgentMgr.h>
#include <corecrt_math_defines.h>
#include <Utils/ArenaNetFileParser.h>
#include <Utils/TextUtils.h>


namespace {

    struct EliteBossLocation {
        GW::Constants::SkillID skill_id;
        uint32_t region_id;
        const char* boss_name;
        GW::Constants::MapID map_id;
        std::vector<GW::Vec2f> coords;
        const char* note = nullptr;
    };

    const ImColor completed_bg = IM_COL32(0, 0x99, 0, 192);
    const ImColor completed_text = IM_COL32(0xE5, 0xFF, 0xCC, 255);

    ImRect controls_window_rect = {0, 0, 0, 0};

    IDirect3DTexture9** quest_icon_texture = nullptr;
    IDirect3DTexture9** player_icon_texture = nullptr;

    bool showing_all_outposts = false;
    bool apply_quest_colors = false;
    bool show_any_elite_capture_locations = false;
    bool show_elite_capture_locations[11];
    bool hide_captured_elites = false;
    bool drawn = false;
    bool show_lines_on_world_map = false;
    bool showing_all_quests = true;

    GW::MemoryPatcher view_all_outposts_patch;
    GW::MemoryPatcher view_all_carto_areas_patch;

    bool world_map_clicking = false;
    GW::Vec2f world_map_click_pos;

    GW::Constants::QuestID hovered_quest_id = GW::Constants::QuestID::None;
    GuiUtils::EncString hovered_quest_name;
    GuiUtils::EncString hovered_quest_description;
    const EliteBossLocation* hovered_boss = nullptr;

    // Cached vars that are updated every draw; avoids having to do the calculation inside DrawQuestMarkerOnWorldMap
    GW::Vec2f player_world_map_pos;
    float player_rotation = .0f;
    GW::Vec2f viewport_offset;
    GW::Vec2f ui_scale;
    float world_map_scale = 1.f;
    GW::WorldMapContext* world_map_context = nullptr;
    float quest_star_rotation_angle = .0f;
    float quest_icon_size = 24.f;
    float quest_icon_size_half = 12.f;
    ImDrawList* draw_list = nullptr;
    

    
const EliteBossLocation elite_boss_locations[] = {
    {GW::Constants::SkillID::Prepared_Shot, 01, "Johon the Oxflinger", GW::Constants::MapID::Ice_Cliff_Chasms, {{1464, 1179}}},
    {GW::Constants::SkillID::Water_Trident, 01, "Elsnil Frigidheart", GW::Constants::MapID::Ice_Cliff_Chasms, {{1454, 1062}}},
    {GW::Constants::SkillID::Skull_Crack, 01, "Baglorag Grumblesnort", GW::Constants::MapID::Norrhart_Domains, {{1111, 785}}, "Patrols the area around the nearby stones."},
    {GW::Constants::SkillID::Crippling_Slash, 01, "Fenrir", GW::Constants::MapID::Norrhart_Domains, {{1205, 904}}},
    {GW::Constants::SkillID::Cleave, 01, "Dvalinn Stonebreaker", GW::Constants::MapID::Drakkar_Lake, {{714, 617}}, "Only before completing \"In the Service of Revenge\" quest."},
    {GW::Constants::SkillID::Skull_Crack, 01, "Lann Browsmasher", GW::Constants::MapID::Drakkar_Lake, {{758, 548}, {869, 679}, {783, 455}}, "Only during \"Nornhood\" quest. In one of the three possible locations."},
    {GW::Constants::SkillID::Skull_Crack, 01, "Chieftain Ulgrimar", GW::Constants::MapID::Drakkar_Lake, {{932, 525}}, "Only during \"The Big Unfriendly Jotun\" quest."},
    {GW::Constants::SkillID::Symbols_of_Inspiration, 01, "Silver Vaettir", GW::Constants::MapID::Drakkar_Lake, {{900, 520}}, "Only if all conditions are met:\n- Favor of the gods active\n- No other party members\n- \"Light of Deldrimor\" in skillbar\n- \"Lucky Aura\" active\n- \"Birthday Cupcake\" active"},
    {GW::Constants::SkillID::Earth_Shaker, 01, "Jormungand", GW::Constants::MapID::Drakkar_Lake, {{811, 719}}, "Rare spawn."},
    {GW::Constants::SkillID::Crippling_Shot, 01, "Koren Wildrunner", GW::Constants::MapID::Drakkar_Lake, {{853, 704}}, "Patrols around the lake via various routes."},
    {GW::Constants::SkillID::Lyssas_Aura, 01, "Nulfastu Earthbound", GW::Constants::MapID::Drakkar_Lake, {{921, 800}}},
    {GW::Constants::SkillID::Onslaught, 01, "Myish, Lady of the Lake", GW::Constants::MapID::Drakkar_Lake, {{810, 700}}},
    {GW::Constants::SkillID::Life_Transfer, 01, "Tanto the Grim", GW::Constants::MapID::Varajar_Fells, {{960, 1026}}, "Only during \"Haunted\" quest."},
    {GW::Constants::SkillID::Pious_Renewal, 01, "Facet of Spirit", GW::Constants::MapID::Varajar_Fells, {{948, 1026}}, "Only during \"The Path to Revelations\" quest."},
    {GW::Constants::SkillID::Gladiators_Defense, 01, "Facet of Destruction", GW::Constants::MapID::Varajar_Fells, {{936, 1026}}, "Only during \"The Path to Revelations\" quest."},
    {GW::Constants::SkillID::Shield_of_Regeneration, 01, "Facet of Existence", GW::Constants::MapID::Varajar_Fells, {{924, 1026}}, "Only during \"The Path to Revelations\" quest."},
    {GW::Constants::SkillID::Grenths_Balance, 01, "Facet of Death", GW::Constants::MapID::Varajar_Fells, {{912, 1026}}, "Only during \"The Path to Revelations\" quest."},
    {GW::Constants::SkillID::Mantra_of_Recall, 01, "Facet of Illusions", GW::Constants::MapID::Varajar_Fells, {{900, 1026}}, "Only during \"The Path to Revelations\" quest."},
    {GW::Constants::SkillID::Melandrus_Resilience, 01, "Facet of Creation", GW::Constants::MapID::Varajar_Fells, {{888, 1026}}, "Only during \"The Path to Revelations\" quest."},
    {GW::Constants::SkillID::Mind_Freeze, 01, "Dazehl Brainfreezer", GW::Constants::MapID::Varajar_Fells, {{604, 988}}, "Only spawns if the northwestern side of the lake has been cleared of Ice Imps and Frozen Elementals.\nClearing out the cave is not necessary."},
    {GW::Constants::SkillID::Reapers_Sweep, 01, "Asterius the Mighty", GW::Constants::MapID::Varajar_Fells, {{699, 1012}}, "Patrols around the southwest section of Varajar Fells."},
    {GW::Constants::SkillID::Earth_Shaker, 01, "Jormungand", GW::Constants::MapID::Varajar_Fells, {{777, 1016}}, "Rare spawn."},
    {GW::Constants::SkillID::Wounding_Strike, 01, "Taameh the Frigid", GW::Constants::MapID::Bjora_Marches, {{1384, 511}}},
    {GW::Constants::SkillID::Mind_Freeze, 01, "Lord Glacius the Eternal", GW::Constants::MapID::Bjora_Marches, {{1523, 390}}, "Can only be encountered if the Stone Summit dwarves at the map location are allowed to complete their ritual and summon him."},
    {GW::Constants::SkillID::Eviscerate, 01, "Tenagg Flametroller", GW::Constants::MapID::Blood_Washes_Blood_mission, {{1514, 343}}, "Only during \"Blood Washes Blood\" mission."},
    {GW::Constants::SkillID::Barrage, 01, "Docu Kindleshot", GW::Constants::MapID::Blood_Washes_Blood_mission, {{1504, 353}}, "Only during \"Blood Washes Blood\" mission."},
    {GW::Constants::SkillID::Mind_Blast, 01, "Kakei Stormcaller", GW::Constants::MapID::Blood_Washes_Blood_mission, {{1494, 346}}, "Only during \"Blood Washes Blood\" mission."},
    {GW::Constants::SkillID::Eviscerate, 01, "Lissah the Packleader", GW::Constants::MapID::Bjora_Marches, {{1500, 598}}},
    {GW::Constants::SkillID::Devastating_Hammer, 01, "Krak Flamewhip", GW::Constants::MapID::Jaga_Moraine, {{1123, 382}}, "Only during \"Krak's Cavalry\" quest."},
    {GW::Constants::SkillID::Jagged_Bones, 01, "Avarr the Fallen", GW::Constants::MapID::Jaga_Moraine, {{1033, 66}}},
    {GW::Constants::SkillID::Shockwave, 01, "Ssissth the Leviathan", GW::Constants::MapID::Jaga_Moraine, {{1220, 68}}, "Only before completing \"Prenuptial Disagreement\" quest."},
    {GW::Constants::SkillID::Mind_Shock, 01, "Edielh Shockhunter", GW::Constants::MapID::Jaga_Moraine, {{1232, 68}}, "Only after \"Prenuptial Disagreement\" quest."},
    {GW::Constants::SkillID::Shatterstone, 01, "Whiteout", GW::Constants::MapID::Jaga_Moraine, {{1112, 84}}},
    {GW::Constants::SkillID::Healing_Light, 01, "Elmohr Snowmender", GW::Constants::MapID::Blood_Washes_Blood_mission, {{1631, 448}}, "Only during \"Blood Washes Blood\" mission."},
    {GW::Constants::SkillID::Glass_Arrows, 01, "Petraj the Evasive", GW::Constants::MapID::Verdant_Cascades, {{74, 1382}}},
    {GW::Constants::SkillID::Withdraw_Hexes, 01, "Kaykitt Hidemender", GW::Constants::MapID::Verdant_Cascades, {{286, 1394}}},
    {GW::Constants::SkillID::Migraine, 01, "Wilderm Wrathspew", GW::Constants::MapID::Verdant_Cascades, {{318, 1281}}},
    {GW::Constants::SkillID::Gladiators_Defense, 01, "Facet of Destruction", GW::Constants::MapID::Verdant_Cascades, {{319, 1380}, {375, 1329}, {177, 1367}}, "Can spawn in one of the three possible locations."},
    {GW::Constants::SkillID::Mist_Form, 02, "Drikard the Foggy", GW::Constants::MapID::Dalada_Uplands, {{625, 345}}},
    {GW::Constants::SkillID::Shield_of_Regeneration, 02, "Horai Wingshielder", GW::Constants::MapID::Dalada_Uplands, {{686, 145}}},
    {GW::Constants::SkillID::Headbutt, 02, "Molotov Rocktail", GW::Constants::MapID::Dalada_Uplands, {{663, 234}}},
    {GW::Constants::SkillID::Backbreaker, 02, "Hodat the Tumbler", GW::Constants::MapID::Dalada_Uplands, {{860, 223}}, "Only after \"Forgotten Relics\" quest."},
    {GW::Constants::SkillID::Life_Sheath, 02, "Chaelse Flameshielder", GW::Constants::MapID::Grothmar_Wardowns, {{454, 351}}, "Does not spawn between the completion of \"Against the Charr\" and \"The Dawn of Rebellion\", or between the completion of \"Be Very, Very Quiet...\" and \"Plan A\"."},
    {GW::Constants::SkillID::Ride_the_Lightning, 02, "Thraexis Thundermaw", GW::Constants::MapID::Grothmar_Wardowns, {{335, 312}}},
    {GW::Constants::SkillID::Barrage, 02, "Groknar Weazlewortz", GW::Constants::MapID::Against_the_Charr_mission, {{350, 270}}, "Only during \"Against the Charr\" mission."},
    {GW::Constants::SkillID::Eviscerate, 02, "Gordam Griefgiver", GW::Constants::MapID::Against_the_Charr_mission, {{428, 322}}, "Only during \"Against the Charr\" mission."},
    {GW::Constants::SkillID::Spiteful_Spirit, 02, "Harvest Soulreign", GW::Constants::MapID::Against_the_Charr_mission, {{440, 322}}, "Only during \"Against the Charr\" mission.\nHard to capture since the mission ends shortly after defeating him."},
    {GW::Constants::SkillID::Ward_Against_Harm, 02, "Frazar Frostfur", GW::Constants::MapID::Against_the_Charr_mission, {{450, 425}}, "Only during \"Against the Charr\" mission."},
    {GW::Constants::SkillID::Preservation, 02, "Fozzy Yeoryios", GW::Constants::MapID::Sacnoth_Valley, {{773, 688}}},
    {GW::Constants::SkillID::Ward_Against_Harm, 02, "Spafrod Iceblood", GW::Constants::MapID::Sacnoth_Valley, {{662, 499}}},
    {GW::Constants::SkillID::Searing_Flames, 02, "Borrguus Blisterbark", GW::Constants::MapID::Sacnoth_Valley, {{830, 671}}, "Patrols all around the forest"},
    {GW::Constants::SkillID::Power_Block, 02, "Anmat the Trickster", GW::Constants::MapID::Sacnoth_Valley, {{680, 504}}},
    {GW::Constants::SkillID::Power_Block, 02, "Anmat the Trickster", GW::Constants::MapID::Against_the_Charr_mission, {{404, 283}}, "Only during \"Against the Charr\" mission."},
    {GW::Constants::SkillID::Reapers_Mark, 02, "Rend Ragemauler", GW::Constants::MapID::Sacnoth_Valley, {{809, 461}}, "Only during \"The Assassin's Revenge\" quest."},
    {GW::Constants::SkillID::Reapers_Mark, 02, "Vyrrgis the Pestilent", GW::Constants::MapID::Sacnoth_Valley, {{669, 524}}, "Only during \"Single Ugly Grawl Seeks Same for Mindless Destruction in Ascalon\" quest."},
    {GW::Constants::SkillID::Eviscerate, 02, "Gharaz the Glutton", GW::Constants::MapID::Sacnoth_Valley, {{643, 517}}, "Only during \"Forbidden Fruit\" quest."},
    {GW::Constants::SkillID::Life_Transfer, 02, "Katye Bloodburner", GW::Constants::MapID::Sacnoth_Valley, {{642, 499}}},
    {GW::Constants::SkillID::Forceful_Blow, 02, "Ignus the Eternal", GW::Constants::MapID::Sacnoth_Valley, {{838, 685}}, "Only during \"The Smell of Titan in the Morning\" quest."},
    {GW::Constants::SkillID::Eviscerate, 02, "Shons the Pretender", GW::Constants::MapID::Sacnoth_Valley, {{753, 466}}},
    {GW::Constants::SkillID::Battle_Rage, 02, "Cobleri Arronn", GW::Constants::MapID::Sacnoth_Valley, {{670, 669}}},
    {GW::Constants::SkillID::Together_as_one, 02, "Fureyst Sharpsight", GW::Constants::MapID::Sacnoth_Valley, {{640, 700}}, "Only if a player has a \"Proof of Triumph\" in their inventory."},
    {GW::Constants::SkillID::Time_Ward, 02, "Fureyst Sharpsight", GW::Constants::MapID::Sacnoth_Valley, {{660, 700}}, "Only if a player has a \"Proof of Triumph\" in their inventory."},
    {GW::Constants::SkillID::Ether_Prodigy, 02, "Hierophant Burntsoul", GW::Constants::MapID::Assault_on_the_Stronghold_mission, {{655, 484}}, "Only during \"Assault on the Stronghold\" mission."},
    {GW::Constants::SkillID::Ether_Prodigy, 02, "Straut Flamebourne", GW::Constants::MapID::Sacnoth_Valley, {{839, 645}}, "Only during \"Fire and Pain\" quest."},
    {GW::Constants::SkillID::Spirit_Light_Weapon, 03, "Reesou the Wanderer", GW::Constants::MapID::Alcazia_Tangle, {{612, 458}}},
    {GW::Constants::SkillID::Golden_Skull_Strike, 03, "Pywatt the Swift", GW::Constants::MapID::Alcazia_Tangle, {{467, 537}}},
    {GW::Constants::SkillID::Ineptitude, 03, "Kemil the Inept", GW::Constants::MapID::Alcazia_Tangle, {{654, 602}}},
    {GW::Constants::SkillID::Melandrus_Resilience, 03, "Facet of Creation", GW::Constants::MapID::Alcazia_Tangle, {{740, 502}}, "Only during \"Cipher of Melandru\" quest.\nKnown spawn locations:\n- Northeast, west of the river\n- Northwest of center, in the patch of dirt surrounded by stones west of Reesou the Wanderer and north of a pond\n- West of center, at the top of the cliff\n- Northwest, near the exit into Magus Stones"},
    {GW::Constants::SkillID::Word_of_Healing, 03, "Flannuss Broadwing", GW::Constants::MapID::Arbor_Bay, {{1199, 212}}, "Route is approximately 5 minutes in length, varying in direction and path per spawn. Generally patrols around the edge of the northeastern lagoon."},
    {GW::Constants::SkillID::Crippling_Anguish, 03, "Alitta Guilebloom", GW::Constants::MapID::Arbor_Bay, {{1086, 315}}, "Patrols all around this area, southern of the central island."},
    {GW::Constants::SkillID::Pious_Renewal, 03, "Facet of Spirit", GW::Constants::MapID::Arbor_Bay, {{1188, 171}}, "Only during \"Cipher of Kormir\" quest.\nKnown spawn locations:\n- In the waters directly south of Vlox's Falls\n- Northeast of center, at the top of the hill northwest of the the northeastern lagoon\n- In the waters southwest of Ventari's Sanctuary\n- In the waters directly east of Ventari's Sanctuary"},
    {GW::Constants::SkillID::Dragon_Slash, 03, "Byndliss Flamecrown", GW::Constants::MapID::Magus_Stones, {{144, 442}}},
    {GW::Constants::SkillID::Steady_Stance, 03, "Skomay Bonehewer", GW::Constants::MapID::Magus_Stones, {{111, 215}}},
    {GW::Constants::SkillID::Enraged_Lunge, 03, "Ashlyn Spiderfriend", GW::Constants::MapID::Magus_Stones, {{101, 487}}},
    {GW::Constants::SkillID::Plague_Signet, 03, "Bredyss Longstride", GW::Constants::MapID::Magus_Stones, {{267, 377}}},
    {GW::Constants::SkillID::Crippling_Anguish, 03, "Veturni Mindsquall", GW::Constants::MapID::Magus_Stones, {{155, 255}}},
    {GW::Constants::SkillID::Mantra_of_Recall, 03, "Facet of Illusions", GW::Constants::MapID::Magus_Stones, {{258, 262}}, "Only during \"Cipher of Lyssa\" quest.\nKnown spawn locations:\n- In the pond west of Rata Sum\n- Northwest from the center, between the large clearing and the waterfall\n- South of center between the three hills.\n- Far northwest corner"},
    {GW::Constants::SkillID::Stunning_Strike, 03, "Timberland Guardian", GW::Constants::MapID::Magus_Stones, {{50, 240}}, "Only during \"Lab Space\" quest."},
    {GW::Constants::SkillID::Skull_Crack, 03, "Bringer of Destruction", GW::Constants::MapID::Genius_Operated_Living_Enchanted_Manifestation_mission, {{604, 67}}, "Only during \"Genius Operated Living Enchanted Manifestation\" mission."},
    {GW::Constants::SkillID::Barrage, 03, "Storm of Destruction", GW::Constants::MapID::Riven_Earth, {{592, 67}}},
    {GW::Constants::SkillID::Crippling_Shot, 03, "Gallow Nooseknitter", GW::Constants::MapID::Riven_Earth, {{791, 137}}},
    {GW::Constants::SkillID::Shield_of_Regeneration, 03, "Facet of Existence", GW::Constants::MapID::Riven_Earth, {{674, 171}}, "Only during \"Cipher of Dwanya\" quest.\nKnown spawn locations:\n- Northwest of the Golem Foundry\n- Directly east from Rata Sum, before the second resurrection shrine\n- Up the hill slightly southwest of the center resurrection shrine\n- Northeast of center, northeast of the short bridge north of center, between the northern and northeastern resurrection shrines\n- Southeast corner, on the northern side of the waterfall.\nAs the first spawn point is so close to Rata Sum, it may be worth reloading the map until you get this spawn. It can be reached without combat"},
    {GW::Constants::SkillID::Shockwave, 03, "Joffs the Mitigator", GW::Constants::MapID::Riven_Earth, {{613, 231}}},
    {GW::Constants::SkillID::Assassins_Promise, 03, "Rekoff Broodmother", GW::Constants::MapID::Riven_Earth, {{475, 280}}},
    {GW::Constants::SkillID::Crippling_Slash, 03, "Mobrin, Lord of the Marsh", GW::Constants::MapID::Sparkfly_Swamp, {{4250, 6560}}},
    {GW::Constants::SkillID::Devastating_Hammer, 03, "Inscribed Ettin", GW::Constants::MapID::Sparkfly_Swamp, {{4218, 6625}}, "Only during \"Finding the Bloodstone\" quest."},
    {GW::Constants::SkillID::Grenths_Balance, 03, "Facet of Death", GW::Constants::MapID::Sparkfly_Swamp, {{4140, 6600}}, "Only during \"Cipher of Grenth\" quest.\nKnown spawn locations:\n- Eastern edge of the central clearing, up the hill south of the easternmost edge of the swamp.\n- Southeast of center, at the three way crossroads north of the resurrection shrine\n- Northeast of center, at the three way crossroads between the resurrection shrine at the eastern edge of the swamp and Brynn Earthporter's alcove\n- In the center of the Agari territory"},
    {GW::Constants::SkillID::Migraine, 03, "Waray Skullflayer", GW::Constants::MapID::Sparkfly_Swamp, {{4100, 6437}}, "Will not spawn if you have \"Frogstomp\" quest active."},
    {GW::Constants::SkillID::Ebon_Dust_Aura, 03, "Brynn Earthporter", GW::Constants::MapID::Sparkfly_Swamp, {{4375, 6310}}},
    {GW::Constants::SkillID::Ravenous_Gaze, 01, "Moa'vu'Kaal", GW::Constants::MapID::Issnur_Isles, {{1059, 798}}, "Only during \"Moa'vu'Kaal, Awakened\" quest."},
    {GW::Constants::SkillID::Icy_Shackles, 01, "Lonolun Waterwalker", GW::Constants::MapID::Issnur_Isles, {{950, 890}}},
    {GW::Constants::SkillID::Headbutt, 02, "Enadiz the Hardheaded", GW::Constants::MapID::Arkjok_Ward, {{658, 1046}}},
    {GW::Constants::SkillID::Rage_of_the_Ntouka, 02, "Onwan, Lord of the Ntouka", GW::Constants::MapID::Arkjok_Ward, {{705, 878}}},
    {GW::Constants::SkillID::Barrage, 02, "Commander Kubeh", GW::Constants::MapID::Arkjok_Ward, {{824, 1045}}, "Only during \"The Great Escape\" quest."},
    {GW::Constants::SkillID::Depravity, 02, "Modti Darkflower", GW::Constants::MapID::Arkjok_Ward, {{753, 1051}}},
    {GW::Constants::SkillID::Enchanters_Conundrum, 02, "Captain Chichor", GW::Constants::MapID::Arkjok_Ward, {{812, 1005}}, "Not during \"The Great Escape\" quest.\nPatrols the three islands counterclockwise."},
    {GW::Constants::SkillID::Lightning_Surge, 02, "Zephyr Hedger", GW::Constants::MapID::Arkjok_Ward, {{640, 921}}, "Only during \"Koss's Elixir\" quest."},
    {GW::Constants::SkillID::Destructive_Was_Glaive, 02, "Lieutenant Kayin", GW::Constants::MapID::Arkjok_Ward, {{837, 885}}},
    {GW::Constants::SkillID::Cruel_Spear, 02, "Eshau Longspear", GW::Constants::MapID::Arkjok_Ward, {{676, 1086}}},
    {GW::Constants::SkillID::Avatar_of_Grenth, 02, "Acolyte of Grenth", GW::Constants::MapID::Arkjok_Ward, {{835, 1022}}},
    {GW::Constants::SkillID::Avatar_of_Dwayna, 02, "Acolyte of Dwayna", GW::Constants::MapID::Dejarin_Estate, {{1079, 973}}, "Not during \"Kodonur Crossroads\" mission."},
    {GW::Constants::SkillID::Cautery_Signet, 02, "Corporal Luluh", GW::Constants::MapID::Dejarin_Estate, {{1066, 951}}, "Not during \"Kodonur Crossroads\" mission."},
    {GW::Constants::SkillID::Weapon_of_Fury, 02, "Podaltur the Angry", GW::Constants::MapID::Dejarin_Estate, {{1255, 846}}, "Not during \"Kodonur Crossroads\" mission."},
    {GW::Constants::SkillID::Way_of_the_Assassin, 02, "Major Jeahr", GW::Constants::MapID::Dejarin_Estate, {{1215, 893}}, "Not during \"Kodonur Crossroads\" mission."},
    {GW::Constants::SkillID::Infuriating_Heat, 02, "Colonel Custo", GW::Constants::MapID::Dejarin_Estate, {{1089, 950}}, "Not during \"Kodonur Crossroads\" mission."},
    {GW::Constants::SkillID::Magehunter_Strike, 02, "Colonel Chaklin", GW::Constants::MapID::Dejarin_Estate, {{1107, 899}}, "Not during \"Kodonur Crossroads\" mission."},
    {GW::Constants::SkillID::Youre_All_Alone, 02, "Commander Wahli", GW::Constants::MapID::Barbarous_Shore, {{1306, 1393}}},
    {GW::Constants::SkillID::Simple_Thievery, 02, "Midshipman Morolah", GW::Constants::MapID::Barbarous_Shore, {{1365, 1352}}},
    {GW::Constants::SkillID::Wielders_Zeal, 02, "Captain Alsin", GW::Constants::MapID::Barbarous_Shore, {{1380, 1366}}},
    {GW::Constants::SkillID::Vow_of_Strength, 02, "Captain Mhedi", GW::Constants::MapID::Barbarous_Shore, {{1348, 1361}}},
    {GW::Constants::SkillID::Crippling_Anthem, 02, "Lieutenant Shagu", GW::Constants::MapID::Barbarous_Shore, {{1466, 1245}}, "Not during \"The Foolhardy Father\" quest."},
    {GW::Constants::SkillID::Blood_is_Power, 02, "Overseer Vanakh", GW::Constants::MapID::Kodonur_Crossroads, {{1067, 954}}, "Only during \"Kodonur Crossroads\" mission."},
    {GW::Constants::SkillID::Zealous_Benediction, 02, "Overseer Sadi-Belai", GW::Constants::MapID::Kodonur_Crossroads, {{1142, 893}}, "Only during \"Kodonur Crossroads\" mission."},
    {GW::Constants::SkillID::Pious_Renewal, 02, "Overseer Suli", GW::Constants::MapID::Kodonur_Crossroads, {{1078, 1020}}, "Only during \"Kodonur Crossroads\" mission."},
    {GW::Constants::SkillID::Cautery_Signet, 02, "Overseer Boktek", GW::Constants::MapID::Kodonur_Crossroads, {{991, 973}}, "Only during \"Kodonur Crossroads\" mission."},
    {GW::Constants::SkillID::Smoke_Trap, 02, "Veldrunner Centaur", GW::Constants::MapID::Kodonur_Crossroads, {{1097, 902}}, "Only during \"Kodonur Crossroads\" mission. Friendly NPC. Needs to be killed by enemy mobs."},
    {GW::Constants::SkillID::Cleave, 02, "Chor the Bladed", GW::Constants::MapID::Marga_Coast, {{208, 828}}, "Not during \"Nundu Bay\" mission."},
    {GW::Constants::SkillID::Burning_Arrow, 02, "Admiral Chiggen", GW::Constants::MapID::Marga_Coast, {{393, 1031}}, "Not during \"Nundu Bay\" mission."},
    {GW::Constants::SkillID::Light_of_Deliverance, 02, "Chidehkir, Light of the Blind", GW::Constants::MapID::Marga_Coast, {{318, 835}}, "Not during \"Nundu Bay\" mission."},
    {GW::Constants::SkillID::Extend_Conditions, 02, "Neoli the Contagious", GW::Constants::MapID::Marga_Coast, {{377, 1015}}, "Not during \"Nundu Bay\" mission."},
    {GW::Constants::SkillID::Elemental_Attunement, 02, "Bosun Mohrti", GW::Constants::MapID::Marga_Coast, {{300, 989}}, "Not during \"Nundu Bay\" mission."},
    {GW::Constants::SkillID::Shadow_Meld, 02, "Lunto Sharpfoot", GW::Constants::MapID::Marga_Coast, {{234, 886}}, "Not during \"Nundu Bay\" mission."},
    {GW::Constants::SkillID::Grenths_Grasp, 02, "Bubahl Icehands", GW::Constants::MapID::Marga_Coast, {{314, 949}}, "Not during \"Nundu Bay\" mission."},
    {GW::Constants::SkillID::Avatar_of_Melandru, 02, "Acolyte of Melandru", GW::Constants::MapID::Marga_Coast, {{318, 891}}, "Not during \"Nundu Bay\" mission."},
    {GW::Constants::SkillID::Arcane_Zeal, 02, "Zealot Sheoli", GW::Constants::MapID::Nundu_Bay, {{377, 800}}, "Only during \"Nundu Bay\" mission.\nOne of the attacking Margonites."},
    {GW::Constants::SkillID::Anthem_of_Guidance, 02, "Commander Chutal", GW::Constants::MapID::Nundu_Bay, {{384, 816}}, "Only during \"Nundu Bay\" mission.\nOne of the attacking Margonites."},
    {GW::Constants::SkillID::Invoke_Lightning, 02, "Scribe Wensal", GW::Constants::MapID::Nundu_Bay, {{393, 827}}, "Only during \"Nundu Bay\" mission.\nOne of the attacking Margonites."},
    {GW::Constants::SkillID::Spell_Breaker, 02, "Priest Zein Zuu", GW::Constants::MapID::Nundu_Bay, {{412, 824}}, "Only during \"Nundu Bay\" mission.\nOne of the attacking Margonites."},
    {GW::Constants::SkillID::Crippling_Slash, 02, "Mahto Sharptooth", GW::Constants::MapID::Sunward_Marches, {{65, 506}}},
    {GW::Constants::SkillID::Crippling_Anguish, 02, "Eshim Mindclouder", GW::Constants::MapID::Sunward_Marches, {{53, 642}}},
    {GW::Constants::SkillID::Lightning_Surge, 02, "Birneh Skybringer", GW::Constants::MapID::Sunward_Marches, {{165, 653}}},
    {GW::Constants::SkillID::Reapers_Sweep, 02, "Dabineh Deathbringer", GW::Constants::MapID::Sunward_Marches, {{380, 594}}, "Spawns if no quest is active."},
    {GW::Constants::SkillID::Avatar_of_Lyssa, 02, "Acolyte of Lyssa", GW::Constants::MapID::Sunward_Marches, {{227, 600}}},
    {GW::Constants::SkillID::Strike_as_One, 02, "Sergeant Behnwa", GW::Constants::MapID::Turais_Procession, {{628, 214}}},
    {GW::Constants::SkillID::Glimmer_of_Light, 02, "Chiossen, Soothing Breeze", GW::Constants::MapID::Turais_Procession, {{574, 358}}},
    {GW::Constants::SkillID::Corrupt_Enchantment, 02, "Torment Weaver", GW::Constants::MapID::Turais_Procession, {{552, 271}}, "Only during \"Battle of Turai's Procession\" quest."},
    {GW::Constants::SkillID::Mantra_of_Recovery, 02, "Olunoss Windwalker", GW::Constants::MapID::Turais_Procession, {{580, 288}}, "Only after \"Battle of Turai's Procession\" quest."},
    {GW::Constants::SkillID::Searing_Flames, 02, "Korr, Living Flame", GW::Constants::MapID::Turais_Procession, {{545, 96}}},
    {GW::Constants::SkillID::Grenths_Grasp, 02, "Lord Yama the Vengeful", GW::Constants::MapID::The_Floodplain_of_Mahnkelon, {{1351, 473}}, "Only during \"Drakes on the Plain\" quest."},
    {GW::Constants::SkillID::Hidden_Caltrops, 02, "Admiral Kaya", GW::Constants::MapID::The_Floodplain_of_Mahnkelon, {{1196, 689}}},
    {GW::Constants::SkillID::Icy_Shackles, 02, "Buhon Icelord", GW::Constants::MapID::The_Floodplain_of_Mahnkelon, {{1304, 692}}},
    {GW::Constants::SkillID::Contagion, 02, "Terob Roundback", GW::Constants::MapID::The_Floodplain_of_Mahnkelon, {{1153, 567}}},
    {GW::Constants::SkillID::Signet_of_Removal, 02, "Commander Noss", GW::Constants::MapID::The_Floodplain_of_Mahnkelon, {{1304, 629}}},
    {GW::Constants::SkillID::Quick_Shot, 02, "Zelnehlun Fastfoot", GW::Constants::MapID::The_Floodplain_of_Mahnkelon, {{1247, 494}}, "Not during \"Drakes on the Plain\" quest."},
    {GW::Constants::SkillID::Decapitate, 02, "Robah Hardback", GW::Constants::MapID::The_Floodplain_of_Mahnkelon, {{1288, 683}}},
    {GW::Constants::SkillID::Tainted_Flesh, 02, "Jerneh Nightbringer", GW::Constants::MapID::The_Floodplain_of_Mahnkelon, {{1278, 660}}, "Spawns from the ground when you get close to the location."},
    {GW::Constants::SkillID::Sandstorm, 02, "The Drought", GW::Constants::MapID::Rilohn_Refuge, {{1351, 519}}, "Only during \"Rilohn Refuge\" mission."},
    {GW::Constants::SkillID::Avatar_of_Balthazar, 02, "Acolyte of Balthazar", GW::Constants::MapID::Jahai_Bluffs, {{814, 627}}},
    {GW::Constants::SkillID::Defensive_Anthem, 02, "Corporal Argon", GW::Constants::MapID::Jahai_Bluffs, {{799, 589}}},
    {GW::Constants::SkillID::Master_of_Magic, 02, "Admiral Kantoh", GW::Constants::MapID::Jahai_Bluffs, {{981, 489}}},
    {GW::Constants::SkillID::Ravenous_Gaze, 02, "Shelboh the Ravenous", GW::Constants::MapID::Jahai_Bluffs, {{944, 637}}},
    {GW::Constants::SkillID::Rampage_as_One, 02, "Tundoss the Destroyer", GW::Constants::MapID::Jahai_Bluffs, {{723, 668}}},
    {GW::Constants::SkillID::Devastating_Hammer, 02, "Churrta the Rock", GW::Constants::MapID::Jahai_Bluffs, {{900, 697}}, "Not during \"Eavesdropping\" quest."},
    {GW::Constants::SkillID::Balthazars_Pendulum, 02, "Riseh the Harmless", GW::Constants::MapID::Bahdok_Caverns, {{1400, 322}}},
    {GW::Constants::SkillID::Reapers_Mark, 02, "Commander Sehden", GW::Constants::MapID::Bahdok_Caverns, {{1404, 336}}},
    {GW::Constants::SkillID::Grenths_Balance, 02, "Armind the Balancer", GW::Constants::MapID::Bahdok_Caverns, {{1514, 369}}},
    {GW::Constants::SkillID::Stone_Sheath, 02, "Joknang Earthturner", GW::Constants::MapID::Bahdok_Caverns, {{1556, 269}}},
    {GW::Constants::SkillID::Focused_Anger, 02, "Wandalz the Angry", GW::Constants::MapID::Bahdok_Caverns, {{1373, 328}}},
    {GW::Constants::SkillID::Zealous_Vow, 02, "Sehlon, Beautiful Water", GW::Constants::MapID::Bahdok_Caverns, {{1570, 245}}},
    {GW::Constants::SkillID::Cruel_Spear, 02, "Yakun Trueshot", GW::Constants::MapID::Moddok_Crevice, {{1466, 340}}, "Only during \"Moddok Crevice\" mission.\nYou will have to enter the passage Dunkoro tells you to avoid, so you will be unable to kill this boss and get the bonus at the same time."},
    {GW::Constants::SkillID::Light_of_Deliverance, 02, "Kenmak the Tranquil", GW::Constants::MapID::Moddok_Crevice, {{1463, 355}}, "Only during \"Moddok Crevice\" mission.\nYou will have to enter the passage Dunkoro tells you to avoid, so you will be unable to kill this boss and get the bonus at the same time."},
    {GW::Constants::SkillID::Magehunter_Strike, 02, "Captain Kavaka", GW::Constants::MapID::Consulate_Docks, {{907, 1345}}, "Only during \"Consulate Docks\" mission."},
    {GW::Constants::SkillID::Master_of_Magic, 02, "Captain Mwende", GW::Constants::MapID::Consulate_Docks, {{883, 1352}}, "Only during \"Consulate Docks\" mission."},
    {GW::Constants::SkillID::Cautery_Signet, 02, "Captain Lumanda", GW::Constants::MapID::Consulate_Docks, {{790, 1351}}, "Only during \"Consulate Docks\" mission."},
    {GW::Constants::SkillID::Pious_Renewal, 02, "Captain Denduru", GW::Constants::MapID::Consulate_Docks, {{822, 1344}}, "Only during \"Consulate Docks\" mission."},
    {GW::Constants::SkillID::Magehunter_Strike, 02, "Executioner Vekil", GW::Constants::MapID::Pogahn_Passage, {{900, 1218}}, "Only during \"Pogahn Passage\" mission."},
    {GW::Constants::SkillID::Enchanters_Conundrum, 02, "Lieutenant Nali", GW::Constants::MapID::Pogahn_Passage, {{906, 1328}}, "Only during \"Pogahn Passage\" mission."},
    {GW::Constants::SkillID::Master_of_Magic, 02, "Captain Nebo", GW::Constants::MapID::Pogahn_Passage, {{823, 1366}}, "Only during \"Pogahn Passage\" mission."},
    {GW::Constants::SkillID::Pious_Renewal, 03, "Corporal Suli", GW::Constants::MapID::Yatendi_Canyons, {{771, 1273}}},
    {GW::Constants::SkillID::Invoke_Lightning, 03, "Lushivahr the Invoker", GW::Constants::MapID::Yatendi_Canyons, {{660, 1356}}},
    {GW::Constants::SkillID::Master_of_Magic, 03, "Hajok Earthguardian", GW::Constants::MapID::Yatendi_Canyons, {{707, 1292}}},
    {GW::Constants::SkillID::Tease, 03, "Makdeh the Aggravating", GW::Constants::MapID::Yatendi_Canyons, {{714, 1276}}},
    {GW::Constants::SkillID::Corrupt_Enchantment, 03, "Tain the Corrupter", GW::Constants::MapID::Yatendi_Canyons, {{691, 1315}}},
    {GW::Constants::SkillID::Blood_is_Power, 03, "Lieutenant Vanahk", GW::Constants::MapID::Yatendi_Canyons, {{811, 1178}}},
    {GW::Constants::SkillID::Steady_Stance, 03, "Chumab the Prideful", GW::Constants::MapID::Vehtendi_Valley, {{1018, 968}}},
    {GW::Constants::SkillID::Weapon_of_Fury, 03, "Bohdalz the Furious", GW::Constants::MapID::Vehtendi_Valley, {{982, 965}}},
    {GW::Constants::SkillID::Wastrels_Collapse, 03, "Jarimiya the Unmerciful", GW::Constants::MapID::Vehtendi_Valley, {{997, 981}}, "Not during \"Rally The Princess\" and \"Valley of the Rifts\" quests."},
    {GW::Constants::SkillID::Crippling_Anguish, 03, "Yamesh Mindclouder", GW::Constants::MapID::Vehtendi_Valley, {{1054, 985}}},
    {GW::Constants::SkillID::Incoming, 03, "Render", GW::Constants::MapID::Forum_Highlands, {{603, 790}}, "Only during \"Desperate Measures\" quest."},
    {GW::Constants::SkillID::Spirits_Strength, 03, "Churahm, Spirit Warrior", GW::Constants::MapID::Forum_Highlands, {{693, 820}}},
    {GW::Constants::SkillID::Searing_Flames, 03, "Korshek the Immolated", GW::Constants::MapID::Forum_Highlands, {{685, 881}}},
    {GW::Constants::SkillID::Contagion, 03, "Harrk Facestab", GW::Constants::MapID::Forum_Highlands, {{594, 721}}, "Only during \"Desperate Measures\" quest."},
    {GW::Constants::SkillID::Zealous_Benediction, 03, "Commander Sadi-Belai", GW::Constants::MapID::Forum_Highlands, {{722, 925}}},
    {GW::Constants::SkillID::Ferocious_Strike, 03, "Bolten Largebelly", GW::Constants::MapID::Forum_Highlands, {{831, 796}}},
    {GW::Constants::SkillID::Magehunters_Smash, 03, "Grabthar the Overbearing", GW::Constants::MapID::Forum_Highlands, {{535, 830}}},
    {GW::Constants::SkillID::Foxs_Promise, 03, "Hanchor Trueblade", GW::Constants::MapID::Holdings_of_Chokhin, {{586, 183}}},
    {GW::Constants::SkillID::Lightning_Surge, 03, "Setikor Fireflower", GW::Constants::MapID::Holdings_of_Chokhin, {{808, 220}}},
    {GW::Constants::SkillID::Shield_of_Regeneration, 03, "Banor Greenbranch", GW::Constants::MapID::Holdings_of_Chokhin, {{687, 183}}},
    {GW::Constants::SkillID::Escape, 03, "Tenezel the Quick", GW::Constants::MapID::Holdings_of_Chokhin, {{641, 272}}},
    {GW::Constants::SkillID::Charging_Strike, 03, "Riktund the Vicious", GW::Constants::MapID::The_Mirror_of_Lyss, {{984, 391}}},
    {GW::Constants::SkillID::Energy_Surge, 03, "Yammiron, Ether Lord", GW::Constants::MapID::The_Mirror_of_Lyss, {{901, 585}, {1197, 471}}, "Not during \"The Search for Survivors\" quest."},
    {GW::Constants::SkillID::Ebon_Dust_Aura, 03, "Shezel Slowreaper", GW::Constants::MapID::The_Mirror_of_Lyss, {{1066, 603}}, "Not during \"The Search for Survivors\" quest."},
    {GW::Constants::SkillID::Anthem_of_Guidance, 03, "Overseer Juntahk", GW::Constants::MapID::The_Mirror_of_Lyss, {{997, 472}}, "Only during \"Warning Kehanni\" quest."},
    {GW::Constants::SkillID::Anthem_of_Guidance, 03, "Briahn the Chosen", GW::Constants::MapID::The_Mirror_of_Lyss, {{1130, 616}}, "Travels back and forth between Honur Hill and The Kodash Bazaar."},
    {GW::Constants::SkillID::Song_of_Purification, 03, "Jishol Darksong", GW::Constants::MapID::Resplendent_Makuun, {{1264, 724}}},
    {GW::Constants::SkillID::Mind_Burn, 03, "Kormab, Burning Heart", GW::Constants::MapID::Resplendent_Makuun, {{1468, 682}}},
    {GW::Constants::SkillID::Healers_Boon, 03, "Josinq the Whisperer", GW::Constants::MapID::Resplendent_Makuun, {{1390, 854}}},
    {GW::Constants::SkillID::Experts_Dexterity, 03, "Bansheh, Gatherer of Branches", GW::Constants::MapID::Resplendent_Makuun, {{1491, 895}}},
    {GW::Constants::SkillID::Incoming, 03, "Kunan the Loudmouth", GW::Constants::MapID::Wilderness_of_Bahdza, {{1530, 384}}, "Only after \"Extinction\" quest."},
    {GW::Constants::SkillID::Incoming, 03, "Screecher", GW::Constants::MapID::Wilderness_of_Bahdza, {{1540, 549}}, "Only before completion of \"Destroy the Harpies\" quest."},
    {GW::Constants::SkillID::Incoming, 03, "Scratcher", GW::Constants::MapID::Wilderness_of_Bahdza, {{1559, 536}}, "Only before completion of \"Destroy the Harpies\" quest."},
    {GW::Constants::SkillID::Blinding_Surge, 03, "Moteh Thundershooter", GW::Constants::MapID::Wilderness_of_Bahdza, {{1391, 483}}},
    {GW::Constants::SkillID::Visions_of_Regret, 03, "Korrub, Flame of Dreams", GW::Constants::MapID::Wilderness_of_Bahdza, {{1409, 524}}, "Only after \"Destroy the Harpies\" quest."},
    {GW::Constants::SkillID::Toxic_Chill, 03, "Eshekibeh Longneck", GW::Constants::MapID::Wilderness_of_Bahdza, {{1473, 348}}},
    {GW::Constants::SkillID::Contagion, 03, "Brokk Ripsnort", GW::Constants::MapID::Wilderness_of_Bahdza, {{1510, 270}}},
    {GW::Constants::SkillID::Anthem_of_Guidance, 03, "General Kumtash", GW::Constants::MapID::Dzagonur_Bastion, {{1385, 421}}, "Only during \"Dzagonur Bastion\" mission."},
    {GW::Constants::SkillID::Invoke_Lightning, 03, "General Tirraj", GW::Constants::MapID::Dzagonur_Bastion, {{1406, 375}}, "Only during \"Dzagonur Bastion\" mission."},
    {GW::Constants::SkillID::Spell_Breaker, 03, "General Nimtak", GW::Constants::MapID::Dzagonur_Bastion, {{1398, 396}}, "Only during \"Dzagonur Bastion\" mission."},
    {GW::Constants::SkillID::Magehunters_Smash, 03, "General Doriah", GW::Constants::MapID::Dzagonur_Bastion, {{1350, 430}}, "Only during \"Dzagonur Bastion\" mission."},
    {GW::Constants::SkillID::Song_of_Restoration, 03, "Toshau Sharpspear", GW::Constants::MapID::Garden_of_Seborhin, {{496, 543}}, "Not during \"Tihark Orchard\" mission."},
    {GW::Constants::SkillID::Air_of_Disenchantment, 03, "Hojanukun Mindstealer", GW::Constants::MapID::Garden_of_Seborhin, {{767, 397}}, "Not during \"Tihark Orchard\" mission."},
    {GW::Constants::SkillID::Contagion, 03, "Mabah Heardheart", GW::Constants::MapID::Garden_of_Seborhin, {{664, 397}}, "Not during \"Tihark Orchard\" mission."},
    {GW::Constants::SkillID::Word_of_Healing, 03, "Hahan, Faithful Protector", GW::Constants::MapID::Garden_of_Seborhin, {{444, 508}}, "Not during \"Tihark Orchard\" mission."},
    {GW::Constants::SkillID::Zealous_Vow, 03, "Leilon, Tranquil Water", GW::Constants::MapID::The_Hidden_City_of_Ahdashim, {{1154, 123}}},
    {GW::Constants::SkillID::Searing_Flames, 03, "Kormab, Burning Heart", GW::Constants::MapID::Dasha_Vestibule, {{1220, 214}}, "Only during \"Dasha Vestibule\" mission."},
    {GW::Constants::SkillID::Vow_of_Silence, 03, "Water Lord", GW::Constants::MapID::The_Hidden_City_of_Ahdashim, {{1175, 22}}, "Only during \"Gift of the Djinn\" quest."},
    {GW::Constants::SkillID::Zealous_Vow, 03, "Hajok Earthguardian", GW::Constants::MapID::Dasha_Vestibule, {{1220, 198}}, "Only during \"Dasha Vestibule\" mission."},
    {GW::Constants::SkillID::Angelic_Bond, 03, "Shakor Firespear", GW::Constants::MapID::Dasha_Vestibule, {{1219, 230}}, "Only during \"Dasha Vestibule\" mission."},
    {GW::Constants::SkillID::Angelic_Bond, 03, "Shakor Firespear", GW::Constants::MapID::The_Hidden_City_of_Ahdashim, {{1219, 167}}, "Not during \"Dasha Vestibule\" mission."},
    {GW::Constants::SkillID::Escape, 03, "Tenshek Roundbody", GW::Constants::MapID::The_Hidden_City_of_Ahdashim, {{1295, 130}}, "Not during \"Dasha Vestibule\" mission."},
    {GW::Constants::SkillID::Wounding_Strike, 03, "Marobeh Sharptail", GW::Constants::MapID::Vehjin_Mines, {{342, 530}}},
    {GW::Constants::SkillID::Song_of_Purification, 03, "Pehnsed the Loudmouth", GW::Constants::MapID::Vehjin_Mines, {{183, 568}}},
    {GW::Constants::SkillID::Pain_of_Disenchantment, 03, "Nehpek the Remorseless", GW::Constants::MapID::Vehjin_Mines, {{117, 418}}, "Does not spawn if you have Master of Whispers in your party and you have \"Coffer of Joko\" quest active."},
    {GW::Constants::SkillID::Charge, 03, "Shak-Jarin the Justicebringer", GW::Constants::MapID::Vehjin_Mines, {{171, 434}}},
    {GW::Constants::SkillID::Soldiers_Stance, 04, "Avah the Crafty ", GW::Constants::MapID::Gate_of_Desolation, {{952, 1127}}, "Only during \"Gate of Desolation\" mission.\nSecond boss in the waves of undead."},
    {GW::Constants::SkillID::Soldiers_Fury, 04, "Chakeh the Lonely", GW::Constants::MapID::Gate_of_Desolation, {{968, 1133}}, "Only during \"Gate of Desolation\" mission.\nFirst boss in the waves of undead."},
    {GW::Constants::SkillID::Vow_of_Silence, 04, "Chundu the Meek", GW::Constants::MapID::Gate_of_Desolation, {{973, 1154}}, "Only during \"Gate of Desolation\" mission.\nThird boss in the waves of undead."},
    {GW::Constants::SkillID::Signet_of_Suffering, 04, "Nehmak the Unpleasant ", GW::Constants::MapID::Gate_of_Desolation, {{953, 1142}}, "Only during \"Gate of Desolation\" mission.\nSpawns in the last wave before riding the wurms. Spawns at the same time as Amind the Bitter."},
    {GW::Constants::SkillID::Hex_Eater_Vortex, 04, "Amind the Bitter", GW::Constants::MapID::Gate_of_Desolation, {{955, 1156}}, "Only during \"Gate of Desolation\" mission.\nSpawns in the last wave before riding the wurms. Spawns at the same time as Nehmak the Unpleasant."},
    {GW::Constants::SkillID::Vow_of_Silence, 04, "Vahlen the Silent", GW::Constants::MapID::The_Sulfurous_Wastes, {{990, 1090}}},
    {GW::Constants::SkillID::Weapon_of_Remedy, 04, "Alem the Unclean", GW::Constants::MapID::The_Sulfurous_Wastes, {{692, 1132}}},
    {GW::Constants::SkillID::Savannah_Heat, 04, "Hajkor, Mystic Flame", GW::Constants::MapID::The_Sulfurous_Wastes, {{925, 953}}},
    {GW::Constants::SkillID::Order_of_Undeath, 04, "Bohdabi the Destructive", GW::Constants::MapID::The_Sulfurous_Wastes, {{693, 934}}},
    {GW::Constants::SkillID::Soldiers_Fury, 04, "Arneh the Vigorous", GW::Constants::MapID::Jokos_Domain, {{845, 583}}},
    {GW::Constants::SkillID::Shadow_Prison, 04, "Ardeh the Quick", GW::Constants::MapID::Jokos_Domain, {{928, 673}}},
    {GW::Constants::SkillID::Echo, 04, "Eshwe the Insane", GW::Constants::MapID::Jokos_Domain, {{894, 756}}},
    {GW::Constants::SkillID::Soldiers_Stance, 04, "Keht the Fierce", GW::Constants::MapID::Jokos_Domain, {{941, 809}}},
    {GW::Constants::SkillID::Martyr, 04, "Dunshek the Purifier", GW::Constants::MapID::The_Shattered_Ravines, {{854, 317}}},
    {GW::Constants::SkillID::Magebane_Shot, 04, "Saushali the Frustrating", GW::Constants::MapID::The_Shattered_Ravines, {{1081, 310}}, "Not during \"Horde of Darkness\" quest."},
    {GW::Constants::SkillID::Smoke_Trap, 04, "Uhiwi the Smoky", GW::Constants::MapID::The_Shattered_Ravines, {{970, 388}}},
    {GW::Constants::SkillID::Quicksand, 04, "Koahm the Weary", GW::Constants::MapID::The_Shattered_Ravines, {{931, 396}}},
    {GW::Constants::SkillID::Its_Just_a_Flesh_Wound, 04, "Churkeh the Defiant", GW::Constants::MapID::The_Alkali_Pan, {{394, 598}}},
    {GW::Constants::SkillID::Hex_Eater_Vortex, 04, "Shelkeh the Hungry", GW::Constants::MapID::The_Alkali_Pan, {{386, 706}}},
    {GW::Constants::SkillID::Divert_Hexes, 04, "Amiresh the Pious", GW::Constants::MapID::The_Alkali_Pan, {{513, 571}}},
    {GW::Constants::SkillID::Spike_Trap, 04, "Vah the Crafty", GW::Constants::MapID::The_Alkali_Pan, {{476, 769}}},
    {GW::Constants::SkillID::The_Power_Is_Yours, 04, "Tureksin the Delegator", GW::Constants::MapID::The_Ruptured_Heart, {{462, 293}}},
    {GW::Constants::SkillID::Energy_Drain, 04, "Rual, Stealer of Hope", GW::Constants::MapID::The_Ruptured_Heart, {{446, 407}}},
    {GW::Constants::SkillID::Corrupt_Enchantment, 04, "Rebirther Jirath", GW::Constants::MapID::The_Ruptured_Heart, {{449, 445}}, "Only during \"A Tasty Morsel\" quest."},
    {GW::Constants::SkillID::Signet_of_Judgment, 04, "Hamlen the Fallen", GW::Constants::MapID::The_Ruptured_Heart, {{497, 243}}},
    {GW::Constants::SkillID::Scavengers_Focus, 04, "Rendabi Deatheater", GW::Constants::MapID::The_Ruptured_Heart, {{351, 405}}},
    {GW::Constants::SkillID::Judgement_Strike, 04, "Yoannh the Rebuilder", GW::Constants::MapID::The_Ruptured_Heart, {{350, 268}}, "Only if a player has a \"Proof of Triumph\" in their inventory."},
    {GW::Constants::SkillID::Heroic_Refrain, 04, "Yoannh the Rebuilder", GW::Constants::MapID::The_Ruptured_Heart, {{364, 268}}, "Only if a player has a \"Proof of Triumph\" in their inventory."},
    {GW::Constants::SkillID::Vow_of_Revolution, 04, "Yoannh the Rebuilder", GW::Constants::MapID::The_Ruptured_Heart, {{378, 268}}, "Only if a player has a \"Proof of Triumph\" in their inventory."},
    {GW::Constants::SkillID::Signet_of_Ghostly_Might, 04, "Jedeh the Mighty", GW::Constants::MapID::Crystal_Overlook, {{31, 482}}},
    {GW::Constants::SkillID::Shattering_Assault, 04, "Achor the Bladed", GW::Constants::MapID::Crystal_Overlook, {{67, 607}}},
    {GW::Constants::SkillID::Plague_Signet, 04, "Wioli the Infectious", GW::Constants::MapID::Crystal_Overlook, {{167, 649}}},
    {GW::Constants::SkillID::Scribes_Insight, 04, "Ajamahn, Servant of the Sands", GW::Constants::MapID::Crystal_Overlook, {{168, 541}}},
    {GW::Constants::SkillID::Anthem_of_Guidance, 04, "Battlelord Turgar", GW::Constants::MapID::Poisoned_Outcrops, {{765, 155}}, "Only during \"The Cold Touch of the Past\" quest."},
    {GW::Constants::SkillID::Reclaim_Essence, 04, "Nehjab the Parched", GW::Constants::MapID::Poisoned_Outcrops, {{881, 196}}},
    {GW::Constants::SkillID::Ether_Prism, 04, "Mekir the Prismatic", GW::Constants::MapID::Poisoned_Outcrops, {{779, 137}}},
    {GW::Constants::SkillID::Sandstorm, 04, "Droajam, Mage of the Sands", GW::Constants::MapID::Poisoned_Outcrops, {{633, 132}}},
    {GW::Constants::SkillID::Signet_of_Suffering, 04, "Fondalz the Spiteful", GW::Constants::MapID::Poisoned_Outcrops, {{1026, 222}}},
    {GW::Constants::SkillID::Obsidian_Flesh, 04, "Shekoss the Stony", GW::Constants::MapID::Jokos_Domain, {{883, 658}}, "Not during \"A Deal's a Deal\" quest."},
    {GW::Constants::SkillID::Youre_All_Alone, 01, "Commander Bahreht", GW::Constants::MapID::Cliffs_of_Dohjok, {{616, 562}}, "Only during \"A Land of Heroes\" quest."},
    {GW::Constants::SkillID::Reapers_Mark, 01, "Midshipman Beraidun", GW::Constants::MapID::Cliffs_of_Dohjok, {{786, 545}}, "Only during \"A Land of Heroes\" quest."},
    {GW::Constants::SkillID::Vow_of_Strength, 01, "Ensign Jahan", GW::Constants::MapID::Cliffs_of_Dohjok, {{755, 572}}, "Only during \"A Land of Heroes\" quest."},
    {GW::Constants::SkillID::Crippling_Anthem, 01, "Lieutenant Mahrik", GW::Constants::MapID::Cliffs_of_Dohjok, {{645, 600}}, "Only during \"A Land of Heroes\" quest."},
    {GW::Constants::SkillID::Balthazars_Pendulum, 01, "Captain Shehnahr", GW::Constants::MapID::Cliffs_of_Dohjok, {{565, 572}}, "Only during \"A Land of Heroes\" quest."},
    {GW::Constants::SkillID::Lightbringer_Signet, 03, "Seeker of Whispers", GW::Constants::MapID::Chantry_of_Secrets_outpost, {{701, 1197}}, "Seeker of Whispers in the Chantry of Secrets will award this skill for achieving the title of Brave Lightbringer."},
    {GW::Constants::SkillID::Junundu_Siege, 04, "Elder Wurms", GW::Constants::MapID::The_Shattered_Ravines, {{1097, 303}}, "Acquired by following the primary quest received at the Bone Palace, \"Horde of Darkness\". You must kill several Elder Wurms, which your wurm then consumes to learn the skill."},
    {GW::Constants::SkillID::Energy_Surge, 03, "Yammirvu, Ether Guardian", GW::Constants::MapID::The_Hidden_City_of_Ahdashim, {{1172, 38}}, "Not during \"Dasha Vestibule\" mission."},
    {GW::Constants::SkillID::Cruel_Spear, 02, "Eshau Longspear", GW::Constants::MapID::Sunspear_Sanctuary_outpost, {{568, 650}}, "Only during \"Hunted!\" quest."},
    {GW::Constants::SkillID::Spell_Breaker, 03, "Preceptor Zunark", GW::Constants::MapID::Vehjin_Mines, {{255, 418}}, "During \"To The Rescue\" quest."},
    {GW::Constants::SkillID::Jagged_Bones, 05, "Emissary of Dhuum", GW::Constants::MapID::Nightfallen_Jahai, {{695, 125}}, "Only during \"Dark Gateway\" quest."},
    {GW::Constants::SkillID::Magebane_Shot, 05, "Abbadon Adjutant", GW::Constants::MapID::Nightfallen_Jahai, {{647, 247}}, "Only during \"They Only Come Out at Night\" quest."},
    {GW::Constants::SkillID::Onslaught, 05, "Emissary of Dhuum", GW::Constants::MapID::Nightfallen_Jahai, {{663, 236}}, "Only during \"They Only Come Out at Night\" quest."},
    {GW::Constants::SkillID::Mark_of_Insecurity, 05, "Chimor the Lightblooded", GW::Constants::MapID::Nightfallen_Jahai, {{680, 230}}},
    {GW::Constants::SkillID::Onslaught, 05, "Onslaught of Terror", GW::Constants::MapID::Nightfallen_Jahai, {{793, 201}}},
    {GW::Constants::SkillID::Signet_of_Judgment, 05, "Rukkassa", GW::Constants::MapID::Nightfallen_Jahai, {{931, 135}}, "Only during \"Invasion From Within\" quest."},
    {GW::Constants::SkillID::Signet_of_Illusions, 05, "Shepherd of Dementia", GW::Constants::MapID::Nightfallen_Jahai, {{908, 223}}},
    {GW::Constants::SkillID::Assault_Enchantments, 05, "Faveo Aggredior", GW::Constants::MapID::Nightfallen_Jahai, {{913, 300}}},
    {GW::Constants::SkillID::Hundred_Blades, 05, "Blade of Corruption", GW::Constants::MapID::Nightfallen_Jahai, {{706, 364}}, "Only during \"Breaking the Broken\" quest."},
    {GW::Constants::SkillID::Jagged_Bones, 05, "Shadow of Fear", GW::Constants::MapID::Nightfallen_Jahai, {{725, 358}}, "Only during \"Breaking the Broken\" quest."},
    {GW::Constants::SkillID::Anthem_of_Fury, 05, "Oath of Profanity", GW::Constants::MapID::Nightfallen_Jahai, {{786, 371}}},
    {GW::Constants::SkillID::Power_Flux, 05, "Vision of Despair", GW::Constants::MapID::Domain_of_Pain, {{160, 631}}},
    {GW::Constants::SkillID::Battle_Rage, 05, "Saevio Proelium", GW::Constants::MapID::Domain_of_Pain, {{188, 639}}},
    {GW::Constants::SkillID::Arcane_Zeal, 05, "Fahralon the Zealous", GW::Constants::MapID::Domain_of_Pain, {{230, 720}}},
    {GW::Constants::SkillID::Healers_Covenant, 05, "Bringer of Deceit", GW::Constants::MapID::Domain_of_Pain, {{279, 762}}},
    {GW::Constants::SkillID::Barrage, 05, "Bearer of Misfortune", GW::Constants::MapID::Domain_of_Pain, {{422, 821}}},
    {GW::Constants::SkillID::Ether_Prism, 05, "Tortureweb Dryder", GW::Constants::MapID::Gate_of_Pain, {{452, 778}}, "Six times."},
    {GW::Constants::SkillID::Invoke_Lightning, 05, "Curator Kali", GW::Constants::MapID::Gate_of_Madness, {{863, 1017}}},
    {GW::Constants::SkillID::Magehunters_Smash, 05, "Champion Puran", GW::Constants::MapID::Gate_of_Madness, {{699, 1003}}},
    {GW::Constants::SkillID::Spell_Breaker, 05, "Topo the Protector", GW::Constants::MapID::Depths_of_Madness, {{604, 924}}},
    {GW::Constants::SkillID::Hundred_Blades, 05, "Reaper of Agony", GW::Constants::MapID::Depths_of_Madness, {{739, 963}}},
    {GW::Constants::SkillID::Xinraes_Weapon, 05, "Consort of Ruin", GW::Constants::MapID::Depths_of_Madness, {{785, 1081}}},
    {GW::Constants::SkillID::Golden_Skull_Strike, 05, "Ravager of Dreams", GW::Constants::MapID::Depths_of_Madness, {{714, 1155}}},
    {GW::Constants::SkillID::Jagged_Bones, 05, "Master of Misery", GW::Constants::MapID::Depths_of_Madness, {{856, 1149}}},
    {GW::Constants::SkillID::Prepared_Shot, 05, "Seed of Suffering", GW::Constants::MapID::Domain_of_Fear, {{1506, 239}}},
    {GW::Constants::SkillID::Stunning_Strike, 05, "Shrieker of Dread", GW::Constants::MapID::Domain_of_Fear, {{1692, 122}}},
    {GW::Constants::SkillID::Mind_Freeze, 05, "Storm of Anguish", GW::Constants::MapID::Domain_of_Fear, {{1713, 170}}},
    {GW::Constants::SkillID::Defenders_Zeal, 05, "Flame of Fervor", GW::Constants::MapID::Domain_of_Fear, {{1734, 304}}},
    {GW::Constants::SkillID::Mind_Blast, 05, "Exuro Flatus", GW::Constants::MapID::Domain_of_Secrets, {{1544, 1218}}},
    {GW::Constants::SkillID::Lingering_Curse, 05, "Creo Vulnero", GW::Constants::MapID::Domain_of_Secrets, {{1509, 1072}}},
    {GW::Constants::SkillID::Offering_of_Spirit, 05, "Shakahm the Summoner", GW::Constants::MapID::Domain_of_Secrets, {{1752, 1210}}},
    {GW::Constants::SkillID::Symbols_of_Inspiration, 05, "Wieshur the Inspiring", GW::Constants::MapID::Domain_of_Secrets, {{1769, 1034}}},
    {GW::Constants::SkillID::Caretakers_Charge, 05, "Hautoh the Pilferer", GW::Constants::MapID::Domain_of_Secrets, {{1646, 971}}},
    {GW::Constants::SkillID::Corrupt_Enchantment, 05, "Razakel", GW::Constants::MapID::Domain_of_Secrets, {{1573, 893}}, "Only during \"Blueprint of the Fall\" quest."},
    {GW::Constants::SkillID::Incendiary_Arrows, 05, "Stygian Underlord (Ranger)", GW::Constants::MapID::Domain_of_Anguish, {{1270, 1267}}, "This Stygian Underlord is a ranger found on the western hill of the five hills that dominate the Stygian Veil. Defeating it is one of the final objectives of \"Breaching the Stygian Veil\" quest.\nUnknown position."},
    {GW::Constants::SkillID::Anthem_of_Guidance, 05, "Lord Jadoth", GW::Constants::MapID::Domain_of_Anguish, {{1185, 1285}}},
    {GW::Constants::SkillID::Magehunters_Smash, 04, "Dupek the Mighty", GW::Constants::MapID::The_Sulfurous_Wastes, {{704, 1152}}, "Only during \"A Show of Force\" quest."},
    {GW::Constants::SkillID::Corrupt_Enchantment, 04, "Hauseh the Defiler", GW::Constants::MapID::The_Sulfurous_Wastes, {{704, 1167}}, "Only during \"A Show of Force\" quest."},
    {GW::Constants::SkillID::Invoke_Lightning, 04, "Tanmahk the Arcane", GW::Constants::MapID::The_Sulfurous_Wastes, {{690, 1158}}, "Only during \"A Show of Force\" quest."},
    {GW::Constants::SkillID::Enraged_Smash, 01, "The Afflicted Tamaya", GW::Constants::MapID::Dragons_Throat, {{1121, 327}}, "As you stand at the starting gate looking into the mesmer area, go in and turn right. You will see 2 afflicted in the next area all by themselves, kill them and more afflicted will appear. Kill the new mobs and more will spawn. Do this 3 times and he will appear."},
    {GW::Constants::SkillID::Broad_Head_Arrow, 01, "The Afflicted Susei", GW::Constants::MapID::Dragons_Throat, {{1202, 305}}},
    {GW::Constants::SkillID::Ray_of_Judgment, 01, "The Afflicted Cho", GW::Constants::MapID::Dragons_Throat, {{1099, 396}}},
    {GW::Constants::SkillID::Order_of_Apostasy, 01, "The Afflicted Xi", GW::Constants::MapID::Dragons_Throat, {{1261, 382}}},
    {GW::Constants::SkillID::Stolen_Speed, 01, "The Afflicted Meeka", GW::Constants::MapID::Dragons_Throat, {{1111, 248}}},
    {GW::Constants::SkillID::Mind_Burn, 01, "The Afflicted Rasa", GW::Constants::MapID::Dragons_Throat, {{1287, 332}}},
    {GW::Constants::SkillID::Shadow_Form, 01, "The Afflicted Huu", GW::Constants::MapID::Dragons_Throat, {{1106, 417}, {1196, 254}}},
    {GW::Constants::SkillID::Weapon_of_Quickening, 01, "The Afflicted Mei", GW::Constants::MapID::Dragons_Throat, {{1246, 236}}, "The creature will only spawn when all the afflicted in the room have been killed. You have to move to the lower platform in the room to trigger the spawn."},
    {GW::Constants::SkillID::Triple_Chop, 01, "Wing, Three Blade", GW::Constants::MapID::Bukdek_Byway, {{481, 338}}},
    {GW::Constants::SkillID::Expel_Hexes, 01, "Jin, the Purifier", GW::Constants::MapID::Bukdek_Byway, {{531, 371}}, "Not during \"Chasing Zenmai\" quest."},
    {GW::Constants::SkillID::Elemental_Attunement, 01, "Chung, the Attuned", GW::Constants::MapID::Bukdek_Byway, {{550, 422}}, "Only during \"Eliminate the Am Fah\" quest."},
    {GW::Constants::SkillID::Palm_Strike, 01, "Kenshi Steelhand", GW::Constants::MapID::Bukdek_Byway, {{544, 399}}, "If \"Eliminate the Jade Brotherhood\" quest is active, Kenshi will spawn twice - once to the southeast of Oroku, once to the southwest. His normal spawn point is to the southwest, but the quest spawns another copy to the southeast at the same time.\nNot during \"Chasing Zenmai\" quest."},
    {GW::Constants::SkillID::Enraged_Smash, 01, "Afflicted Guardsman Chun", GW::Constants::MapID::Bukdek_Byway, {{528, 249}}, "Only during \"The Afflicted Guard\" quest."},
    {GW::Constants::SkillID::Word_of_Censure, 01, "Shen, the Magistrate", GW::Constants::MapID::Wajjun_Bazaar, {{217, 472}}},
    {GW::Constants::SkillID::Double_Dragon, 01, "Lian, Dragon's Petal", GW::Constants::MapID::Wajjun_Bazaar, {{303, 535}}, "Will not be there if you have \"Closer to the Stars\" quest and are supposed to talk to Fishmonger Bihzun for the first time."},
    {GW::Constants::SkillID::Word_of_Censure, 01, "Quufu", GW::Constants::MapID::Wajjun_Bazaar, {{172, 465}}, "Only during \"Straight to the Top\" quest."},
    {GW::Constants::SkillID::Quivering_Blade, 01, "Sun, the Quivering", GW::Constants::MapID::Wajjun_Bazaar, {{359, 615}}, "The Am Fah must be your enemy in order to encounter Sun, the Quivering. If they are your allies you must do \"Seek out Brother Tosai\" quest and complete either \"Drink from the Chalice of Corruption\" quest or \"Refuse to Drink\" quest."},
    {GW::Constants::SkillID::Ray_of_Judgment, 01, "The Afflicted Miju", GW::Constants::MapID::The_Undercity, {{235, 189}}, "Not always during \"The Shadow Blades\" quest."},
    {GW::Constants::SkillID::Martyr, 01, "Am Fah Leader", GW::Constants::MapID::The_Undercity, {{267, 333}}, "Only during \"Capturing the Orrian Tome\" quest."},
    {GW::Constants::SkillID::Order_of_Apostasy, 01, "The Afflicted Huan", GW::Constants::MapID::The_Undercity, {{248, 235}}},
    {GW::Constants::SkillID::Blood_is_Power, 01, "Chan the Dragon's Blood", GW::Constants::MapID::The_Undercity, {{63, 190}}, "If Chan is not aggroed, he starts walking and walks all the way to the south-eastern corner of The Undercity, near Mina Shatter Storm."},
    {GW::Constants::SkillID::Shatter_Storm, 01, "Mina Shatter Storm", GW::Constants::MapID::The_Undercity, {{225, 331}}, "Not during \"Chasing Zenmai\" quest."},
{GW::Constants::SkillID::Double_Dragon, 01, "Lale the Vindictive", GW::Constants::MapID::The_Undercity, {{237, 331}}, "Only during \"Chasing Zenmai\" quest."},
    {GW::Constants::SkillID::Mind_Freeze, 01, "Baubao Wavewrath", GW::Constants::MapID::The_Undercity, {{240, 281}}},
    {GW::Constants::SkillID::Elemental_Attunement, 01, "Chung, the Attuned", GW::Constants::MapID::The_Undercity, {{88, 331}}},
    {GW::Constants::SkillID::Flashing_Blades, 01, "Waeng", GW::Constants::MapID::The_Undercity, {{132, 307}}, "Only during \"Assist the Guards\" quest."},
    {GW::Constants::SkillID::Enraged_Smash, 01, "The Afflicted Ako", GW::Constants::MapID::Vizunah_Square_mission, {{635, 417}}, "Only during \"Vizunah Square\" mission."},
    {GW::Constants::SkillID::Broad_Head_Arrow, 01, "The Afflicted Huan ", GW::Constants::MapID::Vizunah_Square_mission, {{651, 417}}, "Only during \"Vizunah Square\" mission.\nDoesn't always appear."},
    {GW::Constants::SkillID::Ray_of_Judgment, 01, "The Afflicted Miju", GW::Constants::MapID::Vizunah_Square_mission, {{667, 417}}, "Only during \"Vizunah Square\" mission."},
    {GW::Constants::SkillID::Order_of_Apostasy, 01, "The Afflicted Thu", GW::Constants::MapID::Vizunah_Square_mission, {{683, 417}}, "Only during \"Vizunah Square\" mission."},
    {GW::Constants::SkillID::Stolen_Speed, 01, "The Afflicted Li Yun", GW::Constants::MapID::Vizunah_Square_mission, {{699, 417}}, "Only during \"Vizunah Square\" mission."},
    {GW::Constants::SkillID::Mind_Burn, 01, "The Afflicted Kam", GW::Constants::MapID::Vizunah_Square_mission, {{715, 417}}, "Only during \"Vizunah Square\" mission."},
    {GW::Constants::SkillID::Shadow_Form, 01, "The Afflicted Soon Kim", GW::Constants::MapID::Vizunah_Square_mission, {{731, 417}}, "Only during \"Vizunah Square\" mission."},
    {GW::Constants::SkillID::Weapon_of_Quickening, 01, "The Afflicted Hakaru", GW::Constants::MapID::Vizunah_Square_mission, {{747, 417}}, "Only during \"Vizunah Square\" mission."},
    {GW::Constants::SkillID::Enraged_Smash, 01, "The Afflicted Ako", GW::Constants::MapID::Shadows_Passage, {{717, 225}}},
    {GW::Constants::SkillID::Lyssas_Aura, 01, "Hai Jii", GW::Constants::MapID::Nahpui_Quarter_outpost_mission, {{369, 940}}, "Only during \"Nahpui Quarter\" mission."},
    {GW::Constants::SkillID::Lightning_Surge, 01, "Tahmu", GW::Constants::MapID::Nahpui_Quarter_outpost_mission, {{447, 925}}, "Only during \"Nahpui Quarter\" mission."},
    {GW::Constants::SkillID::Grenths_Balance, 01, "Kuonghsang", GW::Constants::MapID::Nahpui_Quarter_outpost_mission, {{501, 833}}, "Only during \"Nahpui Quarter\" mission."},
    {GW::Constants::SkillID::Signet_of_Judgment, 01, "Kaijun Don", GW::Constants::MapID::Nahpui_Quarter_outpost_mission, {{439, 797}}, "Only during \"Nahpui Quarter\" mission."},
    {GW::Constants::SkillID::Enraged_Lunge, 01, "Royen Beastkeeper", GW::Constants::MapID::Nahpui_Quarter_explorable, {{533, 870}}, "Not during \"Nahpui Quarter\" mission."},
    {GW::Constants::SkillID::Word_of_Healing, 01, "Ziinfaun Lifeforce", GW::Constants::MapID::Xaquang_Skyway, {{490, 567}}},
    {GW::Constants::SkillID::Animate_Flesh_Golem, 01, "Ghial the Bone Dancer", GW::Constants::MapID::Xaquang_Skyway, {{549, 524}}},
    {GW::Constants::SkillID::Seeping_Wound, 01, "Shreader Sharptongue", GW::Constants::MapID::Xaquang_Skyway, {{439, 670}}},
    {GW::Constants::SkillID::Tranquil_Was_Tanasen, 01, "Orosen, Tranquil Acolyte", GW::Constants::MapID::Xaquang_Skyway, {{590, 677}}},
    {GW::Constants::SkillID::Auspicious_Parry, 01, "Bound Jaizhanju", GW::Constants::MapID::Tahnnakai_Temple_outpost_mission, {{991, 555}}, "Only during \"Tahnnakai Temple\" mission.\nFifth boss encountered."},
    {GW::Constants::SkillID::Famine, 01, "Bound Zojun", GW::Constants::MapID::Tahnnakai_Temple_outpost_mission, {{1009, 555}}, "Only during \"Tahnnakai Temple\" mission.\nSixth boss encountered."},
    {GW::Constants::SkillID::Defiant_Was_Xinrae, 01, "Bound Kaolai", GW::Constants::MapID::Tahnnakai_Temple_outpost_mission, {{1027, 555}}, "Only during \"Tahnnakai Temple\" mission.\nSeventh boss encountered."},
    {GW::Constants::SkillID::Shroud_of_Silence, 01, "Bound Vizu", GW::Constants::MapID::Tahnnakai_Temple_outpost_mission, {{1044, 555}}, "Only during \"Tahnnakai Temple\" mission.\nLast boss encountered."},
    {GW::Constants::SkillID::Wail_of_Doom, 01, "Bound Naku", GW::Constants::MapID::Tahnnakai_Temple_outpost_mission, {{1009, 539}}, "Only during \"Tahnnakai Temple\" mission.\nSecond boss encountered."},
    {GW::Constants::SkillID::Star_Burst, 01, "Bound Teinai", GW::Constants::MapID::Tahnnakai_Temple_outpost_mission, {{1027, 539}}, "Only during \"Tahnnakai Temple\" mission.\nThird boss encountered."},
    {GW::Constants::SkillID::Arcane_Languor, 01, "Bound Kitah", GW::Constants::MapID::Tahnnakai_Temple_outpost_mission, {{991, 539}}, "Only during \"Tahnnakai Temple\" mission.\nFirst boss encountered."},
    {GW::Constants::SkillID::Spell_Breaker, 01, "Bound Karei", GW::Constants::MapID::Tahnnakai_Temple_outpost_mission, {{1044, 538}}, "Only during \"Tahnnakai Temple\" mission.\nFourth boss encountered."},
    {GW::Constants::SkillID::Wanderlust, 01, "Quansong Spiritspeak", GW::Constants::MapID::Tahnnakai_Temple_explorable, {{994, 413}}, "Not during \"A Meeting With the Emperor\" quest."},
    {GW::Constants::SkillID::Martyr, 01, "Rien, the Martyr", GW::Constants::MapID::Shenzun_Tunnels, {{681, 747}}},
    {GW::Constants::SkillID::Stolen_Speed, 01, "The Afflicted Li Yun", GW::Constants::MapID::Shenzun_Tunnels, {{788, 862}}},
    {GW::Constants::SkillID::Mind_Burn, 01, "The Afflicted Kam", GW::Constants::MapID::Shenzun_Tunnels, {{762, 678}}},
    {GW::Constants::SkillID::Flashing_Blades, 01, "Lou, of the Knives", GW::Constants::MapID::Shenzun_Tunnels, {{843, 744}}, "He is triggered by walking down the stairs into a small sewer full of putrid water, surrounded by Am Fah and Canthan peasants"},
    {GW::Constants::SkillID::Spirit_Channeling, 01, "Cho, Spirit Empath", GW::Constants::MapID::Shenzun_Tunnels, {{724, 879}}},
    {GW::Constants::SkillID::Hundred_Blades, 01, "Warrior's Construct", GW::Constants::MapID::Sunjiang_District_outpost_mission, {{995, 754}}, "Only during \"Sunjiang District\" mission.\nOnly 4 out of these 8 bosses will appear."},
    {GW::Constants::SkillID::Equinox, 01, "Ranger's Construct", GW::Constants::MapID::Sunjiang_District_outpost_mission, {{1007, 754}}, "Only during \"Sunjiang District\" mission.\nOnly 4 out of these 8 bosses will appear."},
    {GW::Constants::SkillID::Life_Sheath, 01, "Monk's Construct", GW::Constants::MapID::Sunjiang_District_outpost_mission, {{1019, 754}}, "Only during \"Sunjiang District\" mission.\nOnly 4 out of these 8 bosses will appear."},
    {GW::Constants::SkillID::Soul_Bind, 01, "Necromancer's Construct", GW::Constants::MapID::Sunjiang_District_outpost_mission, {{995, 766}}, "Only during \"Sunjiang District\" mission.\nOnly 4 out of these 8 bosses will appear."},
    {GW::Constants::SkillID::Psychic_Distraction, 01, "Mesmer's Construct", GW::Constants::MapID::Sunjiang_District_outpost_mission, {{1007, 766}}, "Only during \"Sunjiang District\" mission.\nOnly 4 out of these 8 bosses will appear."},
    {GW::Constants::SkillID::Mirror_of_Ice, 01, "Elemental's Construct", GW::Constants::MapID::Sunjiang_District_outpost_mission, {{1019, 766}}, "Only during \"Sunjiang District\" mission.\nOnly 4 out of these 8 bosses will appear."},
    {GW::Constants::SkillID::Temple_Strike, 01, "Assassin's Construct", GW::Constants::MapID::Sunjiang_District_outpost_mission, {{1007, 778}}, "Only during \"Sunjiang District\" mission.\nOnly 4 out of these 8 bosses will appear."},
    {GW::Constants::SkillID::Soul_Twisting, 01, "Ritualist's Construct", GW::Constants::MapID::Sunjiang_District_outpost_mission, {{1007, 742}}, "Only during \"Sunjiang District\" mission.\nOnly 4 out of these 8 bosses will appear."},
    {GW::Constants::SkillID::Hundred_Blades, 01, "Warrior's Construct", GW::Constants::MapID::Sunjiang_District_explorable, {{968, 742}}, "Not during \"Sunjiang District\" mission."},
    {GW::Constants::SkillID::Equinox, 01, "Ranger's Construct", GW::Constants::MapID::Sunjiang_District_explorable, {{1002, 853}}, "Not during \"Sunjiang District\" mission."},
    {GW::Constants::SkillID::Life_Sheath, 01, "Monk's Construct", GW::Constants::MapID::Sunjiang_District_explorable, {{1029, 724}}, "Not during \"Sunjiang District\" mission."},
    {GW::Constants::SkillID::Soul_Bind, 01, "Necromancer's Construct", GW::Constants::MapID::Sunjiang_District_explorable, {{1012, 804}}, "Not during \"Sunjiang District\" mission."},
    {GW::Constants::SkillID::Psychic_Distraction, 01, "Mesmer's Construct", GW::Constants::MapID::Sunjiang_District_explorable, {{1034, 778}}, "Not during \"Sunjiang District\" mission."},
    {GW::Constants::SkillID::Mirror_of_Ice, 01, "Elemental's Construct", GW::Constants::MapID::Sunjiang_District_explorable, {{1062, 777}}, "Not during \"Sunjiang District\" mission."},
    {GW::Constants::SkillID::Battle_Rage, 01, "Arrahhsh Mountainclub", GW::Constants::MapID::Pongmei_Valley, {{1103, 1075}}},
    {GW::Constants::SkillID::Spike_Trap, 01, "Meynsang the Sadistic", GW::Constants::MapID::Pongmei_Valley, {{1062, 924}}},
    {GW::Constants::SkillID::Assassins_Promise, 01, "Xuekao, the Deceptive", GW::Constants::MapID::Pongmei_Valley, {{975, 972}}},
    {GW::Constants::SkillID::Weapon_of_Quickening, 01, "The Afflicted Hakaru", GW::Constants::MapID::Pongmei_Valley, {{1088, 962}}},
    {GW::Constants::SkillID::Auspicious_Parry, 01, "Sword Ancient Kai", GW::Constants::MapID::Raisu_Palace_outpost_mission, {{592, 166}}, "Only during \"Raisu Palace\" mission."},
    {GW::Constants::SkillID::Famine, 01, "Famished Ancient Brrne", GW::Constants::MapID::Raisu_Palace_outpost_mission, {{682, 168}}, "Only during \"Raisu Palace\" mission."},
    {GW::Constants::SkillID::Spell_Breaker, 01, "Untouched Ancient Ky", GW::Constants::MapID::Raisu_Palace_outpost_mission, {{842, 148}}, "Only during \"Raisu Palace\" mission."},
    {GW::Constants::SkillID::Wail_of_Doom, 01, "Doomed Ancient Kkraz", GW::Constants::MapID::Raisu_Palace_outpost_mission, {{652, 164}}, "Only during \"Raisu Palace\" mission."},
    {GW::Constants::SkillID::Arcane_Languor, 01, "Arcane Ancient Phi", GW::Constants::MapID::Raisu_Palace_outpost_mission, {{842, 165}}, "Only during \"Raisu Palace\" mission."},
    {GW::Constants::SkillID::Star_Burst, 01, "Star Ancient Koosun", GW::Constants::MapID::Raisu_Palace_outpost_mission, {{708, 163}}, "Only during \"Raisu Palace\" mission."},
    {GW::Constants::SkillID::Shroud_of_Silence, 01, "Silent Ancient Onata", GW::Constants::MapID::Raisu_Palace_outpost_mission, {{842, 182}}, "Only during \"Raisu Palace\" mission."},
    {GW::Constants::SkillID::Defiant_Was_Xinrae, 01, "Defiant Ancient Sseer", GW::Constants::MapID::Raisu_Palace_outpost_mission, {{824, 123}}, "Only during \"Raisu Palace\" mission."},
    {GW::Constants::SkillID::Auspicious_Parry, 01, "Sword Ancient Kai", GW::Constants::MapID::Raisu_Palace, {{622, 164}}, "Not during \"Raisu Palace\" mission."},
    {GW::Constants::SkillID::Famine, 01, "Famished Ancient Brrne", GW::Constants::MapID::Raisu_Palace, {{682, 153}}, "Not during \"Raisu Palace\" mission."},
    {GW::Constants::SkillID::Spell_Breaker, 01, "Untouched Ancient Ky", GW::Constants::MapID::Raisu_Palace, {{671, 88}}, "Not during \"Raisu Palace\" mission."},
    {GW::Constants::SkillID::Wail_of_Doom, 01, "Doomed Ancient Kkraz", GW::Constants::MapID::Raisu_Palace, {{743, 191}}, "Not during \"Raisu Palace\" mission."},
    {GW::Constants::SkillID::Arcane_Languor, 01, "Arcane Ancient Phi", GW::Constants::MapID::Raisu_Palace, {{744, 139}}, "Not during \"Raisu Palace\" mission."},
    {GW::Constants::SkillID::Star_Burst, 01, "Star Ancient Koosun", GW::Constants::MapID::Raisu_Palace, {{638, 186}}, "Not during \"Raisu Palace\" mission."},
    {GW::Constants::SkillID::Shroud_of_Silence, 01, "Silent Ancient Onata", GW::Constants::MapID::Raisu_Palace, {{734, 102}}, "Not during \"Raisu Palace\" mission."},
    {GW::Constants::SkillID::Soul_Twisting, 01, "Defiant Ancient Sseer", GW::Constants::MapID::Raisu_Palace, {{580, 148}}, "Not during \"Raisu Palace\" mission."},
    {GW::Constants::SkillID::Seven_Weapon_Stance, 01, "Blade Ancient Syu-Shai", GW::Constants::MapID::Raisu_Palace, {{743, 165}}, "Not during \"Raisu Palace\" mission.\nOnly if a player has a \"Proof of Triumph\" in their inventory."},
    {GW::Constants::SkillID::Weapons_of_Three_Forges, 01, "Blade Ancient Syu-Shai", GW::Constants::MapID::Raisu_Palace, {{755, 165}}, "Not during \"Raisu Palace\" mission.\nOnly if a player has a \"Proof of Triumph\" in their inventory"},
    {GW::Constants::SkillID::Shadow_Theft, 01, "Blade Ancient Syu-Shai", GW::Constants::MapID::Raisu_Palace, {{767, 165}}, "Not during \"Raisu Palace\" mission.\nOnly if a player has a \"Proof of Triumph\" in their inventory"},
    {GW::Constants::SkillID::Forceful_Blow, 02, "Stone Judge", GW::Constants::MapID::Arborstone_outpost_mission, {{241, 341}}, "Only during \"Arborstone\" mission."},
    {GW::Constants::SkillID::Lacerate, 02, "Ryver Mossplanter", GW::Constants::MapID::Arborstone_outpost_mission, {{135, 238}}, "Only during \"Arborstone\" mission.\nUsually behind the urn altar."},
    {GW::Constants::SkillID::Shatterstone, 02, "The Ancient", GW::Constants::MapID::Arborstone_outpost_mission, {{206, 204}}, "Only during \"Arborstone\" mission."},
    {GW::Constants::SkillID::Vampiric_Spirit, 02, "Dark Fang", GW::Constants::MapID::Arborstone_outpost_mission, {{116, 262}}, "Only during \"Arborstone\" mission."},
    {GW::Constants::SkillID::Forceful_Blow, 02, "Mahr Stonebreaker", GW::Constants::MapID::Arborstone_explorable, {{201, 223}}, "Not during \"Arborstone\" mission."},
    {GW::Constants::SkillID::Lacerate, 02, "Ryver Mossplanter", GW::Constants::MapID::Arborstone_explorable, {{186, 286}}, "Not during \"Arborstone\" mission and \"Song and Stone\" quest."},
    {GW::Constants::SkillID::Shield_of_Regeneration, 02, "Meril Stoneweaver", GW::Constants::MapID::Arborstone_explorable, {{119, 301}}, "Not during \"Arborstone\" mission."},
    {GW::Constants::SkillID::Vampiric_Spirit, 02, "Kaswa Webstrider", GW::Constants::MapID::Arborstone_explorable, {{60, 284}}, "Not during \"Arborstone\" mission."},
    {GW::Constants::SkillID::Lingering_Curse, 02, "Craw Stonereap", GW::Constants::MapID::Arborstone_explorable, {{165, 334}}, "Not during \"Arborstone\" mission."},
    {GW::Constants::SkillID::Shockwave, 02, "Zarnas Stonewrath", GW::Constants::MapID::Arborstone_explorable, {{136, 330}}, "Not during \"Arborstone\" mission."},
    {GW::Constants::SkillID::Forceful_Blow, 02, "Dorn Stonebreaker", GW::Constants::MapID::Altrumm_Ruins, {{170, 455}}, "Only during the challenge mission."},
    {GW::Constants::SkillID::Shield_of_Regeneration, 02, "Ennsa Stoneweaver", GW::Constants::MapID::Altrumm_Ruins, {{120, 459}}, "Only during the challenge mission."},
    {GW::Constants::SkillID::Weaken_Knees, 02, "Froth Stonereap", GW::Constants::MapID::Altrumm_Ruins, {{173, 410}}, "Only during the challenge mission."},
    {GW::Constants::SkillID::Shockwave, 02, "Azukhan Stonewrath", GW::Constants::MapID::Altrumm_Ruins, {{126, 377}}, "Only during the challenge mission."},
    {GW::Constants::SkillID::Shove, 02, "Tarnen the Bully", GW::Constants::MapID::Ferndale, {{304, 471}}},
    {GW::Constants::SkillID::Boon_Signet, 02, "Mungri Magicbox", GW::Constants::MapID::Ferndale, {{425, 430}}, "Patrols this area."},
    {GW::Constants::SkillID::Plague_Signet, 02, "Hargg Plaguebinder", GW::Constants::MapID::Ferndale, {{391, 557}}},
    {GW::Constants::SkillID::Mantra_of_Recovery, 02, "Mugra Swiftspell", GW::Constants::MapID::Ferndale, {{357, 620}}},
    {GW::Constants::SkillID::Energy_Boon, 02, "Tarlok Evermind", GW::Constants::MapID::Ferndale, {{449, 460}}},
    {GW::Constants::SkillID::Beguiling_Haze, 02, "Warden of Saprophytes", GW::Constants::MapID::Ferndale, {{324, 556}}, "Only before completing \"Temple of the Dredge\" quest."},
    {GW::Constants::SkillID::Aura_of_Displacement, 02, "Maximole", GW::Constants::MapID::Ferndale, {{397, 460}}, "Only during \"Revolt of the Dredge\" quest.\nPatrols this area."},
    {GW::Constants::SkillID::Aura_of_Displacement, 02, "Urkal the Ambusher", GW::Constants::MapID::Ferndale, {{367, 410}}},
    {GW::Constants::SkillID::Signet_of_Spirits, 02, "Wagg Spiritspeak", GW::Constants::MapID::Ferndale, {{411, 504}}},
    {GW::Constants::SkillID::Barrage, 02, "Chkkr Thousand Tail", GW::Constants::MapID::Drazach_Thicket, {{328, 760}}},
    {GW::Constants::SkillID::Icy_Veins, 02, "Bazzr Icewing", GW::Constants::MapID::Drazach_Thicket, {{281, 917}}},
    {GW::Constants::SkillID::Crippling_Anguish, 02, "Bezzr Wingstorm", GW::Constants::MapID::Drazach_Thicket, {{317, 926}}},
    {GW::Constants::SkillID::Attuned_Was_Songkai, 02, "The Skill Eater", GW::Constants::MapID::Drazach_Thicket, {{383, 941}}},
    {GW::Constants::SkillID::Clamor_of_Souls, 02, "The Pain Eater", GW::Constants::MapID::Drazach_Thicket, {{383, 924}}},
    {GW::Constants::SkillID::Archers_Signet, 02, "Nundak The Archer", GW::Constants::MapID::Melandrus_Hope, {{689, 734}}, "Patrols clockwise in a small circle.\nTogether with Rot Foulbelly."},
    {GW::Constants::SkillID::Weaken_Knees, 02, "Rot Foulbelly", GW::Constants::MapID::Melandrus_Hope, {{705, 734}}, "Patrols clockwise in a small circle.\nTogether with Nundak The Archer."},
    {GW::Constants::SkillID::Empathic_Removal, 02, "Byzzr Wingmender", GW::Constants::MapID::Melandrus_Hope, {{514, 641}}},
    {GW::Constants::SkillID::Heal_as_One, 02, "Salke Fur Friend", GW::Constants::MapID::Melandrus_Hope, {{655, 582}}},
    {GW::Constants::SkillID::Locusts_Fury, 02, "Chkkr Locust Lord", GW::Constants::MapID::Melandrus_Hope, {{565, 651}}},
    {GW::Constants::SkillID::Spirit_Light_Weapon, 02, "Chkkr Brightclaw", GW::Constants::MapID::Melandrus_Hope, {{614, 569}}},
    {GW::Constants::SkillID::Enraged_Smash, 02, "The Afflicted Maaka", GW::Constants::MapID::The_Eternal_Grove_outpost_mission, {{440, 1013}}, "Only during \"The Eternal Grove\" mission."},
    {GW::Constants::SkillID::Shadow_Form, 02, "The Afflicted Senku", GW::Constants::MapID::The_Eternal_Grove_outpost_mission, {{440, 1028}}, "Only during \"The Eternal Grove\" mission."},
    {GW::Constants::SkillID::Weapon_of_Quickening, 02, "The Afflicted Xenxo", GW::Constants::MapID::The_Eternal_Grove_outpost_mission, {{440, 1043}}, "Only during \"The Eternal Grove\" mission."},
    {GW::Constants::SkillID::Devastating_Hammer, 02, "Tembarr Treefall", GW::Constants::MapID::The_Eternal_Grove, {{230, 1075}}},
    {GW::Constants::SkillID::Healing_Burst, 02, "The Scar Eater", GW::Constants::MapID::The_Eternal_Grove, {{500, 1111}}},
    {GW::Constants::SkillID::Air_of_Enchantment, 02, "Jayne Forestlight", GW::Constants::MapID::The_Eternal_Grove, {{401, 1066}}},
    {GW::Constants::SkillID::Echo, 02, "The Time Eater", GW::Constants::MapID::The_Eternal_Grove, {{419, 1103}}},
    {GW::Constants::SkillID::Shatterstone, 02, "Wiseroot Shatterstone", GW::Constants::MapID::The_Eternal_Grove, {{546, 1133}}},
    {GW::Constants::SkillID::Moebius_Strike, 02, "Bramble Everthorn", GW::Constants::MapID::The_Eternal_Grove, {{395, 1010}}},
    {GW::Constants::SkillID::Preservation, 02, "Flower Spiritgarden", GW::Constants::MapID::The_Eternal_Grove, {{423, 1137}}},
    {GW::Constants::SkillID::Ritual_Lord, 02, "Spiritroot Mossbeard", GW::Constants::MapID::The_Eternal_Grove, {{309, 1127}}},
    {GW::Constants::SkillID::Primal_Rage, 02, "Strongroot Tanglebranch", GW::Constants::MapID::Morostav_Trail, {{846, 1004}}},
    {GW::Constants::SkillID::Charge, 02, "Sunreach Warmaker", GW::Constants::MapID::Morostav_Trail, {{836, 1112}}},
    {GW::Constants::SkillID::Glass_Arrows, 02, "Nandet, Glass Weaver", GW::Constants::MapID::Morostav_Trail, {{613, 1087}}},
    {GW::Constants::SkillID::Quick_Shot, 02, "Inallay Splintercall", GW::Constants::MapID::Morostav_Trail, {{664, 1114}}},
    {GW::Constants::SkillID::Cultists_Fervor, 02, "Kyril Oathwarden", GW::Constants::MapID::Morostav_Trail, {{791, 1108}}},
    {GW::Constants::SkillID::Tainted_Flesh, 02, "Konrru, Tainted Stone", GW::Constants::MapID::Morostav_Trail, {{727, 1126}}},
    {GW::Constants::SkillID::Shockwave, 02, "Arbor Earthcall", GW::Constants::MapID::Morostav_Trail, {{763, 1088}}},
    {GW::Constants::SkillID::Beguiling_Haze, 02, "Falaharn Mistwarden", GW::Constants::MapID::Morostav_Trail, {{859, 1097}}, "Only appears when a character approaches his location."},
    {GW::Constants::SkillID::Soul_Twisting, 02, "Ritualist's Construct", GW::Constants::MapID::Morostav_Trail, {{882, 990}}},
    {GW::Constants::SkillID::Cleave, 02, "Chkkr Ironclaw", GW::Constants::MapID::Mourning_Veil_Falls, {{643, 1289}}},
    {GW::Constants::SkillID::Spoil_Victor, 02, "Foalcrest Darkwish", GW::Constants::MapID::Mourning_Veil_Falls, {{340, 1276}}},
    {GW::Constants::SkillID::Recurring_Insecurity, 02, "Xisni Dream Haunt", GW::Constants::MapID::Mourning_Veil_Falls, {{696, 1244}}},
    {GW::Constants::SkillID::Shared_Burden, 02, "Deeproot Sorrow", GW::Constants::MapID::Mourning_Veil_Falls, {{723, 1306}}},
    {GW::Constants::SkillID::Energy_Surge, 02, "Milefaun Mindflayer", GW::Constants::MapID::Mourning_Veil_Falls, {{375, 1208}}},
    {GW::Constants::SkillID::Gust, 02, "Rahse Windcatcher", GW::Constants::MapID::Mourning_Veil_Falls, {{721, 1224}}},
    {GW::Constants::SkillID::Obsidian_Flesh, 02, "Bizzr Ironshell", GW::Constants::MapID::Mourning_Veil_Falls, {{539, 1257}}},
    {GW::Constants::SkillID::Dark_Apostasy, 02, "Darkroot Entrop", GW::Constants::MapID::Mourning_Veil_Falls, {{408, 1287}}},
    {GW::Constants::SkillID::Vengeful_Was_Khanhei, 02, "Bazzr Dustwing", GW::Constants::MapID::Mourning_Veil_Falls, {{746, 1268}}},
    {GW::Constants::SkillID::Dragon_Slash, 03, "Seaguard Eli", GW::Constants::MapID::Boreas_Seabed_outpost_mission, {{219, 52}}, "Only during \"Boreas Seabed\" mission."},
    {GW::Constants::SkillID::Barrage, 03, "Aurora", GW::Constants::MapID::Boreas_Seabed_outpost_mission, {{235, 52}}, "Only during \"Boreas Seabed\" mission."},
    {GW::Constants::SkillID::Broad_Head_Arrow, 03, "Daeman", GW::Constants::MapID::Boreas_Seabed_outpost_mission, {{250, 52}}, "Only during \"Boreas Seabed\" mission."},
    {GW::Constants::SkillID::Life_Sheath, 03, "Seaguard Gita", GW::Constants::MapID::Boreas_Seabed_outpost_mission, {{219, 68}}, "Only during \"Boreas Seabed\" mission."},
    {GW::Constants::SkillID::Power_Leech, 03, "Seaguard Hala", GW::Constants::MapID::Boreas_Seabed_outpost_mission, {{235, 68}}, "Only during \"Boreas Seabed\" mission."},
    {GW::Constants::SkillID::Mind_Burn, 03, "Argo", GW::Constants::MapID::Boreas_Seabed_outpost_mission, {{250, 68}}, "Only during \"Boreas Seabed\" mission."},
    {GW::Constants::SkillID::Whirling_Axe, 03, "Geoffer Pain Bringer", GW::Constants::MapID::Boreas_Seabed_explorable, {{424, 152}}, "Not during \"Boreas Seabed\" mission and \"Protect the Halcyon\" quest."},
    {GW::Constants::SkillID::Dragon_Slash, 03, "Sskai, Dragon's Birth", GW::Constants::MapID::Boreas_Seabed_explorable, {{503, 75}}, "Not during \"Boreas Seabed\" mission."},
    {GW::Constants::SkillID::Ferocious_Strike, 03, "Milodestus the Wrangler", GW::Constants::MapID::Boreas_Seabed_explorable, {{522, 156}}, "Not during \"Boreas Seabed\" mission."},
    {GW::Constants::SkillID::Way_of_the_Empty_Palm, 03, "Ilidus of the Empty Palm", GW::Constants::MapID::Boreas_Seabed_explorable, {{542, 28}}, "Not during \"Boreas Seabed\" mission.\nDuring \"The Halcyon Job\" quest this boss does not spawn; however, Senkai spawns at this location and has the same elite skill."},
    {GW::Constants::SkillID::Way_of_the_Empty_Palm, 03, "Senkai, Lord of the 1,000 Daggers Guild", GW::Constants::MapID::Boreas_Seabed_explorable, {{530, 28}}, "Not during \"Boreas Seabed\" mission.\nOnly during \"The Halcyon Job\" quest."},
    {GW::Constants::SkillID::Melandrus_Shot, 03, "Razortongue Frothspit", GW::Constants::MapID::Archipelagos, {{492, 327}}},
    {GW::Constants::SkillID::Healing_Light, 03, "KaySey Stormray", GW::Constants::MapID::Archipelagos, {{541, 342}}},
    {GW::Constants::SkillID::Blessed_Light, 03, "Ssuns, Blessed of Dwayna", GW::Constants::MapID::Archipelagos, {{582, 423}}},
    {GW::Constants::SkillID::Discord, 03, "Xiriss Stickleback", GW::Constants::MapID::Archipelagos, {{575, 448}}, "Only during \"Night Raiders\" quest."},
    {GW::Constants::SkillID::Energy_Drain, 03, "Siska Scalewand", GW::Constants::MapID::Archipelagos, {{484, 385}}},
    {GW::Constants::SkillID::Second_Wind, 03, "Snapjaw Windshell", GW::Constants::MapID::Archipelagos, {{460, 440}}},
    {GW::Constants::SkillID::Grasping_Was_Kuurong, 03, "Ssyn Coiled Grasp", GW::Constants::MapID::Archipelagos, {{490, 417}}},
    {GW::Constants::SkillID::Escape, 03, "Stsou Swiftscale", GW::Constants::MapID::Maishang_Hills, {{743, 271}}, "If you entered Maishang Hills having \"Attack the Kurzicks\" quest neither this boss nor any normal enemies around Gyala Hatchery will appear."},
    {GW::Constants::SkillID::Discord, 03, "Sessk, Woe Spreader", GW::Constants::MapID::Maishang_Hills, {{874, 214}}},
    {GW::Constants::SkillID::Unsteady_Ground, 03, "Seacrash, Elder Guardian", GW::Constants::MapID::Maishang_Hills, {{591, 177}}},
    {GW::Constants::SkillID::Lightning_Surge, 03, "Sarss, Stormscale", GW::Constants::MapID::Maishang_Hills, {{723, 158}}},
    {GW::Constants::SkillID::Siphon_Strength, 03, "Ssaresh Rattler", GW::Constants::MapID::Maishang_Hills, {{580, 195}}, "Not during \"Stolen Eggs\" quest."},
    {GW::Constants::SkillID::Coward, 03, "Kayali the Brave", GW::Constants::MapID::Mount_Qinkai, {{268, 300}}},
    {GW::Constants::SkillID::Battle_Rage, 03, "Arrahhsh Mountainclub", GW::Constants::MapID::Mount_Qinkai, {{92, 203}}, "Only during \"Return of the Yeti\" quest."},
    {GW::Constants::SkillID::Trappers_Focus, 03, "Chehbaba Roottripper", GW::Constants::MapID::Mount_Qinkai, {{31, 329}}},
    {GW::Constants::SkillID::Withdraw_Hexes, 03, "Hukhrah Earthslove", GW::Constants::MapID::Mount_Qinkai, {{76, 207}}},
    {GW::Constants::SkillID::Consume_Soul, 03, "Tomton Spiriteater", GW::Constants::MapID::Mount_Qinkai, {{44, 270}}},
    {GW::Constants::SkillID::Primal_Rage, 03, "Reefclaw Ragebound", GW::Constants::MapID::Gyala_Hatchery, {{787, 590}}, "Not during \"Gyala Hatchery\" mission."},
    {GW::Constants::SkillID::Charge, 03, "Jacqui The Reaver", GW::Constants::MapID::Gyala_Hatchery, {{751, 542}}, "Not during \"Gyala Hatchery\" mission."},
    {GW::Constants::SkillID::Tainted_Flesh, 03, "Lukrker Foulfist", GW::Constants::MapID::Gyala_Hatchery, {{891, 403}}, "Not during \"Gyala Hatchery\" mission."},
    {GW::Constants::SkillID::Echo, 03, "Mohby Windbeak", GW::Constants::MapID::Gyala_Hatchery, {{786, 512}}, "Not during \"Gyala Hatchery\" mission."},
    {GW::Constants::SkillID::Shockwave, 03, "Bahnba Shockfoot", GW::Constants::MapID::Gyala_Hatchery, {{844, 465}}, "Not during \"Gyala Hatchery\" mission."},
    {GW::Constants::SkillID::Obsidian_Flesh, 03, "Whyk Steelshell", GW::Constants::MapID::Gyala_Hatchery, {{728, 528}}, "Not during \"Gyala Hatchery\" mission."},
    {GW::Constants::SkillID::Beguiling_Haze, 03, "Razorfang Hazeclaw", GW::Constants::MapID::Gyala_Hatchery, {{880, 469}}, "Not during \"Gyala Hatchery\" mission."},
    {GW::Constants::SkillID::Preservation, 03, "Soulwhisper, Elder Guardian", GW::Constants::MapID::Gyala_Hatchery, {{761, 618}}, "Not during \"Gyala Hatchery\" mission."},
    {GW::Constants::SkillID::Glass_Arrows, 03, "Lorelle Jade Cutter", GW::Constants::MapID::Rheas_Crater, {{1060, 661}}},
    {GW::Constants::SkillID::Cleave, 03, "Razorfin Fleshrend", GW::Constants::MapID::Rheas_Crater, {{998, 838}}},
    {GW::Constants::SkillID::Healing_Burst, 03, "Incetol, Devout of Depths", GW::Constants::MapID::Rheas_Crater, {{972, 579}}},
    {GW::Constants::SkillID::Cultists_Fervor, 03, "Cultist Milthuran", GW::Constants::MapID::Rheas_Crater, {{1097, 722}}},
    {GW::Constants::SkillID::Recurring_Insecurity, 03, "Talous the Mad", GW::Constants::MapID::Rheas_Crater, {{989, 776}}},
    {GW::Constants::SkillID::Shatterstone, 03, "Wavecrest Stonebreak", GW::Constants::MapID::Rheas_Crater, {{1030, 742}}, "Wanders around the eastern perimeter once you go near the spawn point.\nGroup sometimes gets attacked by other monsters but usually wins the fight."},
    {GW::Constants::SkillID::Dark_Apostasy, 03, "Arius, Dark Apostle", GW::Constants::MapID::Rheas_Crater, {{975, 673}}, "Depending on where you entered Arius could have already been killed by other mobs by the time you reach this location.\nStill possible to capture the skill."},
    {GW::Constants::SkillID::Vengeful_Was_Khanhei, 03, "Delic the Vengeance Seeker", GW::Constants::MapID::Rheas_Crater, {{996, 612}}},
    {GW::Constants::SkillID::Devastating_Hammer, 03, "Sentasi, The Jade Maul", GW::Constants::MapID::Silent_Surf, {{784, 781}}},
    {GW::Constants::SkillID::Quick_Shot, 03, "Razorjaw Longspine", GW::Constants::MapID::Silent_Surf, {{765, 810}}},
    {GW::Constants::SkillID::Air_of_Enchantment, 03, "Miella Lightwing", GW::Constants::MapID::Silent_Surf, {{812, 981}}, "Patrols the area just outside Unwaking Waters (Luxon)."},
    {GW::Constants::SkillID::Signet_of_Judgment, 03, "Scourgewind, Elder Guardian", GW::Constants::MapID::Silent_Surf, {{856, 703}}},
    {GW::Constants::SkillID::Spoil_Victor, 03, "Sourbeak Rotshell", GW::Constants::MapID::Silent_Surf, {{779, 929}}},
    {GW::Constants::SkillID::Shared_Burden, 03, "Kenrii Sea Sorrow", GW::Constants::MapID::Silent_Surf, {{772, 878}}},
    {GW::Constants::SkillID::Gust, 03, "Amadis, Wind of the Sea", GW::Constants::MapID::Silent_Surf, {{818, 902}}, "Patrols off the southern coast of the middle island.\nGets into fight with several groups of jade fish and will be killed when nearly back to the starting point."},
    {GW::Constants::SkillID::Temple_Strike, 03, "Assassin's Construct", GW::Constants::MapID::Silent_Surf, {{830, 847}}},
    {GW::Constants::SkillID::Ritual_Lord, 03, "Whispering Ritual Lord", GW::Constants::MapID::Silent_Surf, {{868, 953}}, "Not during \"The Dragon Hunter\" quest.\nMakes a small rotation south of where the map indicates."},
    {GW::Constants::SkillID::Broad_Head_Arrow, 03, "The Afflicted Pana", GW::Constants::MapID::Unwaking_Waters_mission, {{632, 968}}, "Only during \"Unwaking Waters\" mission.\nOne of the attacking Afflicted."},
    {GW::Constants::SkillID::Order_of_Apostasy, 03, "The Afflicted Lau", GW::Constants::MapID::Unwaking_Waters_mission, {{618, 968}}, "Only during \"Unwaking Waters\" mission.\nOne of the attacking Afflicted."},
    {GW::Constants::SkillID::Stolen_Speed, 03, "The Afflicted Hsin Jun", GW::Constants::MapID::Unwaking_Waters_mission, {{603, 968}}, "Only during \"Unwaking Waters\" mission.\nOne of the attacking Afflicted."},
    {GW::Constants::SkillID::Mind_Burn, 03, "The Afflicted Shen", GW::Constants::MapID::Unwaking_Waters_mission, {{588, 968}}, "Only during \"Unwaking Waters\" mission.\nOne of the attacking Afflicted."},
    {GW::Constants::SkillID::Shadow_Form, 03, "The Afflicted Senku", GW::Constants::MapID::Unwaking_Waters_mission, {{572, 968}}, "Only during \"Unwaking Waters\" mission.\nOne of the attacking Afflicted."},
    {GW::Constants::SkillID::Weapon_of_Quickening, 03, "The Afflicted Xenxo", GW::Constants::MapID::Unwaking_Waters_mission, {{557, 968}}, "Only during \"Unwaking Waters\" mission.\nOne of the attacking Afflicted."},
    {GW::Constants::SkillID::Charge, 03, "Merki The Reaver", GW::Constants::MapID::Unwaking_Waters, {{664, 994}}, "Not during \"Unwaking Waters\" mission."},
    {GW::Constants::SkillID::Cultists_Fervor, 03, "Cultist Rajazan", GW::Constants::MapID::Unwaking_Waters, {{526, 1047}}, "Not during \"Unwaking Waters\" mission."},
    {GW::Constants::SkillID::Psychic_Instability, 03, "Chazek Plague Herder", GW::Constants::MapID::Unwaking_Waters, {{586, 1026}}, "Not during \"Unwaking Waters\" mission."},
    {GW::Constants::SkillID::Ride_the_Lightning, 03, "Kunvie Firewing", GW::Constants::MapID::Unwaking_Waters, {{553, 1020}}, "Not during \"Unwaking Waters\" mission."},
    {GW::Constants::SkillID::Shadow_Shroud, 03, "Shrouded Oni", GW::Constants::MapID::Unwaking_Waters, {{578, 1083}}, "Not during \"Unwaking Waters\" mission."},
    {GW::Constants::SkillID::Broad_Head_Arrow, 01, "The Afflicted Huan", GW::Constants::MapID::Sunjiang_District_outpost_mission, {{1069, 825}}},
    {GW::Constants::SkillID::Shadow_Form, 01, "The Afflicted Soon Kim", GW::Constants::MapID::Sunjiang_District_outpost_mission, {{1091, 718}}, "Not during \"Count's Daughter\" quest."},
    {GW::Constants::SkillID::Mark_of_Protection, 01, "Kepkhet Marrowfeast", GW::Constants::MapID::Prophets_Path, {{359, 1088}}},
    {GW::Constants::SkillID::Marksmans_Wager, 01, "Custodian Kora", GW::Constants::MapID::Dunes_of_Despair, {{135, 1537}}, "One of the bosses at the end of the mission."},
    {GW::Constants::SkillID::Word_of_Healing, 01, "Dassk Arossyss", GW::Constants::MapID::Dunes_of_Despair, {{106, 1533}}, "One of the bosses at the end of the mission."},
    {GW::Constants::SkillID::Order_of_the_Vampire, 01, "Byssha Hisst", GW::Constants::MapID::Dunes_of_Despair, {{156, 1536}}, "One of the bosses at the end of the mission."},
    {GW::Constants::SkillID::Ether_Renewal, 01, "Cyss Gresshla", GW::Constants::MapID::Dunes_of_Despair, {{103, 1558}}, "One of the bosses at the end of the mission."},
    {GW::Constants::SkillID::Mantra_of_Recovery, 01, "Ayassah Hess", GW::Constants::MapID::Dunes_of_Despair, {{136, 1434}}},
    {GW::Constants::SkillID::Ether_Renewal, 01, "Issah Sshay", GW::Constants::MapID::Thirsty_River, {{923, 1396}}},
    {GW::Constants::SkillID::Mantra_of_Recovery, 01, "Goss Aleessh", GW::Constants::MapID::Thirsty_River, {{977, 1342}}},
    {GW::Constants::SkillID::Order_of_the_Vampire, 01, "Hessper Sasso", GW::Constants::MapID::Thirsty_River, {{921, 1357}}},
    {GW::Constants::SkillID::Word_of_Healing, 01, "Josso Essher", GW::Constants::MapID::Thirsty_River, {{892, 1390}}},
    {GW::Constants::SkillID::Marksmans_Wager, 01, "Custodian Phebus", GW::Constants::MapID::Thirsty_River, {{854, 1396}}},
    {GW::Constants::SkillID::Warriors_Endurance, 01, "Custodian Hulgar", GW::Constants::MapID::Thirsty_River, {{862, 1352}}},
    {GW::Constants::SkillID::Warriors_Endurance, 01, "Custodian Dellus", GW::Constants::MapID::Elona_Reach, {{386, 739}}, "Spawns in one of the six locations."},
    {GW::Constants::SkillID::Mantra_of_Recovery, 01, "Tiss Danssir", GW::Constants::MapID::Elona_Reach, {{400, 739}}, "Spawns in one of the six locations."},
    {GW::Constants::SkillID::Marksmans_Wager, 01, "Custodian Jenus", GW::Constants::MapID::Elona_Reach, {{414, 739}}, "Spawns in one of the six locations."},
    {GW::Constants::SkillID::Word_of_Healing, 01, "Wissper Inssani", GW::Constants::MapID::Elona_Reach, {{428, 739}}, "Spawns in one of the six locations."},
    {GW::Constants::SkillID::Order_of_the_Vampire, 01, "Uussh Visshta", GW::Constants::MapID::Elona_Reach, {{442, 739}}, "Spawns in one of the six locations."},
    {GW::Constants::SkillID::Ether_Renewal, 01, "Vassa Ssiss", GW::Constants::MapID::Elona_Reach, {{456, 739}}, "Spawns in one of the six locations."},
    {GW::Constants::SkillID::Crippling_Anguish, 01, "Yxthoshth", GW::Constants::MapID::Salt_Flats, {{354, 540}}, "Only during \"The Ranger's Path\" quest."},
    {GW::Constants::SkillID::Lightning_Surge, 01, "Facet of Elements", GW::Constants::MapID::The_Dragons_Lair, {{383, 252}}, "Facet number 5."},
    {GW::Constants::SkillID::Mantra_of_Recall, 01, "Facet of Chaos", GW::Constants::MapID::The_Dragons_Lair, {{239, 284}}, "Facet number 3."},
    {GW::Constants::SkillID::Grenths_Balance, 01, "Facet of Darkness", GW::Constants::MapID::The_Dragons_Lair, {{318, 304}}, "Facet number 4."},
    {GW::Constants::SkillID::Shield_of_Regeneration, 01, "Facet of Light", GW::Constants::MapID::The_Dragons_Lair, {{267, 128}}, "Facet number 1."},
    {GW::Constants::SkillID::Melandrus_Resilience, 01, "Facet of Nature", GW::Constants::MapID::The_Dragons_Lair, {{218, 211}}, "Facet number 2."},
    {GW::Constants::SkillID::Gladiators_Defense, 01, "Facet of Strength", GW::Constants::MapID::The_Dragons_Lair, {{363, 148}}, "Facet number 6."},
    {GW::Constants::SkillID::Life_Barrier, 02, "Quickroot", GW::Constants::MapID::Lornars_Pass, {{121, 64}, {187, 61}, {85, 447}}},
    {GW::Constants::SkillID::Offering_of_Blood, 02, "Tonfor Copperblood", GW::Constants::MapID::Lornars_Pass, {{45, 308}, {201, 444}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Defy_Pain, 02, "Clobberhusk", GW::Constants::MapID::Lornars_Pass, {{97, 447}, {109, 64}, {199, 61}}},
    {GW::Constants::SkillID::Crippling_Anguish, 02, "Erzek Runebreaker", GW::Constants::MapID::Lornars_Pass, {{57, 308}, {213, 444}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Water_Trident, 02, "Chunk Clumpfoot", GW::Constants::MapID::Lornars_Pass, {{69, 308}, {207, 456}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Feast_of_Corruption, 02, "Maw the Mountain Heart", GW::Constants::MapID::Dreadnoughts_Drift, {{169, 691}}, "If Maw dies under ground (due to degen), you will not be able to use a Signet of Capture to capture his elite"},
    {GW::Constants::SkillID::Earth_Shaker, 02, "Kor Stonewrath", GW::Constants::MapID::Snake_Dance, {{79, 752}, {50, 878}, {53, 977}}},
    {GW::Constants::SkillID::Battle_Rage, 02, "Smukk Foombool", GW::Constants::MapID::Snake_Dance, {{63, 824}, {113, 1199}}},
    {GW::Constants::SkillID::Punishing_Shot, 02, "Thul Boulderrain", GW::Constants::MapID::Snake_Dance, {{91, 752}, {65, 977}, {62, 878}}},
    {GW::Constants::SkillID::Escape, 02, "Whuup Buumbuul", GW::Constants::MapID::Snake_Dance, {{75, 824}, {125, 1199}}},
    {GW::Constants::SkillID::Peace_and_Harmony, 02, "Marnta Doomspeaker", GW::Constants::MapID::Snake_Dance, {{143, 1226}}},
    {GW::Constants::SkillID::Spell_Breaker, 02, "Raptorhawk", GW::Constants::MapID::Snake_Dance, {{62, 1373}, {47, 929}, {137, 924}}},
    {GW::Constants::SkillID::Signet_of_Judgment, 02, "Fawl Driftstalker", GW::Constants::MapID::Snake_Dance, {{150, 1000}, {143, 1113}, {142, 1161}, {99, 1243}}},
    {GW::Constants::SkillID::Blood_is_Power, 02, "Cry Darkday", GW::Constants::MapID::Snake_Dance, {{144, 1244}}},
    {GW::Constants::SkillID::Spiteful_Spirit, 02, "Sapph Blacktracker", GW::Constants::MapID::Snake_Dance, {{162, 1000}, {155, 1113}, {154, 1161}, {111, 1243}}},
    {GW::Constants::SkillID::Illusionary_Weaponry, 02, "Didn Hopestealer", GW::Constants::MapID::Snake_Dance, {{162, 1012}, {155, 1125}, {154, 1173}, {111, 1255}}},
    {GW::Constants::SkillID::Mantra_of_Recall, 02, "Featherclaw", GW::Constants::MapID::Snake_Dance, {{74, 1373}, {59, 929}, {149, 924}}},
    {GW::Constants::SkillID::Mind_Shock, 02, "Old Red Claw", GW::Constants::MapID::Snake_Dance, {{68, 1361}, {53, 941}, {143, 936}}},
    {GW::Constants::SkillID::Mist_Form, 02, "Sala Chillbringer", GW::Constants::MapID::Snake_Dance, {{150, 1012}, {142, 1173}, {99, 1255}, {143, 1125}}},
    {GW::Constants::SkillID::Dwarven_Battle_Stance, 02, "Thorgall Bludgeonhammer", GW::Constants::MapID::Grenths_Footprint, {{219, 1102}}, "Spawns in one of the 10 spots."},
    {GW::Constants::SkillID::Melandrus_Arrows, 02, "Gargash Thornbeard", GW::Constants::MapID::Grenths_Footprint, {{236, 1102}}, "Spawns in one of the 10 spots."},
    {GW::Constants::SkillID::Mark_of_Protection, 02, "Wroth Yakslapper", GW::Constants::MapID::Grenths_Footprint, {{253, 1102}}, "Spawns in one of the 10 spots."},
    {GW::Constants::SkillID::Offering_of_Blood, 02, "Morgriff Shadestone", GW::Constants::MapID::Grenths_Footprint, {{270, 1102}}, "Spawns in one of the 10 spots."},
    {GW::Constants::SkillID::Crippling_Anguish, 02, "Gorrel Rockmolder", GW::Constants::MapID::Grenths_Footprint, {{287, 1102}}, "Spawns in one of the 10 spots."},
    {GW::Constants::SkillID::Water_Trident, 02, "Flint Fleshcleaver", GW::Constants::MapID::Grenths_Footprint, {{304, 1102}}, "Spawns in one of the 10 spots."},
    {GW::Constants::SkillID::Battle_Rage, 02, "Krogg Shmush", GW::Constants::MapID::Talus_Chute, {{256, 1659}, {286, 1602}}},
    {GW::Constants::SkillID::Escape, 02, "Whuup Buumbuul", GW::Constants::MapID::Talus_Chute, {{268, 1659}, {298, 1602}}},
    {GW::Constants::SkillID::Victory_Is_Mine, 02, "Jono Yawpyawl", GW::Constants::MapID::Talus_Chute, {{247, 1401}, {325, 1410}, {385, 1418}, {421, 1532}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Poison_Arrow, 02, "Kekona Pippip", GW::Constants::MapID::Talus_Chute, {{259, 1401}, {337, 1410}, {397, 1418}, {433, 1532}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Plague_Signet, 02, "Allobo Dimdim", GW::Constants::MapID::Talus_Chute, {{259, 1413}, {337, 1422}, {397, 1430}, {433, 1544}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Glimmering_Mark, 02, "Edibbo Pekpek", GW::Constants::MapID::Talus_Chute, {{247, 1413}, {325, 1422}, {385, 1430}, {421, 1544}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Signet_of_Judgment, 02, "Frostbite", GW::Constants::MapID::Talus_Chute, {{199, 1431}, {211, 1472}, {277, 1464}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Mist_Form, 02, "Brrrr Windburn", GW::Constants::MapID::Talus_Chute, {{211, 1431}, {223, 1472}, {289, 1464}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Illusionary_Weaponry, 02, "Seear Windlash", GW::Constants::MapID::Talus_Chute, {{211, 1443}, {223, 1484}, {289, 1476}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Spiteful_Spirit, 02, "Nighh SpineChill", GW::Constants::MapID::Talus_Chute, {{199, 1443}, {211, 1484}, {277, 1476}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Cleave, 02, "Arlak Stoneleaf", GW::Constants::MapID::Ice_Caves_of_Sorrow, {{880, 1598}}},
    {GW::Constants::SkillID::Cleave, 02, "Virag Bladestone", GW::Constants::MapID::Ice_Caves_of_Sorrow, {{834, 1542}}},
    {GW::Constants::SkillID::Mark_of_Protection, 02, "Darda Goldenchief", GW::Constants::MapID::Ice_Caves_of_Sorrow, {{876, 1566}}},
    {GW::Constants::SkillID::Offering_of_Blood, 02, "Hormak Ironcurse", GW::Constants::MapID::Ice_Caves_of_Sorrow, {{612, 1593}}},
    {GW::Constants::SkillID::Crippling_Anguish, 02, "Jonar Stonebender", GW::Constants::MapID::Ice_Caves_of_Sorrow, {{660, 1624}}},
    {GW::Constants::SkillID::Water_Trident, 02, "Berg Frozenfist", GW::Constants::MapID::Ice_Caves_of_Sorrow, {{810, 1602}}},
    {GW::Constants::SkillID::Unyielding_Aura, 02, "Ipillo Wupwup", GW::Constants::MapID::Witmans_Folly, {{567, 1767}, {601, 1746}, {656, 1730}, {653, 1774}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Victory_Is_Mine, 02, "Sakalo Yawpyawl", GW::Constants::MapID::Witmans_Folly, {{579, 1767}, {613, 1746}, {668, 1730}, {665, 1774}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Poison_Arrow, 02, "Salani Pippip", GW::Constants::MapID::Witmans_Folly, {{567, 1779}, {601, 1758}, {653, 1786}, {656, 1742}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Glimmering_Mark, 02, "Alana Pekpek", GW::Constants::MapID::Witmans_Folly, {{579, 1779}, {613, 1758}, {665, 1786}, {668, 1742}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Plague_Signet, 02, "Karobo Dimdim", GW::Constants::MapID::Witmans_Folly, {{573, 1791}, {607, 1770}, {660, 1798}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Plague_Signet, 02, "Karobo Dimdim", GW::Constants::MapID::Witmans_Folly, {{662, 1754}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Energy_Drain, 02, "Sniik Hungrymind", GW::Constants::MapID::Witmans_Folly, {{737, 1782}, {723, 1817}}},
    {GW::Constants::SkillID::Mind_Freeze, 02, "Maak Frostfriend", GW::Constants::MapID::Witmans_Folly, {{735, 1817}, {749, 1782}}},
    {GW::Constants::SkillID::Signet_of_Judgment, 02, "Frostbite", GW::Constants::MapID::Ice_Floe, {{1246, 1412}}, "Walk into the circle of stones. Frostbite is hidden there.\nFrostbite appears 1 out of 4 times"},
    {GW::Constants::SkillID::Energy_Drain, 02, "Gambol Headrainer", GW::Constants::MapID::Ice_Floe, {{1090, 1654}, {1194, 1639}}},
    {GW::Constants::SkillID::Mind_Freeze, 02, "Skitt Skizzle", GW::Constants::MapID::Ice_Floe, {{1206, 1639}, {1078, 1654}}},
    {GW::Constants::SkillID::Thunderclap, 02, "Mursaat Elementalist", GW::Constants::MapID::Ice_Floe, {{1018, 1556}, {969, 1509}, {1132, 1421}, {1178, 1407}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Aura_of_Faith, 02, "Mursaat Monk ", GW::Constants::MapID::Ice_Floe, {{1166, 1407}, {1120, 1421}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Cleave, 02, "Gornar Bellybreaker", GW::Constants::MapID::Thunderhead_Keep, {{1355, 1306}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Cleave, 02, "Gornar Bellybreaker", GW::Constants::MapID::Thunderhead_Keep, {{1362, 1241}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Devastating_Hammer, 02, "Perfected Armor", GW::Constants::MapID::Thunderhead_Keep, {{1360, 1167}}, "During the siege. Does not always spawn."},
    {GW::Constants::SkillID::Oath_Shot, 02, "Perfected Cloak", GW::Constants::MapID::Thunderhead_Keep, {{1348, 1167}}, "During the siege. Does not always spawn."},
    {GW::Constants::SkillID::Aura_of_Faith, 02, "Demetrios the Enduring", GW::Constants::MapID::Thunderhead_Keep, {{1372, 1167}}, "During the siege. Does not always spawn."},
    {GW::Constants::SkillID::Life_Transfer, 02, "Argyris the Scoundrel", GW::Constants::MapID::Thunderhead_Keep, {{1348, 1179}}, "During the siege. Does not always spawn."},
    {GW::Constants::SkillID::Thunderclap, 02, "Chrysos the Magnetic", GW::Constants::MapID::Thunderhead_Keep, {{1360, 1179}}, "During the siege. Does not always spawn."},
    {GW::Constants::SkillID::Energy_Surge, 02, "Panthar The Deceiver", GW::Constants::MapID::Thunderhead_Keep, {{1372, 1179}}, "During the siege. Does not always spawn."},
    {GW::Constants::SkillID::Melandrus_Arrows, 02, "Ulhar Stonehound", GW::Constants::MapID::Thunderhead_Keep, {{1350, 1241}, {1343, 1306}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Mark_of_Protection, 02, "Bolis Hillshaker", GW::Constants::MapID::Thunderhead_Keep, {{1374, 1241}, {1367, 1306}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Offering_of_Blood, 02, "Riine Windrot", GW::Constants::MapID::Thunderhead_Keep, {{1350, 1253}, {1343, 1318}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Crippling_Anguish, 02, "Fuury Stonewrath", GW::Constants::MapID::Thunderhead_Keep, {{1362, 1253}, {1355, 1318}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Water_Trident, 02, "The Judge", GW::Constants::MapID::Thunderhead_Keep, {{1374, 1253}, {1367, 1318}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Water_Trident, 02, "Boreal Kubeclaw", GW::Constants::MapID::Frozen_Forest, {{783, 1375}, {813, 1301}, {784, 1260}, {861, 1308}, {944, 1322}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Cleave, 02, "Linka Goldensteel", GW::Constants::MapID::Frozen_Forest, {{795, 1375}, {825, 1301}, {796, 1260}, {873, 1308}, {956, 1322}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Melandrus_Arrows, 02, "Resnar Mountainsight", GW::Constants::MapID::Frozen_Forest, {{771, 1375}, {801, 1301}, {772, 1260}, {849, 1308}, {932, 1322}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Crippling_Anguish, 02, "Barl Stormsiege", GW::Constants::MapID::Frozen_Forest, {{801, 1313}, {771, 1387}, {772, 1272}, {849, 1320}, {932, 1334}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Offering_of_Blood, 02, "Jollen Steelblight", GW::Constants::MapID::Frozen_Forest, {{783, 1387}, {861, 1320}, {944, 1334}, {813, 1313}, {784, 1272}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Mark_of_Protection, 02, "Mesqul Ironhealer", GW::Constants::MapID::Frozen_Forest, {{795, 1387}, {873, 1320}, {956, 1334}, {825, 1313}, {796, 1272}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Grenths_Balance, 02, "Mragga", GW::Constants::MapID::Frozen_Forest, {{773, 1197}}, "Only during \"The Hero's Challenge\" quest."},
    {GW::Constants::SkillID::Defy_Pain, 02, "Obrhit Barkwood", GW::Constants::MapID::Frozen_Forest, {{830, 1228}, {847, 1259}, {884, 1272}, {873, 1369}, {748, 1308}}},
    {GW::Constants::SkillID::Life_Barrier, 02, "Esnhal Hardwood", GW::Constants::MapID::Frozen_Forest, {{842, 1228}, {859, 1259}, {896, 1272}, {885, 1369}, {760, 1308}}},
    {GW::Constants::SkillID::Virulence, 02, "Unthet Rotwood", GW::Constants::MapID::Frozen_Forest, {{830, 1240}, {847, 1271}, {884, 1284}, {873, 1381}, {748, 1320}}},
    {GW::Constants::SkillID::Ward_Against_Harm, 02, "Arkhel Havenwood", GW::Constants::MapID::Frozen_Forest, {{842, 1240}, {896, 1284}, {885, 1381}, {760, 1320}, {858, 1271}}},
    {GW::Constants::SkillID::Cleave, 02, "Marika Granitehand", GW::Constants::MapID::Iron_Mines_of_Moladune, {{994, 997}, {1017, 1035}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Mark_of_Protection, 02, "Lokar Icemender", GW::Constants::MapID::Iron_Mines_of_Moladune, {{1006, 997}, {1029, 1035}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Dwarven_Battle_Stance, 02, "Slonak Copperbark", GW::Constants::MapID::Iron_Mines_of_Moladune, {{1000, 1009}, {1023, 1047}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Devastating_Hammer, 02, "Martigris the Stalwart", GW::Constants::MapID::Iron_Mines_of_Moladune, {{1135, 751}, {1102, 868}, {1110, 964}, {1122, 1018}, {1123, 1089}}},
    {GW::Constants::SkillID::Thunderclap, 02, "Kratos the Foul", GW::Constants::MapID::Iron_Mines_of_Moladune, {{1147, 751}, {1114, 868}, {1122, 964}, {1134, 1018}, {1135, 1089}}},
    {GW::Constants::SkillID::Oath_Shot, 02, "Hilios the Dutiful", GW::Constants::MapID::Iron_Mines_of_Moladune, {{1123, 751}, {1090, 868}, {1098, 964}, {1110, 1018}, {1111, 1089}}},
    {GW::Constants::SkillID::Life_Transfer, 02, "Feodor the Baneful", GW::Constants::MapID::Iron_Mines_of_Moladune, {{1130, 763}, {1096, 880}, {1104, 976}, {1117, 1101}, {1116, 1030}}},
    {GW::Constants::SkillID::Energy_Surge, 02, "Balasi the Arcane", GW::Constants::MapID::Iron_Mines_of_Moladune, {{1142, 763}, {1108, 880}, {1116, 976}, {1128, 1030}, {1129, 1101}}},
    {GW::Constants::SkillID::Signet_of_Judgment, 02, "Balt Duskstrider", GW::Constants::MapID::Iron_Mines_of_Moladune, {{1031, 753}, {983, 845}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Spiteful_Spirit, 02, "Ceru Gloomrunner", GW::Constants::MapID::Iron_Mines_of_Moladune, {{1043, 753}, {995, 845}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Illusionary_Weaponry, 02, "Digo Murkstalker", GW::Constants::MapID::Iron_Mines_of_Moladune, {{1037, 765}, {989, 857}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Thunderclap, 02, "The Inquisitor", GW::Constants::MapID::Iron_Mines_of_Moladune, {{1064, 984}}, "He will spawn only if you're trying the bonus. Killing him is the bonus objective."},
    {GW::Constants::SkillID::Mist_Form, 02, "Eidolon", GW::Constants::MapID::Iron_Mines_of_Moladune, {{1067, 791}}, "The owner of the essence. You have to defeat him, to take the essence."},
    {GW::Constants::SkillID::Barrage, 02, "Markis", GW::Constants::MapID::Iron_Mines_of_Moladune, {{1113, 707}}, "Try to pull him out of the circle he stands, and kill him before the Mursaat, otherwise the mission will end!"},
    {GW::Constants::SkillID::Victory_Is_Mine, 02, "Jono Yawpyawl", GW::Constants::MapID::Spearhead_Peak, {{502, 1050}, {624, 1059}, {688, 1060}, {666, 1101}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Glimmering_Mark, 02, "Edibbo Pekpek", GW::Constants::MapID::Spearhead_Peak, {{514, 1050}, {636, 1059}, {700, 1060}, {678, 1101}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Plague_Signet, 02, "Allobo Dimdim", GW::Constants::MapID::Spearhead_Peak, {{526, 1050}, {648, 1059}, {712, 1060}, {690, 1101}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Unyielding_Aura, 02, "Kaia Wupwup", GW::Constants::MapID::Spearhead_Peak, {{509, 1062}, {630, 1071}, {694, 1072}, {673, 1113}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Poison_Arrow, 02, "Kekona Pippip", GW::Constants::MapID::Spearhead_Peak, {{685, 1113}, {706, 1072}, {642, 1071}, {521, 1062}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Skull_Crack, 02, "Hail Blackice", GW::Constants::MapID::Spearhead_Peak, {{560, 990}}},
    {GW::Constants::SkillID::Keystone_Signet, 02, "Rune Ethercrash", GW::Constants::MapID::Spearhead_Peak, {{575, 1000}}},
    {GW::Constants::SkillID::Ferocious_Strike, 02, "Thul The Bull", GW::Constants::MapID::Spearhead_Peak, {{685, 1016}}},
    {GW::Constants::SkillID::Energy_Drain, 02, "Sniik Hungrymind", GW::Constants::MapID::Spearhead_Peak, {{511, 1156}, {559, 1154}}},
    {GW::Constants::SkillID::Mind_Freeze, 02, "Maak Frostfriend", GW::Constants::MapID::Spearhead_Peak, {{523, 1156}, {571, 1154}}},
    {GW::Constants::SkillID::Flourish, 02, "Syr Honorcrest", GW::Constants::MapID::Mineral_Springs, {{628, 574}}, "Spawns in one of six possible locations"},
    {GW::Constants::SkillID::Practiced_Stance, 02, "Ryk Arrowwing", GW::Constants::MapID::Mineral_Springs, {{643, 574}}, "Spawns in one of six possible locations"},
    {GW::Constants::SkillID::Shield_of_Judgment, 02, "Myd Springclaw", GW::Constants::MapID::Mineral_Springs, {{658, 574}}, "Spawns in one of six possible locations"},
    {GW::Constants::SkillID::Well_of_Power, 02, "Nhy Darkclaw", GW::Constants::MapID::Mineral_Springs, {{673, 574}}, "Spawns in one of six possible locations"},
    {GW::Constants::SkillID::Ineptitude, 02, "Wyt Sharpfeather", GW::Constants::MapID::Mineral_Springs, {{688, 574}}, "Spawns in one of six possible locations"},
    {GW::Constants::SkillID::Glyph_of_Energy, 02, "Hyl Thunderwing", GW::Constants::MapID::Mineral_Springs, {{703, 574}}, "Spawns in one of six possible locations"},
    {GW::Constants::SkillID::Mist_Form, 02, "Ice Beast", GW::Constants::MapID::Mineral_Springs, {{1015, 668}}},
    {GW::Constants::SkillID::Charge, 03, "Balthazaar's Cursed", GW::Constants::MapID::Perdition_Rock, {{764, 408}, {827, 439}, {887, 339}, {885, 275}}, "Two bosses can spawn in this location. Does not always spawn."},
    {GW::Constants::SkillID::Charge, 03, "Balthazaar's Cursed", GW::Constants::MapID::Perdition_Rock, {{790, 233}}, "Does not always spawn."},
    {GW::Constants::SkillID::Crippling_Shot, 03, "Melandru's Cursed", GW::Constants::MapID::Perdition_Rock, {{752, 408}, {815, 439}, {875, 339}, {873, 275}}, "Two bosses can spawn in this location. Does not always spawn."},
    {GW::Constants::SkillID::Crippling_Shot, 03, "Melandru's Cursed", GW::Constants::MapID::Perdition_Rock, {{778, 233}}, "Does not always spawn."},
    {GW::Constants::SkillID::Amity, 03, "Pravus Obsideo", GW::Constants::MapID::Perdition_Rock, {{776, 408}, {839, 439}, {899, 339}, {897, 275}}, "Two bosses can spawn in this location. Does not always spawn."},
    {GW::Constants::SkillID::Amity, 03, "Pravus Obsideo", GW::Constants::MapID::Perdition_Rock, {{802, 233}}, "Does not always spawn."},
    {GW::Constants::SkillID::Power_Block, 03, "Lyssa's Cursed", GW::Constants::MapID::Perdition_Rock, {{752, 396}, {815, 427}, {875, 327}, {873, 263}}, "Two bosses can spawn in this location. Does not always spawn."},
    {GW::Constants::SkillID::Power_Block, 03, "Lyssa's Cursed", GW::Constants::MapID::Perdition_Rock, {{778, 221}}, "Does not always spawn."},
    {GW::Constants::SkillID::Signet_of_Midnight, 03, "Malus Phasmatis", GW::Constants::MapID::Perdition_Rock, {{764, 396}, {827, 427}, {887, 327}, {885, 263}}, "Two bosses can spawn in this location. Does not always spawn."},
    {GW::Constants::SkillID::Signet_of_Midnight, 03, "Malus Phasmatis", GW::Constants::MapID::Perdition_Rock, {{790, 221}}, "Does not always spawn."},
    {GW::Constants::SkillID::Wither, 03, "Ignis Effigia", GW::Constants::MapID::Perdition_Rock, {{776, 396}, {839, 427}, {899, 327}, {897, 263}}, "Two bosses can spawn in this location. Does not always spawn."},
    {GW::Constants::SkillID::Wither, 03, "Ignis Effigia", GW::Constants::MapID::Perdition_Rock, {{802, 221}}, "Does not always spawn."},
    {GW::Constants::SkillID::Lingering_Curse, 03, "Grenth's Cursed", GW::Constants::MapID::Perdition_Rock, {{758, 420}, {821, 451}, {881, 351}, {879, 287}}, "Two bosses can spawn in this location. Does not always spawn.. Does not always spawn."},
    {GW::Constants::SkillID::Lingering_Curse, 03, "Grenth's Cursed", GW::Constants::MapID::Perdition_Rock, {{784, 245}}, "Does not always spawn."},
    {GW::Constants::SkillID::Martyr, 03, "Dwayna's Cursed", GW::Constants::MapID::Perdition_Rock, {{770, 420}, {833, 451}, {893, 351}, {891, 287}}, "Two bosses can spawn in this location. Does not always spawn."},
    {GW::Constants::SkillID::Martyr, 03, "Dwayna's Cursed", GW::Constants::MapID::Perdition_Rock, {{796, 245}}, "Does not always spawn."},
    {GW::Constants::SkillID::Bulls_Charge, 03, "Skintekaru Manshredder", GW::Constants::MapID::Perdition_Rock, {{654, 294}, {714, 311}, {806, 269}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Elemental_Attunement, 03, "Geckokaru Earthwind", GW::Constants::MapID::Perdition_Rock, {{666, 294}, {726, 311}, {818, 269}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Tainted_Flesh, 03, "Dosakaru Fevertouch", GW::Constants::MapID::Perdition_Rock, {{660, 306}, {720, 323}, {812, 281}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Healing_Hands, 03, "Rull Browbeater", GW::Constants::MapID::Perdition_Rock, {{636, 326}, {644, 399}, {683, 325}, {872, 420}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Echo, 03, "Rwek Khawl Mawl", GW::Constants::MapID::Perdition_Rock, {{648, 326}, {695, 325}, {656, 399}, {884, 420}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Glyph_of_Renewal, 03, "Nayl Klaw Tuthan", GW::Constants::MapID::Perdition_Rock, {{642, 338}, {689, 337}, {650, 411}, {878, 432}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Obsidian_Flesh, 03, "Harn Coldstone", GW::Constants::MapID::Perdition_Rock, {{828, 315}}, "Shared spawn. Only one boss spawns.\nThis is the point from which he starts patrolling, as soon as you see him. He will patrol all of the east side of the island"},
    {GW::Constants::SkillID::Quick_Shot, 03, "Maxine Coldstone", GW::Constants::MapID::Perdition_Rock, {{840, 315}}, "Shared spawn. Only one boss spawns.\nThis is the point from which she starts patrolling, as soon as you see her. She will patrol all of the east side of the island"},
    {GW::Constants::SkillID::Soul_Taker, 03, "Abaddon's Cursed", GW::Constants::MapID::Perdition_Rock, {{848, 221}}, "Only if a player has a \"Proof of Triumph\" in their inventory."},
    {GW::Constants::SkillID::Over_the_Limit, 03, "Abaddon's Cursed", GW::Constants::MapID::Perdition_Rock, {{860, 221}}, "Only if a player has a \"Proof of Triumph\" in their inventory."},
    {GW::Constants::SkillID::Eviscerate, 03, "Tortitudo Probo", GW::Constants::MapID::Hells_Precipice, {{402, 494}, {387, 548}}},
    {GW::Constants::SkillID::Eviscerate, 03, "Tortitudo Probo", GW::Constants::MapID::Hells_Precipice, {{297, 527}}, "Three bosses can spawn here, or get out of the lava"},
    {GW::Constants::SkillID::Greater_Conflagration, 03, "Valetudo Rubor", GW::Constants::MapID::Hells_Precipice, {{309, 527}}, "Three bosses can spawn here, or get out of the lava"},
    {GW::Constants::SkillID::Greater_Conflagration, 03, "Valetudo Rubor", GW::Constants::MapID::Hells_Precipice, {{414, 494}, {399, 548}}},
    {GW::Constants::SkillID::Aura_of_the_Lich, 03, "Maligo Libens", GW::Constants::MapID::Hells_Precipice, {{390, 494}, {375, 548}}},
    {GW::Constants::SkillID::Aura_of_the_Lich, 03, "Maligo Libens", GW::Constants::MapID::Hells_Precipice, {{285, 527}}, "Three bosses can spawn here, or get out of the lava"},
    {GW::Constants::SkillID::Panic, 03, "Moles Quibus", GW::Constants::MapID::Hells_Precipice, {{291, 539}}, "Three bosses can spawn here, or get out of the lava"},
    {GW::Constants::SkillID::Panic, 03, "Moles Quibus", GW::Constants::MapID::Hells_Precipice, {{381, 560}, {396, 506}}},
    {GW::Constants::SkillID::Mind_Burn, 03, "Scelus Prosum", GW::Constants::MapID::Hells_Precipice, {{393, 560}, {408, 506}}},
    {GW::Constants::SkillID::Mind_Burn, 03, "Scelus Prosum", GW::Constants::MapID::Hells_Precipice, {{303, 539}}, "Three bosses can spawn here, or get out of the lava"},
    {GW::Constants::SkillID::Hundred_Blades, 03, "Undead Prince Rurik", GW::Constants::MapID::Hells_Precipice, {{315, 372}}},
    {GW::Constants::SkillID::Oath_Shot, 03, "Cairn the Relentless", GW::Constants::MapID::Abaddons_Mouth, {{321, 328}, {260, 254}, {210, 274}}, "Two bosses at a time spawn at this location"},
    {GW::Constants::SkillID::Devastating_Hammer, 03, "Cairn the Destroyer", GW::Constants::MapID::Abaddons_Mouth, {{309, 328}, {248, 254}, {198, 274}}, "Two bosses at a time spawn at this location"},
    {GW::Constants::SkillID::Thunderclap, 03, "Optimus Caliph", GW::Constants::MapID::Abaddons_Mouth, {{297, 328}, {236, 254}, {186, 274}}, "Two bosses at a time spawn at this location"},
    {GW::Constants::SkillID::Aura_of_Faith, 03, "Willa the Unpleasant", GW::Constants::MapID::Abaddons_Mouth, {{321, 340}, {260, 266}, {210, 286}}, "Two bosses at a time spawn at this location"},
    {GW::Constants::SkillID::Energy_Surge, 03, "Mercia the Smug", GW::Constants::MapID::Abaddons_Mouth, {{309, 340}, {198, 286}, {248, 266}}, "Two bosses at a time spawn at this location"},
    {GW::Constants::SkillID::Life_Transfer, 03, "Sarlic the Judge", GW::Constants::MapID::Abaddons_Mouth, {{297, 340}, {236, 266}, {186, 286}}, "Two bosses at a time spawn at this location"},
    {GW::Constants::SkillID::Fevered_Dreams, 03, "Plexus Shadowhook", GW::Constants::MapID::Abaddons_Mouth, {{70, 243}, {86, 166}, {208, 168}}, "Does not always spawn."},
    {GW::Constants::SkillID::Restore_Condition, 03, "Spindle Agonyvein", GW::Constants::MapID::Abaddons_Mouth, {{82, 243}, {98, 166}, {220, 168}}, "Does not always spawn."},
    {GW::Constants::SkillID::Soul_Leech, 03, "Goss Darkweb", GW::Constants::MapID::Abaddons_Mouth, {{76, 255}, {92, 178}, {214, 180}}, "Does not always spawn."},
    {GW::Constants::SkillID::Barrage, 03, "Snyk the Hundred Tongue", GW::Constants::MapID::Abaddons_Mouth, {{61, 276}, {66, 312}}, "Does not always spawn."},
    {GW::Constants::SkillID::Mist_Form, 03, "Eidolon", GW::Constants::MapID::Abaddons_Mouth, {{54, 259}}},
    {GW::Constants::SkillID::Oath_Shot, 03, "Cairn the Troubling", GW::Constants::MapID::Ring_of_Fire, {{505, 211}}, "Can spawn in one of the six locations"},
    {GW::Constants::SkillID::Devastating_Hammer, 03, "Cairn the Smug", GW::Constants::MapID::Ring_of_Fire, {{521, 211}}, "Can spawn in one of the six locations"},
    {GW::Constants::SkillID::Energy_Surge, 03, "Melek the Virtuous", GW::Constants::MapID::Ring_of_Fire, {{537, 211}}, "Can spawn in one of the six locations"},
    {GW::Constants::SkillID::Thunderclap, 03, "Maida the Ill Tempered", GW::Constants::MapID::Ring_of_Fire, {{553, 211}}, "Can spawn in one of the six locations"},
    {GW::Constants::SkillID::Life_Transfer, 03, "Odelyn the Displeased", GW::Constants::MapID::Ring_of_Fire, {{569, 211}}, "Can spawn in one of the six locations"},
    {GW::Constants::SkillID::Aura_of_Faith, 03, "Coventina the Matron", GW::Constants::MapID::Ring_of_Fire, {{585, 211}}, "Can spawn in one of the six locations"},
    {GW::Constants::SkillID::Backbreaker, 03, "Ferk Mallet", GW::Constants::MapID::Ring_of_Fire, {{623, 57}}, "Does not always spawn."},
    {GW::Constants::SkillID::Backbreaker, 03, "Ferk Mallet", GW::Constants::MapID::Ring_of_Fire, {{638, 91}}, "Does not always spawn.\nUsually starts moving as soon as a player comes into radar range and may ambush players from behind when they are moving forward on the regular mission path."},
    {GW::Constants::SkillID::Spike_Trap, 03, "Vulg Painbrain", GW::Constants::MapID::Ring_of_Fire, {{650, 91}}, "Does not always spawn.\nUsually starts moving as soon as a player comes into radar range and may ambush players from behind when they are moving forward on the regular mission path."},
    {GW::Constants::SkillID::Spike_Trap, 03, "Vulg Painbrain", GW::Constants::MapID::Ring_of_Fire, {{635, 57}}, "Does not always spawn."},
    {GW::Constants::SkillID::Incendiary_Arrows, 03, "Casses Flameweb", GW::Constants::MapID::Ring_of_Fire, {{458, 95}, {501, 58}}, "Does not always spawn."},
    {GW::Constants::SkillID::Shield_of_Deflection, 03, "Grun Galesurge", GW::Constants::MapID::Ring_of_Fire, {{351, 68}, {434, 41}, {528, 137}}},
    {GW::Constants::SkillID::Migraine, 03, "Pytt Spitespew", GW::Constants::MapID::Ring_of_Fire, {{363, 68}, {446, 41}, {540, 137}}},
    {GW::Constants::SkillID::Ether_Prodigy, 03, "Jyth Sprayburst", GW::Constants::MapID::Ring_of_Fire, {{357, 80}, {440, 53}, {534, 149}}},
    {GW::Constants::SkillID::Mist_Form, 03, "Eidolon", GW::Constants::MapID::Ring_of_Fire, {{449, 129}}, "Spawns once the Ether Seals are defeated."},
    {GW::Constants::SkillID::Bulls_Charge, 02, "Grognard Gravelhead", GW::Constants::MapID::Sorrows_Furnace, {{197, 860}}, "In Tharn Stonerift's room.\nIn the passage to the east, between the bridge and Marshall Whitman.\nDuring \"The Final Assault\" quest, has a chance to appear along with one of the other Warrior bosses in the chamber southeast of the entrance."},
    {GW::Constants::SkillID::Dwarven_Battle_Stance, 02, "Malinon Threshammer", GW::Constants::MapID::Sorrows_Furnace, {{212, 860}}, "During Noble Intentions while defending Orozar Highstone.\nDuring Noble Intentions Plan B after lowering the bridge.\nDuring \"The Final Assault\" quest, has a chance to appear along with one of the other Warrior bosses in the chamber southeast of the entrance."},
    {GW::Constants::SkillID::Eviscerate, 02, "Tanzit Razorstone", GW::Constants::MapID::Sorrows_Furnace, {{227, 860}}, "At the Iron Arch or at the Stone Basilica.\nDuring Orozar Highstone's quests.\nDuring \"The Final Assault\" quest, has a chance to appear along with one of the other Warrior bosses in the chamber southeast of the entrance."},
    {GW::Constants::SkillID::Melandrus_Arrows, 02, "Tarnok Forgerunner", GW::Constants::MapID::Sorrows_Furnace, {{242, 860}}, "In the northeastern crusher room\nin Makar Thoughtslayer's room.\nIn the southeast, at the end of the passage under the bridge."},
    {GW::Constants::SkillID::Melandrus_Arrows, 02, "Drago Stoneherder", GW::Constants::MapID::Sorrows_Furnace, {{257, 860}}, "In the northeast corner near Marshall Whitman or in Sorrow's Belly.\nIn the area south of the entrance and west of Tharn Stonerift's room.\nDuring Orozar Highstone's quests, in one of 6 random boss spawn locations.\nWhere High Priest Alkar returns the Tome of Rubicon, but not during this quest."},
    {GW::Constants::SkillID::Melandrus_Arrows, 02, "Graygore Boulderbeard", GW::Constants::MapID::Sorrows_Furnace, {{272, 860}}, "During \"Noble Intentions\" while defending Orozar Highstone.\nDuring \"Noble Intentions Plan B\" after lowering the bridge"},
    {GW::Constants::SkillID::Mark_of_Protection, 02, "Gulnar Irontoe", GW::Constants::MapID::Sorrows_Furnace, {{287, 860}}, "At the very end of the quest \"Kilroy Stonekin\"."},
    {GW::Constants::SkillID::Mark_of_Protection, 02, "Brohn Stonehart", GW::Constants::MapID::Sorrows_Furnace, {{302, 860}}, "In Sorrow's Belly\nAt the Forest entrance to the Forge Heart\nduring Orozar Highstone's quests, in one of 6 random boss spawn locations."},
    {GW::Constants::SkillID::Mark_of_Protection, 02, "Gardock Stonesoul", GW::Constants::MapID::Sorrows_Furnace, {{317, 860}}, "In the large hall 3 steps southeast of the entrance.\nIn the second large hall southeast of the entrance.\nIn the crusher room east of the entrance.\nIn a room to the east, between the bridge and Marshall Whitman.\nSorrow's Furnace in the southeast, at the end of the passage that goes under the bridge."},
    {GW::Constants::SkillID::Mark_of_Protection, 02, "Ivor Helmhewer", GW::Constants::MapID::Sorrows_Furnace, {{332, 860}}, "Only during \"The Forge Heart\" quest."},
    {GW::Constants::SkillID::Crippling_Anguish, 02, "Vokur Grimshackles", GW::Constants::MapID::Sorrows_Furnace, {{377, 860}}, "Only during Orozar Highstone's quests, in one of 6 random boss spawn locations\nin the Gap, near Marshall Whitman\nin Sorrow's Belly"},
    {GW::Constants::SkillID::Crippling_Anguish, 02, "Garbok Handsmasher", GW::Constants::MapID::Sorrows_Furnace, {{392, 860}}},
    {GW::Constants::SkillID::Crippling_Anguish, 02, "Hierophant Morlog", GW::Constants::MapID::Sorrows_Furnace, {{347, 860}}, "Only during \"Unspeakable, Unknowable, High Priest Alkar\" quest."},
    {GW::Constants::SkillID::Crippling_Anguish, 02, "Korvald Willcrusher", GW::Constants::MapID::Sorrows_Furnace, {{362, 860}}, "Only during \"Noble Intentions\" , \"Noble Intentions Plan B\", \"The Forge Heart\" and \"The Forge Heart at the Iron Arch\" quests."},
    {GW::Constants::SkillID::Energy_Surge, 01, "The Darkness", GW::Constants::MapID::Tomb_of_the_Primeval_Kings, {{621, 112}}, "These are the final bosses of the \"Ruins of the Tomb of the Primeval Kings\". There are three in total."},
    {GW::Constants::SkillID::Unyielding_Aura, 02, "Kaia Wupwup", GW::Constants::MapID::Talus_Chute, {{271, 1407}, {349, 1416}, {409, 1424}, {445, 1538}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Wither, 07, "Desnas Hubor", GW::Constants::MapID::Tangle_Root, {{1098, 957}}, "Only during \"Defend Denravi\" quest."},
    {GW::Constants::SkillID::Eviscerate, 04, "Phlog the Indomitable", GW::Constants::MapID::Flame_Temple_Corridor, {{629, 212}}, "Only during \"The Titan Source\" quest."},
    {GW::Constants::SkillID::Eviscerate, 04, "Phlog the Indomitable", GW::Constants::MapID::Dragons_Gullet, {{591, 136}, {653, 53}, {762, 168}, {841, 145}}, "Only during \"The Titan Source\" quest."},
    {GW::Constants::SkillID::Greater_Conflagration, 04, "Staggh the Fervid", GW::Constants::MapID::Dragons_Gullet, {{750, 168}, {829, 145}, {641, 53}, {579, 136}}, "Only during \"The Titan Source\" quest."},
    {GW::Constants::SkillID::Life_Transfer, 04, "Lugg the Malignant", GW::Constants::MapID::Dragons_Gullet, {{738, 168}, {817, 145}, {629, 53}, {567, 136}}, "Only during \"The Titan Source\" quest."},
    {GW::Constants::SkillID::Energy_Drain, 04, "Druul the Untamed", GW::Constants::MapID::Dragons_Gullet, {{750, 180}, {829, 157}, {641, 65}, {579, 148}}, "Only during \"The Titan Source\" quest."},
    {GW::Constants::SkillID::Melandrus_Resilience, 04, "Gigas Expii", GW::Constants::MapID::Dragons_Gullet, {{732, 97}}, "Only during \"The Titan Source\" quest."},
    {GW::Constants::SkillID::Wither, 04, "Ludo Ossidi", GW::Constants::MapID::Dragons_Gullet, {{754, 95}}, "Only during \"The Titan Source\" quest."},
    {GW::Constants::SkillID::Panic, 04, "Mallus Funo", GW::Constants::MapID::Dragons_Gullet, {{747, 117}}, "Only during \"The Titan Source\" quest."},
    {GW::Constants::SkillID::Greater_Conflagration, 04, "Staggh the Fervid", GW::Constants::MapID::Dragons_Gullet, {{617, 212}}, "Only during the \"The Titan Source\" quest."},
    {GW::Constants::SkillID::Life_Transfer, 04, "Lugg the Malignant", GW::Constants::MapID::Dragons_Gullet, {{605, 212}}, "Only during the \"The Titan Source\" quest."},
    {GW::Constants::SkillID::Energy_Drain, 04, "Druul the Untamed", GW::Constants::MapID::Dragons_Gullet, {{617, 224}}, "Only during the \"The Titan Source\" quest."},
    {GW::Constants::SkillID::Wither, 02, "Evirso Sectus", GW::Constants::MapID::Mineral_Springs, {{987, 722}}, "Only during \"Defend Droknar's Forge\" quest."},
    {GW::Constants::SkillID::Warriors_Endurance, 01, "Custodian Fidius", GW::Constants::MapID::Dunes_of_Despair, {{120, 1535}}},
    {GW::Constants::SkillID::Aura_of_Faith, 02, "Mursaat Monk", GW::Constants::MapID::Ice_Floe, {{1006, 1556}, {957, 1509}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Devastating_Hammer, 02, "Jade Armor", GW::Constants::MapID::Ice_Floe, {{945, 1509}, {994, 1556}, {1108, 1421}, {1154, 1407}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Oath_Shot, 02, "Jade Bow", GW::Constants::MapID::Ice_Floe, {{945, 1521}, {994, 1568}, {1108, 1433}, {1154, 1419}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Life_Transfer, 02, "Mursaat Necromancer", GW::Constants::MapID::Ice_Floe, {{957, 1521}, {1006, 1568}, {1120, 1433}, {1166, 1419}}, "Shared spawn. Does not always spawn."},
    {GW::Constants::SkillID::Energy_Surge, 02, "Mursaat Mesmer", GW::Constants::MapID::Ice_Floe, {{969, 1521}, {1018, 1568}, {1132, 1433}, {1178, 1419}}, "Shared spawn. Does not always spawn."},
    
    // Dungeon bosses that weren't added to MappingOut
    {GW::Constants::SkillID::Flourish, 0xff, "Flame Djinn", GW::Constants::MapID::Catacombs_of_Kathandrax_Level_1, {{0, 0}},"One on level 3"},
    {GW::Constants::SkillID::Savannah_Heat, 0xff, "Regent of Flame", GW::Constants::MapID::Catacombs_of_Kathandrax_Level_1, {{0, 0}},"One on level 1, two on level 2, one on level 3"},
    {GW::Constants::SkillID::Scavengers_Focus, 0xff, "Beastmaster Korg", GW::Constants::MapID::Rragars_Menagerie_Level_1, {{0, 0}},"Level 2"},
    {GW::Constants::SkillID::Burning_Arrow, 0xff, "Charr Warden", GW::Constants::MapID::Rragars_Menagerie_Level_1, {{0, 0}},"4 on level 1"},
    {GW::Constants::SkillID::Toxic_Chill, 0xff, "Flesheater", GW::Constants::MapID::Rragars_Menagerie_Level_1, {{0, 0}},"2 on level 3"},
    {GW::Constants::SkillID::Cruel_Spear, 0xff, "Elder Nephilim", GW::Constants::MapID::Rragars_Menagerie_Level_1, {{0, 0}},"4 on level 2, 2 on level 3"},
    {GW::Constants::SkillID::Power_Block, 0xff, "Faze Magekiller", GW::Constants::MapID::Cathedral_of_Flames_Level_1, {{0, 0}},"Level 1"},
    {GW::Constants::SkillID::Ether_Prodigy, 0xff, "Tyndir Flamecaller", GW::Constants::MapID::Cathedral_of_Flames_Level_1, {{0, 0}},"Level 2"},
    {GW::Constants::SkillID::Jagged_Bones, 0xff, "The Master", GW::Constants::MapID::Cathedral_of_Flames_Level_1, {{0, 0}},"Level 2"},
    {GW::Constants::SkillID::Life_Transfer, 0xff, "The Keeper", GW::Constants::MapID::Cathedral_of_Flames_Level_1, {{0, 0}},"Level 1"},
    {GW::Constants::SkillID::Corrupt_Enchantment, 0xff, "Murakai's Steward", GW::Constants::MapID::Cathedral_of_Flames_Level_1, {{0, 0}},"Level 3"},
    {GW::Constants::SkillID::Soldiers_Fury, 0xff, "Vraxx the Condemned", GW::Constants::MapID::Cathedral_of_Flames_Level_1, {{0, 0}},"Level 2"},
    {GW::Constants::SkillID::Headbutt, 0xff, "Ancient Ooze", GW::Constants::MapID::Ooze_Pit, {{0, 0}}},
    {GW::Constants::SkillID::Water_Trident, 0xff, "Gloop", GW::Constants::MapID::Ooze_Pit, {{0, 0}}},
    {GW::Constants::SkillID::Skull_Crack, 0xff, "Havok-kin", GW::Constants::MapID::Darkrime_Delves_Level_1, {{0, 0}},"Two on level 1, one on level 2, one on level 3"},
    {GW::Constants::SkillID::Skull_Crack, 0xff, "Grelk Icelash", GW::Constants::MapID::Darkrime_Delves_Level_1, {{0, 0}},"Level 2"},
    {GW::Constants::SkillID::Barrage, 0xff, "Frostmaw Spawn", GW::Constants::MapID::Frostmaws_Burrows_Level_1, {{0, 0}},"Two on level 2\nFour on level 3 (sometimes five in Hard Mode)\nTwo on level 4\nTwo on level 5"},
    {GW::Constants::SkillID::Shatterstone, 0xff, "Fragment of Antiquities", GW::Constants::MapID::Sepulchre_of_Dragrimmar_Level_1, {{0, 0}},"Level 1"},
    {GW::Constants::SkillID::Shatterstone, 0xff, "Regent of Ice", GW::Constants::MapID::Sepulchre_of_Dragrimmar_Level_1, {{0, 0}},"About 5-6 on level 1"},
    {GW::Constants::SkillID::Wounding_Strike, 0xff, "Reaper of Destruction", GW::Constants::MapID::Ravens_Point_Level_1, {{0, 0}},"One on level 1, one on level 2"},
    {GW::Constants::SkillID::Shatterstone, 0xff, "Ancient Vaettir", GW::Constants::MapID::Ravens_Point_Level_1, {{0, 0}}, "Two on level 1, three on level 2"},
    {GW::Constants::SkillID::Invoke_Lightning, 0xff, "Shadow Spawn", GW::Constants::MapID::Ravens_Point_Level_1, {{0, 0}}, "Only during \"Shadows in the Night\""},
    {GW::Constants::SkillID::Poison_Arrow, 0xff, "Xalnax", GW::Constants::MapID::Vloxen_Excavations_Level_1, {{0, 0}},"Level 2"},
    {GW::Constants::SkillID::Dwarven_Battle_Stance, 0xff, "Taskmaster Durgon", GW::Constants::MapID::Vloxen_Excavations_Level_1, {{0, 0}},"Level 1"},
    {GW::Constants::SkillID::Jagged_Bones, 0xff, "Taskmaster Bellok", GW::Constants::MapID::Vloxen_Excavations_Level_1, {{0, 0}}, "Level 2"},
    {GW::Constants::SkillID::Crippling_Anguish, 0xff, "Taskmaster Kurg", GW::Constants::MapID::Vloxen_Excavations_Level_1, {{0, 0}},"Level 2"},
    {GW::Constants::SkillID::Soldiers_Fury, 0xff, "Sotanaht the Tomb Guardian", GW::Constants::MapID::Vloxen_Excavations_Level_1, {{0, 0}}, "Level 3"},
    {GW::Constants::SkillID::Spirits_Strength, 0xff, "Gokir Patriarch", GW::Constants::MapID::Bogroot_Growths_Level_1, {{0, 0}}, "One on level 1, three on level 2, during \"Tekks's War\"."},
    {GW::Constants::SkillID::Spirits_Strength, 0xff, "Ophil Patriarch", GW::Constants::MapID::Bogroot_Growths_Level_1, {{0, 0}}, "One on level 1, three on level 2, during \"Giriff's War\"."},
    {GW::Constants::SkillID::Backbreaker, 0xff, "Crystal Ettin", GW::Constants::MapID::Bloodstone_Caves_Level_1, {{0, 0}}, "One on level 2"},
    {GW::Constants::SkillID::Backbreaker, 0xff, "First Inscribed", GW::Constants::MapID::Bloodstone_Caves_Level_1, {{0, 0}}, "One on level 1, two on level 3"},
    {GW::Constants::SkillID::Backbreaker, 0xff, "Paranoia Ettin", GW::Constants::MapID::Bloodstone_Caves_Level_1, {{0, 0}}, "One on level 1"},
    {GW::Constants::SkillID::Blinding_Surge, 0xff, "Cursed Brigand", GW::Constants::MapID::Shards_of_Orr_Level_1, {{0, 0}}, "Three on level 1, two on level 2, two on level 3"},
    {GW::Constants::SkillID::Steady_Stance, 0xff, "Malfunctioning Shield Golem", GW::Constants::MapID::Oolas_Lab_Level_1, {{0, 0}}, "Level 2"},
    {GW::Constants::SkillID::Elemental_Attunement, 0xff, "Flame Guardian", GW::Constants::MapID::Oolas_Lab_Level_1, {{0, 0}}, "Three on level 1, two on level 3"},
    {GW::Constants::SkillID::Elemental_Attunement, 0xff, "Malfunctioning Regulator Golem", GW::Constants::MapID::Oolas_Lab_Level_1, {{0, 0}}, "Two on level 2"},
    {GW::Constants::SkillID::Shadow_Prison, 0xff, "Xien", GW::Constants::MapID::Oolas_Lab_Level_1, {{0, 0}}, "Level 1, only during \"Little Workshop of Horrors\"."},
    {GW::Constants::SkillID::Crippling_Shot, 0xff, "Brood Warden", GW::Constants::MapID::Arachnis_Haunt_Level_1, {{0, 0}}, "Level 1"},
    {GW::Constants::SkillID::Stunning_Strike, 0xff, "Spider Matriarch", GW::Constants::MapID::Arachnis_Haunt_Level_1, {{0, 0}}, "Two on level 1, one on level 2"},
    {GW::Constants::SkillID::Savannah_Heat, 0xff, "Lok The Mischievous", GW::Constants::MapID::Arachnis_Haunt_Level_1, {{0, 0}}, "Level 1"},
    {GW::Constants::SkillID::Tranquil_Was_Tanasen, 0xff, "Dark Watcher", GW::Constants::MapID::Slavers_Exile_Level_1, {{0, 0}}, "Forgewight's level"},
    {GW::Constants::SkillID::Ray_of_Judgment, 0xff, "Gorlos Skinflayer", GW::Constants::MapID::Heart_of_the_Shiverpeaks_Level_1, {{0, 0}}, "Level 1"},
    {GW::Constants::SkillID::Savannah_Heat, 0xff, "Magmus", GW::Constants::MapID::Heart_of_the_Shiverpeaks_Level_1, {{0, 0}}, "Level 3, except during \"Heart of the Shiverpeaks\" quest"},
    {GW::Constants::SkillID::Song_of_Restoration, 0xff, "Erasklion the Prolific", GW::Constants::MapID::Heart_of_the_Shiverpeaks_Level_1, {{0, 0}}, "Level 1"},
    {GW::Constants::SkillID::Toxic_Chill, 0xff, "Jacado the Putrid", GW::Constants::MapID::Heart_of_the_Shiverpeaks_Level_1, {{0, 0}}, "Level 1"},
    {GW::Constants::SkillID::Virulence, 0xff, "Jacado the Putrid", GW::Constants::MapID::Heart_of_the_Shiverpeaks_Level_1, {{0, 0}}, "Level 1"},
    {GW::Constants::SkillID::Battle_Rage, 0xff, "Avatar of Destruction", GW::Constants::MapID::Destructions_Depths_Level_1, {{0, 0}}, "Level 1"},
    {GW::Constants::SkillID::Onslaught, 0xff, "Avatar of Destruction", GW::Constants::MapID::Destructions_Depths_Level_1, {{0, 0}}, "Level 2"},
    {GW::Constants::SkillID::Stunning_Strike, 0xff, "Jadam Spearspinner", GW::Constants::MapID::Warband_of_Brothers_Level_1, {{0, 0}}, "Level 2"},
    {GW::Constants::SkillID::Eviscerate, 0xff, "Charr Prison Guard", GW::Constants::MapID::Warband_of_Brothers_Level_1, {{0, 0}}, "One on level 1, one on level 2, three on level 3"},
    {GW::Constants::SkillID::Mind_Blast, 0xff, "Flamemaster Maultooth", GW::Constants::MapID::Warband_of_Brothers_Level_1, {{0, 0}}, "Level 3"},

    {GW::Constants::SkillID::Devastating_Hammer, 0xff, "Inscribed Sentry", GW::Constants::MapID::Finding_the_Bloodstone_Level_1, {{0, 0}}, "One on level 1, one on level 2"},
    {GW::Constants::SkillID::Feast_of_Corruption, 0xff, "Inscribed Guardian", GW::Constants::MapID::Destructions_Depths_Level_1, {{0, 0}}, "Impossible to capture this boss' elite because the cinematic starts as soon as he dies."},

    {GW::Constants::SkillID::Enraged_Smash, 0xff, "Indestructible Golem", GW::Constants::MapID::The_Elusive_Golemancer_Level_1, {{0, 0}},"Level 2"},

    // Kryta bosses that weren't added to MappingOut
    {GW::Constants::SkillID::Power_Block, 5, "Justiciar Sevaan", GW::Constants::MapID::War_in_Kryta_Divinity_Coast, {{500, 145}}, "Only during \"War in Kryta\"."},
    {GW::Constants::SkillID::Elemental_Attunement, 5, "Vess the Disputant", GW::Constants::MapID::War_in_Kryta_Divinity_Coast, {{333, 115}}, "Only during \"War in Kryta\"."},
    {GW::Constants::SkillID::Forceful_Blow, 5, "Cairn the Berserker", GW::Constants::MapID::War_in_Kryta_Divinity_Coast, {{323, 123}}, "Only during \"War in Kryta\"."},
    {GW::Constants::SkillID::Aura_of_Faith, 5, "Aily the Innocent", GW::Constants::MapID::War_in_Kryta_Divinity_Coast, {{533, 160}}, "Only during \"A Little Help From Above\" quest."},

    {GW::Constants::SkillID::Blessed_Light, 5, "Amalek the Unmerciful", GW::Constants::MapID::Watchtower_Coast, {{590, 250}}, "Only during \"War in Kryta\"."},
    {GW::Constants::SkillID::Destructive_Was_Glaive, 5, "Calamitous", GW::Constants::MapID::Watchtower_Coast, {{760, 200}}, "Only during \"War in Kryta\"."},
    {GW::Constants::SkillID::Energy_Surge, 5, "Minea the Obscene", GW::Constants::MapID::Watchtower_Coast, {{750, 270}}, "Only during \"War in Kryta\"."},
    {GW::Constants::SkillID::Savannah_Heat, 5, "Inquisitor Lovisa", GW::Constants::MapID::Watchtower_Coast, {{700, 187}}, "Only during \"Wanted: Inquisitor Lovisa\" quest."},

    {GW::Constants::SkillID::Ray_of_Judgment, 5, "Barthimus the Provident", GW::Constants::MapID::Cursed_Lands, {{855, 490}}, "Only during \"War in Kryta\"."},

    {GW::Constants::SkillID::Whirling_Axe, 5, "Imuk the Pungent", GW::Constants::MapID::Nebo_Terrace, {{880, 445}}, "Only during \"War in Kryta\"."},
    {GW::Constants::SkillID::Aura_of_the_Lich, 5, "Cerris", GW::Constants::MapID::Nebo_Terrace, {{923, 485}}, "Only during \"War in Kryta\"."},
    {GW::Constants::SkillID::Migraine, 5, "Lerita the Lewd", GW::Constants::MapID::Nebo_Terrace, {{920, 435}}, "Only during \"War in Kryta\"."},
    {GW::Constants::SkillID::Energy_Surge, 5, "Maximilian the Meticulous", GW::Constants::MapID::Nebo_Terrace, {{970, 470}}, "Only during \"War in Kryta\", appears after observing the first two scenes in Shining Blade camp."}, 

    {GW::Constants::SkillID::Soldiers_Fury, 5, "Insatiable Vakar", GW::Constants::MapID::North_Kryta_Province, {{1095, 685}}, "Only during \"War in Kryta\"."},
    {GW::Constants::SkillID::Onslaught, 5, "Lev the Condemned", GW::Constants::MapID::North_Kryta_Province, {{1190, 419}}, "Only during \"War in Kryta\"."},
    {GW::Constants::SkillID::Ravenous_Gaze, 5, "Inquisitor Bauer", GW::Constants::MapID::North_Kryta_Province, {{1123, 429}}, "Only during \"War in Kryta\"."},
    {GW::Constants::SkillID::Temple_Strike, 5, "Sarnia the Red-Handed", GW::Constants::MapID::North_Kryta_Province, {{1135, 555}}, "Only during \"War in Kryta\"."},
    {GW::Constants::SkillID::Burning_Arrow, 5, "Inquisitor Lashona", GW::Constants::MapID::North_Kryta_Province, {{1162, 430}}, "Only during \"Wanted: Inquisitor Lashona\" quest."}, 
    {GW::Constants::SkillID::Wither, 5, "Mentanl Arobo", GW::Constants::MapID::North_Kryta_Province, {{1176, 419}}, "Only during \"Defend North Kryta Province\"."},

    {GW::Constants::SkillID::Magehunters_Smash, 5, "Selenas the Blunt", GW::Constants::MapID::Twin_Serpent_Lakes, {{145, 1050}}, "Only during \"War in Kryta\"."},
    {GW::Constants::SkillID::Melandrus_Shot, 5, "Valis the Rampant", GW::Constants::MapID::Twin_Serpent_Lakes, {{175, 960}}, "Only during \"War in Kryta\"."},

    {GW::Constants::SkillID::Tease, 5, "Justiciar Kimii", GW::Constants::MapID::War_in_Kryta_Riverside_Province, {{270, 1175}}, "Only during \"War in Kryta\"."},
    {GW::Constants::SkillID::Barrage, 5, "Cairn the Cunning", GW::Constants::MapID::War_in_Kryta_Riverside_Province, {{290, 1190}}, "Only during \"War in Kryta\"."},
    {GW::Constants::SkillID::Migraine, 5, "Ven the Conservator", GW::Constants::MapID::War_in_Kryta_Riverside_Province, {{13, 1310}}, "Only during \"War in Kryta\"."},
    {GW::Constants::SkillID::Ravenous_Gaze, 5, "Degaz the Cynical", GW::Constants::MapID::War_in_Kryta_Riverside_Province, {{50, 1210}}, "Only during \"War in Kryta\"."},
    {GW::Constants::SkillID::Energy_Surge, 5, "Willem the Demeaning", GW::Constants::MapID::War_in_Kryta_Riverside_Province, {{290, 1165}}, "Only during \"Riverside Assassination\" quest."},

    {GW::Constants::SkillID::Earth_Shaker, 5, "Carnak the Hungry", GW::Constants::MapID::The_Black_Curtain, {{680, 625}}, "Only during \"War in Kryta\"."},
    {GW::Constants::SkillID::Shove, 5, "Greves the Overbearing", GW::Constants::MapID::The_Black_Curtain, {{595, 510}}, "Only during \"War in Kryta\"."},

    {GW::Constants::SkillID::Magebane_Shot, 5, "Teral the Punisher", GW::Constants::MapID::Kessex_Peak, {{505, 730}}, "Only during \"War in Kryta\"."},
    {GW::Constants::SkillID::Youre_All_Alone, 5, "Justiciar Amilyn", GW::Constants::MapID::Kessex_Peak, {{515, 800}}, "Only during \"War in Kryta\"."},
    {GW::Constants::SkillID::Toxic_Chill, 5, "Justiciar Marron", GW::Constants::MapID::Kessex_Peak, {{490, 842}}, "Only during \"War in Kryta\"."},
    {GW::Constants::SkillID::Spirits_Strength, 5, "Destor the Truth Seeker", GW::Constants::MapID::Kessex_Peak, {{605, 830}}, "Only during \"War in Kryta\"."},
    {GW::Constants::SkillID::Hundred_Blades, 5, "Galrath", GW::Constants::MapID::Kessex_Peak, {{500, 920}}, "Only during \"The Villainy of Galrath\" quest in hard mode."},

    {GW::Constants::SkillID::Zealous_Vow, 5, "Justiciar Kasandra", GW::Constants::MapID::War_in_Kryta_DAlessio_Seaboard, {{930, 810}}, "Only during \"War in Kryta\"."},
    {GW::Constants::SkillID::Spiteful_Spirit, 5, "Zaln the Jaded", GW::Constants::MapID::War_in_Kryta_DAlessio_Seaboard, {{1100, 772}}, "Only during \"War in Kryta\"."},
    {GW::Constants::SkillID::Backbreaker, 5, "Cairn the Grave", GW::Constants::MapID::War_in_Kryta_DAlessio_Seaboard, {{945, 820}}, "Only during \"War in Kryta\"."},
    {GW::Constants::SkillID::Life_Transfer, 5, "Koril the Malignant", GW::Constants::MapID::War_in_Kryta_DAlessio_Seaboard, {{930, 830}}, "Only during \"Temple of the Intolerable\" quest."},

    {GW::Constants::SkillID::Coward, 5, "Joh the Hostile", GW::Constants::MapID::Scoundrels_Rise, {{1415, 510}}, "Only during \"War in Kryta\"."},
    {GW::Constants::SkillID::Weaken_Knees, 5, "Inquisitor Bauer", GW::Constants::MapID::Scoundrels_Rise, {{1465, 535}}, "Only during \"Wanted: Inquisitor Bauer\" quest."},

    {GW::Constants::SkillID::Devastating_Hammer, 5, "Perfected Armor", GW::Constants::MapID::War_in_Kryta_The_Battle_for_Lions_Arch, {{1260, 695}}, "Only during \"The Battle for Lion's Arch\" quest."},
    {GW::Constants::SkillID::Eviscerate, 5, "High Inquisitor Toriimo", GW::Constants::MapID::War_in_Kryta_The_Battle_for_Lions_Arch, {{1275, 695}}, "Only during \"The Battle for Lion's Arch\" quest."},
    {GW::Constants::SkillID::Experts_Dexterity, 5, "Confessor Isaiah", GW::Constants::MapID::War_in_Kryta_The_Battle_for_Lions_Arch, {{1290, 695}}, "Only during \"The Battle for Lion's Arch\" quest."},
    {GW::Constants::SkillID::Incendiary_Arrows, 5, "Confessor Isaiah", GW::Constants::MapID::War_in_Kryta_The_Battle_for_Lions_Arch, {{1305, 695}}, "Only during \"The Battle for Lion's Arch\" quest."},
    {GW::Constants::SkillID::Magebane_Shot, 5, "Cairn the Malignant", GW::Constants::MapID::War_in_Kryta_The_Battle_for_Lions_Arch, {{1320, 695}}, "Only during \"The Battle for Lion's Arch\" quest."},
    {GW::Constants::SkillID::Oath_Shot, 5, "Perfected Cloak", GW::Constants::MapID::War_in_Kryta_The_Battle_for_Lions_Arch, {{1335, 695}}, "Only during \"The Battle for Lion's Arch\" quest."},
    {GW::Constants::SkillID::Prepared_Shot, 5, "Cairn the Vengeful", GW::Constants::MapID::War_in_Kryta_The_Battle_for_Lions_Arch, {{1260, 718}}, "Only during \"The Battle for Lion's Arch\" quest."},
    {GW::Constants::SkillID::Glimmer_of_Light, 5, "Ambrillus the Guardian", GW::Constants::MapID::War_in_Kryta_The_Battle_for_Lions_Arch, {{1275, 718}}, "Only during \"The Battle for Lion's Arch\" quest."},
    {GW::Constants::SkillID::Corrupt_Enchantment, 5, "Oizys the Miserable", GW::Constants::MapID::War_in_Kryta_The_Battle_for_Lions_Arch, {{1290, 718}}, "Only during \"The Battle for Lion's Arch\" quest."},
    {GW::Constants::SkillID::Crippling_Anguish, 5, "Lucent the Spectral", GW::Constants::MapID::War_in_Kryta_The_Battle_for_Lions_Arch, {{1305, 718}}, "Only during \"The Battle for Lion's Arch\" quest."},
    {GW::Constants::SkillID::Thunderclap, 5, "Talios the Resplendent", GW::Constants::MapID::War_in_Kryta_The_Battle_for_Lions_Arch, {{1320, 718}}, "Only during \"The Battle for Lion's Arch\" quest."},
    {GW::Constants::SkillID::Ward_Against_Harm, 5, "Perfected Aegis", GW::Constants::MapID::War_in_Kryta_The_Battle_for_Lions_Arch, {{1335, 718}}, "Only during \"The Battle for Lion's Arch\" quest."},
    };

    std::string BossInfo(const EliteBossLocation* boss)
    {
        auto str = std::format("{} - {}\n{}", boss->boss_name, Resources::GetSkillName(boss->skill_id)->string(), Resources::GetMapName(boss->map_id)->string());
        if (boss->note) {
            str += std::format("\n{}", boss->note);
        }
        return str;
    }   

    uint32_t __cdecl GetCartographyFlagsForArea(uint32_t, uint32_t, uint32_t, uint32_t)
    {
        return 0xffffffff;
    }



    bool MapContainsWorldPos(GW::Constants::MapID map_id, const GW::Vec2f& world_map_pos, GW::Continent continent)
    {
        const auto map = GW::Map::GetMapInfo(map_id);
        if (!(map && map->continent == continent))
            return false;
        ImRect map_bounds;
        return GW::Map::GetMapWorldMapBounds(map, &map_bounds) && map_bounds.Contains(world_map_pos);
    }

    bool ContextMenuMarkerButtons() {
        if (ImGui::Button("Place Marker")) {
            GW::GameThread::Enqueue([] {
                QuestModule::SetCustomQuestMarker(world_map_click_pos, true);
            });
            return false;
        }
        if (QuestModule::GetCustomQuestMarker()) {
            if (ImGui::Button("Remove Marker")) {
                GW::GameThread::Enqueue([] {
                    QuestModule::SetCustomQuestMarker({0, 0});
                });
                return false;
            }
        }
        return true;
    }

    bool WorldMapContextMenu(void*)
    {
        if (!GW::Map::GetWorldMapContext())
            return false;

        ImGui::Text("%.2f, %.2f", world_map_click_pos.x, world_map_click_pos.y);
#ifdef _DEBUG
        GW::GamePos game_pos;
        if (WorldMapWidget::WorldMapToGamePos(world_map_click_pos, game_pos)) {
            ImGui::Text("%.2f, %.2f", game_pos.x, game_pos.y);
        }
#endif
        const auto map_id = WorldMapWidget::GetMapIdForLocation(world_map_click_pos);
        ImGui::TextUnformatted(Resources::GetMapName(map_id)->string().c_str());

        if (!ContextMenuMarkerButtons()) return false;
        return true;
    }

    bool HoveredQuestContextMenu(void* wparam)
    {
        if (!GW::Map::GetWorldMapContext())
            return false;
        const auto quest_id = static_cast<GW::Constants::QuestID>(reinterpret_cast<uint32_t>(wparam));
        const auto quest = GW::QuestMgr::GetQuest(quest_id);
        if (!quest)
            return false;
        if (!hovered_quest_name.IsDecoding())
            hovered_quest_name.reset(quest->name);
        ImGui::TextUnformatted(hovered_quest_name.string().c_str());

        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_Button, ImColor(0, 0, 0, 0).Value);
        const auto size = ImVec2(250.0f * ImGui::GetIO().FontGlobalScale, 0);
        ImGui::Separator();
        const bool set_active = ImGui::Button("Set active quest", size);
        const bool travel = ImGui::Button("Travel to nearest outpost", size);
        const bool wiki = ImGui::Button("Guild Wars Wiki", size);

        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::Separator();
        if (!ContextMenuMarkerButtons()) return false;

        if (set_active) {
            GW::GameThread::Enqueue([quest_id] {
                GW::QuestMgr::SetActiveQuestId(quest_id);
            });
            return false;
        }
        if (travel) {
            if (TravelWindow::Instance().TravelNearest(quest->map_to))
                return false;
        }
        if (wiki) {
            GW::GameThread::Enqueue([quest_id] {
                if (GW::QuestMgr::GetQuest(quest_id)) {
                    const auto wiki_url = std::format("{}Game_link:Quest_{}", GuiUtils::WikiUrl(L""), static_cast<uint32_t>(quest_id));
                    SendUIMessage(GW::UI::UIMessage::kOpenWikiUrl, (void*)wiki_url.c_str());
                }
            });
            return false;
        }
        return true;
    }

    bool EliteBossLocationContextMenu(void* wparam)
    {
        if (!GW::Map::GetWorldMapContext()) return false;
        const auto boss = (EliteBossLocation*)wparam;
        if (!boss) return false;
        ImGui::Text("%s", BossInfo(boss).c_str());

        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_Button, ImColor(0, 0, 0, 0).Value);
        const auto size = ImVec2(250.0f * ImGui::GetIO().FontGlobalScale, 0);
        ImGui::Separator();

        const bool travel = ImGui::Button("Travel to nearest outpost", size);

        const auto boss_label = std::format("{} on Guild Wars Wiki", boss->boss_name);
        const bool boss_wiki = ImGui::Button(boss_label.c_str(), size);

        const auto skill_label = std::format("{} on Guild Wars Wiki", Resources::GetSkillName(boss->skill_id)->string());
        const bool skill_wiki = ImGui::Button(skill_label.c_str(), size);

        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        ImGui::Separator();
        if (!ContextMenuMarkerButtons()) return false;


        if (travel) {
            if (TravelWindow::Instance().TravelNearest(boss->map_id)) return false;
        }
        if (boss_wiki) {
            GuiUtils::SearchWiki(TextUtils::StringToWString(boss->boss_name));
            return false;
        }
        if (skill_wiki) {
            GW::GameThread::Enqueue([boss] {
                const auto wiki_url = std::format("{}Game_link:Skill_{}", GuiUtils::WikiUrl(L""), static_cast<uint32_t>(boss->skill_id));
                SendUIMessage(GW::UI::UIMessage::kOpenWikiUrl, (void*)wiki_url.c_str());
            });
            return false;
        }
        return true;
    }

    uint32_t GetMapPropModelFileId(GW::MapProp* prop)
    {
        if (!(prop && prop->h0034[4]))
            return 0;
        uint32_t* sub_deets = (uint32_t*)prop->h0034[4];
        return ArenaNetFileParser::FileHashToFileId((wchar_t*)sub_deets[1]);
    };

    bool IsTravelPortal(GW::MapProp* prop)
    {
        switch (GetMapPropModelFileId(prop)) {
            case 0x4e6b2: // Eotn asura gate
            case 0x3c5ac: // Eotn, Nightfall
            case 0xa825:  // Prophecies, Factions
                return true;
        }
        return false;
    }

    bool IsValidOutpost(GW::Constants::MapID map_id)
    {
        const auto map_info = GW::Map::GetMapInfo(map_id);
        if (!map_info || !map_info->thumbnail_id || !map_info->name_id || !(map_info->x || map_info->y))
            return false;
        if ((map_info->flags & 0x5000000) == 0x5000000)
            return false; // e.g. "wrong" augury rock is map 119, no NPCs
        if ((map_info->flags & 0x80000000) == 0x80000000)
            return false; // e.g. Debug map
        switch (map_info->type) {
            case GW::RegionType::City:
            case GW::RegionType::CompetitiveMission:
            case GW::RegionType::CooperativeMission:
            case GW::RegionType::EliteMission:
            case GW::RegionType::MissionOutpost:
            case GW::RegionType::Outpost:
                break;
            default:
                return false;
        }
        return true;
    }

    struct MapPortal {
        GW::Constants::MapID from;
        GW::Constants::MapID to;
        GW::Vec2f world_pos;
    };

    std::vector<MapPortal> map_portals;

    GW::Constants::MapID GetClosestMapToPoint(const GW::Vec2f& world_map_point)
    {
        for (size_t i = 0; i < (size_t)GW::Constants::MapID::Count; i++) {
            const auto map_info = GW::Map::GetMapInfo((GW::Constants::MapID)i);
            if (!map_info || !map_info->thumbnail_id || !map_info->name_id || !(map_info->x || map_info->y))
                continue;
            if ((map_info->flags & 0x5000000) == 0x5000000)
                continue; // e.g. "wrong" augury rock is map 119, no NPCs
            if ((map_info->flags & 0x80000000) == 0x80000000)
                continue; // e.g. Debug map
            if (!map_info->GetIsOnWorldMap())
                continue;
            (world_map_point);
            // TODO: distance from point to rect
        }
        return GW::Constants::MapID::None;
    }

    GW::MapProp* GetClosestPortalToLocation(const GW::Vec2f& game_pos)
    {
        GW::MapProp* found = nullptr;
        float closest_distance = .9999f;
        const auto props = GW::Map::GetMapProps();
        if (!props)
            return found;
        for (auto prop : *props) {
            if (!IsTravelPortal(prop))
                continue;
            // TOOD: If found is null or this prop->location is closer than the found one, this wins
            // Calculate the distance between the current portal and the given location
            float distance = GW::GetDistance(prop->position, game_pos);

            // If found is null or this portal is closer than the currently found one, update found
            if (!found || distance < closest_distance) {
                found = prop;
                closest_distance = distance;
            }
        }
        return found;
    }

    bool AppendMapPortals()
    {
        const auto props = GW::Map::GetMapProps();
        const auto map_id = GW::Map::GetMapID();
        if (!props) return false;
        for (auto prop : *props) {
            if (IsTravelPortal(prop)) {
                GW::Vec2f world_pos;
                if (!WorldMapWidget::GamePosToWorldMap({prop->position.x, prop->position.y}, world_pos))
                    continue;
                map_portals.push_back({
                    map_id, GetClosestMapToPoint(world_pos), world_pos
                });
            }
        }
        return true;
    }


    GW::HookEntry OnUIMessage_HookEntry;

    void OnUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void*, void*)
    {
        if (status->blocked)
            return;

        switch (message_id) {
            case GW::UI::UIMessage::kMapLoaded:
                map_portals.clear();
                AppendMapPortals();
                break;
        }
    }

    void TriggerWorldMapRedraw()
    {
        GW::GameThread::Enqueue([] {
            const auto ctx = GW::Map::GetWorldMapContext();
            const auto frame = ctx ? GW::UI::GetFrameById(ctx->frame_id) : nullptr;
            GW::UI::DestroyUIComponent(frame) && GW::UI::Keypress(GW::UI::ControlAction_OpenWorldMap), true;
        });
    }

    // Helper function to calculate rotated points
    void CalculateRotatedPoints(const ImRect& rect, const ImVec2& center, float rotation_angle, ImVec2 out_points[4])
    {
        ImVec2 points[4] = {
            rect.Min,                 // Top-left
            {rect.Max.x, rect.Min.y}, // Top-right
            rect.Max,                 // Bottom-right
            {rect.Min.x, rect.Max.y}  // Bottom-left
        };

        for (int i = 0; i < 4; ++i) {
            const float dx = points[i].x - center.x;
            const float dy = points[i].y - center.y;

            // Apply the rotation transformation using rotation_angle
            out_points[i] = {
                center.x + dx * cos(rotation_angle) - dy * sin(rotation_angle),
                center.y + dx * sin(rotation_angle) + dy * cos(rotation_angle)
            };
        }
    }

    // Helper function to calculate UV coordinates for a sprite map
    void CalculateUVCoords(float uv_start_x, float uv_end_x, ImVec2 uv_points[4])
    {
        uv_points[0] = {uv_start_x, 0.0f}; // Top-left
        uv_points[1] = {uv_end_x, 0.0f};   // Top-right
        uv_points[2] = {uv_end_x, 1.0f};   // Bottom-right
        uv_points[3] = {uv_start_x, 1.0f}; // Bottom-left
    }



    // Function to calculate viewport position
    ImVec2 CalculateViewportPos(const GW::Vec2f& marker_world_pos, const ImVec2& top_left)
    {
        return {
            ui_scale.x * world_map_scale * (marker_world_pos.x - top_left.x) + viewport_offset.x,
            ui_scale.y * world_map_scale * (marker_world_pos.y - top_left.y) + viewport_offset.y
        };
    }

    GW::Vec2f GetMapMarkerPoint(GW::AreaInfo* map_info)
    {
        if (!map_info)
            return {};
        if (map_info->x && map_info->y) {
            // If the map has an icon x and y coord, use that as the custom quest marker position
            // NB: GW places this marker at the top of the outpost icon, not the center - probably to make it easier to see? sounds daft, don't copy it.
            return {
                (float)map_info->x,
                (float)map_info->y
            };
        }
        if (map_info->icon_start_x && map_info->icon_start_y) {
            // Otherwise use the center position of the map name label
            return {
                (float)(map_info->icon_start_x + ((map_info->icon_end_x - map_info->icon_start_x) / 2)),
                (float)(map_info->icon_start_y + ((map_info->icon_end_y - map_info->icon_start_y) / 2))
            };
        }
        // Otherwise use the center position of the map name label
        return {
            (float)(map_info->icon_start_x_dupe + ((map_info->icon_end_x_dupe - map_info->icon_start_x_dupe) / 2)),
            (float)(map_info->icon_start_y_dupe + ((map_info->icon_end_y_dupe - map_info->icon_start_y_dupe) / 2))
        };
    }

    // Pre-calculate some cached vars for this frame to avoid having to recalculate more than once
    bool PreCalculateFrameVars()
    {
        world_map_context = GW::Map::GetWorldMapContext();
        if (!world_map_context)
            return false;
        const auto viewport = ImGui::GetMainViewport();
        viewport_offset = viewport->Pos;
        draw_list = ImGui::GetBackgroundDrawList(viewport);
        ui_scale = GW::UI::GetFrameById(world_map_context->frame_id)->position.GetViewportScale(GW::UI::GetRootFrame());

        const auto me = GW::Agents::GetControlledCharacter();
        if (!(me && WorldMapWidget::GamePosToWorldMap(me->pos, player_world_map_pos)))
            return false;
        player_rotation = me->rotation_angle;

        const GW::Vec2f world_map_size_in_coords = {
            (float)world_map_context->h004c[5],
            (float)world_map_context->h004c[6]
        };
        const GW::Vec2f world_map_zoomed_out_size = {
            world_map_context->h0030,
            world_map_context->h0034
        };

        world_map_scale = 1.f;
        if (world_map_context->zoom != 1.0f) {
            // If we're zoomed out, the world map coordinates aren't 1:1 scale; we need to find the scale factor
            if (world_map_context->top_left.y == 0.f) {
                // The zoomed out map fills vertically
                world_map_scale = world_map_zoomed_out_size.y / world_map_size_in_coords.y;
            }
            else {
                // The zoomed out map fills horizontally
                world_map_scale = world_map_zoomed_out_size.x / world_map_size_in_coords.x;
            }
        }
        quest_icon_size = 24.0f * ui_scale.x;
        quest_icon_size_half = quest_icon_size / 2.f;

        constexpr float FULL_ROTATION_TIME = 16.0f;
        const float elapsed_seconds = static_cast<float>(TIMER_INIT()) / CLOCKS_PER_SEC;
        quest_star_rotation_angle = 2.0f * (float)M_PI * fmod(elapsed_seconds, FULL_ROTATION_TIME) / FULL_ROTATION_TIME;

        return true;
    }

    GW::Vec2f southern_shivers_start = {4570.f, 4230.f};
    GW::Vec2f post_searing_region = {5705.f, 3590.f};
    GW::Vec2f maguuma_region = {220.f, 4150.f};
    GW::Vec2f crystal_desert_region = {6105.f, 6735.f};
    GW::Vec2f ring_of_fire_region = {1070.f, 7190.f};
    GW::Vec2f kryta_region = {2150.f, 3800.f};

    GW::Vec2f shing_jea_island = {700.f,2050.f};
    GW::Vec2f kaineng_city = {2430.f, 590.f};
    GW::Vec2f echovald_forest = {3220.f, 2120.f};
    GW::Vec2f jade_sea = {3790.f, 1925.f};

    GW::Vec2f istan = {625.f, 2660.f};
    GW::Vec2f kourna = {2760.f, 1645.f};
    GW::Vec2f vabbi = {3230.f, 360.f};
    GW::Vec2f desolation = {1860.f, 87.f};

    GW::Vec2f far_shiverpeaks = {4470.f, 810.f};
    GW::Vec2f charr_homelands = {6640.f, 1300.f};
    GW::Vec2f tarnished_coast = {1210.f, 5770.f};

    ImVec2 skill_texture_size = {};

    float default_scale = 1.37f;

    std::unordered_map<GW::Constants::MapID, uint32_t> locations_assigned_to_outposts;

    bool DrawBossLocationOnWorldMap(const EliteBossLocation& boss)
    {
        if (!show_any_elite_capture_locations) return false;
        if (!(world_map_context)) return false;
        if (world_map_context->zoom != 1.f && world_map_context->zoom != .0f) return false; // Map is animating

        const auto map_info = GW::Map::GetMapInfo(boss.map_id);
        if (!(map_info && map_info->continent == world_map_context->continent)) 
            return false;

        const auto skill = GW::SkillbarMgr::GetSkillConstantData(boss.skill_id);
        if (!skill) return false;
        if (!show_elite_capture_locations[(uint32_t)skill->profession]) return false;
        if (hide_captured_elites) {
            const auto me = GW::Agents::GetControlledCharacter();
            if (me->primary == (uint8_t)skill->profession || me->secondary == (uint8_t)skill->profession) {
                if (GW::SkillbarMgr::GetIsSkillLearnt(boss.skill_id)) return false;
            }
            else {
                const auto my_name = GW::PlayerMgr::GetPlayerName();
                const auto& completion = CompletionWindow::Instance().GetCharacterCompletion(my_name, false);
                if (completion) {
                    if (CompletionWindow::IsSkillUnlocked(my_name, skill->skill_id)) return false;
                }
                else
                    return false;
            }
        }

        const auto texture = Resources::GetSkillImage(boss.skill_id);
        // const auto texture = Resources::GetProfessionIcon((GW::Constants::Profession)skill->profession);

        if (!(texture && *texture)) return false;

        if (!Resources::GetTextureSize(*texture, &skill_texture_size)) return false;

        float icon_size = world_map_context->zoom == 1.f ? 32.f : 16.f;
        const auto half_size = icon_size / 2.f;

        bool hovered = false;
        float elites_scale = default_scale;
        for (auto boss_pos : boss.coords) {

            GW::Vec2f* region_offset = nullptr;
            switch (map_info->campaign) {
                case GW::Constants::Campaign::Prophecies: {
                    switch (boss.region_id) {
                        case 1: {
                            region_offset = &crystal_desert_region;
                        } break;
                        case 2: {
                            region_offset = &southern_shivers_start;
                        } break;
                        case 3: {
                            region_offset = &ring_of_fire_region;
                        } break;
                        case 4: {
                            region_offset = &post_searing_region;
                        } break;
                        case 5: {
                            region_offset = &kryta_region;
                        } break;
                        case 7: {
                            region_offset = &maguuma_region;
                        } break;
                    }
                } break;
                case GW::Constants::Campaign::Factions: {
                    switch (boss.region_id) {
                        case 1: {
                            region_offset = &kaineng_city;
                        } break;
                        case 2: {
                            region_offset = &echovald_forest;
                        } break;
                        case 3: {
                            region_offset = &jade_sea;
                        } break;
                        case 4: {
                            region_offset = &shing_jea_island;
                        } break;
                    }
                } break;
                case GW::Constants::Campaign::Nightfall: {
                    switch (boss.region_id) {
                        case 1: {
                            region_offset = &istan;
                        } break;
                        case 2: {
                            region_offset = &kourna;
                        } break;
                        case 3: {
                            region_offset = &vabbi;
                            elites_scale = 1.34f;
                        } break;
                        case 4: {
                            region_offset = &desolation;
                            elites_scale = 1.34f;
                        } break;
                        case 5: {
                            // Domain of Anguish is where the devs just gave up trying to make the world map useful.
                            boss_pos = GW::Vec2f((float)map_info->x - icon_size * 2.f, (float)map_info->y);
                            locations_assigned_to_outposts[boss.map_id]++;
                            boss_pos.x += icon_size * (locations_assigned_to_outposts[boss.map_id] - 1);
                            elites_scale = 1.f;
                        }
                    }
                } break;
                case GW::Constants::Campaign::EyeOfTheNorth: {
                    elites_scale = 1.315f;
                    switch (boss.region_id) {
                        case 1: {
                            region_offset = &far_shiverpeaks;
                        } break;
                        case 2: {
                            region_offset = &charr_homelands;
                        } break;
                        case 3: {
                            region_offset = &tarnished_coast;
                            if (boss.map_id == GW::Constants::MapID::Sparkfly_Swamp) {
                                // MappingOut stitches the tarnished coast together, which means I had to manually place the sparkfly swamp elites myself. No need to offset or scale
                                region_offset = nullptr;
                                elites_scale = 1.f;
                            }
                        } break;
                    }
                } break;
            }
            if (boss.region_id == 0xff) {
                boss_pos = GW::Vec2f((float)map_info->x - icon_size * 2.f, (float)map_info->y);
                locations_assigned_to_outposts[boss.map_id]++;
                boss_pos.x += icon_size * (locations_assigned_to_outposts[boss.map_id] - 1);
                elites_scale = 1.f;
            }

            boss_pos *= elites_scale;
            if (region_offset) {
                boss_pos += *region_offset;
            }


            const auto viewport_quest_pos = CalculateViewportPos(boss_pos, world_map_context->top_left);


            const ImRect icon_rect = {{viewport_quest_pos.x - half_size, viewport_quest_pos.y - half_size}, {viewport_quest_pos.x + half_size, viewport_quest_pos.y + half_size}};

            ImGui::AddImageScaled(draw_list, *texture, icon_rect.Min, skill_texture_size, icon_size, icon_size);
            hovered |= icon_rect.Contains(ImGui::GetMousePos());
        }

        return hovered;

    }

    bool DrawQuestMarkerOnWorldMap(const GW::Quest* quest)
    {
        if (!(world_map_context && quest))
            return false;
        if (world_map_context->zoom != 1.f && world_map_context->zoom != .0f)
            return false; // Map is animating


        bool is_hovered = false;
        auto color = GW::QuestMgr::GetActiveQuestId() == quest->quest_id ? 0 : 0x80FFFFFF;
        if (apply_quest_colors) {
            color = QuestModule::GetQuestColor(quest->quest_id);
        }

        // draw_quest_marker
        const auto draw_quest_marker = [&](const GW::Vec2f& quest_marker_pos) {
            const auto viewport_quest_pos = CalculateViewportPos(quest_marker_pos, world_map_context->top_left);

            const ImRect icon_rect = {
                {viewport_quest_pos.x - quest_icon_size_half, viewport_quest_pos.y - quest_icon_size_half},
                {viewport_quest_pos.x + quest_icon_size_half, viewport_quest_pos.y + quest_icon_size_half}
            };

            ImVec2 rotated_points[4];
            CalculateRotatedPoints(icon_rect, viewport_quest_pos, quest_star_rotation_angle, rotated_points);

            ImVec2 uv_points[4];
            CalculateUVCoords(0.0f, 0.5f, uv_points); // Left-hand side of the sprite map

            draw_list->AddImageQuad(
                *quest_icon_texture,
                rotated_points[0], rotated_points[1], rotated_points[2], rotated_points[3],
                uv_points[0], uv_points[1], uv_points[2], uv_points[3], color & IM_COL32_A_MASK ? color : IM_COL32_WHITE
            );

            return icon_rect.Contains(ImGui::GetMousePos());
        };

        // draw_quest_arrow
        const auto draw_quest_arrow = [&](const GW::Vec2f& quest_marker_pos) {
            const auto viewport_quest_pos = CalculateViewportPos(quest_marker_pos, world_map_context->top_left);
            const auto viewport_player_pos = CalculateViewportPos(player_world_map_pos, world_map_context->top_left);
            // Calculate the vector from your position to the quest marker
            const float dx = viewport_quest_pos.x - viewport_player_pos.x;
            const float dy = viewport_quest_pos.y - viewport_player_pos.y;

            // Calculate the rotation angle in radians using atan2, pointing away from the player
            float rotation_angle = std::atan2f(-dy, -dx);
            rotation_angle += DirectX::XM_PI;

            const ImRect icon_rect = {
                {viewport_quest_pos.x - quest_icon_size_half, viewport_quest_pos.y - quest_icon_size_half},
                {viewport_quest_pos.x + quest_icon_size_half, viewport_quest_pos.y + quest_icon_size_half}
            };

            ImVec2 rotated_points[4];
            CalculateRotatedPoints(icon_rect, viewport_quest_pos, rotation_angle, rotated_points);

            ImVec2 uv_points[4];
            CalculateUVCoords(0.5f, 1.0f, uv_points); // Right-hand side of the sprite map

            draw_list->AddImageQuad(
                *quest_icon_texture,
                rotated_points[0], rotated_points[1], rotated_points[2], rotated_points[3], uv_points[0], uv_points[1], uv_points[2], uv_points[3], color & IM_COL32_A_MASK ? color : IM_COL32_WHITE
            );

            return icon_rect.Contains(ImGui::GetMousePos());
        };

        // The quest doesn't end in this map; the marker icon needs to be an arrow, and the actual marker needs to be positioned onto the label of the destination map
        const auto map_info = GW::Map::GetMapInfo(quest->map_to);
        if (!(map_info && map_info->continent == world_map_context->continent))
            return false;
        GW::Vec2f pos;
        if (WorldMapWidget::GamePosToWorldMap(quest->marker, pos)) {
            if (quest->map_to != GW::Map::GetMapID()) {
                is_hovered |= draw_quest_arrow(pos);
            }
            else {
                is_hovered |= draw_quest_marker(pos);
            }
        }
        if (quest->map_to != GW::Map::GetMapID() || world_map_context->zoom == .0f) {
            is_hovered |= draw_quest_marker(GetMapMarkerPoint(map_info));
        }
        return is_hovered;
    }
}

GW::Constants::MapID WorldMapWidget::GetMapIdForLocation(const GW::Vec2f& world_map_pos, GW::Constants::MapID exclude_map_id)
{
    auto map_id = GW::Map::GetMapID();
    auto map_info = GW::Map::GetMapInfo();
    if (!map_info)
        return GW::Constants::MapID::None;
    const auto continent = map_info->continent;
    if (map_id != exclude_map_id && MapContainsWorldPos(map_id, world_map_pos, continent))
        return map_id;
    for (size_t i = 1; i < static_cast<size_t>(GW::Constants::MapID::Count); i++) {
        map_id = static_cast<GW::Constants::MapID>(i);
        if (map_id == exclude_map_id)
            continue;
        map_info = GW::Map::GetMapInfo(map_id);
        if (!(map_info && map_info->GetIsOnWorldMap()))
            continue;
        if (MapContainsWorldPos(map_id, world_map_pos, continent))
            return map_id;
    }
    return GW::Constants::MapID::None;
}

void WorldMapWidget::Initialize()
{
    ToolboxWidget::Initialize();

    memset(show_elite_capture_locations, true, sizeof(show_elite_capture_locations));
    quest_icon_texture = GwDatTextureModule::LoadTextureFromFileId(0x1b4d5);
    player_icon_texture = GwDatTextureModule::LoadTextureFromFileId(0x5d3b);

    uintptr_t address = GW::Scanner::Find("\x8b\x45\xfc\xf7\x40\x10\x00\x00\x01\x00", "xxxxxxxxxx", 0xa);
    if (address) {
        view_all_outposts_patch.SetPatch(address, "\xeb", 1);
    }
    address = GW::Scanner::Find("\x8b\xd8\x83\xc4\x10\x8b\xcb\x8b\xf3\xd1\xe9", "xxxxxxxxxxx", -0x5);
    if (address) {
        view_all_carto_areas_patch.SetRedirect(address, GetCartographyFlagsForArea);
    }

    ASSERT(view_all_outposts_patch.IsValid());
    ASSERT(view_all_carto_areas_patch.IsValid());

    const GW::UI::UIMessage ui_messages[] = {
        GW::UI::UIMessage::kQuestAdded,
        GW::UI::UIMessage::kSendSetActiveQuest,
        GW::UI::UIMessage::kMapLoaded,
        GW::UI::UIMessage::kOnScreenMessage,
        GW::UI::UIMessage::kSendAbandonQuest
    };
    for (auto ui_message : ui_messages) {
        GW::UI::RegisterUIMessageCallback(&OnUIMessage_HookEntry, ui_message, OnUIMessage);
    }
}

bool WorldMapWidget::WorldMapToGamePos(const GW::Vec2f& world_map_pos, GW::GamePos& game_map_pos)
{
    ImRect map_bounds;
    if (!GW::Map::GetMapWorldMapBounds(GW::Map::GetMapInfo(), &map_bounds))
        return false;

    const auto current_map_context = GW::GetMapContext();
    if (!current_map_context)
        return false;

    const auto game_map_rect = ImRect({
        current_map_context->start_pos.x, 
        current_map_context->start_pos.y, 
        current_map_context->end_pos.x, 
        current_map_context->end_pos.y
    });

    constexpr auto gwinches_per_unit = 96.f;

    // Calculate the mid-point of the map in world coordinates
    GW::Vec2f map_mid_world_point = {
        map_bounds.Min.x + (abs(game_map_rect.Min.x) / gwinches_per_unit),
        map_bounds.Min.y + (abs(game_map_rect.Max.y) / gwinches_per_unit),
    };

    // Convert from world map position to game map position
    game_map_pos.x = (world_map_pos.x - map_mid_world_point.x) * gwinches_per_unit;
    game_map_pos.y = (world_map_pos.y - map_mid_world_point.y) * gwinches_per_unit * -1.f; // Invert Y axis

    return true;
}

bool WorldMapWidget::GamePosToWorldMap(const GW::GamePos& game_map_pos, GW::Vec2f& world_map_pos)
{
    if (game_map_pos.x == INFINITY || game_map_pos.y == INFINITY)
        return false;
    ImRect map_bounds;
    if (!GW::Map::GetMapWorldMapBounds(GW::Map::GetMapInfo(), &map_bounds))
        return false;
    const auto current_map_context = GW::GetMapContext();
    if (!current_map_context)
        return false;

    const auto game_map_rect = ImRect({
        current_map_context->start_pos.x,
        current_map_context->start_pos.y,
        current_map_context->end_pos.x,
        current_map_context->end_pos.y
    });

    // NB: World map is 96 gwinches per unit, this is hard coded in the GW source
    constexpr auto gwinches_per_unit = 96.f;
    GW::Vec2f map_mid_world_point = {
        map_bounds.Min.x + (abs(game_map_rect.Min.x) / gwinches_per_unit),
        map_bounds.Min.y + (abs(game_map_rect.Max.y) / gwinches_per_unit),
    };

    world_map_pos.x = (game_map_pos.x / gwinches_per_unit) + map_mid_world_point.x;
    world_map_pos.y = ((game_map_pos.y * -1.f) / gwinches_per_unit) + map_mid_world_point.y; // Inverted Y Axis
    return true;
}

void WorldMapWidget::SignalTerminate()
{
    ToolboxWidget::Terminate();

    view_all_outposts_patch.Reset();
    view_all_carto_areas_patch.Reset();
    GW::UI::RemoveUIMessageCallback(&OnUIMessage_HookEntry);
}

void WorldMapWidget::ShowAllOutposts(const bool show = showing_all_outposts)
{
    if (view_all_outposts_patch.IsValid())
        view_all_outposts_patch.TogglePatch(show);
    if (view_all_carto_areas_patch.IsValid())
        view_all_carto_areas_patch.TogglePatch(show);
    TriggerWorldMapRedraw();
}

void WorldMapWidget::LoadSettings(ToolboxIni* ini)
{
    ToolboxWidget::LoadSettings(ini);
    LOAD_BOOL(showing_all_outposts);
    LOAD_BOOL(show_lines_on_world_map);
    LOAD_BOOL(showing_all_quests);
    LOAD_BOOL(apply_quest_colors);
    LOAD_BOOL(hide_captured_elites);
    LOAD_BOOL(show_any_elite_capture_locations);
    LOAD_BOOL(hide_captured_elites);
    uint32_t show_elite_capture_locations_val = 0xffffffff;
    LOAD_UINT(show_elite_capture_locations_val);
    for (size_t i = 0; i < _countof(show_elite_capture_locations); i++) {
        show_elite_capture_locations[i] = ((show_elite_capture_locations_val >> i) & 0x1) != 0;
    }
    ShowAllOutposts(showing_all_outposts);
}

void WorldMapWidget::SaveSettings(ToolboxIni* ini)
{
    ToolboxWidget::SaveSettings(ini);
    SAVE_BOOL(showing_all_outposts);
    SAVE_BOOL(show_lines_on_world_map);
    SAVE_BOOL(showing_all_quests);
    SAVE_BOOL(apply_quest_colors);
    SAVE_BOOL(show_any_elite_capture_locations);
    SAVE_BOOL(hide_captured_elites);
    uint32_t show_elite_capture_locations_val = 0;
    for (size_t i = 0; i < _countof(show_elite_capture_locations); i++) {
        if (show_elite_capture_locations[i]) {
            show_elite_capture_locations_val |= (1u << i);
        }
    }
    SAVE_UINT(show_elite_capture_locations_val);
}

void WorldMapWidget::Draw(IDirect3DDevice9*)
{
    if (!(GW::UI::GetIsWorldMapShowing() && PreCalculateFrameVars())) {
        //ShowAllOutposts(showing_all_outposts = false);
        drawn = false;
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::SetNextWindowPos(ImVec2(16.f, 16.f), ImGuiCond_FirstUseEver);
    visible = true;
    ImGuiWindow* window = nullptr;
    auto mouse_offset = viewport_offset;
    mouse_offset.x *= -1;
    mouse_offset.y *= -1;
    if (ImGui::Begin(Name(), &visible, GetWinFlags() | ImGuiWindowFlags_AlwaysAutoResize)) {
        window = ImGui::GetCurrentWindowRead();
        if (ImGui::Checkbox("Show all areas", &showing_all_outposts)) {
            GW::GameThread::Enqueue([] {
                ShowAllOutposts(showing_all_outposts);
            });
        }
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
            bool is_hard_mode = GW::PartyMgr::GetIsPartyInHardMode();
            if (ImGui::Checkbox("Hard mode", &is_hard_mode)) {
                GW::GameThread::Enqueue([] {
                    GW::PartyMgr::SetHardMode(!GW::PartyMgr::GetIsPartyInHardMode());
                });
            }
        }
        ImGui::Checkbox("Show toolbox minimap lines", &show_lines_on_world_map);
        if (ImGui::Checkbox("Show quest markers for all quests", &showing_all_quests)) {
            QuestModule::FetchMissingQuestInfo();
        }
        ImGui::Checkbox("Apply quest marker color overlays", &apply_quest_colors);
        if (apply_quest_colors) {
            ImGui::Indent();
            auto color = &QuestModule::GetQuestColor((GW::Constants::QuestID)0xfff);
            ImGui::ColorButtonPicker("Other Quests", color, ImGuiColorEditFlags_NoLabel);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Color overlay for quests that aren't active.");
            }
            if (GW::QuestMgr::GetActiveQuestId() != GW::Constants::QuestID::None) {
                ImGui::SameLine();
                color = &QuestModule::GetQuestColor(GW::QuestMgr::GetActiveQuestId());
                ImGui::ColorButtonPicker("Active Quest", color, ImGuiColorEditFlags_NoLabel);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Color overlay for the active quest.");
                }
            }
            ImGui::Unindent();
        }
    }
    ImGui::Checkbox("Show elite capture locations",&show_any_elite_capture_locations);
    if (show_any_elite_capture_locations) {
        ImGui::Indent();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0.f, 0.f});
        for (size_t i = 1; i < _countof(show_elite_capture_locations); i++) {
            const auto icon = Resources::GetProfessionIcon((GW::Constants::Profession)i);
            if (!(icon && *icon)) continue;
            if (i != 1) ImGui::SameLine();
            ImGui::PushID(i);
            ImGui::PushStyleColor(ImGuiCol_Button, show_elite_capture_locations[i] ? completed_bg.Value : ImGui::GetStyleColorVec4(ImGuiCol_Button));
            if (ImGui::IconButton("", *icon, {24.f, 24.f})) {
                show_elite_capture_locations[i] = !show_elite_capture_locations[i];
            }
            ImGui::PopStyleColor();
            ImGui::PopID();
        }
        ImGui::PopStyleVar();
        ImGui::Checkbox("Hide elites already captured", &hide_captured_elites);
        if (hide_captured_elites) {
            const auto& completion = CompletionWindow::Instance().GetCharacterCompletion(GW::PlayerMgr::GetPlayerName(), false);
            if (!completion)
                ImGui::TextDisabled("Limited to your primary/secondary profession if Completion Window is disabled");
        }
        ImGui::Unindent();
    }
    //ImGui::InputFloat("region.x", &tarnished_coast.x, 10.f);
    //ImGui::InputFloat("region.y", &tarnished_coast.y, 10.f);
    //ImGui::InputFloat("default_scale", &default_scale, 0.001f);
    ImGui::End();
    ImGui::PopStyleColor();


    if (window) {
        controls_window_rect = window->Rect();
        controls_window_rect.Translate(mouse_offset);
    }

    AppendMapPortals();

    hovered_boss = nullptr;
    locations_assigned_to_outposts.clear();
    for (auto& boss : elite_boss_locations) {
        if (DrawBossLocationOnWorldMap(boss)) {
            hovered_boss = &boss;
        }
    }

    hovered_quest_id = GW::Constants::QuestID::None;
    // Draw all quest markers on world map if applicable
    if (showing_all_quests) {
        if (const auto quest_log = GW::QuestMgr::GetQuestLog()) {
            for (auto& quest : *quest_log) {
                if (DrawQuestMarkerOnWorldMap(&quest)) {
                    hovered_quest_id = quest.quest_id;
                }
            }
        }
    }
    const auto active = GW::QuestMgr::GetActiveQuest();
    if (DrawQuestMarkerOnWorldMap(active)) {
        hovered_quest_id = active->quest_id;
    }
    if (hovered_quest_id != GW::Constants::QuestID::None) {
        if (const auto hovered_quest = GW::QuestMgr::GetQuest(hovered_quest_id)) {
            static GuiUtils::EncString quest_name;
            if (!quest_name.IsDecoding())
                quest_name.reset(hovered_quest->name);
            ImGui::SetTooltip("%s", quest_name.string().c_str());
        }
    }
    if (hovered_boss) {
        ImGui::SetTooltip("%s", BossInfo(hovered_boss).c_str());
    }

    /*for (const auto& portal : map_portals) {
        static constexpr auto uv0 = ImVec2(0.0f, 0.0f);
        static constexpr auto ICON_SIZE = ImVec2(24.0f, 24.0f);

        const ImVec2 portal_pos = {
            ui_scale.x * (portal.world_pos.x - world_map_context->top_left.x) + viewport_offset.x - (ICON_SIZE.x / 2.f),
            ui_scale.y * (portal.world_pos.y - world_map_context->top_left.y) + viewport_offset.y - (ICON_SIZE.y / 2.f)
        };

        const ImRect hover_rect = {
            portal_pos, {portal_pos.x + ICON_SIZE.x, portal_pos.y + ICON_SIZE.y}
        };

        draw_list->AddImage(*GwDatTextureModule::LoadTextureFromFileId(0x1b4d5), hover_rect.GetTL(), hover_rect.GetBR());


        if (hover_rect.Contains(ImGui::GetMousePos())) {
            ImGui::SetTooltip("Portal");
        }
    }*/
    if (show_lines_on_world_map) {
        const auto& lines = Minimap::Instance().custom_renderer.GetLines();
        const auto map_id = GW::Map::GetMapID();
        GW::Vec2f line_start;
        GW::Vec2f line_end;
        for (auto& line : lines | std::views::filter([](auto line) { return line->visible; })) {
            if (line->map != map_id)
                continue;
            if (!GamePosToWorldMap(line->p1, line_start))
                continue;
            if (!GamePosToWorldMap(line->p2, line_end))
                continue;

            line_start.x = (line_start.x - world_map_context->top_left.x) * ui_scale.x + viewport_offset.x;
            line_start.y = (line_start.y - world_map_context->top_left.y) * ui_scale.y + viewport_offset.y;
            line_end.x = (line_end.x - world_map_context->top_left.x) * ui_scale.x + viewport_offset.x;
            line_end.y = (line_end.y - world_map_context->top_left.y) * ui_scale.y + viewport_offset.y;

            draw_list->AddLine(line_start, line_end, line->color);
        }
    }
    if (show_any_elite_capture_locations) {
        const auto rect = draw_list->GetClipRectMax();
        const auto text = "Elite capture locations extracted from MappingOut v4.0.0 by Aylee Sedai";
        draw_list->AddText({16.f, rect.y - 28.f}, ImGui::GetColorU32(ImGuiCol_TextDisabled), text);
    }
    drawn = true;
}

bool WorldMapWidget::WndProc(const UINT Message, WPARAM, LPARAM lParam)
{
    auto check_rect = [lParam](const ImRect& rect) {
        ImVec2 p = {(float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam)};
        return rect.Contains(p);
    };

    switch (Message) {
        case WM_GW_RBUTTONCLICK: {
            if (!(world_map_context && GW::UI::GetIsWorldMapShowing()))
                break;
            if (GW::QuestMgr::GetQuest(hovered_quest_id)) {
                ImGui::SetContextMenu(HoveredQuestContextMenu, (void*)hovered_quest_id);
                break;
            }

            if (!(world_map_context && world_map_context->zoom == 1.0f))
                break;
            world_map_click_pos = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            world_map_click_pos.x /= ui_scale.x;
            world_map_click_pos.y /= ui_scale.y;
            world_map_click_pos.x += world_map_context->top_left.x;
            world_map_click_pos.y += world_map_context->top_left.y;
            if (hovered_boss) {
                ImGui::SetContextMenu(EliteBossLocationContextMenu, (void*)hovered_boss);
                break;
            }
            ImGui::SetContextMenu(WorldMapContextMenu);
        }
        break;
        case WM_LBUTTONDBLCLK:
        case WM_LBUTTONUP:
        case WM_LBUTTONDOWN:
            if (!drawn || !GW::UI::GetIsWorldMapShowing())
                return false;
            if (ImGui::ShowingContextMenu())
                return true;
            if (check_rect(controls_window_rect))
                return true;
            break;
    }
    return false;
}

void WorldMapWidget::DrawSettingsInternal()
{
    ImGui::Text("Note: only visible in Hard Mode explorable areas.");
}
