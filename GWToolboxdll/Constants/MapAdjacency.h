#pragma once

#include <GWCA/Constants/Maps.h>

#include <array>
#include <span>

namespace MapAdjacency {

using MapID = GW::Constants::MapID;

struct AdjacencyEntry {
    MapID map_id;
    std::span<const MapID> neighbors;
};

constexpr MapID adj_Bloodstone_Fen[] = {
    MapID::Silverwood,
};
constexpr MapID adj_The_Wilds[] = {
    MapID::Sage_Lands,
};
constexpr MapID adj_Aurora_Glade[] = {
    MapID::Ettins_Back,
};
constexpr MapID adj_Diessa_Lowlands[] = {
    MapID::Nolani_Academy,
    MapID::Grendich_Courthouse_outpost,
    MapID::The_Breach,
    MapID::Ascalon_Foothills,
    MapID::Flame_Temple_Corridor,
};
constexpr MapID adj_Gates_of_Kryta[] = {
    MapID::Scoundrels_Rise,
};
constexpr MapID adj_DAlessio_Seaboard[] = {
    MapID::North_Kryta_Province,
};
constexpr MapID adj_Divinity_Coast[] = {
    MapID::Watchtower_Coast,
};
constexpr MapID adj_Talmark_Wilderness[] = {
    MapID::The_Black_Curtain,
    MapID::Tears_of_the_Fallen,
    MapID::Majestys_Rest,
};
constexpr MapID adj_The_Black_Curtain[] = {
    MapID::Talmark_Wilderness,
    MapID::Cursed_Lands,
    MapID::Kessex_Peak,
    MapID::Temple_of_the_Ages,
};
constexpr MapID adj_Sanctum_Cay[] = {
    MapID::Stingray_Strand,
};
constexpr MapID adj_Droknars_Forge_outpost[] = {
    MapID::Droknars_Forge_outpost,
    MapID::Talus_Chute,
    MapID::Witmans_Folly,
};
constexpr MapID adj_Ice_Caves_of_Sorrow[] = {
    MapID::Talus_Chute,
};
constexpr MapID adj_Thunderhead_Keep[] = {
    MapID::Ice_Floe,
};
constexpr MapID adj_Iron_Mines_of_Moladune[] = {
    MapID::Frozen_Forest,
};
constexpr MapID adj_Talus_Chute[] = {
    MapID::Droknars_Forge_outpost,
    MapID::Ice_Caves_of_Sorrow,
    MapID::Icedome,
    MapID::Camp_Rankor_outpost,
};
constexpr MapID adj_Griffons_Mouth[] = {
    MapID::Scoundrels_Rise,
    MapID::Deldrimor_Bowl,
};
constexpr MapID adj_The_Great_Northern_Wall[] = {
    MapID::Ascalon_City_outpost,
};
constexpr MapID adj_Fort_Ranik[] = {
    MapID::Regent_Valley,
};
constexpr MapID adj_Ruins_of_Surmia[] = {
    MapID::Eastern_Frontier,
};
constexpr MapID adj_Xaquang_Skyway[] = {
    MapID::Senjis_Corner_outpost,
    MapID::Shenzun_Tunnels,
    MapID::Wajjun_Bazaar,
    MapID::Bukdek_Byway,
};
constexpr MapID adj_Nolani_Academy[] = {
    MapID::Diessa_Lowlands,
};
constexpr MapID adj_Old_Ascalon[] = {
    MapID::Sardelac_Sanitarium_outpost,
    MapID::Ascalon_City_outpost,
    MapID::Regent_Valley,
    MapID::The_Breach,
};
constexpr MapID adj_Ember_Light_Camp_outpost[] = {
    MapID::Perdition_Rock,
};
constexpr MapID adj_Grendich_Courthouse_outpost[] = {
    MapID::Diessa_Lowlands,
};
constexpr MapID adj_Augury_Rock_outpost[] = {
    MapID::Augury_Rock_outpost,
    MapID::Prophets_Path,
    MapID::Skyward_Reach,
};
constexpr MapID adj_Sardelac_Sanitarium_outpost[] = {
    MapID::Old_Ascalon,
};
constexpr MapID adj_Piken_Square_outpost[] = {
    MapID::The_Breach,
};
constexpr MapID adj_Sage_Lands[] = {
    MapID::The_Wilds,
    MapID::Mamnoon_Lagoon,
    MapID::Majestys_Rest,
    MapID::Druids_Overlook_outpost,
};
constexpr MapID adj_Mamnoon_Lagoon[] = {
    MapID::Sage_Lands,
    MapID::Silverwood,
};
constexpr MapID adj_Silverwood[] = {
    MapID::Bloodstone_Fen,
    MapID::Mamnoon_Lagoon,
    MapID::Ettins_Back,
    MapID::Quarrel_Falls_outpost,
};
constexpr MapID adj_Ettins_Back[] = {
    MapID::Aurora_Glade,
    MapID::Silverwood,
    MapID::Reed_Bog,
    MapID::Dry_Top,
    MapID::Ventaris_Refuge_outpost,
};
constexpr MapID adj_Reed_Bog[] = {
    MapID::Ettins_Back,
    MapID::The_Falls,
};
constexpr MapID adj_The_Falls[] = {
    MapID::Reed_Bog,
    MapID::Secret_Underground_Lair,
};
constexpr MapID adj_Dry_Top[] = {
    MapID::Ettins_Back,
    MapID::Tangle_Root,
};
constexpr MapID adj_Tangle_Root[] = {
    MapID::Dry_Top,
    MapID::Henge_of_Denravi_outpost,
    MapID::Riverside_Province,
    MapID::Maguuma_Stade_outpost,
};
constexpr MapID adj_Henge_of_Denravi_outpost[] = {
    MapID::Tangle_Root,
};
constexpr MapID adj_Senjis_Corner_outpost[] = {
    MapID::Xaquang_Skyway,
    MapID::Nahpui_Quarter_explorable,
};
constexpr MapID adj_Tears_of_the_Fallen[] = {
    MapID::Talmark_Wilderness,
    MapID::Twin_Serpent_Lakes,
    MapID::Stingray_Strand,
};
constexpr MapID adj_Scoundrels_Rise[] = {
    MapID::Gates_of_Kryta,
    MapID::Griffons_Mouth,
    MapID::North_Kryta_Province,
};
constexpr MapID adj_Lions_Arch_outpost[] = {
    MapID::North_Kryta_Province,
    MapID::Lions_Gate,
    MapID::War_in_Kryta_Lions_Arch_Keep,
};
constexpr MapID adj_Cursed_Lands[] = {
    MapID::The_Black_Curtain,
    MapID::Nebo_Terrace,
};
constexpr MapID adj_Bergen_Hot_Springs_outpost[] = {
    MapID::Nebo_Terrace,
};
constexpr MapID adj_North_Kryta_Province[] = {
    MapID::DAlessio_Seaboard,
    MapID::Scoundrels_Rise,
    MapID::Lions_Arch_outpost,
    MapID::Nebo_Terrace,
    MapID::Beneath_Lions_Arch,
};
constexpr MapID adj_Nebo_Terrace[] = {
    MapID::Cursed_Lands,
    MapID::Bergen_Hot_Springs_outpost,
    MapID::North_Kryta_Province,
    MapID::Beetletun_outpost,
};
constexpr MapID adj_Majestys_Rest[] = {
    MapID::Talmark_Wilderness,
    MapID::Sage_Lands,
};
constexpr MapID adj_Twin_Serpent_Lakes[] = {
    MapID::Tears_of_the_Fallen,
    MapID::Riverside_Province,
};
constexpr MapID adj_Watchtower_Coast[] = {
    MapID::Divinity_Coast,
    MapID::Beetletun_outpost,
};
constexpr MapID adj_Stingray_Strand[] = {
    MapID::Sanctum_Cay,
    MapID::Tears_of_the_Fallen,
    MapID::Fishermens_Haven_outpost,
};
constexpr MapID adj_Kessex_Peak[] = {
    MapID::The_Black_Curtain,
};
constexpr MapID adj_The_Underworld[] = {
    MapID::Temple_of_the_Ages,
    MapID::Zin_Ku_Corridor_outpost,
    MapID::Scarred_Earth,
    MapID::Chantry_of_Secrets_outpost,
};
constexpr MapID adj_Riverside_Province[] = {
    MapID::Twin_Serpent_Lakes,
};
constexpr MapID adj_House_zu_Heltzer_outpost[] = {
    MapID::Ferndale,
    MapID::Altrumm_Ruins,
};
constexpr MapID adj_Ascalon_City_outpost[] = {
    MapID::The_Great_Northern_Wall,
    MapID::Old_Ascalon,
    MapID::Ascalon_Arena,
};
constexpr MapID adj_Tomb_of_the_Primeval_Kings[] = {
    MapID::The_Dragons_Lair,
    MapID::The_Underworld_PvP,
};
constexpr MapID adj_Icedome[] = {
    MapID::Talus_Chute,
    MapID::Frozen_Forest,
};
constexpr MapID adj_Iron_Horse_Mine[] = {
    MapID::Anvil_Rock,
    MapID::Travelers_Vale,
};
constexpr MapID adj_Anvil_Rock[] = {
    MapID::The_Frost_Gate,
    MapID::Iron_Horse_Mine,
    MapID::Deldrimor_Bowl,
    MapID::Ice_Tooth_Cave_outpost,
};
constexpr MapID adj_Lornars_Pass[] = {
    MapID::The_Underworld,
    MapID::Dreadnoughts_Drift,
    MapID::Beacons_Perch_outpost,
};
constexpr MapID adj_Snake_Dance[] = {
    MapID::Dreadnoughts_Drift,
    MapID::Camp_Rankor_outpost,
    MapID::Grenths_Footprint,
};
constexpr MapID adj_Tascas_Demise[] = {
    MapID::Mineral_Springs,
    MapID::The_Granite_Citadel_outpost,
};
constexpr MapID adj_Spearhead_Peak[] = {
    MapID::The_Granite_Citadel_outpost,
    MapID::Copperhammer_Mines_outpost,
    MapID::Grenths_Footprint,
};
constexpr MapID adj_Ice_Floe[] = {
    MapID::Thunderhead_Keep,
    MapID::Frozen_Forest,
    MapID::Marhans_Grotto_outpost,
};
constexpr MapID adj_Witmans_Folly[] = {
    MapID::Droknars_Forge_outpost,
    MapID::Port_Sledge_outpost,
};
constexpr MapID adj_Mineral_Springs[] = {
    MapID::Tascas_Demise,
};
constexpr MapID adj_Dreadnoughts_Drift[] = {
    MapID::Lornars_Pass,
    MapID::Snake_Dance,
};
constexpr MapID adj_Frozen_Forest[] = {
    MapID::Iron_Mines_of_Moladune,
    MapID::Icedome,
    MapID::Ice_Floe,
    MapID::Copperhammer_Mines_outpost,
};
constexpr MapID adj_Travelers_Vale[] = {
    MapID::Borlis_Pass,
    MapID::Iron_Horse_Mine,
    MapID::Ascalon_Foothills,
    MapID::Yaks_Bend_outpost,
};
constexpr MapID adj_Deldrimor_Bowl[] = {
    MapID::Griffons_Mouth,
    MapID::Anvil_Rock,
    MapID::Beacons_Perch_outpost,
};
constexpr MapID adj_Regent_Valley[] = {
    MapID::Fort_Ranik,
    MapID::Old_Ascalon,
    MapID::Pockmark_Flats,
};
constexpr MapID adj_The_Breach[] = {
    MapID::Diessa_Lowlands,
    MapID::Old_Ascalon,
    MapID::Piken_Square_outpost,
    MapID::Forsaken_Tunnels_Level1,
};
constexpr MapID adj_Ascalon_Foothills[] = {
    MapID::Diessa_Lowlands,
    MapID::Travelers_Vale,
};
constexpr MapID adj_Pockmark_Flats[] = {
    MapID::Regent_Valley,
    MapID::Eastern_Frontier,
    MapID::Serenity_Temple_outpost,
};
constexpr MapID adj_Dragons_Gullet[] = {
    MapID::Flame_Temple_Corridor,
};
constexpr MapID adj_Flame_Temple_Corridor[] = {
    MapID::Diessa_Lowlands,
    MapID::Dragons_Gullet,
};
constexpr MapID adj_Eastern_Frontier[] = {
    MapID::Ruins_of_Surmia,
    MapID::Pockmark_Flats,
    MapID::Frontier_Gate_outpost,
};
constexpr MapID adj_The_Scar[] = {
    MapID::Skyward_Reach,
    MapID::Thirsty_River,
    MapID::Destinys_Gorge_outpost,
};
constexpr MapID adj_The_Amnoon_Oasis_outpost[] = {
    MapID::Prophets_Path,
};
constexpr MapID adj_Diviners_Ascent[] = {
    MapID::Salt_Flats,
    MapID::Skyward_Reach,
    MapID::Elona_Reach,
};
constexpr MapID adj_Vulture_Drifts[] = {
    MapID::The_Arid_Sea,
    MapID::Prophets_Path,
    MapID::Skyward_Reach,
    MapID::Dunes_of_Despair,
};
constexpr MapID adj_The_Arid_Sea[] = {
    MapID::Vulture_Drifts,
    MapID::Skyward_Reach,
    MapID::Crystal_Overlook,
};
constexpr MapID adj_Prophets_Path[] = {
    MapID::Augury_Rock_outpost,
    MapID::The_Amnoon_Oasis_outpost,
    MapID::Vulture_Drifts,
    MapID::Salt_Flats,
    MapID::Heroes_Audience_outpost,
};
constexpr MapID adj_Salt_Flats[] = {
    MapID::Diviners_Ascent,
    MapID::Prophets_Path,
    MapID::Skyward_Reach,
    MapID::Seekers_Passage_outpost,
};
constexpr MapID adj_Skyward_Reach[] = {
    MapID::Augury_Rock_outpost,
    MapID::The_Scar,
    MapID::Diviners_Ascent,
    MapID::Vulture_Drifts,
    MapID::The_Arid_Sea,
    MapID::Salt_Flats,
    MapID::Destinys_Gorge_outpost,
};
constexpr MapID adj_Dunes_of_Despair[] = {
    MapID::Vulture_Drifts,
};
constexpr MapID adj_Thirsty_River[] = {
    MapID::The_Scar,
};
constexpr MapID adj_Elona_Reach[] = {
    MapID::Diviners_Ascent,
};
constexpr MapID adj_The_Dragons_Lair[] = {
    MapID::Tomb_of_the_Primeval_Kings,
};
constexpr MapID adj_Perdition_Rock[] = {
    MapID::Ember_Light_Camp_outpost,
    MapID::Ring_of_Fire,
};
constexpr MapID adj_The_Eternal_Grove[] = {
    MapID::The_Eternal_Grove,
    MapID::Vasburg_Armory_outpost,
    MapID::Drazach_Thicket,
    MapID::Mourning_Veil_Falls,
};
constexpr MapID adj_Lutgardis_Conservatory_outpost[] = {
    MapID::Melandrus_Hope,
    MapID::Ferndale,
};
constexpr MapID adj_Vasburg_Armory_outpost[] = {
    MapID::The_Eternal_Grove,
    MapID::Morostav_Trail,
};
constexpr MapID adj_Serenity_Temple_outpost[] = {
    MapID::Pockmark_Flats,
};
constexpr MapID adj_Beacons_Perch_outpost[] = {
    MapID::Lornars_Pass,
    MapID::Deldrimor_Bowl,
};
constexpr MapID adj_Frontier_Gate_outpost[] = {
    MapID::Eastern_Frontier,
};
constexpr MapID adj_Beetletun_outpost[] = {
    MapID::Nebo_Terrace,
    MapID::Watchtower_Coast,
};
constexpr MapID adj_Fishermens_Haven_outpost[] = {
    MapID::Stingray_Strand,
};
constexpr MapID adj_Temple_of_the_Ages[] = {
    MapID::The_Black_Curtain,
    MapID::The_Fissure_of_Woe,
    MapID::The_Underworld,
};
constexpr MapID adj_Ventaris_Refuge_outpost[] = {
    MapID::Ettins_Back,
};
constexpr MapID adj_Druids_Overlook_outpost[] = {
    MapID::Sage_Lands,
};
constexpr MapID adj_Maguuma_Stade_outpost[] = {
    MapID::Tangle_Root,
};
constexpr MapID adj_Quarrel_Falls_outpost[] = {
    MapID::Silverwood,
};
constexpr MapID adj_Gyala_Hatchery[] = {
    MapID::Gyala_Hatchery,
    MapID::Maishang_Hills,
    MapID::Rheas_Crater,
    MapID::Leviathan_Pits_outpost,
};
constexpr MapID adj_The_Catacombs[] = {
    MapID::Green_Hills_County,
    MapID::Wizards_Folly,
    MapID::Ashford_Abbey_outpost,
};
constexpr MapID adj_Lakeside_County[] = {
    MapID::The_Northlands,
    MapID::Ascalon_City_pre_searing,
    MapID::Green_Hills_County,
    MapID::Wizards_Folly,
    MapID::Regent_Valley_pre_Searing,
    MapID::Ashford_Abbey_outpost,
};
constexpr MapID adj_The_Northlands[] = {
    MapID::Lakeside_County,
    MapID::Piken_Square_pre_Searing_outpost,
    MapID::Forsaken_Tunnels_Presearing_Level1,
};
constexpr MapID adj_Ascalon_City_pre_searing[] = {
    MapID::Ascalon_Academy_outpost,
    MapID::Lakeside_County,
};
constexpr MapID adj_Heroes_Audience_outpost[] = {
    MapID::Prophets_Path,
};
constexpr MapID adj_Seekers_Passage_outpost[] = {
    MapID::Salt_Flats,
};
constexpr MapID adj_Destinys_Gorge_outpost[] = {
    MapID::The_Scar,
    MapID::Skyward_Reach,
};
constexpr MapID adj_Camp_Rankor_outpost[] = {
    MapID::Talus_Chute,
    MapID::Snake_Dance,
};
constexpr MapID adj_The_Granite_Citadel_outpost[] = {
    MapID::Tascas_Demise,
    MapID::Spearhead_Peak,
};
constexpr MapID adj_Marhans_Grotto_outpost[] = {
    MapID::Ice_Floe,
};
constexpr MapID adj_Port_Sledge_outpost[] = {
    MapID::Witmans_Folly,
};
constexpr MapID adj_Copperhammer_Mines_outpost[] = {
    MapID::Spearhead_Peak,
    MapID::Frozen_Forest,
};
constexpr MapID adj_Green_Hills_County[] = {
    MapID::The_Catacombs,
    MapID::Lakeside_County,
    MapID::Wizards_Folly,
    MapID::The_Barradin_Estate_outpost,
};
constexpr MapID adj_Wizards_Folly[] = {
    MapID::The_Catacombs,
    MapID::Lakeside_County,
    MapID::Green_Hills_County,
    MapID::Regent_Valley_pre_Searing,
    MapID::Foibles_Fair_outpost,
};
constexpr MapID adj_Regent_Valley_pre_Searing[] = {
    MapID::Lakeside_County,
    MapID::Wizards_Folly,
    MapID::Fort_Ranik_pre_Searing_outpost,
};
constexpr MapID adj_The_Barradin_Estate_outpost[] = {
    MapID::Green_Hills_County,
};
constexpr MapID adj_Ashford_Abbey_outpost[] = {
    MapID::The_Catacombs,
    MapID::Lakeside_County,
};
constexpr MapID adj_Foibles_Fair_outpost[] = {
    MapID::Wizards_Folly,
};
constexpr MapID adj_Fort_Ranik_pre_Searing_outpost[] = {
    MapID::Regent_Valley_pre_Searing,
};
constexpr MapID adj_Sorrows_Furnace[] = {
    MapID::Grenths_Footprint,
};
constexpr MapID adj_Grenths_Footprint[] = {
    MapID::Snake_Dance,
    MapID::Spearhead_Peak,
    MapID::Sorrows_Furnace,
    MapID::Deldrimor_War_Camp_outpost,
};
constexpr MapID adj_Cavalon_outpost[] = {
    MapID::Archipelagos,
    MapID::Zos_Shivros_Channel,
};
constexpr MapID adj_Kaineng_Center_outpost[] = {
    MapID::Bukdek_Byway,
    MapID::Bejunkan_Pier,
    MapID::Raisu_Pavilion,
};
constexpr MapID adj_Drazach_Thicket[] = {
    MapID::The_Eternal_Grove,
    MapID::Brauer_Academy_outpost,
    MapID::Saint_Anjekas_Shrine_outpost,
};
constexpr MapID adj_Jaya_Bluffs[] = {
    MapID::Haiju_Lagoon,
    MapID::Seitung_Harbor_outpost,
};
constexpr MapID adj_Shenzun_Tunnels[] = {
    MapID::Xaquang_Skyway,
    MapID::Sunjiang_District_explorable,
    MapID::Nahpui_Quarter_explorable,
    MapID::Tahnnakai_Temple_explorable,
    MapID::Maatu_Keep_outpost,
};
constexpr MapID adj_Archipelagos[] = {
    MapID::Cavalon_outpost,
    MapID::Maishang_Hills,
    MapID::Breaker_Hollow_outpost,
    MapID::Jade_Flats_Luxon_outpost,
};
constexpr MapID adj_Maishang_Hills[] = {
    MapID::Gyala_Hatchery,
    MapID::Archipelagos,
    MapID::Bai_Paasu_Reach_outpost,
    MapID::Eredon_Terrace_outpost,
};
constexpr MapID adj_Mount_Qinkai[] = {
    MapID::Boreas_Seabed_explorable,
    MapID::Breaker_Hollow_outpost,
    MapID::Aspenwood_Gate_Luxon_outpost,
};
constexpr MapID adj_Melandrus_Hope[] = {
    MapID::Lutgardis_Conservatory_outpost,
    MapID::Brauer_Academy_outpost,
    MapID::Jade_Flats_Kurzick_outpost,
};
constexpr MapID adj_Rheas_Crater[] = {
    MapID::Gyala_Hatchery,
    MapID::The_Aurios_Mines,
    MapID::Seafarers_Rest_outpost,
};
constexpr MapID adj_Silent_Surf[] = {
    MapID::Leviathan_Pits_outpost,
    MapID::Seafarers_Rest_outpost,
};
constexpr MapID adj_Morostav_Trail[] = {
    MapID::Vasburg_Armory_outpost,
    MapID::Durheim_Archives_outpost,
};
constexpr MapID adj_Deldrimor_War_Camp_outpost[] = {
    MapID::Grenths_Footprint,
};
constexpr MapID adj_Mourning_Veil_Falls[] = {
    MapID::The_Eternal_Grove,
    MapID::Amatz_Basin,
    MapID::Durheim_Archives_outpost,
};
constexpr MapID adj_Ferndale[] = {
    MapID::House_zu_Heltzer_outpost,
    MapID::Lutgardis_Conservatory_outpost,
    MapID::Saint_Anjekas_Shrine_outpost,
    MapID::Aspenwood_Gate_Kurzick_outpost,
};
constexpr MapID adj_Pongmei_Valley[] = {
    MapID::Boreas_Seabed_explorable,
    MapID::Sunjiang_District_explorable,
    MapID::Maatu_Keep_outpost,
    MapID::Tanglewood_Copse_outpost,
};
constexpr MapID adj_Monastery_Overlook1[] = {
    MapID::Shing_Jea_Monastery_outpost,
};
constexpr MapID adj_Zen_Daijun_outpost_mission[] = {
    MapID::Haiju_Lagoon,
};
constexpr MapID adj_Vizunah_Square_mission[] = {
    MapID::Vizunah_Square_Local_Quarter_outpost,
    MapID::Vizunah_Square_Foreign_Quarter_outpost,
};
constexpr MapID adj_Imperial_Sanctum_outpost_mission[] = {
    MapID::Raisu_Palace,
};
constexpr MapID adj_Unwaking_Waters[] = {
    MapID::Harvest_Temple_outpost,
};
constexpr MapID adj_Amatz_Basin[] = {
    MapID::Mourning_Veil_Falls,
};
constexpr MapID adj_Shadows_Passage[] = {
    MapID::Bukdek_Byway,
    MapID::Dragons_Throat,
};
constexpr MapID adj_Raisu_Palace[] = {
    MapID::Imperial_Sanctum_outpost_mission,
    MapID::Raisu_Palace,
    MapID::Raisu_Pavilion,
};
constexpr MapID adj_The_Aurios_Mines[] = {
    MapID::Rheas_Crater,
};
constexpr MapID adj_Panjiang_Peninsula[] = {
    MapID::Kinya_Province,
    MapID::Tsumei_Village_outpost,
};
constexpr MapID adj_Kinya_Province[] = {
    MapID::Panjiang_Peninsula,
    MapID::Sunqua_Vale,
    MapID::Ran_Musu_Gardens_outpost,
};
constexpr MapID adj_Haiju_Lagoon[] = {
    MapID::Jaya_Bluffs,
    MapID::Zen_Daijun_outpost_mission,
};
constexpr MapID adj_Sunqua_Vale[] = {
    MapID::Kinya_Province,
    MapID::Shing_Jea_Monastery_outpost,
    MapID::Minister_Chos_Estate_explorable,
    MapID::Tsumei_Village_outpost,
};
constexpr MapID adj_Wajjun_Bazaar[] = {
    MapID::Xaquang_Skyway,
    MapID::The_Undercity,
    MapID::Nahpui_Quarter_explorable,
    MapID::The_Marketplace_outpost,
};
constexpr MapID adj_Bukdek_Byway[] = {
    MapID::Xaquang_Skyway,
    MapID::Kaineng_Center_outpost,
    MapID::Shadows_Passage,
    MapID::The_Undercity,
    MapID::Vizunah_Square_Foreign_Quarter_outpost,
    MapID::The_Marketplace_outpost,
    MapID::Tunnels_Below_Cantha,
};
constexpr MapID adj_The_Undercity[] = {
    MapID::Wajjun_Bazaar,
    MapID::Bukdek_Byway,
    MapID::Vizunah_Square_Local_Quarter_outpost,
};
constexpr MapID adj_Shing_Jea_Monastery_outpost[] = {
    MapID::Sunqua_Vale,
    MapID::Shing_Jea_Arena,
    MapID::Linnok_Courtyard,
};
constexpr MapID adj_Arborstone_explorable[] = {
    MapID::Arborstone_explorable,
    MapID::Altrumm_Ruins,
    MapID::Tanglewood_Copse_outpost,
};
constexpr MapID adj_Minister_Chos_Estate_explorable[] = {
    MapID::Sunqua_Vale,
    MapID::Minister_Chos_Estate_explorable,
    MapID::Ran_Musu_Gardens_outpost,
};
constexpr MapID adj_Zen_Daijun_explorable[] = {
    MapID::Seitung_Harbor_outpost,
};
constexpr MapID adj_Boreas_Seabed_explorable[] = {
    MapID::Mount_Qinkai,
    MapID::Pongmei_Valley,
    MapID::Boreas_Seabed_explorable,
    MapID::Zos_Shivros_Channel,
};
constexpr MapID adj_Great_Temple_of_Balthazar_outpost[] = {
    MapID::Isle_of_the_Nameless,
    MapID::Isle_of_the_Nameless_PvP,
};
constexpr MapID adj_Tsumei_Village_outpost[] = {
    MapID::Panjiang_Peninsula,
    MapID::Sunqua_Vale,
};
constexpr MapID adj_Seitung_Harbor_outpost[] = {
    MapID::Jaya_Bluffs,
    MapID::Zen_Daijun_explorable,
    MapID::Kaineng_Docks,
    MapID::Saoshang_Trail,
};
constexpr MapID adj_Ran_Musu_Gardens_outpost[] = {
    MapID::Kinya_Province,
    MapID::Minister_Chos_Estate_explorable,
};
constexpr MapID adj_Linnok_Courtyard[] = {
    MapID::Shing_Jea_Monastery_outpost,
    MapID::Saoshang_Trail,
};
constexpr MapID adj_Sunjiang_District_explorable[] = {
    MapID::Shenzun_Tunnels,
    MapID::Pongmei_Valley,
    MapID::Zin_Ku_Corridor_outpost,
};
constexpr MapID adj_Nahpui_Quarter_explorable[] = {
    MapID::Senjis_Corner_outpost,
    MapID::Shenzun_Tunnels,
    MapID::Wajjun_Bazaar,
};
constexpr MapID adj_Tahnnakai_Temple_explorable[] = {
    MapID::Shenzun_Tunnels,
    MapID::Tahnnakai_Temple_explorable,
    MapID::Zin_Ku_Corridor_outpost,
};
constexpr MapID adj_Altrumm_Ruins[] = {
    MapID::House_zu_Heltzer_outpost,
    MapID::Arborstone_explorable,
};
constexpr MapID adj_Zos_Shivros_Channel[] = {
    MapID::Cavalon_outpost,
    MapID::Boreas_Seabed_explorable,
};
constexpr MapID adj_Dragons_Throat[] = {
    MapID::Shadows_Passage,
};
constexpr MapID adj_Harvest_Temple_outpost[] = {
    MapID::Unwaking_Waters,
};
constexpr MapID adj_Breaker_Hollow_outpost[] = {
    MapID::Archipelagos,
    MapID::Mount_Qinkai,
};
constexpr MapID adj_Leviathan_Pits_outpost[] = {
    MapID::Gyala_Hatchery,
    MapID::Silent_Surf,
};
constexpr MapID adj_Isle_of_the_Nameless[] = {
    MapID::Random_Arenas_outpost,
    MapID::Great_Temple_of_Balthazar_outpost,
};
constexpr MapID adj_Zaishen_Challenge_outpost[] = {
    MapID::Great_Temple_of_Balthazar_outpost,
};
constexpr MapID adj_Zaishen_Elite_outpost[] = {
    MapID::Zaishen_Challenge_outpost,
};
constexpr MapID adj_Maatu_Keep_outpost[] = {
    MapID::Shenzun_Tunnels,
    MapID::Pongmei_Valley,
};
constexpr MapID adj_Zin_Ku_Corridor_outpost[] = {
    MapID::The_Fissure_of_Woe,
    MapID::The_Underworld,
    MapID::Sunjiang_District_explorable,
    MapID::Tahnnakai_Temple_explorable,
};
constexpr MapID adj_Brauer_Academy_outpost[] = {
    MapID::Drazach_Thicket,
    MapID::Melandrus_Hope,
};
constexpr MapID adj_Durheim_Archives_outpost[] = {
    MapID::Morostav_Trail,
    MapID::Mourning_Veil_Falls,
};
constexpr MapID adj_Bai_Paasu_Reach_outpost[] = {
    MapID::Maishang_Hills,
};
constexpr MapID adj_Seafarers_Rest_outpost[] = {
    MapID::Rheas_Crater,
    MapID::Silent_Surf,
};
constexpr MapID adj_Bejunkan_Pier[] = {
    MapID::Kaineng_Center_outpost,
};
constexpr MapID adj_Vizunah_Square_Local_Quarter_outpost[] = {
    MapID::The_Undercity,
};
constexpr MapID adj_Vizunah_Square_Foreign_Quarter_outpost[] = {
    MapID::Bukdek_Byway,
};
constexpr MapID adj_Raisu_Pavilion[] = {
    MapID::Kaineng_Center_outpost,
    MapID::Raisu_Palace,
};
constexpr MapID adj_Kaineng_Docks[] = {
    MapID::Seitung_Harbor_outpost,
    MapID::The_Marketplace_outpost,
};
constexpr MapID adj_The_Marketplace_outpost[] = {
    MapID::Wajjun_Bazaar,
    MapID::Bukdek_Byway,
    MapID::Kaineng_Docks,
};
constexpr MapID adj_Saoshang_Trail[] = {
    MapID::Seitung_Harbor_outpost,
    MapID::Linnok_Courtyard,
};
constexpr MapID adj_Imperial_Sanctum_explorable[] = {
    MapID::Imperial_Sanctum_outpost_mission,
};
constexpr MapID adj_The_Hall_of_Heroes[] = {
    MapID::Tomb_of_the_Primeval_Kings,
};
constexpr MapID adj_The_Courtyard[] = {
    MapID::The_Hall_of_Heroes,
};
constexpr MapID adj_Scarred_Earth[] = {
    MapID::The_Courtyard,
};
constexpr MapID adj_The_Underworld_PvP[] = {
    MapID::Tomb_of_the_Primeval_Kings,
};
constexpr MapID adj_Tanglewood_Copse_outpost[] = {
    MapID::Pongmei_Valley,
    MapID::Arborstone_explorable,
};
constexpr MapID adj_Saint_Anjekas_Shrine_outpost[] = {
    MapID::Drazach_Thicket,
    MapID::Ferndale,
};
constexpr MapID adj_Eredon_Terrace_outpost[] = {
    MapID::Maishang_Hills,
};
constexpr MapID adj_Divine_Path[] = {
    MapID::Imperial_Sanctum_outpost_mission,
};
constexpr MapID adj_Jahai_Bluffs[] = {
    MapID::Arkjok_Ward,
    MapID::The_Floodplain_of_Mahnkelon,
    MapID::Turais_Procession,
    MapID::Kodonur_Crossroads,
    MapID::Command_Post,
};
constexpr MapID adj_Marga_Coast[] = {
    MapID::Arkjok_Ward,
    MapID::Yohlon_Haven_outpost,
    MapID::Sunspear_Sanctuary_outpost,
    MapID::Nundu_Bay,
    MapID::Dajkah_Inlet,
};
constexpr MapID adj_Sunward_Marches[] = {
    MapID::Venta_Cemetery,
    MapID::Command_Post,
    MapID::Dajkah_Inlet,
};
constexpr MapID adj_Barbarous_Shore[] = {
    MapID::Camp_Hojanu_outpost,
};
constexpr MapID adj_Camp_Hojanu_outpost[] = {
    MapID::Barbarous_Shore,
    MapID::Dejarin_Estate,
};
constexpr MapID adj_Bahdok_Caverns[] = {
    MapID::Wehhan_Terraces_outpost,
    MapID::Moddok_Crevice,
};
constexpr MapID adj_Wehhan_Terraces_outpost[] = {
    MapID::Bahdok_Caverns,
    MapID::Yatendi_Canyons,
};
constexpr MapID adj_Dejarin_Estate[] = {
    MapID::Camp_Hojanu_outpost,
    MapID::Kodonur_Crossroads,
    MapID::Pogahn_Passage,
};
constexpr MapID adj_Arkjok_Ward[] = {
    MapID::Jahai_Bluffs,
    MapID::Marga_Coast,
    MapID::Yohlon_Haven_outpost,
    MapID::Pogahn_Passage,
    MapID::Command_Post,
};
constexpr MapID adj_Yohlon_Haven_outpost[] = {
    MapID::Marga_Coast,
    MapID::Arkjok_Ward,
};
constexpr MapID adj_Gandara_the_Moon_Fortress[] = {
    MapID::Pogahn_Passage,
};
constexpr MapID adj_The_Floodplain_of_Mahnkelon[] = {
    MapID::Jahai_Bluffs,
    MapID::Kodonur_Crossroads,
    MapID::Rilohn_Refuge,
    MapID::Moddok_Crevice,
};
constexpr MapID adj_Turais_Procession[] = {
    MapID::Jahai_Bluffs,
    MapID::Venta_Cemetery,
    MapID::Command_Post,
    MapID::Gate_of_Desolation,
};
constexpr MapID adj_Sunspear_Sanctuary_outpost[] = {
    MapID::Marga_Coast,
    MapID::Command_Post,
};
constexpr MapID adj_Aspenwood_Gate_Kurzick_outpost[] = {
    MapID::Ferndale,
};
constexpr MapID adj_Aspenwood_Gate_Luxon_outpost[] = {
    MapID::Mount_Qinkai,
};
constexpr MapID adj_Jade_Flats_Kurzick_outpost[] = {
    MapID::Melandrus_Hope,
};
constexpr MapID adj_Jade_Flats_Luxon_outpost[] = {
    MapID::Archipelagos,
};
constexpr MapID adj_Yatendi_Canyons[] = {
    MapID::Wehhan_Terraces_outpost,
    MapID::Chantry_of_Secrets_outpost,
    MapID::Vehtendi_Valley,
};
constexpr MapID adj_Chantry_of_Secrets_outpost[] = {
    MapID::The_Fissure_of_Woe,
    MapID::The_Underworld,
    MapID::Yatendi_Canyons,
    MapID::Gate_of_Anguish_elite_mission,
};
constexpr MapID adj_Garden_of_Seborhin[] = {
    MapID::Forum_Highlands,
};
constexpr MapID adj_Holdings_of_Chokhin[] = {
    MapID::Mihanu_Township_outpost,
    MapID::Vehjin_Mines,
};
constexpr MapID adj_Mihanu_Township_outpost[] = {
    MapID::Holdings_of_Chokhin,
    MapID::The_Mirror_of_Lyss,
};
constexpr MapID adj_Vehjin_Mines[] = {
    MapID::Holdings_of_Chokhin,
    MapID::Basalt_Grotto_outpost,
    MapID::Jennurs_Horde,
};
constexpr MapID adj_Basalt_Grotto_outpost[] = {
    MapID::Vehjin_Mines,
    MapID::Jokos_Domain,
};
constexpr MapID adj_Forum_Highlands[] = {
    MapID::Garden_of_Seborhin,
    MapID::Vehtendi_Valley,
    MapID::The_Kodash_Bazaar_outpost,
    MapID::Tihark_Orchard,
    MapID::Nightfallen_Garden,
    MapID::Jennurs_Horde,
};
constexpr MapID adj_Resplendent_Makuun[] = {
    MapID::Honur_Hill_outpost,
    MapID::Wilderness_of_Bahdza,
    MapID::Yahnur_Market_outpost,
    MapID::Bokka_Amphitheatre,
};
constexpr MapID adj_Honur_Hill_outpost[] = {
    MapID::Resplendent_Makuun,
    MapID::The_Mirror_of_Lyss,
};
constexpr MapID adj_Wilderness_of_Bahdza[] = {
    MapID::Resplendent_Makuun,
    MapID::Dzagonur_Bastion,
};
constexpr MapID adj_Vehtendi_Valley[] = {
    MapID::Yatendi_Canyons,
    MapID::Forum_Highlands,
    MapID::Yahnur_Market_outpost,
    MapID::The_Kodash_Bazaar_outpost,
};
constexpr MapID adj_Yahnur_Market_outpost[] = {
    MapID::Resplendent_Makuun,
    MapID::Vehtendi_Valley,
};
constexpr MapID adj_The_Hidden_City_of_Ahdashim[] = {
    MapID::Dasha_Vestibule,
};
constexpr MapID adj_The_Kodash_Bazaar_outpost[] = {
    MapID::Forum_Highlands,
    MapID::Vehtendi_Valley,
    MapID::The_Mirror_of_Lyss,
};
constexpr MapID adj_Lions_Gate[] = {
    MapID::Lions_Arch_outpost,
};
constexpr MapID adj_The_Mirror_of_Lyss[] = {
    MapID::Mihanu_Township_outpost,
    MapID::Honur_Hill_outpost,
    MapID::The_Kodash_Bazaar_outpost,
    MapID::Dzagonur_Bastion,
    MapID::Dasha_Vestibule,
    MapID::Grand_Court_of_Sebelkeh,
};
constexpr MapID adj_Secure_the_Refuge[] = {
    MapID::Marga_Coast,
};
constexpr MapID adj_Venta_Cemetery[] = {
    MapID::Sunward_Marches,
    MapID::Turais_Procession,
};
constexpr MapID adj_The_Tribunal[] = {
    MapID::Kamadan_Jewel_of_Istan_outpost,
};
constexpr MapID adj_Kodonur_Crossroads[] = {
    MapID::Jahai_Bluffs,
    MapID::Dejarin_Estate,
    MapID::The_Floodplain_of_Mahnkelon,
};
constexpr MapID adj_Rilohn_Refuge[] = {
    MapID::The_Floodplain_of_Mahnkelon,
};
constexpr MapID adj_Pogahn_Passage[] = {
    MapID::Dejarin_Estate,
    MapID::Arkjok_Ward,
    MapID::Gandara_the_Moon_Fortress,
};
constexpr MapID adj_Moddok_Crevice[] = {
    MapID::Bahdok_Caverns,
    MapID::The_Floodplain_of_Mahnkelon,
};
constexpr MapID adj_Tihark_Orchard[] = {
    MapID::Forum_Highlands,
};
constexpr MapID adj_Consulate[] = {
    MapID::Kamadan_Jewel_of_Istan_outpost,
};
constexpr MapID adj_Plains_of_Jarin[] = {
    MapID::Sunspear_Great_Hall_outpost,
    MapID::Kamadan_Jewel_of_Istan_outpost,
    MapID::Champions_Dawn_outpost,
    MapID::The_Astralarium_outpost,
    MapID::Caverns_Below_Kamadan,
};
constexpr MapID adj_Sunspear_Great_Hall_outpost[] = {
    MapID::Plains_of_Jarin,
};
constexpr MapID adj_Cliffs_of_Dohjok[] = {
    MapID::Champions_Dawn_outpost,
    MapID::Zehlon_Reach,
    MapID::Beknur_Harbor_outpost,
    MapID::Jokanur_Diggings,
    MapID::Blacktide_Den,
};
constexpr MapID adj_Dzagonur_Bastion[] = {
    MapID::Wilderness_of_Bahdza,
    MapID::The_Mirror_of_Lyss,
};
constexpr MapID adj_Dasha_Vestibule[] = {
    MapID::The_Hidden_City_of_Ahdashim,
    MapID::The_Mirror_of_Lyss,
};
constexpr MapID adj_Grand_Court_of_Sebelkeh[] = {
    MapID::The_Mirror_of_Lyss,
};
constexpr MapID adj_Command_Post[] = {
    MapID::Jahai_Bluffs,
    MapID::Sunward_Marches,
    MapID::Arkjok_Ward,
    MapID::Turais_Procession,
    MapID::Sunspear_Sanctuary_outpost,
};
constexpr MapID adj_Jokos_Domain[] = {
    MapID::Basalt_Grotto_outpost,
    MapID::Bone_Palace_outpost,
    MapID::The_Shattered_Ravines,
    MapID::Remains_of_Sahlahja,
};
constexpr MapID adj_Bone_Palace_outpost[] = {
    MapID::Jokos_Domain,
    MapID::The_Alkali_Pan,
};
constexpr MapID adj_The_Ruptured_Heart[] = {
    MapID::The_Mouth_of_Torment_outpost,
    MapID::The_Shattered_Ravines,
    MapID::Poisoned_Outcrops,
    MapID::The_Alkali_Pan,
    MapID::Crystal_Overlook,
    MapID::Ruins_of_Morah,
};
constexpr MapID adj_The_Mouth_of_Torment_outpost[] = {
    MapID::The_Ruptured_Heart,
    MapID::Gate_of_Torment_outpost,
};
constexpr MapID adj_The_Shattered_Ravines[] = {
    MapID::Jokos_Domain,
    MapID::The_Ruptured_Heart,
    MapID::Lair_of_the_Forgotten_outpost,
    MapID::The_Alkali_Pan,
};
constexpr MapID adj_Lair_of_the_Forgotten_outpost[] = {
    MapID::The_Shattered_Ravines,
    MapID::Poisoned_Outcrops,
};
constexpr MapID adj_Poisoned_Outcrops[] = {
    MapID::The_Ruptured_Heart,
    MapID::Lair_of_the_Forgotten_outpost,
};
constexpr MapID adj_The_Sulfurous_Wastes[] = {
    MapID::Gate_of_Desolation,
    MapID::Remains_of_Sahlahja,
};
constexpr MapID adj_The_Alkali_Pan[] = {
    MapID::Bone_Palace_outpost,
    MapID::The_Ruptured_Heart,
    MapID::The_Shattered_Ravines,
    MapID::Crystal_Overlook,
    MapID::Ruins_of_Morah,
};
constexpr MapID adj_Crystal_Overlook[] = {
    MapID::The_Arid_Sea,
    MapID::The_Ruptured_Heart,
    MapID::The_Alkali_Pan,
};
constexpr MapID adj_Kamadan_Jewel_of_Istan_outpost[] = {
    MapID::Consulate,
    MapID::Plains_of_Jarin,
    MapID::Churrhir_Fields,
    MapID::Sunspear_Arena,
    MapID::Sun_Docks,
    MapID::Dajkah_Inlet,
};
constexpr MapID adj_Gate_of_Torment_outpost[] = {
    MapID::The_Mouth_of_Torment_outpost,
    MapID::Nightfallen_Jahai,
    MapID::The_Shadow_Nexus,
};
constexpr MapID adj_Gate_of_Anguish_elite_mission[] = {
    MapID::Chantry_of_Secrets_outpost,
};
constexpr MapID adj_Nightfallen_Garden[] = {
    MapID::Forum_Highlands,
};
constexpr MapID adj_Churrhir_Fields[] = {
    MapID::Kamadan_Jewel_of_Istan_outpost,
    MapID::Chahbek_Village,
};
constexpr MapID adj_Heart_of_Abaddon[] = {
    MapID::Abaddons_Gate,
};
constexpr MapID adj_Nightfallen_Jahai[] = {
    MapID::Gate_of_Torment_outpost,
    MapID::Gate_of_Pain,
    MapID::Gate_of_the_Nightfallen_Lands_outpost,
};
constexpr MapID adj_Depths_of_Madness[] = {
    MapID::Gate_of_Madness,
    MapID::Abaddons_Gate,
};
constexpr MapID adj_Domain_of_Fear[] = {
    MapID::Gate_of_Fear_outpost,
    MapID::Gate_of_Secrets_outpost,
};
constexpr MapID adj_Gate_of_Fear_outpost[] = {
    MapID::Domain_of_Fear,
    MapID::Domain_of_Pain,
};
constexpr MapID adj_Domain_of_Pain[] = {
    MapID::Gate_of_Fear_outpost,
    MapID::Gate_of_Pain,
};
constexpr MapID adj_Domain_of_Secrets[] = {
    MapID::Gate_of_Secrets_outpost,
    MapID::Gate_of_Madness,
};
constexpr MapID adj_Gate_of_Secrets_outpost[] = {
    MapID::Domain_of_Fear,
    MapID::Domain_of_Secrets,
};
constexpr MapID adj_Domain_of_Anguish[] = {
    MapID::Gate_of_Anguish_elite_mission,
};
constexpr MapID adj_Ooze_Pit_mission[] = {
    MapID::Grothmar_Wardowns,
};
constexpr MapID adj_Jennurs_Horde[] = {
    MapID::Vehjin_Mines,
    MapID::Forum_Highlands,
};
constexpr MapID adj_Nundu_Bay[] = {
    MapID::Marga_Coast,
};
constexpr MapID adj_Gate_of_Desolation[] = {
    MapID::Turais_Procession,
    MapID::The_Sulfurous_Wastes,
};
constexpr MapID adj_Champions_Dawn_outpost[] = {
    MapID::Plains_of_Jarin,
    MapID::Cliffs_of_Dohjok,
};
constexpr MapID adj_Ruins_of_Morah[] = {
    MapID::The_Ruptured_Heart,
    MapID::The_Alkali_Pan,
};
constexpr MapID adj_Fahranur_The_First_City[] = {
    MapID::Jokanur_Diggings,
    MapID::Blacktide_Den,
};
constexpr MapID adj_Bjora_Marches[] = {
    MapID::Jaga_Moraine,
    MapID::Norrhart_Domains,
    MapID::Darkrime_Delves_Level_1,
    MapID::Longeyes_Ledge_outpost,
};
constexpr MapID adj_Zehlon_Reach[] = {
    MapID::Cliffs_of_Dohjok,
    MapID::Jokanur_Diggings,
    MapID::The_Astralarium_outpost,
};
constexpr MapID adj_Lahtenda_Bog[] = {
    MapID::Blacktide_Den,
};
constexpr MapID adj_Arbor_Bay[] = {
    MapID::Riven_Earth,
    MapID::Alcazia_Tangle,
    MapID::Shards_of_Orr_Level_1,
    MapID::Vloxs_Falls,
};
constexpr MapID adj_Issnur_Isles[] = {
    MapID::Beknur_Harbor_outpost,
    MapID::Kodlonu_Hamlet_outpost,
};
constexpr MapID adj_Beknur_Harbor_outpost[] = {
    MapID::Cliffs_of_Dohjok,
    MapID::Issnur_Isles,
};
constexpr MapID adj_Mehtani_Keys[] = {
    MapID::Kodlonu_Hamlet_outpost,
};
constexpr MapID adj_Kodlonu_Hamlet_outpost[] = {
    MapID::Issnur_Isles,
    MapID::Mehtani_Keys,
};
constexpr MapID adj_Island_of_Shehkah[] = {
    MapID::Chahbek_Village,
};
constexpr MapID adj_Jokanur_Diggings[] = {
    MapID::Cliffs_of_Dohjok,
    MapID::Fahranur_The_First_City,
    MapID::Zehlon_Reach,
};
constexpr MapID adj_Blacktide_Den[] = {
    MapID::Cliffs_of_Dohjok,
    MapID::Fahranur_The_First_City,
    MapID::Lahtenda_Bog,
};
constexpr MapID adj_Consulate_Docks[] = {
    MapID::Bejunkan_Pier,
    MapID::Lions_Gate,
    MapID::Consulate,
};
constexpr MapID adj_Gate_of_Pain[] = {
    MapID::Nightfallen_Jahai,
};
constexpr MapID adj_Gate_of_Madness[] = {
    MapID::Domain_of_Secrets,
};
constexpr MapID adj_Abaddons_Gate[] = {
    MapID::Heart_of_Abaddon,
    MapID::Depths_of_Madness,
    MapID::Throne_of_Secrets,
};
constexpr MapID adj_Ice_Cliff_Chasms[] = {
    MapID::Norrhart_Domains,
    MapID::Battledepths_Level_1,
    MapID::Eye_of_the_North_outpost,
    MapID::Boreal_Station_outpost,
};
constexpr MapID adj_Bokka_Amphitheatre[] = {
    MapID::Resplendent_Makuun,
};
constexpr MapID adj_Riven_Earth[] = {
    MapID::Arbor_Bay,
    MapID::Alcazia_Tangle,
    MapID::Rata_Sum_outpost,
};
constexpr MapID adj_The_Astralarium_outpost[] = {
    MapID::Plains_of_Jarin,
    MapID::Zehlon_Reach,
};
constexpr MapID adj_Drakkar_Lake[] = {
    MapID::Norrhart_Domains,
    MapID::Varajar_Fells,
    MapID::Sepulchre_of_Dragrimmar_Level_1,
    MapID::Sifhalla_outpost,
};
constexpr MapID adj_Sun_Docks[] = {
    MapID::Kamadan_Jewel_of_Istan_outpost,
};
constexpr MapID adj_Remains_of_Sahlahja[] = {
    MapID::Jokos_Domain,
    MapID::The_Sulfurous_Wastes,
};
constexpr MapID adj_Jaga_Moraine[] = {
    MapID::Bjora_Marches,
    MapID::Frostmaws_Burrows_Level_1,
    MapID::Sifhalla_outpost,
};
constexpr MapID adj_Norrhart_Domains[] = {
    MapID::Bjora_Marches,
    MapID::Ice_Cliff_Chasms,
    MapID::Drakkar_Lake,
    MapID::Varajar_Fells,
    MapID::Gunnars_Hold_outpost,
};
constexpr MapID adj_Varajar_Fells[] = {
    MapID::Drakkar_Lake,
    MapID::Norrhart_Domains,
    MapID::Verdant_Cascades,
    MapID::Ravens_Point_Level_1,
    MapID::Battledepths_Level_1,
    MapID::Olafstead_outpost,
};
constexpr MapID adj_Dajkah_Inlet[] = {
    MapID::Marga_Coast,
    MapID::Sunward_Marches,
    MapID::Kamadan_Jewel_of_Istan_outpost,
};
constexpr MapID adj_The_Shadow_Nexus[] = {
    MapID::Gate_of_Torment_outpost,
};
constexpr MapID adj_Sparkfly_Swamp[] = {
    MapID::Bloodstone_Caves_Level_1,
    MapID::Bogroot_Growths_Level_1,
    MapID::Gadds_Encampment_outpost,
};
constexpr MapID adj_Gate_of_the_Nightfallen_Lands_outpost[] = {
    MapID::Nightfallen_Jahai,
};
constexpr MapID adj_Cathedral_of_Flames_Level_1[] = {
    MapID::Doomlore_Shrine_outpost,
};
constexpr MapID adj_Verdant_Cascades[] = {
    MapID::Varajar_Fells,
    MapID::Slavers_Exile_Level_1,
    MapID::Umbral_Grotto_outpost,
};
constexpr MapID adj_Magus_Stones[] = {
    MapID::Alcazia_Tangle,
    MapID::Oolas_Lab_Level_1,
    MapID::Arachnis_Haunt_Level_1,
    MapID::Rata_Sum_outpost,
};
constexpr MapID adj_Catacombs_of_Kathandrax_Level_1[] = {
    MapID::Sacnoth_Valley,
};
constexpr MapID adj_Alcazia_Tangle[] = {
    MapID::Arbor_Bay,
    MapID::Riven_Earth,
    MapID::Magus_Stones,
    MapID::Tarnished_Haven_outpost,
};
constexpr MapID adj_Rragars_Menagerie_Level_1[] = {
    MapID::Sacnoth_Valley,
};
constexpr MapID adj_Slavers_Exile_Level_1[] = {
    MapID::Verdant_Cascades,
};
constexpr MapID adj_Oolas_Lab_Level_1[] = {
    MapID::Magus_Stones,
};
constexpr MapID adj_Shards_of_Orr_Level_1[] = {
    MapID::Arbor_Bay,
};
constexpr MapID adj_Arachnis_Haunt_Level_1[] = {
    MapID::Magus_Stones,
};
constexpr MapID adj_Vloxen_Excavations_Level_1[] = {
    MapID::Vloxs_Falls,
    MapID::Umbral_Grotto_outpost,
};
constexpr MapID adj_Heart_of_the_Shiverpeaks_Level_1[] = {
    MapID::Bogroot_Growths_Level_1,
    MapID::Battledepths_Level_1,
};
constexpr MapID adj_Bloodstone_Caves_Level_1[] = {
    MapID::Sparkfly_Swamp,
};
constexpr MapID adj_Bogroot_Growths_Level_1[] = {
    MapID::Sparkfly_Swamp,
    MapID::Heart_of_the_Shiverpeaks_Level_1,
};
constexpr MapID adj_Ravens_Point_Level_1[] = {
    MapID::Varajar_Fells,
};
constexpr MapID adj_Vloxs_Falls[] = {
    MapID::Arbor_Bay,
    MapID::Vloxen_Excavations_Level_1,
};
constexpr MapID adj_Battledepths_Level_1[] = {
    MapID::Ice_Cliff_Chasms,
    MapID::Varajar_Fells,
    MapID::Heart_of_the_Shiverpeaks_Level_1,
    MapID::Central_Transfer_Chamber_outpost,
};
constexpr MapID adj_Sepulchre_of_Dragrimmar_Level_1[] = {
    MapID::Drakkar_Lake,
};
constexpr MapID adj_Frostmaws_Burrows_Level_1[] = {
    MapID::Jaga_Moraine,
};
constexpr MapID adj_Darkrime_Delves_Level_1[] = {
    MapID::Bjora_Marches,
};
constexpr MapID adj_Gadds_Encampment_outpost[] = {
    MapID::Sparkfly_Swamp,
    MapID::Shards_of_Orr_Level_1,
};
constexpr MapID adj_Umbral_Grotto_outpost[] = {
    MapID::Verdant_Cascades,
    MapID::Vloxen_Excavations_Level_1,
};
constexpr MapID adj_Rata_Sum_outpost[] = {
    MapID::Riven_Earth,
    MapID::Magus_Stones,
};
constexpr MapID adj_Tarnished_Haven_outpost[] = {
    MapID::Alcazia_Tangle,
};
constexpr MapID adj_Eye_of_the_North_outpost[] = {
    MapID::Ice_Cliff_Chasms,
    MapID::Hall_of_Monuments,
};
constexpr MapID adj_Sifhalla_outpost[] = {
    MapID::Drakkar_Lake,
    MapID::Jaga_Moraine,
};
constexpr MapID adj_Gunnars_Hold_outpost[] = {
    MapID::Norrhart_Domains,
};
constexpr MapID adj_Olafstead_outpost[] = {
    MapID::Varajar_Fells,
};
constexpr MapID adj_Hall_of_Monuments[] = {
    MapID::Eye_of_the_North_outpost,
};
constexpr MapID adj_Dalada_Uplands[] = {
    MapID::Doomlore_Shrine_outpost,
    MapID::Grothmar_Wardowns,
    MapID::Sacnoth_Valley,
};
constexpr MapID adj_Doomlore_Shrine_outpost[] = {
    MapID::Cathedral_of_Flames_Level_1,
    MapID::Dalada_Uplands,
};
constexpr MapID adj_Grothmar_Wardowns[] = {
    MapID::Ooze_Pit_mission,
    MapID::Dalada_Uplands,
    MapID::Longeyes_Ledge_outpost,
    MapID::Sacnoth_Valley,
};
constexpr MapID adj_Longeyes_Ledge_outpost[] = {
    MapID::Bjora_Marches,
    MapID::Grothmar_Wardowns,
};
constexpr MapID adj_Sacnoth_Valley[] = {
    MapID::Catacombs_of_Kathandrax_Level_1,
    MapID::Rragars_Menagerie_Level_1,
    MapID::Dalada_Uplands,
    MapID::Grothmar_Wardowns,
};
constexpr MapID adj_Central_Transfer_Chamber_outpost[] = {
    MapID::Battledepths_Level_1,
};
constexpr MapID adj_Boreal_Station_outpost[] = {
    MapID::Ice_Cliff_Chasms,
};
constexpr MapID adj_Piken_Square_pre_Searing_outpost[] = {
    MapID::The_Northlands,
};
constexpr MapID adj_Forsaken_Tunnels_Presearing_Level1[] = {
    MapID::The_Northlands,
};
constexpr MapID adj_Isle_of_the_Nameless_PvP[] = {
    MapID::Random_Arenas_outpost,
    MapID::Great_Temple_of_Balthazar_outpost,
};
constexpr MapID adj_Secret_Underground_Lair[] = {
    MapID::The_Falls,
};
constexpr MapID adj_Zaishen_Menagerie_Grounds[] = {
    MapID::Zaishen_Menagerie_outpost,
};
constexpr MapID adj_Zaishen_Menagerie_outpost[] = {
    MapID::Zaishen_Menagerie_Grounds,
};
constexpr MapID adj_The_Underworld_Something_Wicked_This_Way_Comes[] = {
    MapID::Temple_of_the_Ages,
    MapID::Zin_Ku_Corridor_outpost,
    MapID::Chantry_of_Secrets_outpost,
};
constexpr MapID adj_The_Underworld_Dont_Fear_the_Reapers[] = {
    MapID::Temple_of_the_Ages,
    MapID::Zin_Ku_Corridor_outpost,
    MapID::Chantry_of_Secrets_outpost,
};
constexpr MapID adj_Droknars_Forge_Halloween_outpost[] = {
    MapID::Talus_Chute,
    MapID::Witmans_Folly,
};
constexpr MapID adj_Droknars_Forge_Wintersday_outpost[] = {
    MapID::Talus_Chute,
    MapID::Witmans_Folly,
};
constexpr MapID adj_Tomb_of_the_Primeval_Kings_Halloween_outpost[] = {
    MapID::The_Dragons_Lair,
    MapID::The_Underworld_PvP,
};
constexpr MapID adj_Eye_of_the_North_outpost_Wintersday_outpost[] = {
    MapID::Ice_Cliff_Chasms,
    MapID::Hall_of_Monuments,
};
constexpr MapID adj_War_in_Kryta_Lions_Arch_Keep[] = {
    MapID::Lions_Arch_outpost,
};
constexpr MapID adj_Forsaken_Tunnels_Level1[] = {
    MapID::The_Breach,
};

constexpr AdjacencyEntry adjacency_table[] = {
    {MapID::Bloodstone_Fen, adj_Bloodstone_Fen},
    {MapID::The_Wilds, adj_The_Wilds},
    {MapID::Aurora_Glade, adj_Aurora_Glade},
    {MapID::Diessa_Lowlands, adj_Diessa_Lowlands},
    {MapID::Gates_of_Kryta, adj_Gates_of_Kryta},
    {MapID::DAlessio_Seaboard, adj_DAlessio_Seaboard},
    {MapID::Divinity_Coast, adj_Divinity_Coast},
    {MapID::Talmark_Wilderness, adj_Talmark_Wilderness},
    {MapID::The_Black_Curtain, adj_The_Black_Curtain},
    {MapID::Sanctum_Cay, adj_Sanctum_Cay},
    {MapID::Droknars_Forge_outpost, adj_Droknars_Forge_outpost},
    {MapID::Ice_Caves_of_Sorrow, adj_Ice_Caves_of_Sorrow},
    {MapID::Thunderhead_Keep, adj_Thunderhead_Keep},
    {MapID::Iron_Mines_of_Moladune, adj_Iron_Mines_of_Moladune},
    {MapID::Talus_Chute, adj_Talus_Chute},
    {MapID::Griffons_Mouth, adj_Griffons_Mouth},
    {MapID::The_Great_Northern_Wall, adj_The_Great_Northern_Wall},
    {MapID::Fort_Ranik, adj_Fort_Ranik},
    {MapID::Ruins_of_Surmia, adj_Ruins_of_Surmia},
    {MapID::Xaquang_Skyway, adj_Xaquang_Skyway},
    {MapID::Nolani_Academy, adj_Nolani_Academy},
    {MapID::Old_Ascalon, adj_Old_Ascalon},
    {MapID::Ember_Light_Camp_outpost, adj_Ember_Light_Camp_outpost},
    {MapID::Grendich_Courthouse_outpost, adj_Grendich_Courthouse_outpost},
    {MapID::Augury_Rock_outpost, adj_Augury_Rock_outpost},
    {MapID::Sardelac_Sanitarium_outpost, adj_Sardelac_Sanitarium_outpost},
    {MapID::Piken_Square_outpost, adj_Piken_Square_outpost},
    {MapID::Sage_Lands, adj_Sage_Lands},
    {MapID::Mamnoon_Lagoon, adj_Mamnoon_Lagoon},
    {MapID::Silverwood, adj_Silverwood},
    {MapID::Ettins_Back, adj_Ettins_Back},
    {MapID::Reed_Bog, adj_Reed_Bog},
    {MapID::The_Falls, adj_The_Falls},
    {MapID::Dry_Top, adj_Dry_Top},
    {MapID::Tangle_Root, adj_Tangle_Root},
    {MapID::Henge_of_Denravi_outpost, adj_Henge_of_Denravi_outpost},
    {MapID::Senjis_Corner_outpost, adj_Senjis_Corner_outpost},
    {MapID::Tears_of_the_Fallen, adj_Tears_of_the_Fallen},
    {MapID::Scoundrels_Rise, adj_Scoundrels_Rise},
    {MapID::Lions_Arch_outpost, adj_Lions_Arch_outpost},
    {MapID::Cursed_Lands, adj_Cursed_Lands},
    {MapID::Bergen_Hot_Springs_outpost, adj_Bergen_Hot_Springs_outpost},
    {MapID::North_Kryta_Province, adj_North_Kryta_Province},
    {MapID::Nebo_Terrace, adj_Nebo_Terrace},
    {MapID::Majestys_Rest, adj_Majestys_Rest},
    {MapID::Twin_Serpent_Lakes, adj_Twin_Serpent_Lakes},
    {MapID::Watchtower_Coast, adj_Watchtower_Coast},
    {MapID::Stingray_Strand, adj_Stingray_Strand},
    {MapID::Kessex_Peak, adj_Kessex_Peak},
    {MapID::The_Underworld, adj_The_Underworld},
    {MapID::Riverside_Province, adj_Riverside_Province},
    {MapID::House_zu_Heltzer_outpost, adj_House_zu_Heltzer_outpost},
    {MapID::Ascalon_City_outpost, adj_Ascalon_City_outpost},
    {MapID::Tomb_of_the_Primeval_Kings, adj_Tomb_of_the_Primeval_Kings},
    {MapID::Icedome, adj_Icedome},
    {MapID::Iron_Horse_Mine, adj_Iron_Horse_Mine},
    {MapID::Anvil_Rock, adj_Anvil_Rock},
    {MapID::Lornars_Pass, adj_Lornars_Pass},
    {MapID::Snake_Dance, adj_Snake_Dance},
    {MapID::Tascas_Demise, adj_Tascas_Demise},
    {MapID::Spearhead_Peak, adj_Spearhead_Peak},
    {MapID::Ice_Floe, adj_Ice_Floe},
    {MapID::Witmans_Folly, adj_Witmans_Folly},
    {MapID::Mineral_Springs, adj_Mineral_Springs},
    {MapID::Dreadnoughts_Drift, adj_Dreadnoughts_Drift},
    {MapID::Frozen_Forest, adj_Frozen_Forest},
    {MapID::Travelers_Vale, adj_Travelers_Vale},
    {MapID::Deldrimor_Bowl, adj_Deldrimor_Bowl},
    {MapID::Regent_Valley, adj_Regent_Valley},
    {MapID::The_Breach, adj_The_Breach},
    {MapID::Ascalon_Foothills, adj_Ascalon_Foothills},
    {MapID::Pockmark_Flats, adj_Pockmark_Flats},
    {MapID::Dragons_Gullet, adj_Dragons_Gullet},
    {MapID::Flame_Temple_Corridor, adj_Flame_Temple_Corridor},
    {MapID::Eastern_Frontier, adj_Eastern_Frontier},
    {MapID::The_Scar, adj_The_Scar},
    {MapID::The_Amnoon_Oasis_outpost, adj_The_Amnoon_Oasis_outpost},
    {MapID::Diviners_Ascent, adj_Diviners_Ascent},
    {MapID::Vulture_Drifts, adj_Vulture_Drifts},
    {MapID::The_Arid_Sea, adj_The_Arid_Sea},
    {MapID::Prophets_Path, adj_Prophets_Path},
    {MapID::Salt_Flats, adj_Salt_Flats},
    {MapID::Skyward_Reach, adj_Skyward_Reach},
    {MapID::Dunes_of_Despair, adj_Dunes_of_Despair},
    {MapID::Thirsty_River, adj_Thirsty_River},
    {MapID::Elona_Reach, adj_Elona_Reach},
    {MapID::The_Dragons_Lair, adj_The_Dragons_Lair},
    {MapID::Perdition_Rock, adj_Perdition_Rock},
    {MapID::The_Eternal_Grove, adj_The_Eternal_Grove},
    {MapID::Lutgardis_Conservatory_outpost, adj_Lutgardis_Conservatory_outpost},
    {MapID::Vasburg_Armory_outpost, adj_Vasburg_Armory_outpost},
    {MapID::Serenity_Temple_outpost, adj_Serenity_Temple_outpost},
    {MapID::Beacons_Perch_outpost, adj_Beacons_Perch_outpost},
    {MapID::Frontier_Gate_outpost, adj_Frontier_Gate_outpost},
    {MapID::Beetletun_outpost, adj_Beetletun_outpost},
    {MapID::Fishermens_Haven_outpost, adj_Fishermens_Haven_outpost},
    {MapID::Temple_of_the_Ages, adj_Temple_of_the_Ages},
    {MapID::Ventaris_Refuge_outpost, adj_Ventaris_Refuge_outpost},
    {MapID::Druids_Overlook_outpost, adj_Druids_Overlook_outpost},
    {MapID::Maguuma_Stade_outpost, adj_Maguuma_Stade_outpost},
    {MapID::Quarrel_Falls_outpost, adj_Quarrel_Falls_outpost},
    {MapID::Gyala_Hatchery, adj_Gyala_Hatchery},
    {MapID::The_Catacombs, adj_The_Catacombs},
    {MapID::Lakeside_County, adj_Lakeside_County},
    {MapID::The_Northlands, adj_The_Northlands},
    {MapID::Ascalon_City_pre_searing, adj_Ascalon_City_pre_searing},
    {MapID::Heroes_Audience_outpost, adj_Heroes_Audience_outpost},
    {MapID::Seekers_Passage_outpost, adj_Seekers_Passage_outpost},
    {MapID::Destinys_Gorge_outpost, adj_Destinys_Gorge_outpost},
    {MapID::Camp_Rankor_outpost, adj_Camp_Rankor_outpost},
    {MapID::The_Granite_Citadel_outpost, adj_The_Granite_Citadel_outpost},
    {MapID::Marhans_Grotto_outpost, adj_Marhans_Grotto_outpost},
    {MapID::Port_Sledge_outpost, adj_Port_Sledge_outpost},
    {MapID::Copperhammer_Mines_outpost, adj_Copperhammer_Mines_outpost},
    {MapID::Green_Hills_County, adj_Green_Hills_County},
    {MapID::Wizards_Folly, adj_Wizards_Folly},
    {MapID::Regent_Valley_pre_Searing, adj_Regent_Valley_pre_Searing},
    {MapID::The_Barradin_Estate_outpost, adj_The_Barradin_Estate_outpost},
    {MapID::Ashford_Abbey_outpost, adj_Ashford_Abbey_outpost},
    {MapID::Foibles_Fair_outpost, adj_Foibles_Fair_outpost},
    {MapID::Fort_Ranik_pre_Searing_outpost, adj_Fort_Ranik_pre_Searing_outpost},
    {MapID::Sorrows_Furnace, adj_Sorrows_Furnace},
    {MapID::Grenths_Footprint, adj_Grenths_Footprint},
    {MapID::Cavalon_outpost, adj_Cavalon_outpost},
    {MapID::Kaineng_Center_outpost, adj_Kaineng_Center_outpost},
    {MapID::Drazach_Thicket, adj_Drazach_Thicket},
    {MapID::Jaya_Bluffs, adj_Jaya_Bluffs},
    {MapID::Shenzun_Tunnels, adj_Shenzun_Tunnels},
    {MapID::Archipelagos, adj_Archipelagos},
    {MapID::Maishang_Hills, adj_Maishang_Hills},
    {MapID::Mount_Qinkai, adj_Mount_Qinkai},
    {MapID::Melandrus_Hope, adj_Melandrus_Hope},
    {MapID::Rheas_Crater, adj_Rheas_Crater},
    {MapID::Silent_Surf, adj_Silent_Surf},
    {MapID::Morostav_Trail, adj_Morostav_Trail},
    {MapID::Deldrimor_War_Camp_outpost, adj_Deldrimor_War_Camp_outpost},
    {MapID::Mourning_Veil_Falls, adj_Mourning_Veil_Falls},
    {MapID::Ferndale, adj_Ferndale},
    {MapID::Pongmei_Valley, adj_Pongmei_Valley},
    {MapID::Monastery_Overlook1, adj_Monastery_Overlook1},
    {MapID::Zen_Daijun_outpost_mission, adj_Zen_Daijun_outpost_mission},
    {MapID::Vizunah_Square_mission, adj_Vizunah_Square_mission},
    {MapID::Imperial_Sanctum_outpost_mission, adj_Imperial_Sanctum_outpost_mission},
    {MapID::Unwaking_Waters, adj_Unwaking_Waters},
    {MapID::Amatz_Basin, adj_Amatz_Basin},
    {MapID::Shadows_Passage, adj_Shadows_Passage},
    {MapID::Raisu_Palace, adj_Raisu_Palace},
    {MapID::The_Aurios_Mines, adj_The_Aurios_Mines},
    {MapID::Panjiang_Peninsula, adj_Panjiang_Peninsula},
    {MapID::Kinya_Province, adj_Kinya_Province},
    {MapID::Haiju_Lagoon, adj_Haiju_Lagoon},
    {MapID::Sunqua_Vale, adj_Sunqua_Vale},
    {MapID::Wajjun_Bazaar, adj_Wajjun_Bazaar},
    {MapID::Bukdek_Byway, adj_Bukdek_Byway},
    {MapID::The_Undercity, adj_The_Undercity},
    {MapID::Shing_Jea_Monastery_outpost, adj_Shing_Jea_Monastery_outpost},
    {MapID::Arborstone_explorable, adj_Arborstone_explorable},
    {MapID::Minister_Chos_Estate_explorable, adj_Minister_Chos_Estate_explorable},
    {MapID::Zen_Daijun_explorable, adj_Zen_Daijun_explorable},
    {MapID::Boreas_Seabed_explorable, adj_Boreas_Seabed_explorable},
    {MapID::Great_Temple_of_Balthazar_outpost, adj_Great_Temple_of_Balthazar_outpost},
    {MapID::Tsumei_Village_outpost, adj_Tsumei_Village_outpost},
    {MapID::Seitung_Harbor_outpost, adj_Seitung_Harbor_outpost},
    {MapID::Ran_Musu_Gardens_outpost, adj_Ran_Musu_Gardens_outpost},
    {MapID::Linnok_Courtyard, adj_Linnok_Courtyard},
    {MapID::Sunjiang_District_explorable, adj_Sunjiang_District_explorable},
    {MapID::Nahpui_Quarter_explorable, adj_Nahpui_Quarter_explorable},
    {MapID::Tahnnakai_Temple_explorable, adj_Tahnnakai_Temple_explorable},
    {MapID::Altrumm_Ruins, adj_Altrumm_Ruins},
    {MapID::Zos_Shivros_Channel, adj_Zos_Shivros_Channel},
    {MapID::Dragons_Throat, adj_Dragons_Throat},
    {MapID::Harvest_Temple_outpost, adj_Harvest_Temple_outpost},
    {MapID::Breaker_Hollow_outpost, adj_Breaker_Hollow_outpost},
    {MapID::Leviathan_Pits_outpost, adj_Leviathan_Pits_outpost},
    {MapID::Isle_of_the_Nameless, adj_Isle_of_the_Nameless},
    {MapID::Zaishen_Challenge_outpost, adj_Zaishen_Challenge_outpost},
    {MapID::Zaishen_Elite_outpost, adj_Zaishen_Elite_outpost},
    {MapID::Maatu_Keep_outpost, adj_Maatu_Keep_outpost},
    {MapID::Zin_Ku_Corridor_outpost, adj_Zin_Ku_Corridor_outpost},
    {MapID::Brauer_Academy_outpost, adj_Brauer_Academy_outpost},
    {MapID::Durheim_Archives_outpost, adj_Durheim_Archives_outpost},
    {MapID::Bai_Paasu_Reach_outpost, adj_Bai_Paasu_Reach_outpost},
    {MapID::Seafarers_Rest_outpost, adj_Seafarers_Rest_outpost},
    {MapID::Bejunkan_Pier, adj_Bejunkan_Pier},
    {MapID::Vizunah_Square_Local_Quarter_outpost, adj_Vizunah_Square_Local_Quarter_outpost},
    {MapID::Vizunah_Square_Foreign_Quarter_outpost, adj_Vizunah_Square_Foreign_Quarter_outpost},
    {MapID::Raisu_Pavilion, adj_Raisu_Pavilion},
    {MapID::Kaineng_Docks, adj_Kaineng_Docks},
    {MapID::The_Marketplace_outpost, adj_The_Marketplace_outpost},
    {MapID::Saoshang_Trail, adj_Saoshang_Trail},
    {MapID::Imperial_Sanctum_explorable, adj_Imperial_Sanctum_explorable},
    {MapID::The_Hall_of_Heroes, adj_The_Hall_of_Heroes},
    {MapID::The_Courtyard, adj_The_Courtyard},
    {MapID::Scarred_Earth, adj_Scarred_Earth},
    {MapID::The_Underworld_PvP, adj_The_Underworld_PvP},
    {MapID::Tanglewood_Copse_outpost, adj_Tanglewood_Copse_outpost},
    {MapID::Saint_Anjekas_Shrine_outpost, adj_Saint_Anjekas_Shrine_outpost},
    {MapID::Eredon_Terrace_outpost, adj_Eredon_Terrace_outpost},
    {MapID::Divine_Path, adj_Divine_Path},
    {MapID::Jahai_Bluffs, adj_Jahai_Bluffs},
    {MapID::Marga_Coast, adj_Marga_Coast},
    {MapID::Sunward_Marches, adj_Sunward_Marches},
    {MapID::Barbarous_Shore, adj_Barbarous_Shore},
    {MapID::Camp_Hojanu_outpost, adj_Camp_Hojanu_outpost},
    {MapID::Bahdok_Caverns, adj_Bahdok_Caverns},
    {MapID::Wehhan_Terraces_outpost, adj_Wehhan_Terraces_outpost},
    {MapID::Dejarin_Estate, adj_Dejarin_Estate},
    {MapID::Arkjok_Ward, adj_Arkjok_Ward},
    {MapID::Yohlon_Haven_outpost, adj_Yohlon_Haven_outpost},
    {MapID::Gandara_the_Moon_Fortress, adj_Gandara_the_Moon_Fortress},
    {MapID::The_Floodplain_of_Mahnkelon, adj_The_Floodplain_of_Mahnkelon},
    {MapID::Turais_Procession, adj_Turais_Procession},
    {MapID::Sunspear_Sanctuary_outpost, adj_Sunspear_Sanctuary_outpost},
    {MapID::Aspenwood_Gate_Kurzick_outpost, adj_Aspenwood_Gate_Kurzick_outpost},
    {MapID::Aspenwood_Gate_Luxon_outpost, adj_Aspenwood_Gate_Luxon_outpost},
    {MapID::Jade_Flats_Kurzick_outpost, adj_Jade_Flats_Kurzick_outpost},
    {MapID::Jade_Flats_Luxon_outpost, adj_Jade_Flats_Luxon_outpost},
    {MapID::Yatendi_Canyons, adj_Yatendi_Canyons},
    {MapID::Chantry_of_Secrets_outpost, adj_Chantry_of_Secrets_outpost},
    {MapID::Garden_of_Seborhin, adj_Garden_of_Seborhin},
    {MapID::Holdings_of_Chokhin, adj_Holdings_of_Chokhin},
    {MapID::Mihanu_Township_outpost, adj_Mihanu_Township_outpost},
    {MapID::Vehjin_Mines, adj_Vehjin_Mines},
    {MapID::Basalt_Grotto_outpost, adj_Basalt_Grotto_outpost},
    {MapID::Forum_Highlands, adj_Forum_Highlands},
    {MapID::Resplendent_Makuun, adj_Resplendent_Makuun},
    {MapID::Honur_Hill_outpost, adj_Honur_Hill_outpost},
    {MapID::Wilderness_of_Bahdza, adj_Wilderness_of_Bahdza},
    {MapID::Vehtendi_Valley, adj_Vehtendi_Valley},
    {MapID::Yahnur_Market_outpost, adj_Yahnur_Market_outpost},
    {MapID::The_Hidden_City_of_Ahdashim, adj_The_Hidden_City_of_Ahdashim},
    {MapID::The_Kodash_Bazaar_outpost, adj_The_Kodash_Bazaar_outpost},
    {MapID::Lions_Gate, adj_Lions_Gate},
    {MapID::The_Mirror_of_Lyss, adj_The_Mirror_of_Lyss},
    {MapID::Secure_the_Refuge, adj_Secure_the_Refuge},
    {MapID::Venta_Cemetery, adj_Venta_Cemetery},
    {MapID::The_Tribunal, adj_The_Tribunal},
    {MapID::Kodonur_Crossroads, adj_Kodonur_Crossroads},
    {MapID::Rilohn_Refuge, adj_Rilohn_Refuge},
    {MapID::Pogahn_Passage, adj_Pogahn_Passage},
    {MapID::Moddok_Crevice, adj_Moddok_Crevice},
    {MapID::Tihark_Orchard, adj_Tihark_Orchard},
    {MapID::Consulate, adj_Consulate},
    {MapID::Plains_of_Jarin, adj_Plains_of_Jarin},
    {MapID::Sunspear_Great_Hall_outpost, adj_Sunspear_Great_Hall_outpost},
    {MapID::Cliffs_of_Dohjok, adj_Cliffs_of_Dohjok},
    {MapID::Dzagonur_Bastion, adj_Dzagonur_Bastion},
    {MapID::Dasha_Vestibule, adj_Dasha_Vestibule},
    {MapID::Grand_Court_of_Sebelkeh, adj_Grand_Court_of_Sebelkeh},
    {MapID::Command_Post, adj_Command_Post},
    {MapID::Jokos_Domain, adj_Jokos_Domain},
    {MapID::Bone_Palace_outpost, adj_Bone_Palace_outpost},
    {MapID::The_Ruptured_Heart, adj_The_Ruptured_Heart},
    {MapID::The_Mouth_of_Torment_outpost, adj_The_Mouth_of_Torment_outpost},
    {MapID::The_Shattered_Ravines, adj_The_Shattered_Ravines},
    {MapID::Lair_of_the_Forgotten_outpost, adj_Lair_of_the_Forgotten_outpost},
    {MapID::Poisoned_Outcrops, adj_Poisoned_Outcrops},
    {MapID::The_Sulfurous_Wastes, adj_The_Sulfurous_Wastes},
    {MapID::The_Alkali_Pan, adj_The_Alkali_Pan},
    {MapID::Crystal_Overlook, adj_Crystal_Overlook},
    {MapID::Kamadan_Jewel_of_Istan_outpost, adj_Kamadan_Jewel_of_Istan_outpost},
    {MapID::Gate_of_Torment_outpost, adj_Gate_of_Torment_outpost},
    {MapID::Gate_of_Anguish_elite_mission, adj_Gate_of_Anguish_elite_mission},
    {MapID::Nightfallen_Garden, adj_Nightfallen_Garden},
    {MapID::Churrhir_Fields, adj_Churrhir_Fields},
    {MapID::Heart_of_Abaddon, adj_Heart_of_Abaddon},
    {MapID::Nightfallen_Jahai, adj_Nightfallen_Jahai},
    {MapID::Depths_of_Madness, adj_Depths_of_Madness},
    {MapID::Domain_of_Fear, adj_Domain_of_Fear},
    {MapID::Gate_of_Fear_outpost, adj_Gate_of_Fear_outpost},
    {MapID::Domain_of_Pain, adj_Domain_of_Pain},
    {MapID::Domain_of_Secrets, adj_Domain_of_Secrets},
    {MapID::Gate_of_Secrets_outpost, adj_Gate_of_Secrets_outpost},
    {MapID::Domain_of_Anguish, adj_Domain_of_Anguish},
    {MapID::Ooze_Pit_mission, adj_Ooze_Pit_mission},
    {MapID::Jennurs_Horde, adj_Jennurs_Horde},
    {MapID::Nundu_Bay, adj_Nundu_Bay},
    {MapID::Gate_of_Desolation, adj_Gate_of_Desolation},
    {MapID::Champions_Dawn_outpost, adj_Champions_Dawn_outpost},
    {MapID::Ruins_of_Morah, adj_Ruins_of_Morah},
    {MapID::Fahranur_The_First_City, adj_Fahranur_The_First_City},
    {MapID::Bjora_Marches, adj_Bjora_Marches},
    {MapID::Zehlon_Reach, adj_Zehlon_Reach},
    {MapID::Lahtenda_Bog, adj_Lahtenda_Bog},
    {MapID::Arbor_Bay, adj_Arbor_Bay},
    {MapID::Issnur_Isles, adj_Issnur_Isles},
    {MapID::Beknur_Harbor_outpost, adj_Beknur_Harbor_outpost},
    {MapID::Mehtani_Keys, adj_Mehtani_Keys},
    {MapID::Kodlonu_Hamlet_outpost, adj_Kodlonu_Hamlet_outpost},
    {MapID::Island_of_Shehkah, adj_Island_of_Shehkah},
    {MapID::Jokanur_Diggings, adj_Jokanur_Diggings},
    {MapID::Blacktide_Den, adj_Blacktide_Den},
    {MapID::Consulate_Docks, adj_Consulate_Docks},
    {MapID::Gate_of_Pain, adj_Gate_of_Pain},
    {MapID::Gate_of_Madness, adj_Gate_of_Madness},
    {MapID::Abaddons_Gate, adj_Abaddons_Gate},
    {MapID::Ice_Cliff_Chasms, adj_Ice_Cliff_Chasms},
    {MapID::Bokka_Amphitheatre, adj_Bokka_Amphitheatre},
    {MapID::Riven_Earth, adj_Riven_Earth},
    {MapID::The_Astralarium_outpost, adj_The_Astralarium_outpost},
    {MapID::Drakkar_Lake, adj_Drakkar_Lake},
    {MapID::Sun_Docks, adj_Sun_Docks},
    {MapID::Remains_of_Sahlahja, adj_Remains_of_Sahlahja},
    {MapID::Jaga_Moraine, adj_Jaga_Moraine},
    {MapID::Norrhart_Domains, adj_Norrhart_Domains},
    {MapID::Varajar_Fells, adj_Varajar_Fells},
    {MapID::Dajkah_Inlet, adj_Dajkah_Inlet},
    {MapID::The_Shadow_Nexus, adj_The_Shadow_Nexus},
    {MapID::Sparkfly_Swamp, adj_Sparkfly_Swamp},
    {MapID::Gate_of_the_Nightfallen_Lands_outpost, adj_Gate_of_the_Nightfallen_Lands_outpost},
    {MapID::Cathedral_of_Flames_Level_1, adj_Cathedral_of_Flames_Level_1},
    {MapID::Verdant_Cascades, adj_Verdant_Cascades},
    {MapID::Magus_Stones, adj_Magus_Stones},
    {MapID::Catacombs_of_Kathandrax_Level_1, adj_Catacombs_of_Kathandrax_Level_1},
    {MapID::Alcazia_Tangle, adj_Alcazia_Tangle},
    {MapID::Rragars_Menagerie_Level_1, adj_Rragars_Menagerie_Level_1},
    {MapID::Slavers_Exile_Level_1, adj_Slavers_Exile_Level_1},
    {MapID::Oolas_Lab_Level_1, adj_Oolas_Lab_Level_1},
    {MapID::Shards_of_Orr_Level_1, adj_Shards_of_Orr_Level_1},
    {MapID::Arachnis_Haunt_Level_1, adj_Arachnis_Haunt_Level_1},
    {MapID::Vloxen_Excavations_Level_1, adj_Vloxen_Excavations_Level_1},
    {MapID::Heart_of_the_Shiverpeaks_Level_1, adj_Heart_of_the_Shiverpeaks_Level_1},
    {MapID::Bloodstone_Caves_Level_1, adj_Bloodstone_Caves_Level_1},
    {MapID::Bogroot_Growths_Level_1, adj_Bogroot_Growths_Level_1},
    {MapID::Ravens_Point_Level_1, adj_Ravens_Point_Level_1},
    {MapID::Vloxs_Falls, adj_Vloxs_Falls},
    {MapID::Battledepths_Level_1, adj_Battledepths_Level_1},
    {MapID::Sepulchre_of_Dragrimmar_Level_1, adj_Sepulchre_of_Dragrimmar_Level_1},
    {MapID::Frostmaws_Burrows_Level_1, adj_Frostmaws_Burrows_Level_1},
    {MapID::Darkrime_Delves_Level_1, adj_Darkrime_Delves_Level_1},
    {MapID::Gadds_Encampment_outpost, adj_Gadds_Encampment_outpost},
    {MapID::Umbral_Grotto_outpost, adj_Umbral_Grotto_outpost},
    {MapID::Rata_Sum_outpost, adj_Rata_Sum_outpost},
    {MapID::Tarnished_Haven_outpost, adj_Tarnished_Haven_outpost},
    {MapID::Eye_of_the_North_outpost, adj_Eye_of_the_North_outpost},
    {MapID::Sifhalla_outpost, adj_Sifhalla_outpost},
    {MapID::Gunnars_Hold_outpost, adj_Gunnars_Hold_outpost},
    {MapID::Olafstead_outpost, adj_Olafstead_outpost},
    {MapID::Hall_of_Monuments, adj_Hall_of_Monuments},
    {MapID::Dalada_Uplands, adj_Dalada_Uplands},
    {MapID::Doomlore_Shrine_outpost, adj_Doomlore_Shrine_outpost},
    {MapID::Grothmar_Wardowns, adj_Grothmar_Wardowns},
    {MapID::Longeyes_Ledge_outpost, adj_Longeyes_Ledge_outpost},
    {MapID::Sacnoth_Valley, adj_Sacnoth_Valley},
    {MapID::Central_Transfer_Chamber_outpost, adj_Central_Transfer_Chamber_outpost},
    {MapID::Boreal_Station_outpost, adj_Boreal_Station_outpost},
    {MapID::Piken_Square_pre_Searing_outpost, adj_Piken_Square_pre_Searing_outpost},
    {MapID::Forsaken_Tunnels_Presearing_Level1, adj_Forsaken_Tunnels_Presearing_Level1},
    {MapID::Isle_of_the_Nameless_PvP, adj_Isle_of_the_Nameless_PvP},
    {MapID::Secret_Underground_Lair, adj_Secret_Underground_Lair},
    {MapID::Zaishen_Menagerie_Grounds, adj_Zaishen_Menagerie_Grounds},
    {MapID::Zaishen_Menagerie_outpost, adj_Zaishen_Menagerie_outpost},
    {MapID::The_Underworld_Something_Wicked_This_Way_Comes, adj_The_Underworld_Something_Wicked_This_Way_Comes},
    {MapID::The_Underworld_Dont_Fear_the_Reapers, adj_The_Underworld_Dont_Fear_the_Reapers},
    {MapID::Droknars_Forge_Halloween_outpost, adj_Droknars_Forge_Halloween_outpost},
    {MapID::Droknars_Forge_Wintersday_outpost, adj_Droknars_Forge_Wintersday_outpost},
    {MapID::Tomb_of_the_Primeval_Kings_Halloween_outpost, adj_Tomb_of_the_Primeval_Kings_Halloween_outpost},
    {MapID::Eye_of_the_North_outpost_Wintersday_outpost, adj_Eye_of_the_North_outpost_Wintersday_outpost},
    {MapID::War_in_Kryta_Lions_Arch_Keep, adj_War_in_Kryta_Lions_Arch_Keep},
    {MapID::Forsaken_Tunnels_Level1, adj_Forsaken_Tunnels_Level1},
};

constexpr std::span<const MapID> GetNeighbors(MapID map_id) {
    for (const auto& entry : adjacency_table) {
        if (entry.map_id == map_id) return entry.neighbors;
    }
    return {};
}

} // namespace MapAdjacency