#pragma once

#include <GWCA\Constants\Constants.h>
#include <GWCA\Constants\Maps.h>

#include <ToolboxWindow.h>

class TravelWindow : public ToolboxWindow {
    TravelWindow() = default;
    ~TravelWindow() = default;

public:
    static TravelWindow& Instance() {
        static TravelWindow instance;
        return instance;
    }

    const char* Name() const override { return "Travel"; }
    const char* Icon() const override { return ICON_FA_GLOBE_EUROPE; }

    void Initialize() override;

    void Terminate() override;

    bool TravelFavorite(unsigned int idx);

    // Travel with checks, returns false if already in outpost or outpost not unlocked
    bool Travel(GW::Constants::MapID MapID, GW::Constants::District district, uint32_t district_number = 0);

    // Travel via UI interface, allowing "Are you sure" dialogs
    void UITravel(GW::Constants::MapID MapID, GW::Constants::District district,
                  uint32_t district_number = 0);

    // Travel to relevent outpost, then use scroll to access Urgoz/Deep
    void ScrollToOutpost(
        GW::Constants::MapID outpost_id,
        GW::Constants::District district = GW::Constants::District::Current,
        uint32_t district_number = 0);

    bool IsWaitingForMapTravel();

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    void Update(float delta) override;

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingInternal() override;
    static int RegionFromDistrict(GW::Constants::District district);
    static int LanguageFromDistrict(GW::Constants::District district);
    static GW::Constants::MapID GetNearestOutpost(GW::Constants::MapID map_to);

    static void CmdTP(const wchar_t *message, int argc, LPWSTR *argv);

private:
    // ==== Helpers ====
    void TravelButton(const char* text, int x_idx, GW::Constants::MapID mapid);
    GW::Constants::MapID IndexToOutpostID(int index);
    static bool ParseDistrict(const std::wstring &s, GW::Constants::District &district, uint32_t &number);
    static bool ParseOutpost(const std::wstring &s, GW::Constants::MapID &outpost, GW::Constants::District &district, uint32_t &number);

    // ==== Travel variables ====
    GW::Constants::District district = GW::Constants::District::Current;
    uint32_t district_number = 0;

    // ==== Favorites ====
    int fav_count = 0;
    std::vector<int> fav_index;

    // ==== options ====
    bool close_on_travel = false;

    // ==== scroll to outpost ====
    GW::Constants::MapID scroll_to_outpost_id = GW::Constants::MapID::None;     // Which outpost do we want to end up in?
    GW::Constants::MapID scroll_from_outpost_id = GW::Constants::MapID::None;   // Which outpost did we start from?

    bool map_travel_countdown_started = false;
    bool pending_map_travel = false;

    IDirect3DTexture9** scroll_texture = nullptr;

    /* Not used, but good to keep for reference!
    enum error_message_ids {
        error_B29 = 52,
        error_B30,
        error_B31,
        error_B32,
        error_B33,
        error_B34,
        error_B35,
        error_B36,
        error_B37,
        error_B38
    };
    enum error_message_trans_codes {
        error_B29 = 0xB29, // The target party has members who do not meet this mission's level requirements.
        error_B30, // You may not enter that outpost
        error_B31, // Your party leader must be a member of this guild. An officer from this guild must also be in the party.
        error_B32, // You must be the leader of your party to do that.
        error_B33, // You must be a member of a party to do that.
        error_B34, // Your party is already waiting to go somewhere else.
        error_B35, // Your party is already in that guild hall.
        error_B36, // Your party is already in that district.
        error_B37, // Your party is already in the active district.
        error_B38, // The merged party would be too large.
    };  */

    struct OutpostAlias
    {
        OutpostAlias(GW::Constants::MapID m, GW::Constants::District d = GW::Constants::District::Current, uint8_t n = 0)
            : map_id(m)
            , district(d)
            , district_number(n){};
        GW::Constants::MapID map_id = GW::Constants::MapID::None;
        GW::Constants::District district = GW::Constants::District::Current;
        uint8_t district_number = 0;
    };
    struct DistrictAlias
    {
        DistrictAlias(GW::Constants::District d, uint8_t n = 0)
            : district(d)
            , district_number(n){};
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

