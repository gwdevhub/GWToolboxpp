#include "stdafx.h"

#include <GWCA/Context/WorldContext.h>

#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Quest.h>

#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Utilities/Hook.h>

#include <Logger.h>
#include <Utils/GuiUtils.h>

#include <Constants/EncStrings.h>
#include <Constants/ZaishenMissionMaps.h>
#include <Modules/InventoryManager.h>
#include <Modules/Resources.h>
#include <Timer.h>
#include <Utils/TextUtils.h>
#include <Utils/ToolboxUtils.h>
#include <Windows/CompletionWindow.h>
#include <Windows/DailyQuestsWindow.h>
#include <Windows/TravelWindow.h>

using GW::Constants::MapID;
using GW::Constants::QuestID;

namespace {
    constexpr size_t ZAISHEN_BOUNTY_COUNT = 66;
    constexpr size_t ZAISHEN_MISSION_COUNT = 69;
    constexpr size_t ZAISHEN_COMBAT_COUNT = 28;
    constexpr size_t ZAISHEN_VANQUISH_COUNT = 136;
    constexpr size_t WANTED_COUNT = 21;
    constexpr size_t WEEKLY_BONUS_PVE_COUNT = 9;
    constexpr size_t WEEKLY_BONUS_PVP_COUNT = 6;
    constexpr size_t VANGUARD_COUNT = 9;
    constexpr size_t NICHOLAS_PRE_COUNT = 52;
    constexpr size_t NICHOLAS_POST_COUNT = 137;
    constexpr time_t NICHOLAS_POST_START_DATE = 1405954800; // Monday, July 21, 2014 3:00:00 PM | Matches with the first Red Iris Flowers in Regent Valley
    constexpr int SECONDSINAWEEK = 604800;


    class ZaishenQuestData : public DailyQuests::QuestData {
    public:
        ZaishenQuestData(MapID map_id = (MapID)0, const wchar_t* enc_name = nullptr) : QuestData(map_id, enc_name) {};

        ZaishenQuestData(const wchar_t* enc_name = nullptr, MapID map_id = (MapID)0) : QuestData(map_id, enc_name) {};
        const MapID GetQuestGiverOutpost() override;
    };

    class ZaishenVanquishQuestData : public ZaishenQuestData {
    public:
        ZaishenVanquishQuestData(MapID map_id = (MapID)0, const wchar_t* enc_name = nullptr) : ZaishenQuestData(map_id, enc_name) {};
        const MapID GetQuestGiverOutpost() override;
    };

    class WantedQuestData : public DailyQuests::QuestData {
    public:
        WantedQuestData(MapID map_id = (MapID)0, const wchar_t* enc_name = nullptr) : QuestData(map_id, enc_name) {};
        const MapID GetQuestGiverOutpost() override;
    };


    constexpr std::array hard_coded_wanted_by_shining_blade_names = {"Justiciar Kimii",        "Zaln the Jaded",          "Justiciar Sevaan",  "Insatiable Vakar",   "Amalek the Unmerciful",     "Carnak the Hungry", "Valis the Rampant",       "Cerris",
                                                                     "Sarnia the Red-Handed",  "Destor the Truth Seeker", "Selenas the Blunt", "Justiciar Amilyn",   "Maximilian the Meticulous", "Joh the Hostile",   "Barthimus the Provident", "Calamitous",
                                                                     "Greves the Overbearing", "Lev the Condemned",       "Justiciar Marron",  "Justiciar Kasandra", "Vess the Disputant"};

    static_assert(hard_coded_wanted_by_shining_blade_names.size() == WANTED_COUNT);

    constexpr std::array hard_coded_vanguard_names = {
        "Bandits",
        "Utini Wupwup",
        "Ascalonian Noble",
        "Undead", // 0x8102 0x7050 0xFC72 0xBD17 0x552E
        "Blazefiend Griefblade",
        "Farmer Hamnet",
        "Charr",
        "Countess Nadya",
        "Footman Tate"
    };

    static_assert(hard_coded_vanguard_names.size() == VANGUARD_COUNT);

    std::unordered_map<std::wstring, uint16_t> nicholas_sandford_item_collected_count;

    DailyQuests::QuestData nicholas_sandford_cycles[] = {
        {MapID::None, GW::EncStrings::GrawlNecklaces},      {MapID::None, GW::EncStrings::BakedHusks},     {MapID::None, GW::EncStrings::SkeletalLimbs},       {MapID::None, GW::EncStrings::UnnaturalSeeds},
        {MapID::None, GW::EncStrings::EnchantedLodestones}, {MapID::None, GW::EncStrings::SkaleFinPreSearing},      {MapID::None, GW::EncStrings::IcyLodestones},       {MapID::None, GW::EncStrings::GargoyleSkulls},
        {MapID::None, GW::EncStrings::DullCarapaces},       {MapID::None, GW::EncStrings::BakedHusks},     {MapID::None, GW::EncStrings::RedIrisFlowers},      {MapID::None, GW::EncStrings::SpiderLegs},
        {MapID::None, GW::EncStrings::SkeletalLimbs},       {MapID::None, GW::EncStrings::CharrCarvings},  {MapID::None, GW::EncStrings::EnchantedLodestones}, {MapID::None, GW::EncStrings::GrawlNecklaces},
        {MapID::None, GW::EncStrings::IcyLodestones},       {MapID::None, GW::EncStrings::WornBelts},      {MapID::None, GW::EncStrings::GargoyleSkulls},      {MapID::None, GW::EncStrings::UnnaturalSeeds},
        {MapID::None, GW::EncStrings::SkaleFinPreSearing},  {MapID::None, GW::EncStrings::RedIrisFlowers},     {MapID::None, GW::EncStrings::EnchantedLodestones}, {MapID::None, GW::EncStrings::SkeletalLimbs},
        {MapID::None, GW::EncStrings::CharrCarvings},       {MapID::None, GW::EncStrings::SpiderLegs},     {MapID::None, GW::EncStrings::BakedHusks},          {MapID::None, GW::EncStrings::GargoyleSkulls},
        {MapID::None, GW::EncStrings::UnnaturalSeeds},      {MapID::None, GW::EncStrings::IcyLodestones},  {MapID::None, GW::EncStrings::GrawlNecklaces},      {MapID::None, GW::EncStrings::EnchantedLodestones},
        {MapID::None, GW::EncStrings::WornBelts},           {MapID::None, GW::EncStrings::DullCarapaces},  {MapID::None, GW::EncStrings::SpiderLegs},          {MapID::None, GW::EncStrings::GargoyleSkulls},
        {MapID::None, GW::EncStrings::IcyLodestones},       {MapID::None, GW::EncStrings::UnnaturalSeeds}, {MapID::None, GW::EncStrings::WornBelts},           {MapID::None, GW::EncStrings::GrawlNecklaces},
        {MapID::None, GW::EncStrings::BakedHusks},          {MapID::None, GW::EncStrings::SkeletalLimbs},  {MapID::None, GW::EncStrings::RedIrisFlowers},      {MapID::None, GW::EncStrings::CharrCarvings},
        {MapID::None, GW::EncStrings::SkaleFinPreSearing},  {MapID::None, GW::EncStrings::DullCarapaces},      {MapID::None, GW::EncStrings::EnchantedLodestones}, {MapID::None, GW::EncStrings::CharrCarvings},
        {MapID::None, GW::EncStrings::SpiderLegs},          {MapID::None, GW::EncStrings::RedIrisFlowers}, {MapID::None, GW::EncStrings::WornBelts},           {MapID::None, GW::EncStrings::DullCarapaces}
    };
    static_assert(_countof(nicholas_sandford_cycles) == NICHOLAS_PRE_COUNT);

    // these vectors are built inside Initialise() because the strings are hard coded

    std::vector<WantedQuestData> wanted_by_shining_blade_cycles;
    std::vector<DailyQuests::QuestData> vanguard_cycles;

    ZaishenQuestData zaishen_bounty_cycles[] = {
        {MapID::Poisoned_Outcrops, L"Droajam, Mage of the Sands"},
        {MapID::Nahpui_Quarter_explorable, L"Royen Beastkeeper"},
        {MapID::Bloodstone_Caves_Level_1, L"Eldritch Ettin"},
        {MapID::The_Underworld, L"Vengeful Aatxe"},
        {MapID::Fronis_Irontoes_Lair_mission, L"Fronis Irontoe"},
        {MapID::Urgozs_Warren, L"Urgoz"},
        {MapID::Norrhart_Domains, L"Fenrir"},
        {MapID::Slavers_Exile_Level_1, L"Selvetarm"},
        {MapID::Gyala_Hatchery, L"Mohby Windbeak"},
        {MapID::The_Underworld, L"Charged Blackness"},
        {MapID::Majestys_Rest, L"Rotscale"},
        {MapID::Vloxen_Excavations_Level_1, L"Zoldark the Unholy"},
        {MapID::Forum_Highlands, L"Korshek the Immolated"},
        {MapID::Drakkar_Lake, L"Myish, Lady of the Lake"},
        {MapID::Frostmaws_Burrows_Level_1, L"Frostmaw the Kinslayer"},
        {MapID::Unwaking_Waters, L"Kunvie Firewing"},
        {MapID::Bogroot_Growths_Level_1, L"Z'him Monns"},
        {MapID::Domain_of_Anguish, L"The Greater Darkness"},
        {MapID::Oolas_Lab_Level_1, L"TPS Regulator Golem"},
        {MapID::Ravens_Point_Level_1, L"Plague of Destruction"},
        {MapID::Tomb_of_the_Primeval_Kings, L"The Darknesses"},
        {MapID::Jahai_Bluffs, L"Admiral Kantoh"},
        {MapID::Sacnoth_Valley, L"Borrguus Blisterbark"},
        {MapID::Slavers_Exile_Level_1, L"Forgewight"},
        {MapID::The_Undercity, L"Baubao Wavewrath"},
        {MapID::Riven_Earth, L"Joffs the Mitigator"},
        {MapID::Rragars_Menagerie_Level_1, L"Rragar Maneater"},
        {MapID::The_Undercity, L"Chung, the Attuned"},
        {MapID::Domain_of_Anguish, L"Lord Jadoth"},
        {MapID::Drakkar_Lake, L"Nulfastu, Earthbound"},
        {MapID::Sorrows_Furnace, L"The Iron Forgeman"},
        {MapID::Heart_of_the_Shiverpeaks_Level_1, L"Magmus"},
        {MapID::Sparkfly_Swamp, L"Mobrin, Lord of the Marsh"},
        {MapID::Vehtendi_Valley, L"Jarimiya the Unmerciful"},
        {MapID::Slavers_Exile_Level_1, L"Duncan the Black"},
        {MapID::Tahnnakai_Temple_explorable, L"Quansong Spiritspeak"},
        {MapID::Domain_of_Anguish, L"The Stygian Underlords"},
        {MapID::Sacnoth_Valley, L"Fozzy Yeoryios"},
        {MapID::Domain_of_Anguish, L"The Black Beast of Arrgh"},
        {MapID::Arachnis_Haunt_Level_1, L"Arachni"},
        {MapID::The_Underworld, L"The Four Horsemen"},
        {MapID::Sepulchre_of_Dragrimmar_Level_1, L"Remnant of Antiquities"},
        {MapID::Morostav_Trail, L"Arbor Earthcall"},
        {MapID::Ooze_Pit_mission, L"Prismatic Ooze"},
        {MapID::The_Fissure_of_Woe, L"Lord Khobay"},
        {MapID::Crystal_Overlook, L"Jedeh the Mighty"},
        {MapID::Archipelagos, L"Ssuns, Blessed of Dwayna"},
        {MapID::Slavers_Exile_Level_1, L"Justiciar Thommis"},
        {MapID::Perdition_Rock, L"Harn and Maxine Coldstone"},
        {MapID::Alcazia_Tangle, L"Pywatt the Swift"},
        {MapID::Shards_of_Orr_Level_1, L"Fendi Nin"},
        {MapID::Ferndale, L"Mungri Magicbox"},
        {MapID::The_Fissure_of_Woe, L"Priest of Menzies"},
        {MapID::Catacombs_of_Kathandrax_Level_1, L"Ilsundur, Lord of Fire"},
        {MapID::Prophets_Path, L"Kepkhet Marrowfeast"},
        {MapID::Barbarous_Shore, L"Commander Wahli"},
        {MapID::The_Deep, L"Kanaxai"},
        {MapID::Bogroot_Growths_Level_1, L"Khabuus"},
        {MapID::Dalada_Uplands, L"Molotov Rocktail"},
        {MapID::Domain_of_Anguish, L"The Stygian Lords"},
        {MapID::The_Fissure_of_Woe, L"Dragon Lich"},
        {MapID::Darkrime_Delves_Level_1, L"Havok Soulwail"},
        {MapID::Xaquang_Skyway, L"Ghial the Bone Dancer"},
        {MapID::Cathedral_of_Flames_Level_1, L"Murakai, Lady of the Night"},
        {MapID::Slavers_Exile_Level_1, L"Rand Stormweaver"},
        {MapID::Kessex_Peak, L"Verata"}
    };
    static_assert(_countof(zaishen_bounty_cycles) == ZAISHEN_BOUNTY_COUNT);

    // These vectors are good to go

    std::unordered_map<DailyQuests::NicholasCycleData*, uint16_t> nicholas_item_collected_count;

