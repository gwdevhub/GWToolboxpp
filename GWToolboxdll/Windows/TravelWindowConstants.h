#pragma once

struct OutpostAlias {
    OutpostAlias(const GW::Constants::MapID m, const GW::Constants::District d = GW::Constants::District::Current, const uint8_t n = 0)
        : map_id(m)
        , district(d)
        , district_number(n) { }

    GW::Constants::MapID map_id = GW::Constants::MapID::None;
    GW::Constants::District district = GW::Constants::District::Current;
    uint8_t district_number = 0;
};

struct DistrictAlias {
    DistrictAlias(const GW::Constants::District d, const uint8_t n = 0)
        : district(d)
        , district_number(n) { }

    GW::Constants::District district = GW::Constants::District::Current;
    uint8_t district_number = 0;
};

// List of shorthand outpost names. This is checked for an exact match first.
const std::map<const std::string, const OutpostAlias> shorthand_outpost_names = {
    {"bestarea", {GW::Constants::MapID::The_Deep}},
    {"toa", {GW::Constants::MapID::Temple_of_the_Ages}},
    {"doa", {GW::Constants::MapID::Domain_of_Anguish}},
    {"goa", {GW::Constants::MapID::Domain_of_Anguish}},
    {"tdp", {GW::Constants::MapID::Domain_of_Anguish}},
    {"eee", {GW::Constants::MapID::Embark_Beach, GW::Constants::District::EuropeEnglish}},
    {"gtob", {GW::Constants::MapID::Great_Temple_of_Balthazar_outpost}},
    {"la", {GW::Constants::MapID::Lions_Arch_outpost}},
    {"ac", {GW::Constants::MapID::Ascalon_City_outpost}},
    {"eotn", {GW::Constants::MapID::Eye_of_the_North_outpost}},
    {"kc", {GW::Constants::MapID::Kaineng_Center_outpost}},
    {"hzh", {GW::Constants::MapID::House_zu_Heltzer_outpost}},
    {"ctc", {GW::Constants::MapID::Central_Transfer_Chamber_outpost}},
    {"topk", {GW::Constants::MapID::Tomb_of_the_Primeval_Kings}},
    {"ra", {GW::Constants::MapID::Random_Arenas_outpost}},
    {"ha", {GW::Constants::MapID::Heroes_Ascent_outpost}},
    {"ra", {GW::Constants::MapID::Random_Arenas_outpost}},
    {"fa", {GW::Constants::MapID::Fort_Aspenwood_Kurzick_outpost}},
    {"jq", {GW::Constants::MapID::The_Jade_Quarry_Kurzick_outpost}}
};

// List of shorthand district names. This is checked for an exact match.
const std::map<const std::string, const DistrictAlias> shorthand_district_names =
{
    {"ae", {GW::Constants::District::American}},
    {"int", {GW::Constants::District::International}},
    {"ee", {GW::Constants::District::EuropeEnglish}},
    {"eg", {GW::Constants::District::EuropeGerman}},
    {"de", {GW::Constants::District::EuropeGerman}},
    {"dd", {GW::Constants::District::EuropeGerman}},
    {"fr", {GW::Constants::District::EuropeFrench}},
    {"it", {GW::Constants::District::EuropeItalian}},
    {"es", {GW::Constants::District::EuropeSpanish}},
    {"pl", {GW::Constants::District::EuropePolish}},
    {"ru", {GW::Constants::District::EuropeRussian}},
    {"kr", {GW::Constants::District::AsiaKorean}},
    {"cn", {GW::Constants::District::AsiaChinese}},
    {"jp", {GW::Constants::District::AsiaJapanese}},
};

constexpr std::array presearing_map_ids = {
    GW::Constants::MapID::Ashford_Abbey_outpost,
    GW::Constants::MapID::Ascalon_City_pre_searing,
    GW::Constants::MapID::Foibles_Fair_outpost,
    GW::Constants::MapID::Fort_Ranik_pre_Searing_outpost,
    GW::Constants::MapID::The_Barradin_Estate_outpost, 
    GW::Constants::MapID::Piken_Square_pre_Searing_outpost
};

constexpr std::array presearing_map_names = {
    "ashford abbey",
    "ascalon city",
    "foibles fair",
    "fort ranik",
    "barradin estate", 
    "piken square"
};

constexpr std::array dungeon_map_ids{
    GW::Constants::MapID::Doomlore_Shrine_outpost,
    GW::Constants::MapID::Doomlore_Shrine_outpost,
    GW::Constants::MapID::Doomlore_Shrine_outpost,
    GW::Constants::MapID::Doomlore_Shrine_outpost,
    GW::Constants::MapID::Longeyes_Ledge_outpost,
    GW::Constants::MapID::Longeyes_Ledge_outpost,
    GW::Constants::MapID::Sifhalla_outpost,
    GW::Constants::MapID::Sifhalla_outpost,
    GW::Constants::MapID::Olafstead_outpost,
    GW::Constants::MapID::Umbral_Grotto_outpost,
    GW::Constants::MapID::Gadds_Encampment_outpost,
    GW::Constants::MapID::Deldrimor_War_Camp_outpost
};

constexpr std::array searchable_dungeon_names{
    "catacombs of kathandrax",
    "kathandrax",
    "rragars menagerie",
    "cathedral of flames",
    "ooze pit",
    "darkrime delves",
    "frostmaws burrows",
    "sepulchre of dragrimmar",
    "ravens point",
    "vloxen excavations",
    "bogroot growths",
    "sorrows furnace"
};

constexpr std::array district_words = {
    "Current District",
    "International",
    "American",
    "American District 1",
    "Europe English",
    "Europe French",
    "Europe German",
    "Europe Italian",
    "Europe Spanish",
    "Europe Polish",
    "Europe Russian",
    "Asian Korean",
    "Asia Chinese",
    "Asia Japanese",
};

constexpr std::array district_ids = {
    GW::Constants::District::Current,
    GW::Constants::District::International,
    GW::Constants::District::American,
    GW::Constants::District::American,
    GW::Constants::District::EuropeEnglish,
    GW::Constants::District::EuropeFrench,
    GW::Constants::District::EuropeGerman,
    GW::Constants::District::EuropeItalian,
    GW::Constants::District::EuropeSpanish,
    GW::Constants::District::EuropePolish,
    GW::Constants::District::EuropeRussian,
    GW::Constants::District::AsiaKorean,
    GW::Constants::District::AsiaChinese,
    GW::Constants::District::AsiaJapanese,
};