    const GW::Constants::MapID presearing_map_ids[5] = {
        GW::Constants::MapID::Ashford_Abbey_outpost,
        GW::Constants::MapID::Ascalon_City_pre_searing,
        GW::Constants::MapID::Foibles_Fair_outpost,
        GW::Constants::MapID::Fort_Ranik_pre_Searing_outpost,
        GW::Constants::MapID::The_Barradin_Estate_outpost
    };
    const char* presearing_map_names[5] = {
        "ashford abbey",
        "ascalon city",
        "foibles fair",
        "fort ranik",
        "barradin estate"
    };
    const GW::Constants::MapID searchable_map_ids[187] = {
        GW::Constants::MapID::Abaddons_Gate,
        GW::Constants::MapID::Abaddons_Mouth,
        GW::Constants::MapID::Altrumm_Ruins,
        GW::Constants::MapID::Amatz_Basin,
        GW::Constants::MapID::The_Amnoon_Oasis_outpost,
        GW::Constants::MapID::Arborstone_outpost_mission,
        GW::Constants::MapID::Ascalon_City_outpost,
        GW::Constants::MapID::Aspenwood_Gate_Kurzick_outpost,
        GW::Constants::MapID::Aspenwood_Gate_Luxon_outpost,
        GW::Constants::MapID::The_Astralarium_outpost,
        GW::Constants::MapID::Augury_Rock_outpost,
        GW::Constants::MapID::The_Aurios_Mines,
        GW::Constants::MapID::Aurora_Glade,
        GW::Constants::MapID::Bai_Paasu_Reach_outpost,
        GW::Constants::MapID::Basalt_Grotto_outpost,
        GW::Constants::MapID::Beacons_Perch_outpost,
        GW::Constants::MapID::Beetletun_outpost,
        GW::Constants::MapID::Beknur_Harbor,
        GW::Constants::MapID::Bergen_Hot_Springs_outpost,
        GW::Constants::MapID::Blacktide_Den,
        GW::Constants::MapID::Bloodstone_Fen,
        GW::Constants::MapID::Bone_Palace_outpost,
        GW::Constants::MapID::Boreal_Station_outpost,
        GW::Constants::MapID::Boreas_Seabed_outpost_mission,
        GW::Constants::MapID::Borlis_Pass,
        GW::Constants::MapID::Brauer_Academy_outpost,
        GW::Constants::MapID::Breaker_Hollow_outpost,
        GW::Constants::MapID::Camp_Hojanu_outpost,
        GW::Constants::MapID::Camp_Rankor_outpost,
        GW::Constants::MapID::Cavalon_outpost,
        GW::Constants::MapID::Central_Transfer_Chamber_outpost,
        GW::Constants::MapID::Chahbek_Village,
        GW::Constants::MapID::Champions_Dawn_outpost,
        GW::Constants::MapID::Chantry_of_Secrets_outpost,
        GW::Constants::MapID::Codex_Arena_outpost,
        GW::Constants::MapID::Consulate_Docks,
        GW::Constants::MapID::Copperhammer_Mines_outpost,
        GW::Constants::MapID::DAlessio_Seaboard,
        GW::Constants::MapID::Dajkah_Inlet,
        GW::Constants::MapID::Dasha_Vestibule,
        GW::Constants::MapID::The_Deep,
        GW::Constants::MapID::Deldrimor_War_Camp_outpost,
        GW::Constants::MapID::Destinys_Gorge_outpost,
        GW::Constants::MapID::Divinity_Coast,
        GW::Constants::MapID::Doomlore_Shrine_outpost,
        GW::Constants::MapID::The_Dragons_Lair,
        GW::Constants::MapID::Dragons_Throat,
        GW::Constants::MapID::Droknars_Forge_outpost,
        GW::Constants::MapID::Druids_Overlook_outpost,
        GW::Constants::MapID::Dunes_of_Despair,
        GW::Constants::MapID::Durheim_Archives_outpost,
        GW::Constants::MapID::Dzagonur_Bastion,
        GW::Constants::MapID::Elona_Reach,
        GW::Constants::MapID::Embark_Beach,
        GW::Constants::MapID::Ember_Light_Camp_outpost,
        GW::Constants::MapID::Eredon_Terrace_outpost,
        GW::Constants::MapID::The_Eternal_Grove_outpost_mission,
        GW::Constants::MapID::Eye_of_the_North_outpost,
        GW::Constants::MapID::Fishermens_Haven_outpost,
        GW::Constants::MapID::Fort_Aspenwood_Kurzick_outpost,
        GW::Constants::MapID::Fort_Aspenwood_Kurzick_outpost,
        GW::Constants::MapID::Fort_Aspenwood_Luxon_outpost,
        GW::Constants::MapID::Fort_Aspenwood_Luxon_outpost,
        GW::Constants::MapID::Fort_Ranik,
        GW::Constants::MapID::Frontier_Gate_outpost,
        GW::Constants::MapID::The_Frost_Gate,
        GW::Constants::MapID::Gadds_Encampment_outpost,
        GW::Constants::MapID::Domain_of_Anguish,
        GW::Constants::MapID::Gate_of_Desolation,
        GW::Constants::MapID::Gate_of_Fear_outpost,
        GW::Constants::MapID::Gate_of_Madness,
        GW::Constants::MapID::Gate_of_Pain,
        GW::Constants::MapID::Gate_of_Secrets_outpost,
        GW::Constants::MapID::Gate_of_the_Nightfallen_Lands_outpost,
        GW::Constants::MapID::Gate_of_Torment_outpost,
        GW::Constants::MapID::Gates_of_Kryta,
        GW::Constants::MapID::Grand_Court_of_Sebelkeh,
        GW::Constants::MapID::The_Granite_Citadel_outpost,
        GW::Constants::MapID::The_Great_Northern_Wall,
        GW::Constants::MapID::Great_Temple_of_Balthazar_outpost,
        GW::Constants::MapID::Grendich_Courthouse_outpost,
        GW::Constants::MapID::Gunnars_Hold_outpost,
        GW::Constants::MapID::Gyala_Hatchery_outpost_mission,
        GW::Constants::MapID::Harvest_Temple_outpost,
        GW::Constants::MapID::Hells_Precipice,
        GW::Constants::MapID::Henge_of_Denravi_outpost,
        GW::Constants::MapID::Heroes_Ascent_outpost,
        GW::Constants::MapID::Heroes_Audience_outpost,
        GW::Constants::MapID::Honur_Hill_outpost,
        GW::Constants::MapID::House_zu_Heltzer_outpost,
        GW::Constants::MapID::Ice_Caves_of_Sorrow,
        GW::Constants::MapID::Ice_Tooth_Cave_outpost,
        GW::Constants::MapID::Imperial_Sanctum_outpost_mission,
        GW::Constants::MapID::Iron_Mines_of_Moladune,
        GW::Constants::MapID::Jade_Flats_Kurzick_outpost,
        GW::Constants::MapID::Jade_Flats_Luxon_outpost,
        GW::Constants::MapID::The_Jade_Quarry_Kurzick_outpost,
        GW::Constants::MapID::The_Jade_Quarry_Kurzick_outpost,
        GW::Constants::MapID::The_Jade_Quarry_Luxon_outpost,
        GW::Constants::MapID::The_Jade_Quarry_Luxon_outpost,
        GW::Constants::MapID::Jennurs_Horde,
        GW::Constants::MapID::Jokanur_Diggings,
        GW::Constants::MapID::Kaineng_Center_outpost,
        GW::Constants::MapID::Kamadan_Jewel_of_Istan_outpost,
        GW::Constants::MapID::The_Kodash_Bazaar_outpost,
        GW::Constants::MapID::Kodlonu_Hamlet_outpost,
        GW::Constants::MapID::Kodonur_Crossroads,
        GW::Constants::MapID::Lair_of_the_Forgotten_outpost,
        GW::Constants::MapID::Leviathan_Pits_outpost,
        GW::Constants::MapID::Lions_Arch_outpost,
        GW::Constants::MapID::Longeyes_Ledge_outpost,
        GW::Constants::MapID::Lutgardis_Conservatory_outpost,
        GW::Constants::MapID::Maatu_Keep_outpost,
        GW::Constants::MapID::Maguuma_Stade_outpost,
        GW::Constants::MapID::Marhans_Grotto_outpost,
        GW::Constants::MapID::The_Marketplace_outpost,
        GW::Constants::MapID::Mihanu_Township_outpost,
        GW::Constants::MapID::Minister_Chos_Estate_outpost_mission,
        GW::Constants::MapID::Moddok_Crevice,
        GW::Constants::MapID::The_Mouth_of_Torment_outpost,
        GW::Constants::MapID::Nahpui_Quarter_outpost_mission,
        GW::Constants::MapID::Nolani_Academy,
        GW::Constants::MapID::Nundu_Bay,
        GW::Constants::MapID::Olafstead_outpost,
        GW::Constants::MapID::Piken_Square_outpost,
        GW::Constants::MapID::Pogahn_Passage,
        GW::Constants::MapID::Port_Sledge_outpost,
        GW::Constants::MapID::Quarrel_Falls_outpost,
        GW::Constants::MapID::Raisu_Palace_outpost_mission,
        GW::Constants::MapID::Ran_Musu_Gardens_outpost,
        GW::Constants::MapID::Random_Arenas_outpost,
        GW::Constants::MapID::Rata_Sum_outpost,
        GW::Constants::MapID::Remains_of_Sahlahja,
        GW::Constants::MapID::Rilohn_Refuge,
        GW::Constants::MapID::Ring_of_Fire,
        GW::Constants::MapID::Riverside_Province,
        GW::Constants::MapID::Ruins_of_Morah,
        GW::Constants::MapID::Ruins_of_Morah,
        GW::Constants::MapID::Ruins_of_Surmia,
        GW::Constants::MapID::Ruins_of_Surmia,
        GW::Constants::MapID::Saint_Anjekas_Shrine_outpost,
        GW::Constants::MapID::Sanctum_Cay,
        GW::Constants::MapID::Sardelac_Sanitarium_outpost,
        GW::Constants::MapID::Seafarers_Rest_outpost,
        GW::Constants::MapID::Seekers_Passage_outpost,
        GW::Constants::MapID::Seitung_Harbor_outpost,
        GW::Constants::MapID::Senjis_Corner_outpost,
        GW::Constants::MapID::Serenity_Temple_outpost,
        GW::Constants::MapID::The_Shadow_Nexus,
        GW::Constants::MapID::Shing_Jea_Arena,
        GW::Constants::MapID::Shing_Jea_Monastery_outpost,
        GW::Constants::MapID::Sifhalla_outpost,
        GW::Constants::MapID::Sunjiang_District_outpost_mission,
        GW::Constants::MapID::Sunspear_Arena,
        GW::Constants::MapID::Sunspear_Great_Hall_outpost,
        GW::Constants::MapID::Sunspear_Sanctuary_outpost,
        GW::Constants::MapID::Tahnnakai_Temple_outpost_mission,
        GW::Constants::MapID::Tanglewood_Copse_outpost,
        GW::Constants::MapID::Tarnished_Haven_outpost,
        GW::Constants::MapID::Temple_of_the_Ages,
        GW::Constants::MapID::Thirsty_River,
        GW::Constants::MapID::Thunderhead_Keep,
        GW::Constants::MapID::Tihark_Orchard,
        GW::Constants::MapID::Tomb_of_the_Primeval_Kings,
        GW::Constants::MapID::Tsumei_Village_outpost,
        GW::Constants::MapID::Umbral_Grotto_outpost,
        GW::Constants::MapID::Unwaking_Waters_Kurzick_outpost,
        GW::Constants::MapID::Unwaking_Waters_Luxon_outpost,
        GW::Constants::MapID::Urgozs_Warren,
        GW::Constants::MapID::Vasburg_Armory_outpost,
        GW::Constants::MapID::Venta_Cemetery,
        GW::Constants::MapID::Ventaris_Refuge_outpost,
        GW::Constants::MapID::Vizunah_Square_Foreign_Quarter_outpost,
        GW::Constants::MapID::Vizunah_Square_Local_Quarter_outpost,
        GW::Constants::MapID::Vloxs_Falls,
        GW::Constants::MapID::Wehhan_Terraces_outpost,
        GW::Constants::MapID::The_Wilds,
        GW::Constants::MapID::Yahnur_Market_outpost,
        GW::Constants::MapID::Yaks_Bend_outpost,
        GW::Constants::MapID::Yohlon_Haven_outpost,
        GW::Constants::MapID::Zaishen_Challenge_outpost,
        GW::Constants::MapID::Zaishen_Elite_outpost,
        GW::Constants::MapID::Zaishen_Menagerie_outpost,
        GW::Constants::MapID::Zen_Daijun_outpost_mission,
        GW::Constants::MapID::Zin_Ku_Corridor_outpost,
        GW::Constants::MapID::Zos_Shivros_Channel,
        GW::Constants::MapID::Great_Temple_of_Balthazar_outpost
    };
    const GW::Constants::MapID dungeon_map_ids[12]{
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
    const char* searchable_dungeon_names[12]{
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
    const char* searchable_map_names[187] {
        "abaddons gate",
        "abaddons mouth",
        "altrumm ruins",
        "amatz basin",
        "amnoon oasis",
        "arborstone",
        "ascalon city",
        "aspenwood gate kurzick",
        "aspenwood gate luxon",
        "astralarium",
        "augury rock",
        "aurios mines",
        "aurora glade",
        "bai paasu reach",
        "basalt grotto",
        "beacons perch",
        "beetletun",
        "beknur harbor",
        "bergen hot springs",
        "blacktide den",
        "bloodstone fen",
        "bone palace",
        "boreal station",
        "boreas seabed",
        "borlis pass",
        "brauer academy",
        "breaker hollow",
        "camp hojanu",
        "camp rankor",
        "cavalon",
        "central transfer chamber",
        "chahbek village",
        "champions dawn",
        "chantry of secrets",
        "codex arena",
        "consulate docks",
        "copperhammer mines",
        "dalessio seaboard",
        "dajkah inlet",
        "dasha vestibule",
        "deep",
        "deldrimor war camp",
        "destinys gorge",
        "divinity coast",
        "doomlore shrine",
        "dragons lair",
        "dragons throat",
        "droknars forge",
        "druids overlook",
        "dunes of despair",
        "durheim archives",
        "dzagonur bastion",
        "elona reach",
        "embark beach",
        "ember light camp",
        "eredon terrace",
        "eternal grove",
        "eye of the north",
        "fishermens haven",
        "fort aspenwood kurzick",
        "fa kurzick",
        "fort aspenwood luxon",
        "fa luxon",
        "fort ranik",
        "frontier gate",
        "frost gate",
        "gadds encampment",
        "domain of anguish",
        "gate of desolation",
        "gate of fear",
        "gate of madness",
        "gate of pain",
        "gate of secrets",
        "gate of the nightfallen lands",
        "gate of torment",
        "gates of kryta",
        "grand court of sebelkeh",
        "granite citadel",
        "great northern wall",
        "great temple of balthazar",
        "grendich courthouse",
        "gunnars hold",
        "gyala hatchery",
        "harvest temple",
        "hells precipice",
        "henge of denravi",
        "heroes ascent",
        "heroes audience",
        "honur hill",
        "house zu heltzer",
        "ice caves of sorrow",
        "ice tooth cave",
        "imperial sanctum",
        "iron mines of moladune",
        "jade flats kurzick",
        "jade flats luxon",
        "jade quarry kurzick",
        "jq kurzick",
        "jade quarry luxon",
        "jq luxon",
        "jennurs horde",
        "jokanur diggings",
        "kaineng center",
        "kamadan jewel of istan",
        "kodash bazaar",
        "kodlonu hamlet",
        "kodonur crossroads",
        "lair of the forgotten",
        "leviathan pits",
        "lions arch",
        "longeyes ledge",
        "lutgardis conservatory",
        "maatu keep",
        "maguuma stade",
        "marhans grotto",
        "marketplace",
        "mihanu township",
        "minister chos estate",
        "moddok crevice",
        "mouth of torment",
        "nahpui quarter",
        "nolani academy",
        "nundu bay",
        "olafstead",
        "piken square",
        "pogahn passage",
        "port sledge",
        "quarrel falls",
        "raisu palace",
        "ran musu gardens",
        "random arenas",
        "rata sum",
        "remains of sahlahja",
        "rilohn refuge",
        "ring of fire",
        "riverside province",
        "ruins of morah",
        "morah",
        "ruins of surmia",
        "surmia",
        "saint anjekas shrine",
        "sanctum cay",
        "sardelac sanitarium",
        "seafarers rest",
        "seekers passage",
        "seitung harbor",
        "senjis corner",
        "serenity temple",
        "shadow nexus",
        "shing jea arena",
        "shing jea monastery",
        "sifhalla",
        "sunjiang district",
        "sunspear arena",
        "sunspear great hall",
        "sunspear sanctuary",
        "tahnnakai temple",
        "tanglewood copse",
        "tarnished haven",
        "temple of the ages",
        "thirsty river",
        "thunderhead keep",
        "tihark orchard",
        "tomb of the primeval kings",
        "tsumei village",
        "umbral grotto",
        "unwaking waters kurzick",
        "unwaking waters luxon",
        "urgozs warren",
        "vasburg armory",
        "venta cemetery",
        "ventaris refuge",
        "vizunah square foreign quarter",
        "vizunah square local quarter",
        "vloxs falls",
        "wehhan terraces",
        "wilds",
        "yahnur market",
        "yaks bend",
        "yohlon haven",
        "zaishen challenge",
        "zaishen elite",
        "zaishen menagerie",
        "zen daijun",
        "zin ku corridor",
        "zos shivros channel",
        "great temple of balthazar"
    };

    std::vector<char*> searchable_explorable_areas;
    std::vector<GuiUtils::EncString*> searchable_explorable_areas_decode;
    std::vector<GW::Constants::MapID> searchable_explorable_area_ids;
    enum FetchedMapNames : uint8_t {
        Pending,
        Decoding,
        Decoded,
        Ready
    } fetched_searchable_explorable_areas = Pending;
};