    DailyQuests::NicholasCycleData nicholas_cycles[] = {
        {GW::EncStrings::RedIrisFlowers, 3, MapID::Regent_Valley},
        {GW::EncStrings::FeatheredAvicaraScalps, 3, MapID::Mineral_Springs},
        {GW::EncStrings::MargoniteMasks, 2, MapID::Poisoned_Outcrops},
        {GW::EncStrings::QuetzalCrests, 2, MapID::Alcazia_Tangle},
        {GW::EncStrings::PlagueIdols, 3, MapID::Wajjun_Bazaar},
        {GW::EncStrings::AzureRemains, 2, MapID::Dreadnoughts_Drift},
        {GW::EncStrings::MandragorRootCake, 1, MapID::Arkjok_Ward},
        {GW::EncStrings::MahgoClaw, 1, MapID::Perdition_Rock},
        {GW::EncStrings::MantidPincers, 5, MapID::Saoshang_Trail},
        {GW::EncStrings::SentientSeeds, 3, MapID::Fahranur_The_First_City},
        {GW::EncStrings::StoneGrawlNecklaces, 2, MapID::Sacnoth_Valley},
        {GW::EncStrings::Herring, 1, MapID::Twin_Serpent_Lakes},
        {GW::EncStrings::NagaSkins, 3, MapID::Mount_Qinkai},
        {GW::EncStrings::GloomSeed, 1, MapID::The_Falls},
        {GW::EncStrings::CharrHide, 1, MapID::The_Breach},
        {GW::EncStrings::RubyDjinnEssence, 1, MapID::The_Alkali_Pan},
        {GW::EncStrings::ThornyCarapaces, 2, MapID::Majestys_Rest},
        {GW::EncStrings::BoneCharms, 3, MapID::Rheas_Crater},
        {GW::EncStrings::ModniirManes, 3, MapID::Varajar_Fells},
        {GW::EncStrings::SuperbCharrCarvings, 3, MapID::Dalada_Uplands},
        {GW::EncStrings::RollsofParchment, 5, MapID::Zen_Daijun_explorable},
        {GW::EncStrings::RoaringEtherClaws, 2, MapID::Garden_of_Seborhin},
        {GW::EncStrings::BranchesofJuniBerries, 3, MapID::Bukdek_Byway},
        {GW::EncStrings::ShiverpeakManes, 3, MapID::Deldrimor_Bowl},
        {GW::EncStrings::FetidCarapaces, 3, MapID::Eastern_Frontier},
        {GW::EncStrings::MoonShells, 2, MapID::Gyala_Hatchery},
        {GW::EncStrings::MassiveJawbone, 1, MapID::The_Arid_Sea},
        {GW::EncStrings::ChromaticScale, 1, MapID::Ice_Cliff_Chasms},
        {GW::EncStrings::MursaatTokens, 3, MapID::Ice_Floe},
        {GW::EncStrings::SentientLodestone, 1, MapID::Bahdok_Caverns},
        {GW::EncStrings::JungleTrollTusks, 3, MapID::Tangle_Root},
        {GW::EncStrings::SapphireDjinnEssence, 1, MapID::Resplendent_Makuun},
        {GW::EncStrings::StoneCarving, 1, MapID::Arborstone_explorable},
        {GW::EncStrings::FeatheredCaromiScalps, 3, MapID::North_Kryta_Province},
        {GW::EncStrings::PillagedGoods, 1, MapID::Holdings_of_Chokhin},
        {GW::EncStrings::GoldCrimsonSkullCoin, 1, MapID::Haiju_Lagoon},
        {GW::EncStrings::JadeBracelets, 3, MapID::Tahnnakai_Temple_explorable},
        {GW::EncStrings::MinotaurHorns, 2, MapID::Prophets_Path},
        {GW::EncStrings::FrostedGriffonWings, 2, MapID::Snake_Dance},
        {GW::EncStrings::SilverBullionCoins, 2, MapID::Mehtani_Keys},
        {GW::EncStrings::Truffle, 1, MapID::Morostav_Trail},
        {GW::EncStrings::SkelkClaws, 3, MapID::Verdant_Cascades},
        {GW::EncStrings::DessicatedHydraClaws, 2, MapID::The_Scar},
        {GW::EncStrings::FrigidHearts, 3, MapID::Spearhead_Peak},
        {GW::EncStrings::CelestialEssences, 3, MapID::Nahpui_Quarter_explorable},
        {GW::EncStrings::PhantomResidue, 1, MapID::Lornars_Pass},
        {GW::EncStrings::DrakeKabob, 1, MapID::Issnur_Isles},
        {GW::EncStrings::AmberChunks, 3, MapID::Ferndale},
        {GW::EncStrings::GlowingHearts, 2, MapID::Stingray_Strand},
        {GW::EncStrings::SaurianBones, 5, MapID::Riven_Earth},
        {GW::EncStrings::BehemothHides, 2, MapID::Wilderness_of_Bahdza},
        {GW::EncStrings::LuminousStone, 1, MapID::Crystal_Overlook},
        {GW::EncStrings::IntricateGrawlNecklaces, 3, MapID::Witmans_Folly},
        {GW::EncStrings::JadeiteShards, 3, MapID::Shadows_Passage},
        {GW::EncStrings::GoldDoubloon, 1, MapID::Barbarous_Shore},
        {GW::EncStrings::ShriveledEyes, 2, MapID::Skyward_Reach},
        {GW::EncStrings::IcyLodestones, 2, MapID::Icedome},
        {GW::EncStrings::KeenOniTalon, 1, MapID::Silent_Surf},
        {GW::EncStrings::HardenedHumps, 2, MapID::Nebo_Terrace},
        {GW::EncStrings::PilesofElementalDust, 2, MapID::Drakkar_Lake},
        {GW::EncStrings::NagaHides, 3, MapID::Panjiang_Peninsula},
        {GW::EncStrings::SpiritwoodPlanks, 3, MapID::Griffons_Mouth},
        {GW::EncStrings::StormyEye, 1, MapID::Pockmark_Flats},
        {GW::EncStrings::SkreeWings, 3, MapID::Forum_Highlands},
        {GW::EncStrings::SoulStones, 3, MapID::Raisu_Palace},
        {GW::EncStrings::SpikedCrest, 1, MapID::Tears_of_the_Fallen},
        {GW::EncStrings::DragonRoot, 1, MapID::Drazach_Thicket},
        {GW::EncStrings::BerserkerHorns, 3, MapID::Jaga_Moraine},
        {GW::EncStrings::BehemothJaw, 1, MapID::Mamnoon_Lagoon},
        {GW::EncStrings::BowlofSkalefinSoup, 1, MapID::Zehlon_Reach},
        {GW::EncStrings::ForestMinotaurHorns, 2, MapID::Kessex_Peak},
        {GW::EncStrings::PutridCysts, 3, MapID::Sunjiang_District_explorable},
        {GW::EncStrings::JadeMandibles, 2, MapID::Salt_Flats},
        {GW::EncStrings::MaguumaManes, 2, MapID::Silverwood},
        {GW::EncStrings::SkullJuju, 1, MapID::The_Eternal_Grove},
        {GW::EncStrings::MandragorSwamproots, 3, MapID::Lahtenda_Bog},
        {GW::EncStrings::BottleofVabbianWine, 1, MapID::Vehtendi_Valley},
        {GW::EncStrings::WeaverLegs, 2, MapID::Magus_Stones},
        {GW::EncStrings::TopazCrest, 1, MapID::Diviners_Ascent},
        {GW::EncStrings::RotWallowTusks, 2, MapID::Pongmei_Valley},
        {GW::EncStrings::FrostfireFangs, 2, MapID::Anvil_Rock},
        {GW::EncStrings::DemonicRelic, 1, MapID::The_Ruptured_Heart},
        {GW::EncStrings::AbnormalSeeds, 2, MapID::Talmark_Wilderness},
        {GW::EncStrings::DiamondDjinnEssence, 1, MapID::The_Hidden_City_of_Ahdashim},
        {GW::EncStrings::ForgottenSeals, 2, MapID::Vulture_Drifts},
        {GW::EncStrings::CopperCrimsonSkullCoins, 5, MapID::Kinya_Province},
        {GW::EncStrings::MossyMandibles, 3, MapID::Ettins_Back},
        {GW::EncStrings::EnslavementStones, 2, MapID::Grenths_Footprint},
        {GW::EncStrings::ElonianLeatherSquares, 5, MapID::Jahai_Bluffs},
        {GW::EncStrings::CobaltTalons, 2, MapID::Vehjin_Mines},
        {GW::EncStrings::MaguumaSpiderWeb, 1, MapID::Reed_Bog},
        {GW::EncStrings::ForgottenTrinketBoxes, 5, MapID::Minister_Chos_Estate_explorable},
        {GW::EncStrings::IcyHumps, 3, MapID::Iron_Horse_Mine},
        {GW::EncStrings::SandblastedLodestone, 1, MapID::The_Shattered_Ravines},
        {GW::EncStrings::BlackPearls, 3, MapID::Archipelagos},
        {GW::EncStrings::InsectCarapaces, 3, MapID::Marga_Coast},
        {GW::EncStrings::MergoyleSkulls, 3, MapID::Watchtower_Coast},
        {GW::EncStrings::DecayedOrrEmblems, 3, MapID::Cursed_Lands},
        {GW::EncStrings::TemperedGlassVials, 5, MapID::Mourning_Veil_Falls},
        {GW::EncStrings::ScorchedLodestones, 3, MapID::Old_Ascalon},
        {GW::EncStrings::WaterDjinnEssence, 1, MapID::Turais_Procession},
        {GW::EncStrings::GuardianMoss, 1, MapID::Maishang_Hills},
        {GW::EncStrings::DwarvenAles, 6, MapID::The_Floodplain_of_Mahnkelon},
        {GW::EncStrings::AmphibianTongues, 2, MapID::Sparkfly_Swamp},
        {GW::EncStrings::AlpineSeeds, 2, MapID::Frozen_Forest},
        {GW::EncStrings::TangledSeeds, 2, MapID::Dry_Top},
        {GW::EncStrings::StolenSupplies, 3, MapID::Jaya_Bluffs},
        {GW::EncStrings::PahnaiSalad, 1, MapID::Plains_of_Jarin},
        {GW::EncStrings::VerminHides, 3, MapID::Xaquang_Skyway},
        {GW::EncStrings::RoaringEtherHeart, 1, MapID::The_Mirror_of_Lyss},
        {GW::EncStrings::LeatheryClaws, 3, MapID::Ascalon_Foothills},
        {GW::EncStrings::AzureCrest, 1, MapID::Unwaking_Waters},
        {GW::EncStrings::JotunPelt, 1, MapID::Bjora_Marches},
        {GW::EncStrings::HeketTongues, 2, MapID::Dejarin_Estate},
        {GW::EncStrings::MountainTrollTusks, 5, MapID::Talus_Chute},
        {GW::EncStrings::VialsofInk, 3, MapID::Shenzun_Tunnels},
        {GW::EncStrings::KournanPendants, 3, MapID::Gandara_the_Moon_Fortress},
        {GW::EncStrings::SingedGargoyleSkulls, 3, MapID::Diessa_Lowlands},
        {GW::EncStrings::DredgeIncisors, 3, MapID::Melandrus_Hope},
        {GW::EncStrings::StoneSummitBadges, 3, MapID::Tascas_Demise},
        {GW::EncStrings::KraitSkins, 3, MapID::Arbor_Bay},
        {GW::EncStrings::InscribedShards, 2, MapID::Jokos_Domain},
        {GW::EncStrings::FeatheredScalps, 3, MapID::Sunqua_Vale},
        {GW::EncStrings::MummyWrappings, 3, MapID::The_Sulfurous_Wastes},
        {GW::EncStrings::ShadowyRemnants, 2, MapID::The_Black_Curtain},
        {GW::EncStrings::AncientKappaShells, 3, MapID::The_Undercity},
        {GW::EncStrings::Geode, 1, MapID::Yatendi_Canyons},
        {GW::EncStrings::FibrousMandragorRoots, 2, MapID::Grothmar_Wardowns},
        {GW::EncStrings::GruesomeRibcages, 3, MapID::Dragons_Gullet},
        {GW::EncStrings::KrakenEyes, 2, MapID::Boreas_Seabed_explorable},
        {GW::EncStrings::BogSkaleFins, 3, MapID::Scoundrels_Rise},
        {GW::EncStrings::SentientSpores, 2, MapID::Sunward_Marches},
        {GW::EncStrings::AncientEyes, 2, MapID::Sage_Lands},
        {GW::EncStrings::CopperShillings, 3, MapID::Cliffs_of_Dohjok},
        {GW::EncStrings::FrigidMandragorHusks, 3, MapID::Norrhart_Domains},
        {GW::EncStrings::BoltsofLinen, 3, MapID::Travelers_Vale},
        {GW::EncStrings::CharrCarvings, 3, MapID::Flame_Temple_Corridor},
    };
    static_assert(_countof(nicholas_cycles) == NICHOLAS_POST_COUNT);

    ZaishenQuestData zaishen_combat_cycles[] = {
        {MapID::The_Jade_Quarry_mission},
        {MapID::Codex_Arena_outpost},
        {MapID::Heroes_Ascent_outpost},
        {MapID::Isle_of_the_Dead_guild_hall, GW::EncStrings::GuildVersusGuild},
        {MapID::Isle_of_the_Dead_guild_hall, GW::EncStrings::AllianceBattles},
        {MapID::Heroes_Ascent_outpost},
        {MapID::Isle_of_the_Dead_guild_hall, GW::EncStrings::GuildVersusGuild},
        {MapID::Codex_Arena_outpost},
        {MapID::Fort_Aspenwood_mission},
        {MapID::The_Jade_Quarry_mission},
        {MapID::Random_Arenas_outpost},
        {MapID::Codex_Arena_outpost},
        {MapID::Isle_of_the_Dead_guild_hall, GW::EncStrings::GuildVersusGuild},
        {MapID::The_Jade_Quarry_mission},
        {MapID::Isle_of_the_Dead_guild_hall, GW::EncStrings::AllianceBattles},
        {MapID::Heroes_Ascent_outpost},
        {MapID::Random_Arenas_outpost},
        {MapID::Fort_Aspenwood_mission},
        {MapID::The_Jade_Quarry_mission},
        {MapID::Random_Arenas_outpost},
        {MapID::Fort_Aspenwood_mission},
        {MapID::Heroes_Ascent_outpost},
        {MapID::Isle_of_the_Dead_guild_hall, GW::EncStrings::AllianceBattles},
        {MapID::Isle_of_the_Dead_guild_hall, GW::EncStrings::GuildVersusGuild},
        {MapID::Codex_Arena_outpost},
        {MapID::Random_Arenas_outpost},
        {MapID::Fort_Aspenwood_mission},
        {MapID::Isle_of_the_Dead_guild_hall, GW::EncStrings::AllianceBattles}
    };
    static_assert(_countof(zaishen_combat_cycles) == ZAISHEN_COMBAT_COUNT);

