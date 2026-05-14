#pragma once

#include <GWCA/Constants/Maps.h>
#include <GWCA/Constants/QuestIDs.h>

namespace ZaishenMissionMaps {

using MapID = GW::Constants::MapID;
using QuestID = GW::Constants::QuestID;

struct Entry {
    MapID map_id;
    QuestID quest_id;
};

// Maps each Zaishen Mission cycle map_id to its corresponding QuestID,
// used to look up the copper zaishen coin reward for the current daily mission.
constexpr Entry map_quest_ids[] = {
    {MapID::The_Great_Northern_Wall, QuestID::ZaishenMission_The_Great_Northern_Wall},
    {MapID::Fort_Ranik, QuestID::ZaishenMission_Fort_Ranik},
    {MapID::Ruins_of_Surmia, QuestID::ZaishenMission_Ruins_of_Surmia},
    {MapID::Nolani_Academy, QuestID::ZaishenMission_Nolani_Academy},
    {MapID::Borlis_Pass, QuestID::ZaishenMission_Borlis_Pass},
    {MapID::The_Frost_Gate, QuestID::ZaishenMission_The_Frost_Gate},
    {MapID::Gates_of_Kryta, QuestID::ZaishenMission_Gates_of_Kryta},
    {MapID::DAlessio_Seaboard, QuestID::ZaishenMission_DAlessio_Seaboard},
    {MapID::Divinity_Coast, QuestID::ZaishenMission_Divinity_Coast},
    {MapID::The_Wilds, QuestID::ZaishenMission_The_Wilds},
    {MapID::Bloodstone_Fen, QuestID::ZaishenMission_Bloodstone_Fen},
    {MapID::Aurora_Glade, QuestID::ZaishenMission_Aurora_Glade},
    {MapID::Riverside_Province, QuestID::ZaishenMission_Riverside_Province},
    {MapID::Sanctum_Cay, QuestID::ZaishenMission_Sanctum_Cay},
    {MapID::Dunes_of_Despair, QuestID::ZaishenMission_Dunes_of_Despair},
    {MapID::Thirsty_River, QuestID::ZaishenMission_Thirsty_River},
    {MapID::Elona_Reach, QuestID::ZaishenMission_Elona_Reach},
    {MapID::Augury_Rock_mission, QuestID::ZaishenMission_Augury_Rock},
    {MapID::The_Dragons_Lair, QuestID::ZaishenMission_The_Dragons_Lair},
    {MapID::Ice_Caves_of_Sorrow, QuestID::ZaishenMission_Ice_Caves_of_Sorrow},
    {MapID::Iron_Mines_of_Moladune, QuestID::ZaishenMission_Iron_Mines_of_Moladune},
    {MapID::Thunderhead_Keep, QuestID::ZaishenMission_Thunderhead_Keep},
    {MapID::Ring_of_Fire, QuestID::ZaishenMission_Ring_of_Fire},
    {MapID::Abaddons_Mouth, QuestID::ZaishenMission_Abaddons_Mouth},
    {MapID::Hells_Precipice, QuestID::ZaishenMission_Hells_Precipice},
    {MapID::Zen_Daijun_outpost_mission, QuestID::ZaishenMission_Zen_Daijun},
    {MapID::Vizunah_Square_mission, QuestID::ZaishenMission_Vizunah_Square},
    {MapID::Nahpui_Quarter_outpost_mission, QuestID::ZaishenMission_Nahpui_Quarter},
    {MapID::Tahnnakai_Temple_outpost_mission, QuestID::ZaishenMission_Tahnnakai_Temple},
    {MapID::Arborstone_outpost_mission, QuestID::ZaishenMission_Arborstone},
    {MapID::Boreas_Seabed_outpost_mission, QuestID::ZaishenMission_Boreas_Seabed},
    {MapID::Sunjiang_District_outpost_mission, QuestID::ZaishenMission_Sunjiang_District},
    {MapID::The_Eternal_Grove_outpost_mission, QuestID::ZaishenMission_The_Eternal_Grove},
    {MapID::Unwaking_Waters_mission, QuestID::ZaishenMission_Unwaking_Waters},
    {MapID::Gyala_Hatchery_outpost_mission, QuestID::ZaishenMission_Gyala_Hatchery},
    {MapID::Raisu_Palace_outpost_mission, QuestID::ZaishenMission_Raisu_Palace},
    {MapID::Imperial_Sanctum_outpost_mission, QuestID::ZaishenMission_Imperial_Sanctum},
    {MapID::Chahbek_Village, QuestID::ZaishenMission_Chahbek_Village},
    {MapID::Jokanur_Diggings, QuestID::ZaishenMission_Jokanur_Diggings},
    {MapID::Blacktide_Den, QuestID::ZaishenMission_Blacktide_Den},
    {MapID::Consulate_Docks, QuestID::ZaishenMission_Consulate_Docks},
    {MapID::Venta_Cemetery, QuestID::ZaishenMission_Venta_Cemetery},
    {MapID::Kodonur_Crossroads, QuestID::ZaishenMission_Kodonur_Crossroads},
    {MapID::Pogahn_Passage, QuestID::ZaishenMission_Pogahn_Passage},
    {MapID::Rilohn_Refuge, QuestID::ZaishenMission_Rilohn_Refuge},
    {MapID::Moddok_Crevice, QuestID::ZaishenMission_Moddok_Crevice},
    {MapID::Tihark_Orchard, QuestID::ZaishenMission_Tihark_Orchard},
    {MapID::Dasha_Vestibule, QuestID::ZaishenMission_Dasha_Vestibule},
    {MapID::Dzagonur_Bastion, QuestID::ZaishenMission_Dzagonur_Bastion},
    {MapID::Grand_Court_of_Sebelkeh, QuestID::ZaishenMission_Grand_Court_of_Sebelkeh},
    {MapID::Jennurs_Horde, QuestID::ZaishenMission_Jennurs_Horde},
    {MapID::Nundu_Bay, QuestID::ZaishenMission_Nundu_Bay},
    {MapID::Gate_of_Desolation, QuestID::ZaishenMission_Gate_of_Desolation},
    {MapID::Ruins_of_Morah, QuestID::ZaishenMission_Ruins_of_Morah},
    {MapID::Gate_of_Pain, QuestID::ZaishenMission_Gate_of_Pain},
    {MapID::Gate_of_Madness, QuestID::ZaishenMission_Gate_of_Madness},
    {MapID::Abaddons_Gate, QuestID::ZaishenMission_Abaddons_Gate},
    {MapID::Minister_Chos_Estate_outpost_mission, QuestID::ZaishenMission_Minister_Chos_Estate},
    {MapID::Finding_the_Bloodstone_mission, QuestID::ZaishenMission_Finding_the_Bloodstone},
    {MapID::The_Elusive_Golemancer_mission, QuestID::ZaishenMission_The_Elusive_Golemancer},
    {MapID::Genius_Operated_Living_Enchanted_Manifestation_mission, QuestID::ZaishenMission_G_O_L_E_M},
    {MapID::Against_the_Charr_mission, QuestID::ZaishenMission_Against_the_Charr},
    {MapID::Warband_of_brothers_mission, QuestID::ZaishenMission_Warband_of_Brothers},
    {MapID::Assault_on_the_Stronghold_mission, QuestID::ZaishenMission_Assault_on_the_Stronghold},
    {MapID::Curse_of_the_Nornbear_mission, QuestID::ZaishenMission_Curse_of_the_Nornbear},
    {MapID::Blood_Washes_Blood_mission, QuestID::ZaishenMission_Blood_Washes_Blood},
    {MapID::A_Gate_Too_Far_mission, QuestID::ZaishenMission_A_Gate_Too_Far},
    {MapID::Destructions_Depths_mission, QuestID::ZaishenMission_Destructions_Depths},
    {MapID::A_Time_for_Heroes_mission, QuestID::ZaishenMission_A_Time_for_Heroes},
};

inline const QuestID* GetQuestID(const MapID map_id)
{
    for (const auto& entry : map_quest_ids) {
        if (entry.map_id == map_id) return &entry.quest_id;
    }
    return nullptr;
}

} // namespace ZaishenMissionMaps