    ZaishenVanquishQuestData zaishen_vanquish_cycles[] = {
        {MapID::Jaya_Bluffs},
        {MapID::Holdings_of_Chokhin},
        {MapID::Ice_Cliff_Chasms},
        {MapID::Griffons_Mouth},
        {MapID::Kinya_Province},
        {MapID::Issnur_Isles},
        {MapID::Jaga_Moraine},
        {MapID::Ice_Floe},
        {MapID::Maishang_Hills},
        {MapID::Jahai_Bluffs},
        {MapID::Riven_Earth},
        {MapID::Icedome},
        {MapID::Minister_Chos_Estate_explorable},
        {MapID::Mehtani_Keys},
        {MapID::Sacnoth_Valley},
        {MapID::Iron_Horse_Mine},
        {MapID::Morostav_Trail},
        {MapID::Plains_of_Jarin},
        {MapID::Sparkfly_Swamp},
        {MapID::Kessex_Peak},
        {MapID::Mourning_Veil_Falls},
        {MapID::The_Alkali_Pan},
        {MapID::Varajar_Fells},
        {MapID::Lornars_Pass},
        {MapID::Pongmei_Valley},
        {MapID::The_Floodplain_of_Mahnkelon},
        {MapID::Verdant_Cascades},
        {MapID::Majestys_Rest},
        {MapID::Raisu_Palace},
        {MapID::The_Hidden_City_of_Ahdashim},
        {MapID::Rheas_Crater},
        {MapID::Mamnoon_Lagoon},
        {MapID::Shadows_Passage},
        {MapID::The_Mirror_of_Lyss},
        {MapID::Saoshang_Trail},
        {MapID::Nebo_Terrace},
        {MapID::Shenzun_Tunnels},
        {MapID::The_Ruptured_Heart},
        {MapID::Salt_Flats},
        {MapID::North_Kryta_Province},
        {MapID::Silent_Surf},
        {MapID::The_Shattered_Ravines},
        {MapID::Scoundrels_Rise},
        {MapID::Old_Ascalon},
        {MapID::Sunjiang_District_explorable},
        {MapID::The_Sulfurous_Wastes},
        {MapID::Magus_Stones},
        {MapID::Perdition_Rock},
        {MapID::Sunqua_Vale},
        {MapID::Turais_Procession},
        {MapID::Norrhart_Domains},
        {MapID::Pockmark_Flats},
        {MapID::Tahnnakai_Temple_explorable},
        {MapID::Vehjin_Mines},
        {MapID::Poisoned_Outcrops},
        {MapID::Prophets_Path},
        {MapID::The_Eternal_Grove},
        {MapID::Tascas_Demise},
        {MapID::Resplendent_Makuun},
        {MapID::Reed_Bog},
        {MapID::Unwaking_Waters},
        {MapID::Stingray_Strand},
        {MapID::Sunward_Marches},
        {MapID::Regent_Valley},
        {MapID::Wajjun_Bazaar},
        {MapID::Yatendi_Canyons},
        {MapID::Twin_Serpent_Lakes},
        {MapID::Sage_Lands},
        {MapID::Xaquang_Skyway},
        {MapID::Zehlon_Reach},
        {MapID::Tangle_Root},
        {MapID::Silverwood},
        {MapID::Zen_Daijun_explorable},
        {MapID::The_Arid_Sea},
        {MapID::Nahpui_Quarter_explorable},
        {MapID::Skyward_Reach},
        {MapID::The_Scar},
        {MapID::The_Black_Curtain},
        {MapID::Panjiang_Peninsula},
        {MapID::Snake_Dance},
        {MapID::Travelers_Vale},
        {MapID::The_Breach},
        {MapID::Lahtenda_Bog},
        {MapID::Spearhead_Peak},
        {MapID::Mount_Qinkai},
        {MapID::Marga_Coast},
        {MapID::Melandrus_Hope},
        {MapID::The_Falls},
        {MapID::Jokos_Domain},
        {MapID::Vulture_Drifts},
        {MapID::Wilderness_of_Bahdza},
        {MapID::Talmark_Wilderness},
        {MapID::Vehtendi_Valley},
        {MapID::Talus_Chute},
        {MapID::Mineral_Springs},
        {MapID::Anvil_Rock},
        {MapID::Arborstone_explorable},
        {MapID::Witmans_Folly},
        {MapID::Arkjok_Ward},
        {MapID::Ascalon_Foothills},
        {MapID::Bahdok_Caverns},
        {MapID::Cursed_Lands},
        {MapID::Alcazia_Tangle},
        {MapID::Archipelagos},
        {MapID::Eastern_Frontier},
        {MapID::Dejarin_Estate},
        {MapID::Watchtower_Coast},
        {MapID::Arbor_Bay},
        {MapID::Barbarous_Shore},
        {MapID::Deldrimor_Bowl},
        {MapID::Boreas_Seabed_explorable},
        {MapID::Cliffs_of_Dohjok},
        {MapID::Diessa_Lowlands},
        {MapID::Bukdek_Byway},
        {MapID::Bjora_Marches},
        {MapID::Crystal_Overlook},
        {MapID::Diviners_Ascent},
        {MapID::Dalada_Uplands},
        {MapID::Drazach_Thicket},
        {MapID::Fahranur_The_First_City, GW::EncStrings::Quest::ZaishenVanquish_Fahranur_the_First_City},
        {MapID::Dragons_Gullet},
        {MapID::Ferndale},
        {MapID::Forum_Highlands},
        {MapID::Dreadnoughts_Drift},
        {MapID::Drakkar_Lake},
        {MapID::Dry_Top},
        {MapID::Tears_of_the_Fallen},
        {MapID::Gyala_Hatchery},
        {MapID::Ettins_Back},
        {MapID::Gandara_the_Moon_Fortress},
        {MapID::Grothmar_Wardowns},
        {MapID::Flame_Temple_Corridor},
        {MapID::Haiju_Lagoon},
        {MapID::Frozen_Forest},
        {MapID::Garden_of_Seborhin},
        {MapID::Grenths_Footprint}
    };
    static_assert(_countof(zaishen_vanquish_cycles) == ZAISHEN_VANQUISH_COUNT);

    ZaishenQuestData zaishen_mission_cycles[] = {
        MapID::Augury_Rock_mission,
        MapID::Grand_Court_of_Sebelkeh,
        MapID::Ice_Caves_of_Sorrow,
        MapID::Raisu_Palace_outpost_mission,
        MapID::Gate_of_Desolation,
        MapID::Thirsty_River,
        MapID::Blacktide_Den,
        MapID::Against_the_Charr_mission,
        MapID::Abaddons_Mouth,
        MapID::Nundu_Bay,
        MapID::Divinity_Coast,
        MapID::Zen_Daijun_outpost_mission,
        MapID::Pogahn_Passage,
        MapID::Tahnnakai_Temple_outpost_mission,
        MapID::The_Great_Northern_Wall,
        MapID::Dasha_Vestibule,
        MapID::The_Wilds,
        MapID::Unwaking_Waters_mission,
        MapID::Chahbek_Village,
        MapID::Aurora_Glade,
        MapID::A_Time_for_Heroes_mission,
        MapID::Consulate_Docks,
        MapID::Ring_of_Fire,
        MapID::Nahpui_Quarter_outpost_mission,
        MapID::The_Dragons_Lair,
        MapID::Dzagonur_Bastion,
        MapID::DAlessio_Seaboard,
        MapID::Assault_on_the_Stronghold_mission,
        MapID::The_Eternal_Grove_outpost_mission,
        MapID::Sanctum_Cay,
        MapID::Rilohn_Refuge,
        MapID::Warband_of_brothers_mission,
        MapID::Borlis_Pass,
        MapID::Imperial_Sanctum_outpost_mission,
        MapID::Moddok_Crevice,
        MapID::Nolani_Academy,
        MapID::Destructions_Depths_mission,
        MapID::Venta_Cemetery,
        MapID::Fort_Ranik,
        MapID::A_Gate_Too_Far_mission,
        MapID::Minister_Chos_Estate_outpost_mission,
        MapID::Thunderhead_Keep,
        MapID::Tihark_Orchard,
        MapID::Finding_the_Bloodstone_mission,
        MapID::Dunes_of_Despair,
        MapID::Vizunah_Square_mission,
        MapID::Jokanur_Diggings,
        MapID::Iron_Mines_of_Moladune,
        MapID::Kodonur_Crossroads,
        {MapID::Genius_Operated_Living_Enchanted_Manifestation_mission, GW::EncStrings::Quest::ZaishenQuest_GOLEM},
        MapID::Arborstone_outpost_mission,
        MapID::Gates_of_Kryta,
        MapID::Gate_of_Madness,
        MapID::The_Elusive_Golemancer_mission,
        MapID::Riverside_Province,
        MapID::Boreas_Seabed_outpost_mission,
        MapID::Ruins_of_Morah,
        MapID::Hells_Precipice,
        MapID::Ruins_of_Surmia,
        MapID::Curse_of_the_Nornbear_mission,
        MapID::Sunjiang_District_outpost_mission,
        MapID::Elona_Reach,
        MapID::Gate_of_Pain,
        MapID::Blood_Washes_Blood_mission,
        MapID::Bloodstone_Fen,
        MapID::Jennurs_Horde,
        MapID::Gyala_Hatchery_outpost_mission,
        MapID::Abaddons_Gate,
        MapID::The_Frost_Gate
    };
    static_assert(_countof(zaishen_mission_cycles) == ZAISHEN_MISSION_COUNT);

    DailyQuests::QuestData pve_weekly_bonus_cycles[] = {{MapID::None, GW::EncStrings::ExtraLuckBonus},     {MapID::None, GW::EncStrings::ElonianSupportBonus},  {MapID::None, GW::EncStrings::ZaishenBountyBonus},
                                                        {MapID::None, GW::EncStrings::FactionsEliteBonus}, {MapID::None, GW::EncStrings::NorthernSupportBonus}, {MapID::None, GW::EncStrings::ZaishenMissionBonus},
                                                        {MapID::None, GW::EncStrings::PantheonBonus},      {MapID::None, GW::EncStrings::FactionSupportBonus},  {MapID::None, GW::EncStrings::ZaishenVanquishingBonus}};
    static_assert(_countof(pve_weekly_bonus_cycles) == WEEKLY_BONUS_PVE_COUNT);

    DailyQuests::QuestData pvp_weekly_bonus_cycles[] = {{MapID::None, GW::EncStrings::RandomArenasBonus}, {MapID::None, GW::EncStrings::GuildVersusGuildBonus}, {MapID::None, GW::EncStrings::CompetitiveMissionBonus},
                                                        {MapID::None, GW::EncStrings::HeroesAscentBonus}, {MapID::None, GW::EncStrings::CodexArenaBonus},       {MapID::None, GW::EncStrings::AllianceBattleBonus}};
    static_assert(_countof(pvp_weekly_bonus_cycles) == WEEKLY_BONUS_PVP_COUNT);

    uint32_t GetZaishenBountyIdx(const time_t* unix)
    {
        return static_cast<uint32_t>((*unix - 1244736000) / 86400 % ZAISHEN_BOUNTY_COUNT);
    }

    uint32_t GetWeeklyBonusPvEIdx(const time_t* unix)
    {
        return static_cast<uint32_t>((*unix - 1368457200) / SECONDSINAWEEK % WEEKLY_BONUS_PVE_COUNT);
    }

    uint32_t GetWeeklyBonusPvPIdx(const time_t* unix)
    {
        return static_cast<uint32_t>((*unix - 1368457200) / SECONDSINAWEEK % WEEKLY_BONUS_PVP_COUNT);
    }

    uint32_t GetZaishenCombatIdx(const time_t* unix)
    {
        return static_cast<uint32_t>((*unix - 1256227200) / 86400 % ZAISHEN_COMBAT_COUNT);
    }

    uint32_t GetZaishenMissionIdx(const time_t* unix)
    {
        return static_cast<uint32_t>((*unix - 1299168000) / 86400 % ZAISHEN_MISSION_COUNT);
    }

    uint32_t GetZaishenVanquishIdx(const time_t* unix)
    {
        return static_cast<uint32_t>((*unix - 1299168000) / 86400 % ZAISHEN_VANQUISH_COUNT);
    }

    uint32_t GetNicholasTheTravellerIdx(const time_t* unix)
    {
        return static_cast<uint32_t>((*unix - 1323097200) / SECONDSINAWEEK % NICHOLAS_POST_COUNT);
    }

    uint32_t GetNicholasSandfordIdx(const time_t* unix)
    {
        return static_cast<uint32_t>((*unix - 1239260400) / 86400 % NICHOLAS_PRE_COUNT);
    }

    uint32_t GetWantedByShiningBladeIdx(const time_t* unix)
    {
        return static_cast<uint32_t>((*unix - 1276012800) / 86400 % WANTED_COUNT);
    }

    uint32_t GetVanguardIdx(const time_t* unix)
    {
        return static_cast<uint32_t>((*unix - 1299168000) / 86400 % VANGUARD_COUNT);
    }

    time_t GetWeeklyRotationTime(const time_t* unix)
    {
        return static_cast<time_t>(floor((*unix - 1368457200) / SECONDSINAWEEK) * SECONDSINAWEEK) + 1368457200;
    }

    // Returns the start of the next rotation period after `unix`, given the cycle's epoch and period length.
    time_t GetNextRotationTime(const time_t unix, const time_t epoch, const time_t period)
    {
        return epoch + (((unix - epoch) / period) + 1) * period;
    }

    uint32_t GetWeeklyPvEBonusIdx(const time_t* unix)
    {
        return static_cast<uint32_t>((GetWeeklyRotationTime(unix) - 1368457200) / SECONDSINAWEEK % WEEKLY_BONUS_PVE_COUNT);
    }

    uint32_t GetWeeklyPvPBonusIdx(const time_t* unix)
    {
        return static_cast<uint32_t>((GetWeeklyRotationTime(unix) - 1368457200) / SECONDSINAWEEK % WEEKLY_BONUS_PVP_COUNT);
    }

    time_t GetNextEventTime(const time_t cycle_start_time, const time_t current_time, const int event_index, const int event_count, const int interval_in_seconds)
    {
        const auto cycle_duration = interval_in_seconds * event_count;
        const auto cycles_since_start = (current_time - cycle_start_time) / cycle_duration;
        const auto current_cycle_start_time = cycle_start_time + (cycles_since_start * cycle_duration);
        const auto time_in_currentCycle = event_index * interval_in_seconds;
        auto next_event_time = current_cycle_start_time + time_in_currentCycle;
        if (next_event_time < current_time) {
            // The event started in the past. We need to check if the event is ongoing or has already finished
            if (next_event_time + interval_in_seconds < current_time) {
                // The event ended already so we offset the start time with one cycle
                next_event_time += cycle_duration;
            }
            else {
                // The event is ongoing so we set the start time as the current time
                next_event_time = current_time;
            }
        }

        return next_event_time;
    }

    bool subscribed_zaishen_bounties[ZAISHEN_BOUNTY_COUNT] = {false};
    bool subscribed_zaishen_combats[ZAISHEN_COMBAT_COUNT] = {false};
    bool subscribed_zaishen_missions[ZAISHEN_MISSION_COUNT] = {false};
    bool subscribed_zaishen_vanquishes[ZAISHEN_VANQUISH_COUNT] = {false};
    bool subscribed_wanted_quests[WANTED_COUNT] = {false};
    bool subscribed_weekly_bonus_pve[WEEKLY_BONUS_PVE_COUNT] = {false};
    bool subscribed_weekly_bonus_pvp[WEEKLY_BONUS_PVP_COUNT] = {false};
    bool subscribed_vanguard[VANGUARD_COUNT] = {false};
    bool subscribed_nicholas_sandford[NICHOLAS_PRE_COUNT] = {false};

    DailyQuests::Settings settings;
    bool pending_zaishen_mission_check = false;

    uint32_t subscriptions_lookahead_days = 7;

    float text_width = 200.0f;
    int daily_quest_window_count = 90;

    bool subscriptions_changed = false;
    bool checked_subscriptions = false;
    time_t start_time;

    std::vector<std::pair<const wchar_t*, GW::Chat::ChatCommandCallback>> chat_commands;

    const ImColor subscribed_color(102, 187, 238, 255);
    const ImColor normal_color(255, 255, 255, 255);
    const ImColor incomplete_color(102, 238, 187, 255);
    const ImColor complete_color(102, 247, 126, 255);

    DailyQuests::QuestData* pending_quest_take = nullptr;

    GW::HookEntry ChatCmd_HookEntry;

    // rollover: the timestamp when this quest will be replaced by the next one.
    // Pass 0 to indicate the current quest (no date shown).
    void PrintDaily(const wchar_t* quest_type_enc, const wchar_t* quest_name_enc, const time_t rollover)
    {
        std::wstring to_send;
        std::wstring quest_name_as_link = std::format(L"\x108\x107[\x1\x2{}\x2\x108\x107;wiki:\x1\x2{}\x2\x108\x107]\x1", quest_name_enc, quest_name_enc);
        std::wstring quest_type_as_link = std::format(L"\x108\x107[\x1\x2{}\x2\x108\x107;wiki:\x1\x2{}\x2\x108\x107]\x1", quest_type_enc, quest_type_enc);
        if (rollover) {
            to_send += std::format(L"\x108\x107<quote>\x1\x2{}\x2\x108\x107, {} @ {}: \x1\x2{}", quest_type_as_link, TextUtils::RelativeTimeW(rollover), TextUtils::StringToWString(TextUtils::TimeToString(rollover)), quest_name_as_link);
        }
        else {
            to_send += std::format(L"\x108\x107<quote>\x1\x2{}\x2\x108\x107: \x1\x2{}", quest_type_as_link, quest_name_as_link);
        }
        WriteChatEnc(GW::Chat::Channel::CHANNEL_GLOBAL, to_send.c_str(), nullptr, true);
    }

    void CmdDaily(const wchar_t* quest_type, const std::function<DailyQuests::DailyQuestResult(time_t)>& get_quest_func, int argc, const LPWSTR* argv)
    {
        const time_t now = time(nullptr);
        const bool is_tomorrow = argc > 1 && !wcscmp(argv[1], L"tomorrow");
        const auto [quest, rollover] = get_quest_func(is_tomorrow ? now + 86400 : now);
        if (argc > 1 && (wcscmp(argv[1], L"take") == 0 || wcscmp(argv[1], L"travel") == 0) && quest->GetQuestGiverOutpost() != MapID::None) {
            pending_quest_take = quest;
            return;
        }
        PrintDaily(quest_type, quest->GetQuestNameEnc(), is_tomorrow ? rollover : 0);
    }

    void CHAT_CMD_FUNC(CmdWeeklyBonus)
    {
        CmdDaily(L"\x108\x107Weekly Bonus PvE\x1", DailyQuests::GetWeeklyPvEBonus, argc, argv);
        CmdDaily(L"\x108\x107Weekly Bonus PvP\x1", DailyQuests::GetWeeklyPvPBonus, argc, argv);
    }

    void CHAT_CMD_FUNC(CmdZaishenBounty)
    {
        CmdDaily(GW::EncStrings::ZaishenBounty, DailyQuests::GetZaishenBounty, argc, argv);
    }

    void CHAT_CMD_FUNC(CmdZaishenMission)
    {
        CmdDaily(GW::EncStrings::ZaishenMission, DailyQuests::GetZaishenMission, argc, argv);
    }

    void CHAT_CMD_FUNC(CmdZaishenVanquish)
    {
        CmdDaily(GW::EncStrings::ZaishenVanquish, DailyQuests::GetZaishenVanquish, argc, argv);
    }

    void CHAT_CMD_FUNC(CmdZaishenCombat)
    {
        CmdDaily(GW::EncStrings::ZaishenCombat, DailyQuests::GetZaishenCombat, argc, argv);
    }

    void CHAT_CMD_FUNC(CmdWantedByShiningBlade)
    {
        CmdDaily(GW::EncStrings::WantedByTheShiningBlade, DailyQuests::GetWantedByShiningBlade, argc, argv);
    }

    void CHAT_CMD_FUNC(CmdVanguard)
    {
        CmdDaily(L"\x108\x107Vanguard Quest\x1", DailyQuests::GetVanguardQuest, argc, argv);
    }

    void CHAT_CMD_FUNC(CmdNicholas)
    {
        const time_t now = time(nullptr);
        const bool is_tomorrow = argc > 1 && !wcscmp(argv[1], L"tomorrow");
        const time_t query_time = is_tomorrow ? now + 86400 : now;
        std::wstring buf;

        if (GW::Map::IsPreSearing()) {
            const auto [quest, rollover] = DailyQuests::GetNicholasSandford(query_time);
            buf = std::format(L"\x108\x107{} \x1\x2{}", 5, quest->GetQuestNameEnc());
            PrintDaily(L"\x108\x107Nicholas Sandford\x1", buf.c_str(), is_tomorrow ? rollover : 0);
        }
        else {
            const auto [nick, rollover] = DailyQuests::GetNicholasTheTraveller(query_time);
            buf = std::format(L"{}\x2\x108\x107 (\x1\x2{}\x2\x108\x107)\x1", nick->GetQuestNameEnc(), Resources::GetMapName(nick->map_id)->encoded());
            PrintDaily(GW::EncStrings::NicholasTheTraveller, buf.c_str(), is_tomorrow ? rollover : 0);
        }
    }

    using QuestLogNames = std::unordered_map<GW::Constants::QuestID, std::unique_ptr<GuiUtils::EncString>>;

    bool IsQuestAvailable(DailyQuests::QuestData* info)
    {
        return info && info->GetQuestGiverOutpost() != MapID::None &&
               (info == DailyQuests::GetZaishenVanquish().quest || info == DailyQuests::GetZaishenBounty().quest || info == DailyQuests::GetZaishenCombat().quest || info == DailyQuests::GetZaishenMission().quest ||
                info == DailyQuests::GetVanguardQuest().quest || info == DailyQuests::GetWantedByShiningBlade().quest);
    }

    QuestLogNames quest_log_names;

    void ClearQuestLogInfo()
    {
        quest_log_names.clear();
    }

    QuestLogNames* GetQuestLogInfo()
    {
        const auto w = GW::GetWorldContext();
        if (!w) return nullptr;
        bool processing = false;
        for (auto& entry : w->quest_log) {
            if (entry.name && !quest_log_names.contains(entry.quest_id)) {
                auto enc_string = std::make_unique<GuiUtils::EncString>();
                enc_string->reset(entry.name)->language(GW::Constants::Language::English)->wstring();
                quest_log_names[entry.quest_id] = std::move(enc_string);
            }
            if (!processing && quest_log_names[entry.quest_id]->IsDecoding()) {
                processing = true;
            }
        }
        return processing ? nullptr : &quest_log_names;
    }

    const bool wcseq(const wchar_t* a, const wchar_t* b)
    {
        return wcscmp(a, b) == 0;
    }

    const bool IsDailyQuest(const GW::Quest& quest)
    {
        return wcseq(quest.location, GW::EncStrings::ZaishenMission) || wcseq(quest.location, GW::EncStrings::ZaishenBounty) || wcseq(quest.location, GW::EncStrings::ZaishenCombat) || wcseq(quest.location, GW::EncStrings::ZaishenVanquish) ||
               wcseq(quest.location, GW::EncStrings::WantedByTheShiningBlade);
    }

    GW::Quest* GetQuestByName(const char* quest_name_english, const wchar_t* location_enc = nullptr)
    {
        const auto w = GW::GetWorldContext();
        if (!w) return nullptr;
        const auto decoded_quest_names = GetQuestLogInfo();
        if (!decoded_quest_names) return nullptr;
        for (auto& entry : w->quest_log) {
            if (!IsDailyQuest(entry)) continue;
            if (location_enc && !wcseq(entry.location, location_enc)) continue;
            if (entry.name && decoded_quest_names->at(entry.quest_id)->string() == quest_name_english) return &entry;
        }
        return nullptr;
    }

    const bool HasDailyQuest(const char* quest_name_english, const wchar_t* location_enc = nullptr)
    {
        return GetQuestByName(quest_name_english, location_enc) != nullptr;
    }

    const char* you_have_this_quest = "You have this quest in your log";

    bool OnNicholasContextMenu(void* wparam)
    {
        const auto info = (DailyQuests::NicholasCycleData*)wparam;
        ImGui::Text("Collecting %s in %s", info->GetQuestName(), info->GetMapName());

        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_Button, ImColor(0, 0, 0, 0).Value);
        const auto size = ImVec2(250.0f * ImGui::FontScale(), 0);
        ImGui::Separator();
        bool travel = ImGui::Button("Travel to nearest outpost", size);
        bool wiki = ImGui::Button("Guild Wars Wiki", size);
        const auto withdraw_amount = static_cast<uint32_t>(info->quantity * settings.nicholas_withdraw_gott_count);
        char withdraw_label[64];
        snprintf(withdraw_label, sizeof(withdraw_label), "Withdraw for %d GOTTs (%d items)", settings.nicholas_withdraw_gott_count, withdraw_amount);
        bool withdraw = ImGui::Button(withdraw_label, size);

        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        if (travel) {
            if (TravelWindow::Instance().TravelNearest(info->map_id)) return false;
        }
        if (wiki) {
            GuiUtils::SearchWiki(info->GetWikiName());
            return false;
        }
        if (withdraw) {
            InventoryManager::WithdrawItemsByName(info->enc_name.c_str(), withdraw_amount);
            return false;
        }
        return true;
    }

    bool OnDailyQuestContextMenu(void* wparam)
    {
        const auto info = (DailyQuests::QuestData*)wparam;
        const auto has_quest = HasDailyQuest(info->GetQuestName(), info->quest_location_enc);
        const auto quest_available = IsQuestAvailable(info);
        ImGui::TextUnformatted(info->GetQuestName());

        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_Button, ImColor(0, 0, 0, 0).Value);
        const auto size = ImVec2(250.0f * ImGui::FontScale(), 0);
        if (has_quest) {
            ImGui::TextColored(incomplete_color, you_have_this_quest);
        }
        ImGui::Separator();
        bool travel = false;
        if (has_quest) {
            travel = ImGui::Button("Travel to nearest outpost", size);
        }
        else if (quest_available) {
            travel = ImGui::Button("Travel to take quest", size);
        }
        const bool wiki = info->GetWikiName().empty() ? false : ImGui::Button("Guild Wars Wiki", size);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        if (travel) {
            if (has_quest) {
                if (TravelWindow::Instance().TravelNearest(info->map_id)) return false;
            }
            if (quest_available) {
                if (TravelWindow::Instance().Travel(info->GetQuestGiverOutpost())) return false;
            }
            Log::Error("Failed to travel to outpost for quest");
            return false;
        }
        if (wiki) {
            GuiUtils::SearchWiki(info->GetWikiName());
            return false;
        }
        return true;
    }

    void OnLanguageChanged(GW::Constants::Language) {}

    void OnDailyQuestTooltip(DailyQuests::QuestData* info)
    {
        const auto quest_name = info->GetQuestName();
        const auto map_name = info->map_id != MapID::None ? info->GetMapName() : nullptr;
        if (!map_name || !quest_name || strcmp(quest_name, map_name) == 0) {
            ImGui::TextUnformatted(quest_name);
        }
        else {
            ImGui::Text("%s (%s)", quest_name, map_name);
        }
        const auto chars_without_completed = CompletionWindow::GetCharactersWithoutAreaComplete(info->map_id);
        if (!chars_without_completed.empty()) {
            ImGui::Separator();
            ImGui::TextUnformatted("Characters who have not completed this area:");
            auto icon_size = ImGui::CalcTextSize(" ");
            icon_size.x = icon_size.y;
            for (auto char_completion : chars_without_completed) {
                ImGui::Image(*Resources::GetProfessionIcon(char_completion->profession), icon_size);
                ImGui::SameLine();
                ImGui::TextUnformatted(char_completion->name_str.c_str());
            }
        }
    }

    GW::HookEntry OnUIMessage_HookEntry;

    bool IsZaishenMissionBonusActive(time_t unix)
    {
        const auto pve_idx = GetWeeklyPvEBonusIdx(&unix);
        return pve_weekly_bonus_cycles[pve_idx].enc_name == GW::EncStrings::ZaishenMissionBonus;
    }

    bool IsZaishenMissionOutpost(GW::Constants::MapID current_map, GW::Constants::MapID zaishen_map)
    {
        if (current_map == zaishen_map) return true;
        // Factions joint missions don't have a single entry outpost. Hardcode the entry outposts
        // for each one. (AreaInfo has a mission_maps_to field that could in principle replace
        // this, but it's not used anywhere else in the codebase and is untested.)
        switch (zaishen_map) {
            case MapID::Vizunah_Square_mission:
                return current_map == MapID::Vizunah_Square_Local_Quarter_outpost
                       || current_map == MapID::Vizunah_Square_Foreign_Quarter_outpost;
            case MapID::Unwaking_Waters_mission:
                return current_map == MapID::Cavalon_outpost
                       || current_map == MapID::House_zu_Heltzer_outpost;
            default:
                return false;
        }
    }

    void OnMapLoaded_CheckZaishenMission()
    {
        if (!settings.notify_zaishen_mission_outpost) return;
        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost) return;
        if (GW::Map::IsPreSearing()) return;

        const time_t now = time(nullptr);
        const auto zm_result = DailyQuests::GetZaishenMission(now);
        if (!zm_result.quest) return;
        const auto current_map = GW::Map::GetMapID();
        if (!IsZaishenMissionOutpost(current_map, zm_result.quest->map_id)) return;

        const bool has_quest = HasDailyQuest(zm_result.quest->GetQuestName(), GW::EncStrings::ZaishenMission);
        const bool bonus_active = IsZaishenMissionBonusActive(now);
        const uint32_t bonus_mult = bonus_active ? 2 : 1;

        const DailyQuests::ZaishenCoinReward* reward = nullptr;
        if (const auto* quest_id = ZaishenMissionMaps::GetQuestID(zm_result.quest->map_id)) {
            reward = DailyQuests::GetZaishenCoinReward(*quest_id);
        }

        Log::Flash("This is today's Zaishen Mission: %s", zm_result.quest->GetQuestName());
        if (reward) {
            const auto bonus_suffix = bonus_active ? " (Zaishen Mission Bonus week: 2x)" : "";
            Log::Info("Zaishen Coin reward: %u (NM) / %u (HM)%s", reward->nm * bonus_mult, reward->hm * bonus_mult, bonus_suffix);
        }
        if (!has_quest) {
            Log::Info("You don't have this quest yet — use \"/zm take\" to travel to Embark Beach and pick it up.");
        }
    }

    void OnUIMessage(GW::HookStatus*, GW::UI::UIMessage message_id, void* wparam, void*)
    {
        switch (message_id) {
            case GW::UI::UIMessage::kPreferenceValueChanged: {
                const auto packet = (GW::UI::UIPacket::kPreferenceValueChanged*)wparam;
                if (packet->preference_id == GW::UI::NumberPreference::Language) OnLanguageChanged((GW::Constants::Language)packet->new_value);
                break;
            }
            case GW::UI::UIMessage::kMapLoaded:
                pending_zaishen_mission_check = true;
                break;
            default:
                break;
        }
    }

    size_t GetNicholasSandfordCollectedQuantity(DailyQuests::QuestData* quest_data)
    {
        static clock_t last_inv_check_sandford = 0;

        // If the map is empty, populate it with unique keys from the cycle array
        if (nicholas_sandford_item_collected_count.empty()) {
            for (auto& item : nicholas_sandford_cycles) {
                nicholas_sandford_item_collected_count[item.enc_name] = 0;
            }
        }

        if (!last_inv_check_sandford || TIMER_DIFF(last_inv_check_sandford) > 10000) {
            // Check inventory for each Nicholas Sandford item
            for (const auto& [enc_name, count] : nicholas_sandford_item_collected_count) {
                nicholas_sandford_item_collected_count[enc_name] = InventoryManager::CountItemsByName(enc_name.c_str());
            }
            last_inv_check_sandford = TIMER_INIT();
        }

        const auto found = nicholas_sandford_item_collected_count.find(quest_data->GetQuestNameEnc());
        ASSERT(found != nicholas_sandford_item_collected_count.end());
        return found->second;
    }
    const std::unordered_map<GW::Constants::QuestID, DailyQuests::ZaishenCoinReward> zaishen_coin_rewards = {
        // ZaishenMission
        {QuestID::ZaishenMission_The_Great_Northern_Wall, {15, 74}},
        {QuestID::ZaishenMission_Fort_Ranik, {15, 74}},
        {QuestID::ZaishenMission_Ruins_of_Surmia, {15, 74}},
        {QuestID::ZaishenMission_Nolani_Academy, {15, 74}},
        {QuestID::ZaishenMission_Borlis_Pass, {15, 74}},
        {QuestID::ZaishenMission_The_Frost_Gate, {15, 74}},
        {QuestID::ZaishenMission_Gates_of_Kryta, {15, 74}},
        {QuestID::ZaishenMission_DAlessio_Seaboard, {20, 100}},
        {QuestID::ZaishenMission_Divinity_Coast, {20, 100}},
        {QuestID::ZaishenMission_The_Wilds, {20, 100}},
        {QuestID::ZaishenMission_Bloodstone_Fen, {20, 100}},
        {QuestID::ZaishenMission_Aurora_Glade, {20, 100}},
        {QuestID::ZaishenMission_Riverside_Province, {20, 100}},
        {QuestID::ZaishenMission_Sanctum_Cay, {20, 100}},
        {QuestID::ZaishenMission_Dunes_of_Despair, {20, 100}},
        {QuestID::ZaishenMission_Thirsty_River, {20, 100}},
        {QuestID::ZaishenMission_Elona_Reach, {20, 100}},
        {QuestID::ZaishenMission_Augury_Rock, {20, 100}},
        {QuestID::ZaishenMission_The_Dragons_Lair, {20, 100}},
        {QuestID::ZaishenMission_Ice_Caves_of_Sorrow, {30, 150}},
        {QuestID::ZaishenMission_Iron_Mines_of_Moladune, {30, 150}},
        {QuestID::ZaishenMission_Thunderhead_Keep, {30, 150}},
        {QuestID::ZaishenMission_Ring_of_Fire, {30, 150}},
        {QuestID::ZaishenMission_Abaddons_Mouth, {30, 150}},
        {QuestID::ZaishenMission_Hells_Precipice, {30, 150}},
        {QuestID::ZaishenMission_Zen_Daijun, {15, 74}},
        {QuestID::ZaishenMission_Vizunah_Square, {20, 100}},
        {QuestID::ZaishenMission_Nahpui_Quarter, {20, 100}},
        {QuestID::ZaishenMission_Tahnnakai_Temple, {20, 100}},
        {QuestID::ZaishenMission_Arborstone, {30, 150}},
        {QuestID::ZaishenMission_Boreas_Seabed, {30, 150}},
        {QuestID::ZaishenMission_Sunjiang_District, {30, 150}},
        {QuestID::ZaishenMission_The_Eternal_Grove, {30, 150}},
        {QuestID::ZaishenMission_Unwaking_Waters, {30, 150}},
        {QuestID::ZaishenMission_Gyala_Hatchery, {30, 150}},
        {QuestID::ZaishenMission_Raisu_Palace, {30, 150}},
        {QuestID::ZaishenMission_Imperial_Sanctum, {30, 150}},
        {QuestID::ZaishenMission_Chahbek_Village, {15, 74}},
        {QuestID::ZaishenMission_Jokanur_Diggings, {15, 74}},
        {QuestID::ZaishenMission_Blacktide_Den, {15, 74}},
        {QuestID::ZaishenMission_Consulate_Docks, {15, 74}},
        {QuestID::ZaishenMission_Venta_Cemetery, {20, 100}},
        {QuestID::ZaishenMission_Kodonur_Crossroads, {20, 100}},
        {QuestID::ZaishenMission_Rilohn_Refuge, {20, 100}},
        {QuestID::ZaishenMission_Moddok_Crevice, {20, 100}},
        {QuestID::ZaishenMission_Tihark_Orchard, {20, 100}},
        {QuestID::ZaishenMission_Dzagonur_Bastion, {30, 150}},
        {QuestID::ZaishenMission_Dasha_Vestibule, {30, 150}},
        {QuestID::ZaishenMission_Grand_Court_of_Sebelkeh, {30, 150}},
        {QuestID::ZaishenMission_Jennurs_Horde, {30, 150}},
        {QuestID::ZaishenMission_Nundu_Bay, {30, 150}},
        {QuestID::ZaishenMission_Gate_of_Desolation, {30, 150}},
        {QuestID::ZaishenMission_Ruins_of_Morah, {30, 150}},
        {QuestID::ZaishenMission_Gate_of_Pain, {30, 150}},
        {QuestID::ZaishenMission_Gate_of_Madness, {30, 150}},
        {QuestID::ZaishenMission_Abaddons_Gate, {30, 150}},
        {QuestID::ZaishenMission_Finding_the_Bloodstone, {30, 105}},
        {QuestID::ZaishenMission_The_Elusive_Golemancer, {30, 105}},
        {QuestID::ZaishenMission_G_O_L_E_M, {40, 140}},
        {QuestID::ZaishenMission_Against_the_Charr, {30, 105}},
        {QuestID::ZaishenMission_Warband_of_Brothers, {30, 105}},
        {QuestID::ZaishenMission_Assault_on_the_Stronghold, {40, 140}},
        {QuestID::ZaishenMission_Curse_of_the_Nornbear, {30, 105}},
        {QuestID::ZaishenMission_Blood_Washes_Blood, {30, 105}},
        {QuestID::ZaishenMission_A_Gate_Too_Far, {40, 140}},
        {QuestID::ZaishenMission_Destructions_Depths, {40, 140}},
        {QuestID::ZaishenMission_A_Time_for_Heroes, {40, 140}},
        {QuestID::ZaishenMission_Minister_Chos_Estate, {15, 74}},
        {QuestID::ZaishenMission_Pogahn_Passage, {30, 150}},

        // ZaishenBounty
        {QuestID::ZaishenBounty_Urgoz, {60, 210}},
        {QuestID::ZaishenBounty_Chung_The_Attuned, {20, 70}},
        {QuestID::ZaishenBounty_Mungri_Magicbox, {20, 70}},
        {QuestID::ZaishenBounty_The_Stygian_Lords, {40, 140}},
        {QuestID::ZaishenBounty_Ilsundur_Lord_of_Fire, {40, 140}},
        {QuestID::ZaishenBounty_Rragar_Maneater, {40, 140}},
        {QuestID::ZaishenBounty_Murakai_Lady_of_the_Night, {40, 140}},
        {QuestID::ZaishenBounty_Prismatic_Ooze, {30, 105}},
        {QuestID::ZaishenBounty_Havok_Soulwail, {40, 140}},
        {QuestID::ZaishenBounty_Frostmaw_the_Kinslayer, {40, 140}},
        {QuestID::ZaishenBounty_Remnant_of_Antiquities, {40, 140}},
        {QuestID::ZaishenBounty_Plague_of_Destruction, {40, 140}},
        {QuestID::ZaishenBounty_Zoldark_the_Unholy, {40, 140}},
        {QuestID::ZaishenBounty_Khabuus, {40, 140}},
        {QuestID::ZaishenBounty_Zhim_Monns, {40, 140}},
        {QuestID::ZaishenBounty_Eldritch_Ettin, {40, 140}},
        {QuestID::ZaishenBounty_Fendi_Nin, {40, 140}},
        {QuestID::ZaishenBounty_TPS_Regulator_Golem, {40, 140}},
        {QuestID::ZaishenBounty_Arachni, {40, 140}},
        {QuestID::ZaishenBounty_Forgewight, {40, 140}},
        {QuestID::ZaishenBounty_Selvetarm, {40, 140}},
        {QuestID::ZaishenBounty_Justiciar_Thommis, {40, 140}},
        {QuestID::ZaishenBounty_Rand_Stormweaver, {40, 140}},
        {QuestID::ZaishenBounty_Duncan_the_Black, {60, 210}},
        {QuestID::ZaishenBounty_Fronis_Irontoe, {20, 70}},
        {QuestID::ZaishenBounty_Magmus, {40, 140}},
        {QuestID::ZaishenBounty_Lord_Khobay, {40, 140}},

        // ZaishenVanquish
        {QuestID::ZaishenVanquish_Dejarin_Estate, {150, 150}},
        {QuestID::ZaishenVanquish_Watchtower_Coast, {50, 50}},
        {QuestID::ZaishenVanquish_Arbor_Bay, {250, 250}},
        {QuestID::ZaishenVanquish_Barbarous_Shore, {150, 150}},
        {QuestID::ZaishenVanquish_Deldrimor_Bowl, {150, 150}},
        {QuestID::ZaishenVanquish_Boreas_Seabed, {150, 150}},
        {QuestID::ZaishenVanquish_Cliffs_of_Dohjok, {50, 50}},
        {QuestID::ZaishenVanquish_Diessa_Lowlands, {150, 150}},
        {QuestID::ZaishenVanquish_Bukdek_Byway, {150, 150}},
        {QuestID::ZaishenVanquish_Bjora_Marches, {150, 150}},
        {QuestID::ZaishenVanquish_Crystal_Overlook, {150, 150}},
        {QuestID::ZaishenVanquish_Diviners_Ascent, {50, 50}},
        {QuestID::ZaishenVanquish_Dalada_Uplands, {250, 250}},
        {QuestID::ZaishenVanquish_Drazach_Thicket, {150, 150}},
        {QuestID::ZaishenVanquish_Fahranur_the_First_City, {50, 50}},
        {QuestID::ZaishenVanquish_Dragons_Gullet, {250, 250}},
        {QuestID::ZaishenVanquish_Ferndale, {250, 250}},
        {QuestID::ZaishenVanquish_Forum_Highlands, {150, 150}},
        {QuestID::ZaishenVanquish_Dreadnoughts_Drift, {250, 250}},
        {QuestID::ZaishenVanquish_Drakkar_Lake, {250, 250}},
        {QuestID::ZaishenVanquish_Dry_Top, {50, 50}},
        {QuestID::ZaishenVanquish_Tears_of_the_Fallen, {50, 50}},
        {QuestID::ZaishenVanquish_Gyala_Hatchery, {150, 150}},
        {QuestID::ZaishenVanquish_Ettins_Back, {50, 50}},
        {QuestID::ZaishenVanquish_Gandara_the_Moon_Fortress, {50, 50}},
        {QuestID::ZaishenVanquish_Grothmar_Wardowns, {150, 150}},
        {QuestID::ZaishenVanquish_Flame_Temple_Corridor, {50, 50}},
        {QuestID::ZaishenVanquish_Haiju_Lagoon, {150, 150}},
        {QuestID::ZaishenVanquish_Frozen_Forest, {150, 150}},
        {QuestID::ZaishenVanquish_Garden_of_Seborhin, {150, 150}},
        {QuestID::ZaishenVanquish_Grenths_Footprint, {150, 150}},
        {QuestID::ZaishenVanquish_Jaya_Bluffs, {150, 150}},
        {QuestID::ZaishenVanquish_Holdings_of_Chokhin, {150, 150}},
        {QuestID::ZaishenVanquish_Ice_Cliff_Chasms, {150, 150}},
        {QuestID::ZaishenVanquish_Griffons_Mouth, {50, 50}},
        {QuestID::ZaishenVanquish_Kinya_Province, {50, 50}},
        {QuestID::ZaishenVanquish_Issnur_Isles, {150, 150}},
        {QuestID::ZaishenVanquish_Jaga_Moraine, {250, 250}},
        {QuestID::ZaishenVanquish_Ice_Floe, {150, 150}},
        {QuestID::ZaishenVanquish_Maishang_Hills, {150, 150}},
        {QuestID::ZaishenVanquish_Jahai_Bluffs, {250, 250}},
        {QuestID::ZaishenVanquish_Riven_Earth, {250, 250}},
        {QuestID::ZaishenVanquish_Icedome, {250, 250}},
        {QuestID::ZaishenVanquish_Minister_Chos_Estate, {50, 50}},
        {QuestID::ZaishenVanquish_Mehtani_Keys, {150, 150}},
        {QuestID::ZaishenVanquish_Sacnoth_Valley, {250, 250}},
        {QuestID::ZaishenVanquish_Iron_Horse_Mine, {150, 150}},
        {QuestID::ZaishenVanquish_Morostav_Trail, {250, 250}},
        {QuestID::ZaishenVanquish_Plains_of_Jarin, {150, 150}},
        {QuestID::ZaishenVanquish_Sparkfly_Swamp, {250, 250}},
        {QuestID::ZaishenVanquish_Kessex_Peak, {150, 150}},
        {QuestID::ZaishenVanquish_Mourning_Veil_Falls, {150, 150}},
        {QuestID::ZaishenVanquish_The_Alkali_Pan, {250, 250}},
        {QuestID::ZaishenVanquish_Varajar_Fells, {250, 250}},
        {QuestID::ZaishenVanquish_Lornars_Pass, {250, 250}},
        {QuestID::ZaishenVanquish_Pongmei_Valley, {150, 150}},
        {QuestID::ZaishenVanquish_The_Floodplain_of_Mahnkelon, {150, 150}},
        {QuestID::ZaishenVanquish_Verdant_Cascades, {150, 150}},
        {QuestID::ZaishenVanquish_Majestys_Rest, {150, 150}},
        {QuestID::ZaishenVanquish_Raisu_Palace, {150, 150}},
        {QuestID::ZaishenVanquish_The_Hidden_City_of_Ahdashim, {150, 150}},
        {QuestID::ZaishenVanquish_Rheas_Crater, {150, 150}},
        {QuestID::ZaishenVanquish_Mamnoon_Lagoon, {50, 50}},
        {QuestID::ZaishenVanquish_Shadows_Passage, {50, 50}},
        {QuestID::ZaishenVanquish_The_Mirror_of_Lyss, {150, 150}},
        {QuestID::ZaishenVanquish_Saoshang_Trail, {50, 50}},
        {QuestID::ZaishenVanquish_Nebo_Terrace, {150, 150}},
        {QuestID::ZaishenVanquish_Shenzun_Tunnels, {150, 150}},
        {QuestID::ZaishenVanquish_The_Ruptured_Heart, {150, 150}},
        {QuestID::ZaishenVanquish_Salt_Flats, {150, 150}},
        {QuestID::ZaishenVanquish_North_Kryta_Province, {150, 150}},
        {QuestID::ZaishenVanquish_Silent_Surf, {150, 150}},
        {QuestID::ZaishenVanquish_The_Shattered_Ravines, {250, 250}},
        {QuestID::ZaishenVanquish_Scoundrels_Rise, {50, 50}},
        {QuestID::ZaishenVanquish_Old_Ascalon, {150, 150}},
        {QuestID::ZaishenVanquish_Sunjiang_District, {150, 150}},
        {QuestID::ZaishenVanquish_The_Sulphurous_Wastes, {250, 250}},
        {QuestID::ZaishenVanquish_Magus_Stones, {250, 250}},
        {QuestID::ZaishenVanquish_Perdition_Rock, {250, 250}},
        {QuestID::ZaishenVanquish_Sunqua_Vale, {50, 50}},
        {QuestID::ZaishenVanquish_Turais_Procession, {250, 250}},
        {QuestID::ZaishenVanquish_Norrhart_Domains, {150, 150}},
        {QuestID::ZaishenVanquish_Pockmark_Flats, {50, 50}},
        {QuestID::ZaishenVanquish_Tahnnakai_Temple, {150, 150}},
        {QuestID::ZaishenVanquish_Vehjin_Mines, {150, 150}},
        {QuestID::ZaishenVanquish_Poisoned_Outcrops, {250, 250}},
        {QuestID::ZaishenVanquish_Prophets_Path, {250, 250}},
        {QuestID::ZaishenVanquish_The_Eternal_Grove, {150, 150}},
        {QuestID::ZaishenVanquish_Tascas_Demise, {50, 50}},
        {QuestID::ZaishenVanquish_Respendent_Makuun, {150, 150}},
        {QuestID::ZaishenVanquish_Reed_Bog, {50, 50}},
        {QuestID::ZaishenVanquish_Unwaking_Waters, {150, 150}},
        {QuestID::ZaishenVanquish_Stingray_Strand, {150, 150}},
        {QuestID::ZaishenVanquish_Sunward_Marches, {150, 150}},
        {QuestID::ZaishenVanquish_Regent_Valley, {50, 50}},
        {QuestID::ZaishenVanquish_Wajjun_Bazaar, {150, 150}},
        {QuestID::ZaishenVanquish_Yatendi_Canyons, {150, 150}},
        {QuestID::ZaishenVanquish_Twin_Serpent_Lakes, {150, 150}},
        {QuestID::ZaishenVanquish_Sage_Lands, {150, 150}},
        {QuestID::ZaishenVanquish_Xaquang_Skyway, {150, 150}},
        {QuestID::ZaishenVanquish_Zehlon_Reach, {50, 50}},
        {QuestID::ZaishenVanquish_Tangle_Root, {150, 150}},
        {QuestID::ZaishenVanquish_Silverwood, {150, 150}},
        {QuestID::ZaishenVanquish_Zen_Daijun, {150, 150}},
        {QuestID::ZaishenVanquish_The_Arid_Sea, {150, 150}},
        {QuestID::ZaishenVanquish_Nahpui_Quarter, {150, 150}},
        {QuestID::ZaishenVanquish_Skyward_Reach, {150, 150}},
        {QuestID::ZaishenVanquish_The_Scar, {250, 250}},
        {QuestID::ZaishenVanquish_The_Black_Curtain, {150, 150}},
        {QuestID::ZaishenVanquish_Panjiang_Peninsula, {150, 150}},
        {QuestID::ZaishenVanquish_Snake_Dance, {250, 250}},
        {QuestID::ZaishenVanquish_Travelers_Vale, {150, 150}},
        {QuestID::ZaishenVanquish_The_Breach, {150, 150}},
        {QuestID::ZaishenVanquish_Lahtenda_Bog, {250, 250}},
        {QuestID::ZaishenVanquish_Spearhead_Peak, {150, 150}},
    };

} // namespace

const MapID ZaishenQuestData::GetQuestGiverOutpost()
{
    const auto _map_id = MapID::Great_Temple_of_Balthazar_outpost;
    if (GW::Map::GetIsMapUnlocked(_map_id)) return _map_id;
    return MapID::Embark_Beach;
}

const MapID ZaishenVanquishQuestData::GetQuestGiverOutpost()
{
    return MapID::Embark_Beach;
}

const MapID WantedQuestData::GetQuestGiverOutpost()
{
    return MapID::Lions_Arch_outpost;
}

void DailyQuests::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }

    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        return ImGui::End();
    }
    const float short_text_width = 120.0f * ImGui::FontScale();
    const float long_text_width = text_width * ImGui::FontScale();
    const float zm_width = 170.0f * ImGui::FontScale();
    const float zb_width = 185.0f * ImGui::FontScale();
    const float zc_width = 135.0f * ImGui::FontScale();
    const float zv_width = 200.0f * ImGui::FontScale();
    const float ws_width = 180.0f * ImGui::FontScale();
    const float nicholas_width = 180.0f * ImGui::FontScale();
    const float wbe_width = 145.0f * ImGui::FontScale();
    const float vanguard_width = 180.0f * ImGui::FontScale();
    const float sandford_width = 200.0f * ImGui::FontScale();

    const bool is_pre = GW::Map::IsPreSearing();

    // Checkbox in top-right corner
    const char* other_label = is_pre ? "Show post searing dailies" : "Show pre searing dailies";
    const float checkbox_w = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(other_label).x;
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - checkbox_w - ImGui::GetStyle().WindowPadding.x);
    ImGui::Checkbox(other_label, &settings.show_other_searing_dailies);

    auto write_daily_info = [](bool* subscribed, QuestData* info, bool check_completion) {
        auto col = &normal_color;
        if (check_completion && !CompletionWindow::IsAreaComplete(GW::AccountMgr::GetCurrentPlayerName(), info->map_id)) col = &incomplete_color;
        if (*subscribed) col = &subscribed_color;
        ImGui::TextColored(*col, info->GetQuestName());
        auto lmb_clicked = ImGui::IsItemClicked();
        auto rmb_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);
        const auto hovered = ImGui::IsItemHovered();
        if (HasDailyQuest(info->GetQuestName(), info->quest_location_enc)) {
            ImGui::SameLine();
            ImGui::TextColored(incomplete_color, ICON_FA_EXCLAMATION);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(you_have_this_quest);
            }
            lmb_clicked |= ImGui::IsItemClicked();
            rmb_clicked |= ImGui::IsItemClicked(ImGuiMouseButton_Right);
        }
        if (rmb_clicked) {
            ImGui::SetContextMenu(OnDailyQuestContextMenu, info);
        }
        if (lmb_clicked) {
            *subscribed = !*subscribed;
        }
        if (hovered && check_completion) {
            ImGui::SetTooltip([info]() {
                OnDailyQuestTooltip(info);
            });
        }
    };

    struct ColumnDef {
        const char* header;
        float width;
        std::function<void(time_t)> draw;
    };

    std::vector<ColumnDef> columns;

    auto add_pre_cols = [&]() {
        columns.push_back({"Vanguard Quest", vanguard_width, [&](time_t t) {
            write_daily_info(&subscribed_vanguard[GetVanguardIdx(&t)], GetVanguardQuest(t).quest, false);
        }});
        columns.push_back({"Nicholas Sandford", sandford_width, [&](time_t t) {
            const auto si = GetNicholasSandfordIdx(&t);
            const bool prev = subscribed_nicholas_sandford[si];
            const auto sandford_quest = GetNicholasSandford(t).quest;
            write_daily_info(&subscribed_nicholas_sandford[si], sandford_quest, false);
            const auto collected = GetNicholasSandfordCollectedQuantity(sandford_quest);
            if (collected > 0) {
                ImGui::SameLine(0, 0);
                const ImColor* col = &normal_color;
                if (collected >= 5) col = &incomplete_color;
                if (collected >= 25) col = &complete_color;
                ImGui::TextColored(*col, " (%d/5)", static_cast<int>(collected));
            }
            if (subscribed_nicholas_sandford[si] != prev) {
                for (size_t j = 0; j < NICHOLAS_PRE_COUNT; ++j) {
                    if (nicholas_sandford_cycles[j].GetQuestNameEnc() && wcscmp(nicholas_sandford_cycles[j].GetQuestNameEnc(), sandford_quest->GetQuestNameEnc()) == 0)
                        subscribed_nicholas_sandford[j] = subscribed_nicholas_sandford[si];
                }
            }
        }});
    };

    auto add_post_cols = [&]() {
        if (settings.show_zaishen_missions_in_window)
            columns.push_back({"Zaishen Mission", zm_width, [&](time_t t) {
                write_daily_info(&subscribed_zaishen_missions[GetZaishenMissionIdx(&t)], GetZaishenMission(t).quest, true);
            }});
        if (settings.show_zaishen_bounty_in_window)
            columns.push_back({"Zaishen Bounty", zb_width, [&](time_t t) {
                write_daily_info(&subscribed_zaishen_bounties[GetZaishenBountyIdx(&t)], GetZaishenBounty(t).quest, true);
            }});
        if (settings.show_zaishen_combat_in_window)
            columns.push_back({"Zaishen Combat", zc_width, [&](time_t t) {
                write_daily_info(&subscribed_zaishen_combats[GetZaishenCombatIdx(&t)], GetZaishenCombat(t).quest, false);
            }});
        if (settings.show_zaishen_vanquishes_in_window)
            columns.push_back({"Zaishen Vanquish", zv_width, [&](time_t t) {
                write_daily_info(&subscribed_zaishen_vanquishes[GetZaishenVanquishIdx(&t)], GetZaishenVanquish(t).quest, true);
            }});
        if (settings.show_wanted_quests_in_window)
            columns.push_back({"Wanted", ws_width, [&](time_t t) {
                write_daily_info(&subscribed_wanted_quests[GetWantedByShiningBladeIdx(&t)], GetWantedByShiningBlade(t).quest, false);
            }});
        if (settings.show_nicholas_in_window)
            columns.push_back({"Nicholas the Traveler", nicholas_width, [&](time_t t) {
                const auto nick = static_cast<NicholasCycleData*>(GetNicholasTheTraveller(t).quest);
                ImGui::TextUnformatted(nick->GetQuestName());
                const auto rmb_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);
                const auto hovered = ImGui::IsItemHovered();
                const auto collected = nick->GetCollectedQuantity();
                if (collected > 0) {
                    ImGui::SameLine();
                    const ImColor* col = &normal_color;
                    if (collected >= nick->quantity) col = &incomplete_color;
                    ImGui::TextColored(*col, "(%d/%d)", collected, nick->quantity);
                }
                if (rmb_clicked) ImGui::SetContextMenu(OnNicholasContextMenu, nick);
                if (hovered) ImGui::SetTooltip("%s in %s", nick->GetQuestName(), nick->GetMapName());
            }});
        if (settings.show_weekly_bonus_pve_in_window)
            columns.push_back({"Weekly Bonus PvE", wbe_width, [&](time_t t) {
                const auto i = GetWeeklyBonusPvEIdx(&t);
                write_daily_info(&subscribed_weekly_bonus_pve[i], &pve_weekly_bonus_cycles[i], false);
            }});
        if (settings.show_weekly_bonus_pvp_in_window)
            columns.push_back({"Weekly Bonus PvP", long_text_width, [&](time_t t) {
                const auto i = GetWeeklyBonusPvPIdx(&t);
                write_daily_info(&subscribed_weekly_bonus_pvp[i], &pvp_weekly_bonus_cycles[i], false);
            }});
    };

    if (is_pre) {
        add_pre_cols();
        if (settings.show_other_searing_dailies) add_post_cols();
    }
    else {
        add_post_cols();
        if (settings.show_other_searing_dailies) add_pre_cols();
    }

    // Header row
    float offset = 0.0f;
    ImGui::Text("Date");
    ImGui::SameLine(offset += short_text_width);
    for (const auto& col : columns) {
        ImGui::Text(col.header);
        ImGui::SameLine(offset += col.width);
    }
    ImGui::NewLine();
    ImGui::Separator();

    ImGui::BeginChild("dailies_scroll", ImVec2(0, -1 * (40.0f * ImGui::FontScale()) - ImGui::GetStyle().ItemInnerSpacing.y));
    time_t unix = time(nullptr);

    for (size_t i = 0; i < static_cast<size_t>(daily_quest_window_count); i++) {
        offset = 0.0f;
        switch (i) {
            case 0:
                ImGui::Text("Today");
                break;
            case 1:
                ImGui::Text("Tomorrow");
                break;
            default:
                char mbstr[100];
                std::strftime(mbstr, sizeof(mbstr), "%a %d %b", std::localtime(&unix));
                ImGui::Text(mbstr);
                break;
        }
        ImGui::SameLine(offset += short_text_width);
        for (const auto& col : columns) {
            col.draw(unix);
            ImGui::SameLine(offset += col.width);
        }
        ImGui::NewLine();
        unix += 86400;
    }

    ImGui::EndChild();
    ImGui::TextDisabled("Click on a daily quest to get notified when its coming up.");

    ImGui::TextDisabled("Subscribed quests are highlighted in ");
    ImGui::SameLine(0, 0);
    ImGui::TextColored(subscribed_color, "this color");
    ImGui::SameLine(0, 0);
    ImGui::TextDisabled(".");
    if (!is_pre) {
        ImGui::SameLine(0, 0);
        ImGui::TextDisabled(" Areas that you haven't completed on this player are highlighted in ");
        ImGui::SameLine(0, 0);
        ImGui::TextColored(incomplete_color, "this color");
        ImGui::SameLine(0, 0);
        ImGui::TextDisabled(".");
    }

    return ImGui::End();
}

void DailyQuests::DrawHelp()
{
    if (!ImGui::TreeNodeEx("Daily Quest Chat Commands", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        return;
    }
    ImGui::Text("You can create a 'Send Chat' hotkey to perform any command.");
    ImGui::Bullet();
    ImGui::Text("'/zb' prints current zaishen bounty.");
    ImGui::Bullet();
    ImGui::Text("'/zm' prints current zaishen mission.");
    ImGui::Bullet();
    ImGui::Text("'/zv' prints current zaishen vanquish.");
    ImGui::Bullet();
    ImGui::Text("'/zc' prints current zaishen combat.");
    ImGui::Bullet();
    ImGui::Text("'/vanguard' prints current pre-searing vanguard quest.");
    ImGui::Bullet();
    ImGui::Text("'/wanted' prints current shining blade bounty.");
    ImGui::Bullet();
    ImGui::Text("'/nicholas' prints current nicholas location.");
    ImGui::Bullet();
    ImGui::Text("'/weekly' prints current weekly bonus.");
    ImGui::Bullet();
    ImGui::Text("'/today' prints current daily activities.");
    ImGui::Bullet();
    ImGui::Text("'/tomorrow' prints tomorrow's daily activities.");
    ImGui::TreePop();
}

void DailyQuests::DrawSettingsInternal()
{
    ToolboxWindow::DrawSettingsInternal();
    ImGui::PushItemWidth(200.f * ImGui::FontScale());
    ImGui::InputInt("Show daily quests for the next N days", &daily_quest_window_count);
    ImGui::InputInt("Number of GOTTs to withdraw items for (Nicholas)", &settings.nicholas_withdraw_gott_count);
    ImGui::PopItemWidth();
    ImGui::Text("Quests to show in Daily Quests window:");
    ImGui::Indent();
    ImGui::StartSpacedElements(200.f);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Zaishen Bounty", &settings.show_zaishen_bounty_in_window);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Zaishen Combat", &settings.show_zaishen_combat_in_window);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Zaishen Mission", &settings.show_zaishen_missions_in_window);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Zaishen Vanquish", &settings.show_zaishen_vanquishes_in_window);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Wanted by Shining Blade", &settings.show_wanted_quests_in_window);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Nicholas The Traveler", &settings.show_nicholas_in_window);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Weekly Bonus (PvE)", &settings.show_weekly_bonus_pve_in_window);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Weekly Bonus (PvP)", &settings.show_weekly_bonus_pvp_in_window);

    ImGui::Unindent();

    ImGui::Checkbox("Alert when entering today's Zaishen Mission outpost", &settings.notify_zaishen_mission_outpost);
    ImGui::ShowHelp("Shows a flash message in chat with the mission name and coin reward when you enter the outpost that matches today's Zaishen Mission.");
}

namespace {
    // Subscriptions keep their legacy bitset-string encoding in JSON ("0101..."), same as the INI.
    template <size_t N>
    void LoadSubscriptions(const SettingsDoc& doc, const ToolboxIni* legacy, const char* section, const char* key, bool (&out)[N])
    {
        std::string value;
        if (!doc.Get(section, key, value)) {
            value = legacy->GetValue(section, key, "0");
        }
        const std::bitset<N> bits(value);
        for (auto i = 0u; i < bits.size(); i++) {
            out[i] = bits[i] == 1;
        }
    }

    template <size_t N>
    void SaveSubscriptions(SettingsDoc& doc, const char* section, const char* key, const bool (&in)[N])
    {
        std::bitset<N> bits;
        for (auto i = 0u; i < bits.size(); i++) {
            bits[i] = in[i] ? 1 : 0;
        }
        doc.Set(section, key, bits.to_string());
    }
}

void DailyQuests::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWindow::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);

    LoadSubscriptions(doc, legacy, Name(), VAR_NAME(subscribed_zaishen_missions), subscribed_zaishen_missions);
    LoadSubscriptions(doc, legacy, Name(), VAR_NAME(subscribed_zaishen_bounties), subscribed_zaishen_bounties);
    LoadSubscriptions(doc, legacy, Name(), VAR_NAME(subscribed_zaishen_combats), subscribed_zaishen_combats);
    LoadSubscriptions(doc, legacy, Name(), VAR_NAME(subscribed_zaishen_vanquishes), subscribed_zaishen_vanquishes);
    LoadSubscriptions(doc, legacy, Name(), VAR_NAME(subscribed_wanted_quests), subscribed_wanted_quests);
    LoadSubscriptions(doc, legacy, Name(), VAR_NAME(subscribed_weekly_bonus_pve), subscribed_weekly_bonus_pve);
    LoadSubscriptions(doc, legacy, Name(), VAR_NAME(subscribed_weekly_bonus_pvp), subscribed_weekly_bonus_pvp);
    LoadSubscriptions(doc, legacy, Name(), VAR_NAME(subscribed_vanguard), subscribed_vanguard);
    LoadSubscriptions(doc, legacy, Name(), VAR_NAME(subscribed_nicholas_sandford), subscribed_nicholas_sandford);
}

void DailyQuests::SaveSettings(SettingsDoc& doc)
{
    ToolboxWindow::SaveSettings(doc);
    doc.SetStruct(Name(), settings);

    SaveSubscriptions(doc, Name(), VAR_NAME(subscribed_zaishen_missions), subscribed_zaishen_missions);
    SaveSubscriptions(doc, Name(), VAR_NAME(subscribed_zaishen_bounties), subscribed_zaishen_bounties);
    SaveSubscriptions(doc, Name(), VAR_NAME(subscribed_zaishen_combats), subscribed_zaishen_combats);
    SaveSubscriptions(doc, Name(), VAR_NAME(subscribed_zaishen_vanquishes), subscribed_zaishen_vanquishes);
    SaveSubscriptions(doc, Name(), VAR_NAME(subscribed_wanted_quests), subscribed_wanted_quests);
    SaveSubscriptions(doc, Name(), VAR_NAME(subscribed_weekly_bonus_pve), subscribed_weekly_bonus_pve);
    SaveSubscriptions(doc, Name(), VAR_NAME(subscribed_weekly_bonus_pvp), subscribed_weekly_bonus_pvp);
    SaveSubscriptions(doc, Name(), VAR_NAME(subscribed_vanguard), subscribed_vanguard);
    SaveSubscriptions(doc, Name(), VAR_NAME(subscribed_nicholas_sandford), subscribed_nicholas_sandford);
}

void DailyQuests::Initialize()
{
    ToolboxWindow::Initialize();
    SettingsRegistry::Register(this, settings);

    for (auto& cycle : zaishen_bounty_cycles) {
        if (cycle.enc_name[0] != L'\x108') cycle.enc_name = std::format(L"\x108\x107{}\x1", cycle.enc_name);
    }


    if (wanted_by_shining_blade_cycles.empty()) {
        // TODO: Find the encoded names and maps for these
        for (const auto hard_coded_name : hard_coded_wanted_by_shining_blade_names) {
            const auto wrapped = std::format(L"\x108\x107{}\x1", TextUtils::StringToWString(hard_coded_name));
            wanted_by_shining_blade_cycles.push_back({MapID::None, wrapped.c_str()});
        }
    }

    if (vanguard_cycles.empty()) {
        // TODO: Find the encoded names and maps for these
        for (const auto hard_coded_name : hard_coded_vanguard_names) {
            const auto wrapped = std::format(L"\x108\x107{}\x1", TextUtils::StringToWString(hard_coded_name));
            vanguard_cycles.push_back({MapID::None, wrapped.c_str()});
        }
    }

    // Trigger string decodes
    for (auto& it : wanted_by_shining_blade_cycles) {
        it.GetQuestName();
        it.quest_location_enc = GW::EncStrings::WantedByTheShiningBlade;
    }
    for (auto& it : vanguard_cycles) {
        it.GetQuestName();
    }
    for (auto& it : nicholas_sandford_cycles) {
        it.GetQuestName();
    }

    for (auto& it : nicholas_cycles) {
        it.GetQuestName();
    }
    for (auto& it : zaishen_bounty_cycles) {
        it.GetQuestName();
        it.quest_location_enc = GW::EncStrings::ZaishenBounty;
    }
    for (auto& it : zaishen_combat_cycles) {
        it.GetQuestName();
        it.quest_location_enc = GW::EncStrings::ZaishenCombat;
    }
    for (auto& it : zaishen_vanquish_cycles) {
        it.GetQuestName();
        it.quest_location_enc = GW::EncStrings::ZaishenVanquish;
    }
    for (auto& it : zaishen_mission_cycles) {
        it.GetQuestName();
        it.quest_location_enc = GW::EncStrings::ZaishenMission;
    }
    for (auto& it : pvp_weekly_bonus_cycles) {
        it.GetQuestName();
    }
    for (auto& it : pve_weekly_bonus_cycles) {
        it.GetQuestName();
    }

    chat_commands =
        {{L"zm", CmdZaishenMission},
         {L"zb", CmdZaishenBounty},
         {L"zc", CmdZaishenCombat},
         {L"zv", CmdZaishenVanquish},
         {L"vanguard", CmdVanguard},
         {L"wanted", CmdWantedByShiningBlade},
         {L"nicholas", CmdNicholas},
         {L"weekly", CmdWeeklyBonus},
         {L"today",
          [](GW::HookStatus*, const wchar_t*, const int, const LPWSTR*) -> void {
              if (GW::Map::IsPreSearing()) {
                  GW::Chat::SendChat('/', "vanguard");
                  GW::Chat::SendChat('/', "nicholas");
                  return;
              }
              GW::Chat::SendChat('/', "zm");
              GW::Chat::SendChat('/', "zb");
              GW::Chat::SendChat('/', "zc");
              GW::Chat::SendChat('/', "zv");
              GW::Chat::SendChat('/', "wanted");
              GW::Chat::SendChat('/', "nicholas");
              GW::Chat::SendChat('/', "weekly");
          }},
         {L"daily",
          [](GW::HookStatus*, const wchar_t*, const int, const LPWSTR*) -> void {
              GW::Chat::SendChat('/', "today");
          }},
         {L"dailies",
          [](GW::HookStatus*, const wchar_t*, const int, const LPWSTR*) -> void {
              GW::Chat::SendChat('/', "today");
          }},
         {L"tomorrow", [](GW::HookStatus*, const wchar_t*, const int, const LPWSTR*) -> void {
              if (GW::Map::IsPreSearing()) {
                  GW::Chat::SendChat('/', "vanguard tomorrow");
                  GW::Chat::SendChat('/', "nicholas tomorrow");
                  return;
              }
              GW::Chat::SendChat('/', "zm tomorrow");
              GW::Chat::SendChat('/', "zb tomorrow");
              GW::Chat::SendChat('/', "zc tomorrow");
              GW::Chat::SendChat('/', "zv tomorrow");
              GW::Chat::SendChat('/', "wanted tomorrow");
              GW::Chat::SendChat('/', "nicholas tomorrow");
          }}};
    for (const auto& it : chat_commands) {
        GW::Chat::CreateCommand(&ChatCmd_HookEntry, it.first, it.second);
    }

    RegisterUIMessageCallback(&OnUIMessage_HookEntry, GW::UI::UIMessage::kPreferenceValueChanged, OnUIMessage, 0x8000);
    RegisterUIMessageCallback(&OnUIMessage_HookEntry, GW::UI::UIMessage::kMapLoaded, OnUIMessage, 0x8000);
}

void DailyQuests::Terminate()
{
    ToolboxWindow::Terminate();

    for (auto& it : wanted_by_shining_blade_cycles) {
        it.Terminate();
    }
    for (auto& it : vanguard_cycles) {
        it.Terminate();
    }
    for (auto& it : nicholas_sandford_cycles) {
        it.Terminate();
    }

    for (auto& it : nicholas_cycles) {
        it.Terminate();
    }
    for (auto& it : zaishen_bounty_cycles) {
        it.Terminate();
    }
    for (auto& it : zaishen_combat_cycles) {
        it.Terminate();
    }
    for (auto& it : zaishen_vanquish_cycles) {
        it.Terminate();
    }
    for (auto& it : zaishen_mission_cycles) {
        it.Terminate();
    }
    for (auto& it : pvp_weekly_bonus_cycles) {
        it.Terminate();
    }
    for (auto& it : pve_weekly_bonus_cycles) {
        it.Terminate();
    }

    GW::Chat::DeleteCommand(&ChatCmd_HookEntry);
    ClearQuestLogInfo();
    GW::UI::RemoveUIMessageCallback(&OnUIMessage_HookEntry);
}

void DailyQuests::Update(const float)
{
    if (pending_zaishen_mission_check && !GW::UI::IsLoadingScreenShown()) {
        pending_zaishen_mission_check = false;
        OnMapLoaded_CheckZaishenMission();
    }
    if (pending_quest_take && GetQuestLogInfo() && *pending_quest_take->GetQuestName()) {
        const auto has_quest = GetQuestByName(pending_quest_take->GetQuestName(), pending_quest_take->quest_location_enc);
        if (!has_quest) {
            TravelWindow::Instance().Travel(pending_quest_take->GetQuestGiverOutpost());
        }
        else {
            auto map_to = has_quest->map_to;
            // NB: Quest rewards are easier to get from gtob
            if (map_to == MapID::Embark_Beach) map_to = MapID::Great_Temple_of_Balthazar_outpost;
            TravelWindow::Instance().TravelNearest(map_to);
        }
        pending_quest_take = nullptr;
    }
    if (subscriptions_changed) {
        checked_subscriptions = false;
    }
    if (checked_subscriptions) {
        return;
    }
    subscriptions_changed = false;
    if (!start_time) {
        start_time = time(nullptr);
    }
    if (GW::Map::GetIsMapLoaded() && time(nullptr) - start_time > 1) {
        checked_subscriptions = true;
        // Check daily quests for the next 6 days, and send a message if found. Only runs once when TB is opened.
        const time_t now = time(nullptr);
        time_t unix = now + 0;
        uint32_t quest_idx;
        for (auto i = 0u; i < subscriptions_lookahead_days; i++) {
            char date_str[32];
            switch (i) {
                case 0:
                    sprintf(date_str, "today");
                    break;
                case 1:
                    sprintf(date_str, "tomorrow");
                    break;
                default:
                    std::strftime(date_str, 32, "on %A", std::localtime(&unix));
                    break;
            }
            if (subscribed_zaishen_missions[quest_idx = GetZaishenMissionIdx(&unix)]) {
                Log::Flash("%s is the Zaishen Mission %s", zaishen_mission_cycles[quest_idx].GetQuestName(), date_str);
            }
            if (subscribed_zaishen_bounties[quest_idx = GetZaishenBountyIdx(&unix)]) {
                Log::Flash("%s is the Zaishen Bounty %s", zaishen_bounty_cycles[quest_idx].GetQuestName(), date_str);
            }
            if (subscribed_zaishen_combats[quest_idx = GetZaishenCombatIdx(&unix)]) {
                Log::Flash("%s is the Zaishen Combat %s", zaishen_combat_cycles[quest_idx].GetQuestName(), date_str);
            }
            if (subscribed_zaishen_vanquishes[quest_idx = GetZaishenVanquishIdx(&unix)]) {
                Log::Flash("%s is the Zaishen Vanquish %s", zaishen_vanquish_cycles[quest_idx].GetQuestName(), date_str);
            }
            if (subscribed_wanted_quests[quest_idx = GetWantedByShiningBladeIdx(&unix)]) {
                Log::Flash("%s is Wanted by the Shining Blade %s", wanted_by_shining_blade_cycles[quest_idx].GetQuestName(), date_str);
            }
            unix += 86400;
        }

        // Check weekly bonuses / special events
        unix = GetWeeklyRotationTime(&now);
        for (auto i = 0u; i < 2; i++) {
            char date_str[32];
            switch (i) {
                case 0:
                    std::strftime(date_str, 32, "until %R on %A", std::localtime(&unix));
                    break;
                default:
                    std::strftime(date_str, 32, "on %A at %R", std::localtime(&unix));
                    break;
            }
            if (subscribed_weekly_bonus_pve[quest_idx = GetWeeklyBonusPvPIdx(&unix)]) {
                Log::Flash("%s is the Weekly PvE Bonus %s", pve_weekly_bonus_cycles[quest_idx].GetQuestName(), date_str);
            }
            if (subscribed_weekly_bonus_pvp[quest_idx = GetWeeklyBonusPvPIdx(&unix)]) {
                Log::Flash("%s is the Weekly PvP Bonus %s", pvp_weekly_bonus_cycles[quest_idx].GetQuestName(), date_str);
            }
            unix += SECONDSINAWEEK;
        }
    }
}

DailyQuests::QuestData::QuestData(MapID map_id, const wchar_t* enc_name) : map_id(map_id)
{
    if (enc_name) this->enc_name = enc_name;
}

DailyQuests::QuestData::~QuestData()
{
    ASSERT(!name_translated && !name_english);
}

void DailyQuests::QuestData::Terminate()
{
    name_translated.reset();
    name_english.reset();
}

void DailyQuests::QuestData::Decode(bool force)
{
    if (name_translated && name_english && !force) return;
    if (!name_translated) name_translated = std::make_unique<GuiUtils::EncString>(nullptr, false);
    if (!name_english) name_english = std::make_unique<GuiUtils::EncString>(nullptr, false);
    name_english->language(GW::Constants::Language::English);
    name_translated->language(GW::UI::GetTextLanguage());

    if (enc_name.empty()) {
        const auto map_info = GW::Map::GetMapInfo(map_id);
        if (map_info && map_info->name_id) {
            name_translated->reset(map_info->name_id);
            name_english->reset(map_info->name_id);
        }
    }
    else {
        name_translated->reset(enc_name.c_str());
        name_english->reset(enc_name.c_str());
    }
    name_translated->wstring();
    name_english->wstring();
    GetMapName();
}

const wchar_t* DailyQuests::QuestData::GetQuestNameEnc()
{
    return name_translated->encoded().c_str();
}

const char* DailyQuests::QuestData::GetQuestName()
{
    Decode();
    return name_translated->string().c_str();
}

const std::wstring& DailyQuests::QuestData::GetWikiName()
{
    Decode();
    return name_english->wstring();
}

void DailyQuests::QuestData::Travel()
{
    TravelWindow::Instance().TravelNearest(map_id);
}

const char* DailyQuests::QuestData::GetMapName()
{
    return Resources::GetMapName(map_id)->string().c_str();
}

const std::string& DailyQuests::QuestData::GetRegionName()
{
    return Resources::GetRegionName(map_id)->string(); // Resources owns the decode + cache
}

DailyQuests::NicholasCycleData::NicholasCycleData(const wchar_t* enc_name, uint32_t quantity, MapID map_id) : QuestData(map_id, enc_name), quantity(quantity) {}

size_t DailyQuests::NicholasCycleData::GetCollectedQuantity()
{
    static clock_t last_inv_check = 0;
    if (!last_inv_check || TIMER_DIFF(last_inv_check) > 10000) {
        // Check inventory
        for (auto& item : nicholas_cycles) {
            nicholas_item_collected_count[&item] = InventoryManager::CountItemsByName(item.enc_name.c_str());
        }
        last_inv_check = TIMER_INIT();
    }
    const auto found = nicholas_item_collected_count.find(this);
    ASSERT(found != nicholas_item_collected_count.end());
    return found->second;
}

void DailyQuests::NicholasCycleData::Decode(bool force)
{
    if (name_translated && !force) return;
    const auto old_enc_name = enc_name;
    enc_name = std::format(L"\xa35\x101{}\x10a{}\x1", (wchar_t)(quantity + 0x100), old_enc_name);
    QuestData::Decode(force);
    enc_name = old_enc_name;
}

const MapID DailyQuests::QuestData::GetQuestGiverOutpost()
{
    return MapID::None;
}

DailyQuests::NicholasCycleData* DailyQuests::GetNicholasItemInfo(const wchar_t* item_name_encoded)
{
    if (!item_name_encoded) return nullptr;
    for (auto& nicholas_item : nicholas_cycles) {
        if (nicholas_item.enc_name == item_name_encoded) {
            return &nicholas_item;
        }
    }
    return nullptr;
}

DailyQuests::QuestData* DailyQuests::GetNicholasSandfordItemInfo(const wchar_t* item_name_encoded)
{
    if (!item_name_encoded) return nullptr;
    for (auto& sandford_item : nicholas_sandford_cycles) {
        if (sandford_item.enc_name == item_name_encoded) {
            return &sandford_item;
        }
    }
    return nullptr;
}

bool DailyQuests::IsNicholasItem(const GW::Item* item) {
    return item && item->name_enc && (GetNicholasIngredientInfo(item->name_enc) || GetNicholasItemInfo(item->name_enc));
}


const DailyQuests::NicholasIngredientInfo* DailyQuests::GetNicholasIngredientInfo(const wchar_t* ingredient_enc)
{
    // Crafting ingredients whose end product Nicholas The Traveller collects.
    // If an item's enc name isn't known yet, it will be a placeholder that won't match - find it in-game and update EncStrings.h.
    // TODO: update ingredient_quantity values to reflect actual counts needed to craft a full set of nick items
    static NicholasIngredientInfo ingredients[] = {
        {GW::EncStrings::SkaleFins, GW::EncStrings::BowlofSkalefinSoup, 2},
        {GW::EncStrings::ChunkOfDrakeFlesh, GW::EncStrings::DrakeKabob, 1},
        {GW::EncStrings::IbogaPetals, GW::EncStrings::PahnaiSalad, 2},
        {GW::EncStrings::MandragorRoot, GW::EncStrings::MandragorRootCake, 3},
        {GW::EncStrings::BogSkaleFins, GW::EncStrings::Herring, 5},         
        {GW::EncStrings::SentientSpores, GW::EncStrings::BottleofVabbianWine, 5}
    };
    if (!ingredient_enc) return 0;
    for (const auto& entry : ingredients) {
        if (wcscmp(ingredient_enc, entry.ingredient) == 0) return &entry;
    }
    return 0;
}

time_t DailyQuests::GetTimestampFromNicholasSandford(QuestData* data)
{
    constexpr time_t NICHOLAS_PRE_START_DATE = 1239260400;
    constexpr int SECONDSINADAY = 86400;
    auto index = -1;
    for (auto i = 0; i < static_cast<int>(NICHOLAS_PRE_COUNT); i++) {
        if (&nicholas_sandford_cycles[i] == data) {
            index = i;
        }
    }
    assert(index != -1);
    return GetNextEventTime(NICHOLAS_PRE_START_DATE, time(nullptr), index, NICHOLAS_PRE_COUNT, SECONDSINADAY);
}

DailyQuests::DailyQuestResult DailyQuests::GetNicholasTheTraveller(time_t unix)
{
    constexpr time_t EPOCH = 1323097200;
    constexpr time_t PERIOD = SECONDSINAWEEK;
    if (!unix) unix = time(nullptr);
    return {&nicholas_cycles[GetNicholasTheTravellerIdx(&unix)], GetNextRotationTime(unix, EPOCH, PERIOD)};
}

DailyQuests::DailyQuestResult DailyQuests::GetZaishenBounty(time_t unix)
{
    constexpr time_t EPOCH = 1244736000;
    constexpr time_t PERIOD = 86400;
    if (!unix) unix = time(nullptr);
    return {&zaishen_bounty_cycles[GetZaishenBountyIdx(&unix)], GetNextRotationTime(unix, EPOCH, PERIOD)};
}

DailyQuests::DailyQuestResult DailyQuests::GetZaishenMission(time_t unix)
{
    constexpr time_t EPOCH = 1299168000;
    constexpr time_t PERIOD = 86400;
    if (!unix) unix = time(nullptr);
    return {&zaishen_mission_cycles[GetZaishenMissionIdx(&unix)], GetNextRotationTime(unix, EPOCH, PERIOD)};
}

DailyQuests::DailyQuestResult DailyQuests::GetZaishenCombat(time_t unix)
{
    constexpr time_t EPOCH = 1256227200;
    constexpr time_t PERIOD = 86400;
    if (!unix) unix = time(nullptr);
    return {&zaishen_combat_cycles[GetZaishenCombatIdx(&unix)], GetNextRotationTime(unix, EPOCH, PERIOD)};
}

DailyQuests::DailyQuestResult DailyQuests::GetZaishenVanquish(time_t unix)
{
    constexpr time_t EPOCH = 1299168000;
    constexpr time_t PERIOD = 86400;
    if (!unix) unix = time(nullptr);
    return {&zaishen_vanquish_cycles[GetZaishenVanquishIdx(&unix)], GetNextRotationTime(unix, EPOCH, PERIOD)};
}

DailyQuests::DailyQuestResult DailyQuests::GetNicholasSandford(time_t unix)
{
    constexpr time_t EPOCH = 1239260400;
    constexpr time_t PERIOD = 86400;
    if (!unix) unix = time(nullptr);
    return {&nicholas_sandford_cycles[GetNicholasSandfordIdx(&unix)], GetNextRotationTime(unix, EPOCH, PERIOD)};
}

DailyQuests::DailyQuestResult DailyQuests::GetVanguardQuest(time_t unix)
{
    constexpr time_t EPOCH = 1299168000;
    constexpr time_t PERIOD = 86400;
    if (!unix) unix = time(nullptr);
    return {&vanguard_cycles[GetVanguardIdx(&unix)], GetNextRotationTime(unix, EPOCH, PERIOD)};
}

DailyQuests::DailyQuestResult DailyQuests::GetWantedByShiningBlade(time_t unix)
{
    constexpr time_t EPOCH = 1276012800;
    constexpr time_t PERIOD = 86400;
    if (!unix) unix = time(nullptr);
    return {&wanted_by_shining_blade_cycles[GetWantedByShiningBladeIdx(&unix)], GetNextRotationTime(unix, EPOCH, PERIOD)};
}

DailyQuests::DailyQuestResult DailyQuests::GetWeeklyPvEBonus(time_t unix)
{
    constexpr time_t EPOCH = 1368457200;
    constexpr time_t PERIOD = SECONDSINAWEEK;
    if (!unix) unix = time(nullptr);
    return {&pve_weekly_bonus_cycles[GetWeeklyPvEBonusIdx(&unix)], GetNextRotationTime(unix, EPOCH, PERIOD)};
}

DailyQuests::DailyQuestResult DailyQuests::GetWeeklyPvPBonus(time_t unix)
{
    constexpr time_t EPOCH = 1368457200;
    constexpr time_t PERIOD = SECONDSINAWEEK;
    if (!unix) unix = time(nullptr);
    return {&pvp_weekly_bonus_cycles[GetWeeklyPvPBonusIdx(&unix)], GetNextRotationTime(unix, EPOCH, PERIOD)};
}

const DailyQuests::ZaishenCoinReward* DailyQuests::GetZaishenCoinReward(GW::Constants::QuestID quest_id)
{
    const auto it = zaishen_coin_rewards.find(quest_id);
    return it != zaishen_coin_rewards.end() ? &it->second : nullptr;
}

time_t DailyQuests::GetTimestampFromNicholasTheTraveller(NicholasCycleData* data)
{
    /*
    This function returns the next start time of the cycle data or the
    current time if the cycle is ongoing
    */
    auto index = -1;
    for (auto i = 0; i < NICHOLAS_POST_COUNT; i++) {
        if (&nicholas_cycles[i] == data) {
            index = i;
        }
    }

    assert(index != -1);
    return GetNextEventTime(NICHOLAS_POST_START_DATE, time(nullptr), index, NICHOLAS_POST_COUNT, SECONDSINAWEEK);
}
