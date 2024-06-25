#include "stdafx.h"

#include <GWCA/Context/WorldContext.h>

#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Quest.h>

#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <GWCA/Utilities/Hook.h>

#include <Utils/GuiUtils.h>
#include <Logger.h>

#include <Windows/TravelWindow.h>
#include <Windows/DailyQuestsWindow.h>
#include <Constants/EncStrings.h>
#include <Modules/Resources.h>
#include <Utils/ToolboxUtils.h>
#include <Utils/ToolboxUtils.h>
#include <GWCA/Managers/QuestMgr.h>


using GW::Constants::MapID;
namespace {

    static constexpr size_t ZAISHEN_BOUNTY_COUNT = 66;
    static constexpr size_t ZAISHEN_MISSION_COUNT = 69;
    static constexpr size_t ZAISHEN_COMBAT_COUNT = 28;
    static constexpr size_t ZAISHEN_VANQUISH_COUNT = 136;
    static constexpr size_t WANTED_COUNT = 21;
    static constexpr size_t WEEKLY_BONUS_PVE_COUNT = 9;
    static constexpr size_t WEEKLY_BONUS_PVP_COUNT = 6;
    static constexpr size_t VANGUARD_COUNT = 9;
    static constexpr size_t NICHOLAS_PRE_COUNT = 52;
    static constexpr size_t NICHOLAS_POST_COUNT = 137;


    class ZaishenQuestData : public DailyQuests::QuestData {
    public:
        ZaishenQuestData(GW::Constants::MapID map_id = (GW::Constants::MapID)0, const wchar_t* enc_name = nullptr) : DailyQuests::QuestData(map_id, enc_name) {};
        const GW::Constants::MapID GetQuestGiverOutpost() override;
    };
    class ZaishenVanquishQuestData : public ZaishenQuestData {
    public:
        ZaishenVanquishQuestData(GW::Constants::MapID map_id = (GW::Constants::MapID)0, const wchar_t* enc_name = nullptr) : ZaishenQuestData(map_id, enc_name) {};
        const GW::Constants::MapID GetQuestGiverOutpost() override;
    };
    class WantedQuestData : public DailyQuests::QuestData {
    public:
        WantedQuestData(GW::Constants::MapID map_id = (GW::Constants::MapID)0, const wchar_t* enc_name = nullptr) : DailyQuests::QuestData(map_id, enc_name) {};
        const GW::Constants::MapID GetQuestGiverOutpost() override;
    };
    

    // Cache map
    std::map<std::wstring, GuiUtils::EncString*> region_names;

    const char* hard_coded_zaishen_bounty_names[] = {
        "Droajam, Mage of the Sands",
        "Royen Beastkeeper",
        "Eldritch Ettin",
        "Vengeful Aatxe",
        "Fronis Irontoe",
        "Urgoz",
        "Fenrir",
        "Selvetarm",
        "Mohby Windbeak",
        "Charged Blackness",
        "Rotscale",
        "Zoldark the Unholy",
        "Korshek the Immolated",
        "Myish, Lady of the Lake",
        "Frostmaw the Kinslayer",
        "Kunvie Firewing",
        "Z'him Monns",
        "The Greater Darkness",
        "TPS Regulator Golem",
        "Plague of Destruction",
        "The Darknesses",
        "Admiral Kantoh",
        "Borrguus Blisterbark",
        "Forgewight",
        "Baubao Wavewrath",
        "Joffs the Mitigator",
        "Rragar Maneater",
        "Chung, the Attuned",
        "Lord Jadoth",
        "Nulfastu, Earthbound",
        "The Iron Forgeman",
        "Magmus",
        "Mobrin, Lord of the Marsh",
        "Jarimiya the Unmerciful",
        "Duncan the Black",
        "Quansong Spiritspeak",
        "The Stygian Underlords",
        "Fozzy Yeoryios",
        "The Black Beast of Arrgh",
        "Arachni",
        "The Four Horsemen",
        "Remnant of Antiquities",
        "Arbor Earthcall",
        "Prismatic Ooze",
        "Lord Khobay",
        "Jedeh the Mighty",
        "Ssuns, Blessed of Dwayna",
        "Justiciar Thommis",
        "Harn and Maxine Coldstone",
        "Pywatt the Swift",
        "Fendi Nin",
        "Mungri Magicbox",
        "Priest of Menzies",
        "Ilsundur, Lord of Fire",
        "Kepkhet Marrowfeast",
        "Commander Wahli",
        "Kanaxai",
        "Khabuus",
        "Molotov Rocktail",
        "The Stygian Lords",
        "Dragon Lich",
        "Havok Soulwail",
        "Ghial the Bone Dancer",
        "Murakai, Lady of the Night",
        "Rand Stormweaver",
        "Verata"
    };

    static_assert(_countof(hard_coded_zaishen_bounty_names) == ZAISHEN_BOUNTY_COUNT);

    const char* hard_coded_wanted_by_shining_blade_names[] = {
        "Justiciar Kimii",
        "Zaln the Jaded",
        "Justiciar Sevaan",
        "Insatiable Vakar",
        "Amalek the Unmerciful",
        "Carnak the Hungry",
        "Valis the Rampant",
        "Cerris",
        "Sarnia the Red-Handed",
        "Destor the Truth Seeker",
        "Selenas the Blunt",
        "Justiciar Amilyn",
        "Maximilian the Meticulous",
        "Joh the Hostile",
        "Barthimus the Provident",
        "Calamitous",
        "Greves the Overbearing",
        "Lev the Condemned",
        "Justiciar Marron",
        "Justiciar Kasandra",
        "Vess the Disputant"
    };

    static_assert(_countof(hard_coded_wanted_by_shining_blade_names) == WANTED_COUNT);

    const char* hard_coded_vanguard_names[] = {
        "Bandits",
        "Utini Wupwup",
        "Ascalonian Noble",
        "Undead",                // 0x8102 0x7050 0xFC72 0xBD17 0x552E
        "Blazefiend Griefblade",
        "Farmer Hamnet",
        "Charr",
        "Countess Nadya",
        "Footman Tate"
    };

    static_assert(_countof(hard_coded_vanguard_names) == VANGUARD_COUNT);

    const char* hard_coded_nicholas_sandford_names[] = {
        "Grawl Necklaces",
        "Baked Husks",
        "Skeletal Limbs",
        "Unnatural Seeds",
        "Enchanted Lodestones",
        "Skale Fins",
        "Icy Lodestones",
        "Gargoyle Skulls",
        "Dull Carapaces",
        "Baked Husks",
        "Red Iris Flowers",
        "Spider Legs",
        "Skeletal Limbs",
        "Charr Carvings",
        "Enchanted Lodestones",
        "Grawl Necklaces",
        "Icy Lodestones",
        "Worn Belts",
        "Gargoyle Skulls",
        "Unnatural Seeds",
        "Skale Fins",
        "Red Iris Flowers",
        "Enchanted Lodestones",
        "Skeletal Limbs",
        "Charr Carvings",
        "Spider Legs",
        "Baked Husks",
        "Gargoyle Skulls",
        "Unnatural Seeds",
        "Icy Lodestones",
        "Grawl Necklaces",
        "Enchanted Lodestones",
        "Worn Belts",
        "Dull Carapaces",
        "Spider Legs",
        "Gargoyle Skulls",
        "Icy Lodestones",
        "Unnatural Seeds",
        "Worn Belts",
        "Grawl Necklaces",
        "Baked Husks",
        "Skeletal Limbs",
        "Red Iris Flowers",
        "Charr Carvings",
        "Skale Fins",
        "Dull Carapaces",
        "Enchanted Lodestones",
        "Charr Carvings",
        "Spider Legs",
        "Red Iris Flowers",
        "Worn Belts",
        "Dull Carapaces"
    };

    static_assert(_countof(hard_coded_nicholas_sandford_names) == NICHOLAS_PRE_COUNT);

    // these vectors are built inside Initialise() because the strings are hard coded

    std::vector< WantedQuestData> wanted_by_shining_blade_cycles;
    std::vector< DailyQuests::QuestData> vanguard_cycles;
    std::vector< DailyQuests::QuestData> nicholas_sandford_cycles;
    std::vector< ZaishenQuestData> zaishen_bounty_cycles;

    // These vectors are good to go

    DailyQuests::NicholasCycleData nicholas_cycles[] = {
            { GW::EncStrings::RedIrisFlowers, 3, MapID::Regent_Valley }, // Red Iris Flowers
            { GW::EncStrings::FeatheredAvicaraScalps,  3, MapID::Mineral_Springs }, // Feathered Avicara Scalps
            { GW::EncStrings::MargoniteMasks, 2, MapID::Poisoned_Outcrops }, // Margonite Masks
            { GW::EncStrings::QuetzalCrests, 2, MapID::Alcazia_Tangle }, // Quetzal Crests
            { GW::EncStrings::PlagueIdols,       3, MapID::Wajjun_Bazaar }, // Plague Idols
            { GW::EncStrings::AzureRemains,                         2, MapID::Dreadnoughts_Drift }, // Azure Remains
            { GW::EncStrings::MandragorRootCake, 1, MapID::Arkjok_Ward }, // Mandragor Root Cake
            { GW::EncStrings::MahgoClaw,                         1, MapID::Perdition_Rock }, // Mahgo Claw
            { L"\x56EF\xD1D8\xC773\x2C26",       5, MapID::Saoshang_Trail }, // Mantid Pincers
            { L"\x8101\x43DE\xD124\xA4D9\x7D4A", 3, MapID::Fahranur_The_First_City }, // Sentient Seeds
            { L"\x8102\x26EA\x8A6F\xD31C\x31DD", 2, MapID::Sacnoth_Valley }, // Stone Grawl Necklaces
            { L"\x8102\x26D1",                   1, MapID::Twin_Serpent_Lakes }, // Herring
            { L"\x5702\xA954\x959D\x51B8",       3, MapID::Mount_Qinkai }, // Naga Skins
            { L"\x296A",                         1, MapID::The_Falls }, // Gloom Seed
            { L"\x2882",                         1, MapID::The_Breach }, // Charr Hide
            { L"\x8101\x43F3\xF5F8\xC245\x41F2", 1, MapID::The_Alkali_Pan }, // Ruby Djinn Essence
            { L"\x2930",                         2, MapID::Majestys_Rest }, // Thorny Carapaces
            { L"\x56DD\xC82C\xB7E0\x3EB9",       3, MapID::Rheas_Crater }, // Bone Charms
            { L"\x8102\x26E0\xA884\xE2D3\x7E01", 3, MapID::Varajar_Fells }, // Modniir Manes
            { L"\x8102\x26E9\x96D3\x8E81\x64D1", 3, MapID::Dalada_Uplands }, // Superb Charr Carvings
            { L"\x22EE\xF65A\x86E6\x1C6C",       5, MapID::Zen_Daijun_explorable }, // Rolls of Parchment
            { L"\x8101\x5208\xA22C\xC074\x2373", 2, MapID::Garden_of_Seborhin }, // Roaring Ether Claws
            { L"\x8101\x5721",                   3, MapID::Bukdek_Byway }, // Branches of Juni Berries
            { L"\x2945",                         3, MapID::Deldrimor_Bowl }, // Shiverpeak Manes
            { L"\x293C",                         3, MapID::Eastern_Frontier }, // Fetid Carapaces
            { L"\x6CCD\xC6FD\xA37B\x3529",       2, MapID::Gyala_Hatchery }, // Moon Shells
            { L"\x2921",                         1, MapID::The_Arid_Sea }, // Massive Jawbone
            { L"\x8102\x26FA\x8E00\xEA86\x3A1D", 1, MapID::Ice_Cliff_Chasms }, // Chromatic Scale
            { L"\x292B",                         3, MapID::Ice_Floe }, // Mursaat Tokens
            { L"\x8101\x43FA\xA429\xC255\x23C4", 1, MapID::Bahdok_Caverns }, // Sentient Lodestone
            { L"\x2934",                         3, MapID::Tangle_Root }, // Jungle Troll Tusks
            { L"\x8101\x57DD\xF97D\xB5AD\x21FF", 1, MapID::Resplendent_Makuun }, // Sapphire Djinn Essence
            { L"\x56E6\xB928\x9FA2\x43E1",       1, MapID::Arborstone_explorable }, // Stone Carving
            { L"\x2919",                         3, MapID::North_Kryta_Province }, // Feathered Caromi Scalps
            { L"\x8101\x52EE\xBF76\xE319\x2B39", 1, MapID::Holdings_of_Chokhin }, // Pillaged Goods
            { L"\x56D5\x8B0F\xAB5B\x8A6",        1, MapID::Haiju_Lagoon }, // Gold Crimson Skull Coin
            { L"\x56D7\xDD87\x8A67\x167D",       3, MapID::Tahnnakai_Temple_explorable }, // Jade Bracelets
            { L"\x2924",                         2, MapID::Prophets_Path }, // Minotaur Horns
            { L"\x294A",                         2, MapID::Snake_Dance }, // Frosted Griffon Wings
            { L"\x8101\x43E6\xBE4C\xE956\x780",  2, MapID::Mehtani_Keys }, // Silver Bullion Coins
            { L"\x56DF\xFEE4\xCA2D\x27A",        1, MapID::Morostav_Trail }, // Truffle
            { L"\x8102\x26DD\x85C5\xD98F\x5CCB", 3, MapID::Verdant_Cascades }, // Skelk Claws
            { L"\x2923",                         2, MapID::The_Scar }, // Dessicated Hydra Claws
            { L"\x294B",                         3, MapID::Spearhead_Peak }, // Frigid Hearts
            { L"\x570A\x9453\x84A6\x64D4",       3, MapID::Nahpui_Quarter_explorable }, // Celestial Essences
            { L"\x2937",                         1, MapID::Lornars_Pass }, // Phantom Residue
            { L"\x8101\x42D1\xFB15\xD39E\x5A26", 1, MapID::Issnur_Isles }, // Drake Kabob
            { L"\x55D0\xF8B7\xB108\x6018",       3, MapID::Ferndale }, // Amber Chunks
            { L"\x2914",                         2, MapID::Stingray_Strand }, // Glowing Hearts
            { L"\x8102\x26D8\xB5B9\x9AF6\x42D6", 5, MapID::Riven_Earth }, // Saurian Bones
            { L"\x8101\x5207\xEBD7\xB733\x2E27", 2, MapID::Wilderness_of_Bahdza }, // Behemoth Hides
            { L"\x8101\x4E35\xD63F\xCAB4\xDD1",  1, MapID::Crystal_Overlook }, // Luminous Stone
            { L"\x2950",                         3, MapID::Witmans_Folly }, // Intricate Grawl Necklaces
            { L"\x55D1\xD189\x845A\x7164",       3, MapID::Shadows_Passage }, // Jadeite Shards
            { L"\x8101\x43E5\xA891\xA83A\x426D", 1, MapID::Barbarous_Shore }, // Gold Doubloon
            { L"\x291B",                         2, MapID::Skyward_Reach }, // Shriveled Eyes
            { L"\x28EF",                         2, MapID::Icedome }, // Icy Lodestones
            { L"\x5701\xD258\xC958\x506F",       1, MapID::Silent_Surf }, // Keen Oni Talon
            { L"\x2910",                         2, MapID::Nebo_Terrace }, // Hardened Humps
            { L"\x8102\x26E7\xC330\xC111\x4058", 2, MapID::Drakkar_Lake }, // Piles of Elemental Dust
            { L"\x56F2\x876E\xEACB\x730",        3, MapID::Panjiang_Peninsula }, // Naga Hides
            { L"\x22F3\xA11C\xC924\x5E15",       3, MapID::Griffons_Mouth }, // Spiritwood Planks
            { L"\x293A",                         1, MapID::Pockmark_Flats }, // Stormy Eye
            { L"\x8101\x43F0\xFF3B\x8E3E\x20B1", 3, MapID::Forum_Highlands }, // Skree Wings
            { L"\x5706\xC61F\xF23D\x3C4",        3, MapID::Raisu_Palace }, // Soul Stones
            { L"\x290F",                         1, MapID::Tears_of_the_Fallen }, // Spiked Crest
            { L"\x56E5\x922D\xCF17\x7258",       1, MapID::Drazach_Thicket }, // Dragon Root
            { L"\x8102\x26E3\xB76F\xE56C\x1A2",  3, MapID::Jaga_Moraine }, // Berserker Horns
            { L"\x292E",                         1, MapID::Mamnoon_Lagoon }, // Behemoth Jaw
            { L"\x8101\x42D2\xE08B\xB81A\x604",  1, MapID::Zehlon_Reach }, // Bowl of Skalefin Soup
            { L"\x2915",                         2, MapID::Kessex_Peak }, // Forest Minotaur Horns
            { L"\x56ED\xE607\x9B27\x7299",       3, MapID::Sunjiang_District_explorable }, // Putrid Cysts
            { L"\x2926",                         2, MapID::Salt_Flats }, // Jade Mandibles
            { L"\x292F",                         2, MapID::Silverwood }, // Maguuma Manes
            { L"\x56E0\xFBEB\xA429\x7B5",        1, MapID::The_Eternal_Grove }, // Skull Juju
            { L"\x8101\x5840\xB4F5\xB2A7\x5E0F", 3, MapID::Lahtenda_Bog }, // Mandragor Swamproots
            { L"\x8101\x52EA",                   1, MapID::Vehtendi_Valley }, // Bottle of Vabbian Wine
            { L"\x8102\x26DA\x950E\x82F1\xA3D",  2, MapID::Magus_Stones }, // Weaver Legs
            { L"\x291F",                         1, MapID::Diviners_Ascent }, // Topaz Crest
            { L"\x56FC\xD503\x9D77\x730C",       2, MapID::Pongmei_Valley }, // Rot Wallow Tusks
            { L"\x2946",                         2, MapID::Anvil_Rock }, // Frostfire Fangs
            { L"\x8101\x43E7\xD854\xC981\x54DD", 1, MapID::The_Ruptured_Heart }, // Demonic Relic
            { L"\x2917",                         2, MapID::Talmark_Wilderness }, // Abnormal Seeds
            { L"\x8101\x43EA\xE72E\xAA23\x3C54", 1, MapID::The_Hidden_City_of_Ahdashim }, // Diamond Djinn Essence
            { L"\x2928",                         2, MapID::Vulture_Drifts }, // Forgotten Seals
            { L"\x56D4\x8663\xA244\x50F5",       5, MapID::Kinya_Province }, // Copper Crimson Skull Coins
            { L"\x2932",                         3, MapID::Ettins_Back }, // Mossy Mandibles
            { L"\x2954",                         2, MapID::Grenths_Footprint }, // Enslavement Stones
            { L"\x22E6\xE8F4\xA898\x75CB",       5, MapID::Jahai_Bluffs }, // Elonian Leather Squares
            { L"\x8101\x43EC\x8335\xBAA8\x153C", 2, MapID::Vehjin_Mines }, // Cobalt Talons
            { L"\x288C",                         1, MapID::Reed_Bog }, // Maguuma Spider Web
            { L"\x56EB\xB8B7\xF734\x2985",       5, MapID::Minister_Chos_Estate_explorable }, // Forgotten Trinket Boxes
            { L"\x2947",                         3, MapID::Iron_Horse_Mine }, // Icy Humps
            { L"\x8101\x43D2\x8CB3\xFC99\x602F", 1, MapID::The_Shattered_Ravines }, // Sandblasted Lodestone
            { L"\x56FB\xA16B\x9DAD\x62B6",       3, MapID::Archipelagos }, // Black Pearls
            { L"\x8101\x43F7\xFD85\x9D52\x6DFA", 3, MapID::Marga_Coast }, // Insect Carapaces
            { L"\x2911",                         3, MapID::Watchtower_Coast }, // Mergoyle Skulls
            { L"\x2957",                         3, MapID::Cursed_Lands }, // Decayed Orr Emblems
            { L"\x22E2\xCE9B\x8771\x7DC7",       5, MapID::Mourning_Veil_Falls }, // Tempered Glass Vials
            { L"\x2943",                         3, MapID::Old_Ascalon }, // Scorched Lodestones
            { L"\x8101\x583C\xD7B3\xDD92\x598F", 1, MapID::Turais_Procession }, // Water Djinn Essence
            { L"\x5703\xE1CC\xFE29\x4525",       1, MapID::Maishang_Hills }, // Guardian Moss
            { L"\x22C1",                         6, MapID::The_Floodplain_of_Mahnkelon }, // Dwarven Ales
            { L"\x8102\x26D9\xABE9\x9082\x4999", 2, MapID::Sparkfly_Swamp }, // Amphibian Tongues
            { L"\x294E",                         2, MapID::Frozen_Forest }, // Alpine Seeds
            { L"\x2931",                         2, MapID::Dry_Top }, // Tangled Seeds
            { L"\x56FA\xE3AB\xA19E\x5D6A",       3, MapID::Jaya_Bluffs }, // Stolen Supplies
            { L"\x8101\x42D3\xD9E7\xD4E3\x259E", 1, MapID::Plains_of_Jarin }, // Pahnai Salad
            { L"\x5707\xF70E\xCAA2\x5CC5",       3, MapID::Xaquang_Skyway }, // Vermin Hides
            { L"\x8101\x52ED\x86E9\xCEF3\x69D3", 1, MapID::The_Mirror_of_Lyss }, // Roaring Ether Heart
            { L"\x2941",                         3, MapID::Ascalon_Foothills }, // Leathery Claws
            { L"\x56FE\xF2B0\x8B62\x116A",       1, MapID::Unwaking_Waters }, // Azure Crest
            { L"\x8102\x26E2\xC8E7\x8B1F\x716A", 1, MapID::Bjora_Marches }, // Jotun Pelt
            { L"\x8101\x583E\xE3F5\x87A2\x194F", 2, MapID::Dejarin_Estate }, // Heket Tongues
            { L"\x2951",                         5, MapID::Talus_Chute }, // Mountain Troll Tusks
            { L"\x22E7\xC1DA\xF2C1\x452A",       3, MapID::Shenzun_Tunnels }, // Vials of Ink
            { L"\x8101\x43E9\xDBD0\xA0C6\x4AF1", 3, MapID::Gandara_the_Moon_Fortress }, // Kournan Pendants
            { L"\x293D",                         3, MapID::Diessa_Lowlands }, // Singed Gargoyle Skulls
            { L"\x56E4\xDF8C\xAD76\x3958",       3, MapID::Melandrus_Hope }, // Dredge Incisors
            { L"\x2955",                         3, MapID::Tascas_Demise }, // Stone Summit Badges
            { L"\x8102\x26F4\xE764\xC908\x52E2", 3, MapID::Arbor_Bay }, // Krait Skins
            { L"\x8101\x43D0\x843D\x98D1\x775C", 2, MapID::Jokos_Domain }, // Inscribed Shards
            { L"\x56F6\xB464\x9A9E\x11EF",       3, MapID::Sunqua_Vale }, // Feathered Scalps
            { L"\x8101\x43EB\xF92D\xD469\x73A8", 3, MapID::The_Sulfurous_Wastes }, // Mummy Wrappings
            { L"\x2916",                         2, MapID::The_Black_Curtain }, // Shadowy Remnants
            { L"\x570B\xFE7B\xBD8A\x7CF4",       3, MapID::The_Undercity }, // Ancient Kappa Shells
            { L"\x8101\x5206\x8286\xFEFA\x191C", 1, MapID::Yatendi_Canyons }, // Geode
            { L"\x8102\x26E8\xC0DB\xD26E\x4711", 2, MapID::Grothmar_Wardowns }, // Fibrous Mandragor Roots
            { L"\x293F",                         3, MapID::Dragons_Gullet }, // Gruesome Ribcages
            { L"\x56FD\xC65F\xF6F1\x26B4",       2, MapID::Boreas_Seabed_explorable }, // Kraken Eyes
            { L"\x2918",                         3, MapID::Scoundrels_Rise }, // Bog Skale Fins
            { L"\x8101\x583D\xB904\xF476\x59A7", 2, MapID::Sunward_Marches }, // Sentient Spores
            { L"\x292D",                         2, MapID::Sage_Lands }, // Ancient Eyes
            { L"\x8101\x43E4\x8D6E\x83E5\x4C07", 3, MapID::Cliffs_of_Dohjok }, // Copper Shillings
            { L"\x8102\x26DF\xF8E8\x8ACB\x58B4", 3, MapID::Norrhart_Domains }, // Frigid Mandragor Husks
            { L"\x22D5\x8371\x8ED5\x56B4",       3, MapID::Travelers_Vale }, // Bolts of Linen
            { L"\x28EE",                         3, MapID::Flame_Temple_Corridor }, // Charr Carvings
    };
    static_assert(_countof(nicholas_cycles) == NICHOLAS_POST_COUNT);

    ZaishenQuestData zaishen_combat_cycles[] = {
     {GW::Constants::MapID::The_Jade_Quarry_mission},
     {GW::Constants::MapID::Codex_Arena_outpost},
     {GW::Constants::MapID::Heroes_Ascent_outpost},
     {GW::Constants::MapID::Isle_of_the_Dead_guild_hall,GW::EncStrings::GuildVersusGuild},
     { GW::Constants::MapID::Isle_of_the_Dead_guild_hall,GW::EncStrings::AllianceBattles },
     { GW::Constants::MapID::Heroes_Ascent_outpost },
     {GW::Constants::MapID::Isle_of_the_Dead_guild_hall,GW::EncStrings::GuildVersusGuild},
     {GW::Constants::MapID::Codex_Arena_outpost},
     {GW::Constants::MapID::Fort_Aspenwood_mission},
     {GW::Constants::MapID::The_Jade_Quarry_mission},
     {GW::Constants::MapID::Random_Arenas_outpost},
     {GW::Constants::MapID::Codex_Arena_outpost},
     {GW::Constants::MapID::Isle_of_the_Dead_guild_hall,GW::EncStrings::GuildVersusGuild},
     {GW::Constants::MapID::The_Jade_Quarry_mission},
     { GW::Constants::MapID::Isle_of_the_Dead_guild_hall,GW::EncStrings::AllianceBattles },
     { GW::Constants::MapID::Heroes_Ascent_outpost },
     {GW::Constants::MapID::Random_Arenas_outpost},
     {GW::Constants::MapID::Fort_Aspenwood_mission},
     {GW::Constants::MapID::The_Jade_Quarry_mission},
     {GW::Constants::MapID::Random_Arenas_outpost},
     {GW::Constants::MapID::Fort_Aspenwood_mission},
     { GW::Constants::MapID::Heroes_Ascent_outpost },
     { GW::Constants::MapID::Isle_of_the_Dead_guild_hall,GW::EncStrings::AllianceBattles },
     {GW::Constants::MapID::Isle_of_the_Dead_guild_hall,GW::EncStrings::GuildVersusGuild},
     {GW::Constants::MapID::Codex_Arena_outpost},
     {GW::Constants::MapID::Random_Arenas_outpost},
     {GW::Constants::MapID::Fort_Aspenwood_mission},
     { GW::Constants::MapID::Isle_of_the_Dead_guild_hall,GW::EncStrings::AllianceBattles }
    };
    static_assert(_countof(zaishen_combat_cycles) == ZAISHEN_COMBAT_COUNT);

    ZaishenVanquishQuestData zaishen_vanquish_cycles[] = {
        {GW::Constants::MapID::Jaya_Bluffs},
        {GW::Constants::MapID::Holdings_of_Chokhin},
        {GW::Constants::MapID::Ice_Cliff_Chasms},
        {GW::Constants::MapID::Griffons_Mouth},
        { GW::Constants::MapID::Kinya_Province },
        { GW::Constants::MapID::Issnur_Isles },
        {GW::Constants::MapID::Jaga_Moraine},
        {GW::Constants::MapID::Ice_Floe},
        {GW::Constants::MapID::Maishang_Hills},
        {GW::Constants::MapID::Jahai_Bluffs},
        {GW::Constants::MapID::Riven_Earth},
        {GW::Constants::MapID::Icedome},
        {GW::Constants::MapID::Minister_Chos_Estate_explorable},
        {GW::Constants::MapID::Mehtani_Keys},
        { GW::Constants::MapID::Sacnoth_Valley },
        { GW::Constants::MapID::Iron_Horse_Mine },
        {GW::Constants::MapID::Morostav_Trail},
        {GW::Constants::MapID::Plains_of_Jarin},
        {GW::Constants::MapID::Sparkfly_Swamp},
        {GW::Constants::MapID::Kessex_Peak},
        {GW::Constants::MapID::Mourning_Veil_Falls},
        { GW::Constants::MapID::The_Alkali_Pan },
        { GW::Constants::MapID::Varajar_Fells },
        {GW::Constants::MapID::Lornars_Pass},
        {GW::Constants::MapID::Pongmei_Valley},
        {GW::Constants::MapID::The_Floodplain_of_Mahnkelon},
        {GW::Constants::MapID::Verdant_Cascades},
        { GW::Constants::MapID::Majestys_Rest },
        {GW::Constants::MapID::Raisu_Palace},
        {GW::Constants::MapID::The_Hidden_City_of_Ahdashim},
        {GW::Constants::MapID::Rheas_Crater},
        { GW::Constants::MapID::Mamnoon_Lagoon },
        { GW::Constants::MapID::Shadows_Passage },
        {GW::Constants::MapID::The_Mirror_of_Lyss},
        {GW::Constants::MapID::Saoshang_Trail},
        {GW::Constants::MapID::Nebo_Terrace},
        {GW::Constants::MapID::Shenzun_Tunnels},
        {GW::Constants::MapID::The_Ruptured_Heart},
        { GW::Constants::MapID::Salt_Flats },
        { GW::Constants::MapID::North_Kryta_Province },
        {GW::Constants::MapID::Silent_Surf},
        {GW::Constants::MapID::The_Shattered_Ravines},
        {GW::Constants::MapID::Scoundrels_Rise},
        {GW::Constants::MapID::Old_Ascalon},
        { GW::Constants::MapID::Sunjiang_District_explorable },
        {GW::Constants::MapID::The_Sulfurous_Wastes},
        {GW::Constants::MapID::Magus_Stones},
        {GW::Constants::MapID::Perdition_Rock},
        { GW::Constants::MapID::Sunqua_Vale },
        { GW::Constants::MapID::Turais_Procession },
        {GW::Constants::MapID::Norrhart_Domains},
        {GW::Constants::MapID::Pockmark_Flats},
        {GW::Constants::MapID::Tahnnakai_Temple_explorable},
        {GW::Constants::MapID::Vehjin_Mines},
        {GW::Constants::MapID::Poisoned_Outcrops},
        { GW::Constants::MapID::Prophets_Path },
        { GW::Constants::MapID::The_Eternal_Grove },
        {GW::Constants::MapID::Tascas_Demise},
        {GW::Constants::MapID::Resplendent_Makuun},
        {GW::Constants::MapID::Reed_Bog},
        {GW::Constants::MapID::Unwaking_Waters},
        { GW::Constants::MapID::Stingray_Strand },
        {GW::Constants::MapID::Sunward_Marches},
        {GW::Constants::MapID::Regent_Valley},
        {GW::Constants::MapID::Wajjun_Bazaar},
        { GW::Constants::MapID::Yatendi_Canyons },
        { GW::Constants::MapID::Twin_Serpent_Lakes },
        {GW::Constants::MapID::Sage_Lands},
        {GW::Constants::MapID::Xaquang_Skyway},
        {GW::Constants::MapID::Zehlon_Reach},
        {GW::Constants::MapID::Tangle_Root},
        {GW::Constants::MapID::Silverwood},
        { GW::Constants::MapID::Zen_Daijun_explorable },
        { GW::Constants::MapID::The_Arid_Sea },
        {GW::Constants::MapID::Nahpui_Quarter_explorable},
        {GW::Constants::MapID::Skyward_Reach},
        {GW::Constants::MapID::The_Scar},
        {GW::Constants::MapID::The_Black_Curtain},
        { GW::Constants::MapID::Panjiang_Peninsula },
        {GW::Constants::MapID::Snake_Dance},
        {GW::Constants::MapID::Travelers_Vale},
        {GW::Constants::MapID::The_Breach},
        { GW::Constants::MapID::Lahtenda_Bog },
        { GW::Constants::MapID::Spearhead_Peak },
        {GW::Constants::MapID::Mount_Qinkai},
        {GW::Constants::MapID::Marga_Coast},
        {GW::Constants::MapID::Melandrus_Hope},
        {GW::Constants::MapID::The_Falls},
        {GW::Constants::MapID::Jokos_Domain},
        { GW::Constants::MapID::Vulture_Drifts },
        { GW::Constants::MapID::Wilderness_of_Bahdza },
        {GW::Constants::MapID::Talmark_Wilderness},
        {GW::Constants::MapID::Vehtendi_Valley},
        {GW::Constants::MapID::Talus_Chute},
        {GW::Constants::MapID::Mineral_Springs},
        { GW::Constants::MapID::Anvil_Rock },
        {GW::Constants::MapID::Arborstone_explorable},
        {GW::Constants::MapID::Witmans_Folly},
        {GW::Constants::MapID::Arkjok_Ward},
        { GW::Constants::MapID::Ascalon_Foothills },
        { GW::Constants::MapID::Bahdok_Caverns },
        {GW::Constants::MapID::Cursed_Lands},
        {GW::Constants::MapID::Alcazia_Tangle},
        {GW::Constants::MapID::Archipelagos},
        {GW::Constants::MapID::Eastern_Frontier},
        {GW::Constants::MapID::Dejarin_Estate},
        { GW::Constants::MapID::Watchtower_Coast },
        { GW::Constants::MapID::Arbor_Bay },
        {GW::Constants::MapID::Barbarous_Shore},
        {GW::Constants::MapID::Deldrimor_Bowl},
        {GW::Constants::MapID::Boreas_Seabed_explorable},
        {GW::Constants::MapID::Cliffs_of_Dohjok},
        { GW::Constants::MapID::Diessa_Lowlands },
        { GW::Constants::MapID::Bukdek_Byway },
        { GW::Constants::MapID::Bjora_Marches },
        { GW::Constants::MapID::Crystal_Overlook },
        { GW::Constants::MapID::Diviners_Ascent },
        { GW::Constants::MapID::Dalada_Uplands },
        { GW::Constants::MapID::Drazach_Thicket },
        { GW::Constants::MapID::Fahranur_The_First_City },
        { GW::Constants::MapID::Dragons_Gullet },
        { GW::Constants::MapID::Ferndale },
        { GW::Constants::MapID::Forum_Highlands },
        { GW::Constants::MapID::Dreadnoughts_Drift },
        { GW::Constants::MapID::Drakkar_Lake },
        { GW::Constants::MapID::Dry_Top },
        { GW::Constants::MapID::Tears_of_the_Fallen },
        { GW::Constants::MapID::Gyala_Hatchery },
        { GW::Constants::MapID::Ettins_Back },
        { GW::Constants::MapID::Gandara_the_Moon_Fortress },
        { GW::Constants::MapID::Grothmar_Wardowns },
        { GW::Constants::MapID::Flame_Temple_Corridor },
        { GW::Constants::MapID::Haiju_Lagoon },
        { GW::Constants::MapID::Frozen_Forest },
        { GW::Constants::MapID::Garden_of_Seborhin },
        { GW::Constants::MapID::Grenths_Footprint }
    };
    static_assert(_countof(zaishen_vanquish_cycles) == ZAISHEN_VANQUISH_COUNT);

    ZaishenQuestData zaishen_mission_cycles[] = {
        GW::Constants::MapID::Augury_Rock_mission,
        GW::Constants::MapID::Grand_Court_of_Sebelkeh,
        GW::Constants::MapID::Ice_Caves_of_Sorrow,
        GW::Constants::MapID::Raisu_Palace_outpost_mission,
        GW::Constants::MapID::Gate_of_Desolation,
        GW::Constants::MapID::Thirsty_River,
        GW::Constants::MapID::Blacktide_Den,
        GW::Constants::MapID::Against_the_Charr_mission,
        GW::Constants::MapID::Abaddons_Mouth,
        GW::Constants::MapID::Nundu_Bay,
        GW::Constants::MapID::Divinity_Coast,
        GW::Constants::MapID::Zen_Daijun_outpost_mission,
        GW::Constants::MapID::Pogahn_Passage,
        GW::Constants::MapID::Tahnnakai_Temple_outpost_mission,
        GW::Constants::MapID::The_Great_Northern_Wall,
        GW::Constants::MapID::Dasha_Vestibule,
        GW::Constants::MapID::The_Wilds,
        GW::Constants::MapID::Unwaking_Waters_mission,
        GW::Constants::MapID::Chahbek_Village,
        GW::Constants::MapID::Aurora_Glade,
        GW::Constants::MapID::A_Time_for_Heroes_mission,
        GW::Constants::MapID::Consulate_Docks,
        GW::Constants::MapID::Ring_of_Fire,
        GW::Constants::MapID::Nahpui_Quarter_outpost_mission,
        GW::Constants::MapID::The_Dragons_Lair,
        GW::Constants::MapID::Dzagonur_Bastion,
        GW::Constants::MapID::DAlessio_Seaboard,
        GW::Constants::MapID::Assault_on_the_Stronghold_mission,
        GW::Constants::MapID::The_Eternal_Grove_outpost_mission,
        GW::Constants::MapID::Sanctum_Cay,
        GW::Constants::MapID::Rilohn_Refuge,
        GW::Constants::MapID::Warband_of_brothers_mission,
        GW::Constants::MapID::Borlis_Pass,
        GW::Constants::MapID::Imperial_Sanctum_outpost_mission,
        GW::Constants::MapID::Moddok_Crevice,
        GW::Constants::MapID::Nolani_Academy,
        GW::Constants::MapID::Destructions_Depths_mission,
        GW::Constants::MapID::Venta_Cemetery,
        GW::Constants::MapID::Fort_Ranik,
        GW::Constants::MapID::A_Gate_Too_Far_mission,
        GW::Constants::MapID::Minister_Chos_Estate_outpost_mission,
        GW::Constants::MapID::Thunderhead_Keep,
        GW::Constants::MapID::Tihark_Orchard,
        GW::Constants::MapID::Finding_the_Bloodstone_mission,
        GW::Constants::MapID::Dunes_of_Despair,
        GW::Constants::MapID::Vizunah_Square_mission,
        GW::Constants::MapID::Jokanur_Diggings,
        GW::Constants::MapID::Iron_Mines_of_Moladune,
        GW::Constants::MapID::Kodonur_Crossroads,
        GW::Constants::MapID::Genius_Operated_Living_Enchanted_Manifestation_mission,
        GW::Constants::MapID::Arborstone_outpost_mission,
        GW::Constants::MapID::Gates_of_Kryta,
        GW::Constants::MapID::Gate_of_Madness,
        GW::Constants::MapID::The_Elusive_Golemancer_mission,
        GW::Constants::MapID::Riverside_Province,
        GW::Constants::MapID::Boreas_Seabed_outpost_mission,
        GW::Constants::MapID::Ruins_of_Morah,
        GW::Constants::MapID::Hells_Precipice,
        GW::Constants::MapID::Ruins_of_Surmia,
        GW::Constants::MapID::Curse_of_the_Nornbear_mission,
        GW::Constants::MapID::Sunjiang_District_outpost_mission,
        GW::Constants::MapID::Elona_Reach,
        GW::Constants::MapID::Gate_of_Pain,
        GW::Constants::MapID::Blood_Washes_Blood_mission,
        GW::Constants::MapID::Bloodstone_Fen,
        GW::Constants::MapID::Jennurs_Horde,
        GW::Constants::MapID::Gyala_Hatchery_outpost_mission,
        GW::Constants::MapID::Abaddons_Gate,
        GW::Constants::MapID::The_Frost_Gate
    };
    static_assert(_countof(zaishen_mission_cycles) == ZAISHEN_MISSION_COUNT);

    DailyQuests::QuestData pve_weekly_bonus_cycles[] = {
        {GW::Constants::MapID::None,GW::EncStrings::ExtraLuckBonus},
        {GW::Constants::MapID::None,GW::EncStrings::ElonianSupportBonus},
        {GW::Constants::MapID::None,GW::EncStrings::ZaishenBountyBonus},
        {GW::Constants::MapID::None,GW::EncStrings::FactionsEliteBonus},
        {GW::Constants::MapID::None,GW::EncStrings::NorthernSupportBonus},
        {GW::Constants::MapID::None,GW::EncStrings::ZaishenMissionBonus},
        {GW::Constants::MapID::None,GW::EncStrings::PantheonBonus},
        {GW::Constants::MapID::None,GW::EncStrings::FactionSupportBonus},
        {GW::Constants::MapID::None,GW::EncStrings::ZaishenVanquishingBonus}
    };
    static_assert(_countof(pve_weekly_bonus_cycles) == WEEKLY_BONUS_PVE_COUNT);

    DailyQuests::QuestData pvp_weekly_bonus_cycles[] = {
        {GW::Constants::MapID::None,GW::EncStrings::RandomArenasBonus},
        {GW::Constants::MapID::None,GW::EncStrings::GuildVersusGuildBonus},
        {GW::Constants::MapID::None,GW::EncStrings::CompetitiveMissionBonus},
        {GW::Constants::MapID::None,GW::EncStrings::HeroesAscentBonus},
        {GW::Constants::MapID::None,GW::EncStrings::CodexArenaBonus},
        {GW::Constants::MapID::None,GW::EncStrings::AllianceBattleBonus}
    };
    static_assert(_countof(pvp_weekly_bonus_cycles) == WEEKLY_BONUS_PVP_COUNT);

    uint32_t GetZaishenBountyIdx(const time_t* unix)
    {
        return static_cast<uint32_t>((*unix - 1244736000) / 86400 % ZAISHEN_BOUNTY_COUNT);
    }

    uint32_t GetWeeklyBonusPvEIdx(const time_t* unix)
    {
        return static_cast<uint32_t>((*unix - 1368457200) / 604800 % WEEKLY_BONUS_PVE_COUNT);
    }

    uint32_t GetWeeklyBonusPvPIdx(const time_t* unix)
    {
        return static_cast<uint32_t>((*unix - 1368457200) / 604800 % WEEKLY_BONUS_PVP_COUNT);
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
        return static_cast<uint32_t>((*unix - 1323097200) / 604800 % NICHOLAS_POST_COUNT);
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
    
    time_t GetWeeklyRotationTime(const time_t* unix) {
        return static_cast<time_t>(floor((*unix - 1368457200) / 604800) * 604800) + 1368457200;
    }
    uint32_t GetWeeklyPvEBonusIdx(const time_t* unix)
    {
        return static_cast<uint32_t>((GetWeeklyRotationTime(unix) - 1368457200) / 604800 % WEEKLY_BONUS_PVE_COUNT);
    }
    uint32_t GetWeeklyPvPBonusIdx(const time_t* unix)
    {
        return static_cast<uint32_t>((GetWeeklyRotationTime(unix) - 1368457200) / 604800 % WEEKLY_BONUS_PVP_COUNT);
    }


    bool subscribed_zaishen_bounties[ZAISHEN_BOUNTY_COUNT] = { false };
    bool subscribed_zaishen_combats[ZAISHEN_COMBAT_COUNT] = { false };
    bool subscribed_zaishen_missions[ZAISHEN_MISSION_COUNT] = { false };
    bool subscribed_zaishen_vanquishes[ZAISHEN_VANQUISH_COUNT] = { false };
    bool subscribed_wanted_quests[WANTED_COUNT] = { false };
    bool subscribed_weekly_bonus_pve[WEEKLY_BONUS_PVE_COUNT] = { false };
    bool subscribed_weekly_bonus_pvp[WEEKLY_BONUS_PVP_COUNT] = { false };

    bool show_zaishen_bounty_in_window = true;
    bool show_zaishen_combat_in_window = true;
    bool show_zaishen_missions_in_window = true;
    bool show_zaishen_vanquishes_in_window = true;
    bool show_wanted_quests_in_window = true;
    bool show_nicholas_in_window = true;
    bool show_weekly_bonus_pve_in_window = true;
    bool show_weekly_bonus_pvp_in_window = true;

    uint32_t subscriptions_lookahead_days = 7;

    float text_width = 200.0f;
    int daily_quest_window_count = 90;

    bool subscriptions_changed = false;
    bool checked_subscriptions = false;
    time_t start_time;

    std::vector<std::pair< const wchar_t*, GW::Chat::ChatCommandCallback>> chat_commands;

    const ImColor subscribed_color(102, 187, 238, 255);
    const ImColor normal_color(255, 255, 255, 255);
    const ImColor incomplete_color(102, 238, 187, 255);

    DailyQuests::QuestData* pending_quest_take = nullptr;


    bool GetIsPreSearing()
    {
        const GW::AreaInfo* i = GW::Map::GetCurrentMapInfo();
        return i && i->region == GW::Region::Region_Presearing;
    }

    const wchar_t* DateString(const time_t* unix)
    {
        const std::tm* now = std::localtime(unix);
        static wchar_t buf[12];
        swprintf(buf, sizeof(buf), L"%d-%02d-%02d", now->tm_year + 1900, now->tm_mon + 1, now->tm_mday);
        return buf;
    }
    
    void PrintDaily(const wchar_t* label, const char* value, const time_t unix, const bool as_wiki_link = true)
    {
        const bool show_date = unix != time(nullptr);
        wchar_t buf[139];
        if (show_date) {
            swprintf(buf, _countof(buf), as_wiki_link ? L"%s, %s: <a=1>\x200B%S</a>" : L"%s, %s: <a=1>%S</a>", label, DateString(&unix), value);
        }
        else {
            swprintf(buf, _countof(buf), as_wiki_link ? L"%s: <a=1>\x200B%S</a>" : L"%s: %S", label, value);
        }
        WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, buf, nullptr, true);
    }

    void CmdDaily(const wchar_t* quest_type, std::function<DailyQuests::QuestData*(time_t)> get_quest_func, int argc, const LPWSTR* argv) {
        time_t now = time(nullptr);
        if (argc > 1 && !wcscmp(argv[1], L"tomorrow")) {
            now += 86400;
        }
        const auto quest = get_quest_func(now);
        if (argc > 1 && !wcscmp(argv[1], L"take") && quest->GetQuestGiverOutpost() != GW::Constants::MapID::None) {
            pending_quest_take = quest;
            return;
        }
        PrintDaily(quest_type, quest->GetQuestName(), now);
    }

    void CHAT_CMD_FUNC(CmdWeeklyBonus)
    {
        CmdDaily(L"Weekly Bonus PvE", DailyQuests::GetWeeklyPvEBonus, argc, argv);
        CmdDaily(L"Weekly Bonus PvP", DailyQuests::GetWeeklyPvPBonus, argc, argv);
    }

    void CHAT_CMD_FUNC(CmdZaishenBounty)
    {
        CmdDaily(L"Zaishen Bounty", DailyQuests::GetZaishenBounty, argc, argv);
    }

    void CHAT_CMD_FUNC(CmdZaishenMission)
    {
        CmdDaily(L"Zaishen Mission", DailyQuests::GetZaishenMission, argc, argv);
    }

    void CHAT_CMD_FUNC(CmdZaishenVanquish)
    {
        CmdDaily(L"Zaishen Vanquish", DailyQuests::GetZaishenVanquish, argc, argv);
    }

    void CHAT_CMD_FUNC(CmdZaishenCombat)
    {
        CmdDaily(L"Zaishen Combat", DailyQuests::GetZaishenCombat, argc, argv);
    }

    void CHAT_CMD_FUNC(CmdWantedByShiningBlade)
    {
        CmdDaily(L"Wanted", DailyQuests::GetWantedByShiningBlade, argc, argv);
    }

    void CHAT_CMD_FUNC(CmdVanguard)
    {
        CmdDaily(L"Vanguard Quest", DailyQuests::GetVanguardQuest, argc, argv);
    }

    void CHAT_CMD_FUNC(CmdNicholas)
    {
        time_t now = time(nullptr);
        if (argc > 1 && !wcscmp(argv[1], L"tomorrow")) {
            now += 86400;
        }
        std::string buf;

        if (GetIsPreSearing()) {
            buf = std::format("5 {}", DailyQuests::GetNicholasSandford(now)->GetQuestName());

            PrintDaily(L"Nicholas Sandford", buf.c_str(), now, false);
        }
        else {
            auto nick = DailyQuests::GetNicholasTheTraveller(now);
            buf = std::format("{} ({})", nick->GetQuestName(), nick->GetMapName());
            PrintDaily(L"Nicholas the Traveler", buf.c_str(), now, false);
        }
    }

    const char* GetIncompleteStatusMessage(GW::Constants::MapID mission_map_id) {
        const char* incomplete_hard_mode = "This character hasn't completed this area in hard mode";
        const char* incomplete_normal_mode = "This character hasn't completed this area in normal mode";

        const auto w = GW::GetWorldContext();
        const auto map = w ? GW::Map::GetMapInfo(mission_map_id) : nullptr;
        if (!map) return nullptr;
        if (map->type == GW::RegionType::ExplorableZone) {
            if (!ToolboxUtils::ArrayBoolAt(w->vanquished_areas, static_cast<uint32_t>(mission_map_id))) {
                return incomplete_hard_mode;
            }
            return nullptr;
        }

        bool has_bonus = map->campaign != GW::Constants::Campaign::EyeOfTheNorth;
        if (!ToolboxUtils::ArrayBoolAt(w->missions_completed,static_cast<uint32_t>(mission_map_id))) {
            return incomplete_normal_mode;
        }
        if (!ToolboxUtils::ArrayBoolAt(w->missions_completed_hm, static_cast<uint32_t>(mission_map_id))) {
            return incomplete_hard_mode;
        }
        if (has_bonus) {
            if (!ToolboxUtils::ArrayBoolAt(w->missions_bonus, static_cast<uint32_t>(mission_map_id))) {
                return incomplete_normal_mode;
            }
            if (!ToolboxUtils::ArrayBoolAt(w->missions_bonus_hm, static_cast<uint32_t>(mission_map_id))) {
                return incomplete_hard_mode;
            }
        }
        return nullptr;
    }

    using QuestLogNames = std::unordered_map<GW::Constants::QuestID, GuiUtils::EncString*>;

    bool IsQuestAvailable(DailyQuests::QuestData* info) {
        return info && info->GetQuestGiverOutpost() != GW::Constants::MapID::None
            && (info == DailyQuests::GetZaishenVanquish()
                || info == DailyQuests::GetZaishenBounty()
                || info == DailyQuests::GetZaishenCombat()
                || info == DailyQuests::GetZaishenMission()
                || info == DailyQuests::GetVanguardQuest()
                || info == DailyQuests::GetWantedByShiningBlade());
    }
    QuestLogNames quest_log_names;
    QuestLogNames* GetQuestLogInfo() {
        const auto w = GW::GetWorldContext();
        if (!w) return nullptr;
        bool processing = false;
        for (auto& entry : w->quest_log) {
            if (entry.name && !quest_log_names.contains(entry.quest_id)) {
                auto enc_string = new GuiUtils::EncString();
                enc_string
                    ->language(GW::Constants::Language::English)
                    ->reset(entry.name)
                    ->wstring();
                quest_log_names[entry.quest_id] = enc_string;
            }
            if (!processing && quest_log_names[entry.quest_id]->IsDecoding()) {
                processing = true;
            }
        }
        return processing ? nullptr : &quest_log_names;
    }
    const bool wcseq(const wchar_t* a, const wchar_t* b) {
        return wcscmp(a, b) == 0;
    }
    const bool IsDailyQuest(const GW::Quest& quest) {
        return wcseq(quest.location, GW::EncStrings::ZaishenMission)
            || wcseq(quest.location, GW::EncStrings::ZaishenBounty)
            || wcseq(quest.location, GW::EncStrings::ZaishenCombat)
            || wcseq(quest.location, GW::EncStrings::ZaishenVanquish)
            || wcseq(quest.location, GW::EncStrings::WantedByTheShiningBlade);
    }

    GW::Quest* GetQuestByName(const char* quest_name) {
        const auto w = GW::GetWorldContext();
        if (!w) return nullptr;
        const auto decoded_quest_names = GetQuestLogInfo();
        if (!decoded_quest_names)
            return nullptr;
        for (auto& entry : w->quest_log) {
            if (!IsDailyQuest(entry))
                continue;
            if (entry.name && decoded_quest_names->at(entry.quest_id)->string() == quest_name)
                return &entry;
        }
        return nullptr;
    }

    const bool HasDailyQuest(const char* quest_name) {
        return GetQuestByName(quest_name) != nullptr;
    }


    bool OnDailyQuestContextMenu(void* wparam) {
        const auto info = (DailyQuests::QuestData*)wparam;
        const auto has_quest = HasDailyQuest(info->GetQuestName());
        const auto quest_available = IsQuestAvailable(info);
        ImGui::TextUnformatted(info->GetQuestName());

        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_Button, ImColor(0, 0, 0, 0).Value);
        const auto size = ImVec2(250.0f * ImGui::GetIO().FontGlobalScale, 0);
        if (has_quest) {
            ImGui::TextColored(incomplete_color, "You have this quest in your log");
        }
        ImGui::Separator();
        bool travel = false;
        if (has_quest) {
            travel = ImGui::Button("Travel to nearest outpost", size);
        }
        else if (quest_available) {
            travel = ImGui::Button("Travel to take quest", size);
        }
        bool wiki = info->GetWikiName().empty() ? false : ImGui::Button("Guild Wars Wiki", size);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        if (travel) {
            if (has_quest) {
                if (TravelWindow::Instance().TravelNearest(info->map_id))
                    return false;
            }
            if (quest_available) {
                if(TravelWindow::Instance().Travel(info->GetQuestGiverOutpost()))
                    return false;
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

}

const GW::Constants::MapID ZaishenQuestData::GetQuestGiverOutpost()
{
    const auto _map_id = GW::Constants::MapID::Great_Temple_of_Balthazar_outpost;
    if (GW::Map::GetIsMapUnlocked(_map_id))
        return _map_id;
    return GW::Constants::MapID::Embark_Beach;
}
const GW::Constants::MapID ZaishenVanquishQuestData::GetQuestGiverOutpost()
{
    return GW::Constants::MapID::Embark_Beach;
}
const GW::Constants::MapID WantedQuestData::GetQuestGiverOutpost()
{
    return GW::Constants::MapID::Lions_Arch_outpost;
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
    float offset = 0.0f;
    const float short_text_width = 120.0f * ImGui::GetIO().FontGlobalScale;
    const float long_text_width = text_width * ImGui::GetIO().FontGlobalScale;
    const float zm_width = 170.0f * ImGui::GetIO().FontGlobalScale;
    const float zb_width = 185.0f * ImGui::GetIO().FontGlobalScale;
    const float zc_width = 135.0f * ImGui::GetIO().FontGlobalScale;
    const float zv_width = 200.0f * ImGui::GetIO().FontGlobalScale;
    const float ws_width = 180.0f * ImGui::GetIO().FontGlobalScale;
    const float nicholas_width = 180.0f * ImGui::GetIO().FontGlobalScale;
    const float wbe_width = 145.0f * ImGui::GetIO().FontGlobalScale;

    ImGui::Text("Date");
    ImGui::SameLine(offset += short_text_width);
    if (show_zaishen_missions_in_window) {
        ImGui::Text("Zaishen Mission");
        ImGui::SameLine(offset += zm_width);
    }
    if (show_zaishen_bounty_in_window) {
        ImGui::Text("Zaishen Bounty");
        ImGui::SameLine(offset += zb_width);
    }
    if (show_zaishen_combat_in_window) {
        ImGui::Text("Zaishen Combat");
        ImGui::SameLine(offset += zc_width);
    }
    if (show_zaishen_vanquishes_in_window) {
        ImGui::Text("Zaishen Vanquish");
        ImGui::SameLine(offset += zv_width);
    }
    if (show_wanted_quests_in_window) {
        ImGui::Text("Wanted");
        ImGui::SameLine(offset += ws_width);
    }
    if (show_nicholas_in_window) {
        ImGui::Text("Nicholas the Traveler");
        ImGui::SameLine(offset += nicholas_width);
    }
    if (show_weekly_bonus_pve_in_window) {
        ImGui::Text("Weekly Bonus PvE");
        ImGui::SameLine(offset += wbe_width);
    }
    if (show_weekly_bonus_pvp_in_window) {
        ImGui::Text("Weekly Bonus PvP");
        ImGui::SameLine(offset += long_text_width);
    }
    ImGui::NewLine();
    ImGui::Separator();
    ImGui::BeginChild("dailies_scroll", ImVec2(0, -1 * (20.0f * ImGui::GetIO().FontGlobalScale) - ImGui::GetStyle().ItemInnerSpacing.y));
    time_t unix = time(nullptr);
    uint32_t idx = 0;

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
        auto write_daily_info = [](bool* subscribed, DailyQuests::QuestData* info, bool check_completion) {
            const auto incomplete_message = check_completion ? GetIncompleteStatusMessage(info->map_id) : nullptr;
            auto col = &normal_color;
            if (incomplete_message) col = &incomplete_color;
            if (*subscribed) col = &subscribed_color;
            const auto start = ImGui::GetCursorScreenPos();
            ImGui::TextColored(*col, info->GetQuestName());
            auto lmb_clicked = ImGui::IsItemClicked();
            auto rmb_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);
            auto hovered = ImGui::IsItemHovered();
            if (HasDailyQuest(info->GetQuestName())) {
                ImGui::SameLine();
                ImGui::TextColored(incomplete_color, ICON_FA_EXCLAMATION);
                hovered |= ImGui::IsItemHovered();
                lmb_clicked |= ImGui::IsItemClicked();
                rmb_clicked |= ImGui::IsItemClicked(ImGuiMouseButton_Right);
            }
            if (rmb_clicked) {
                ImGui::SetContextMenu(OnDailyQuestContextMenu, info);
            }
            if (lmb_clicked) {
                *subscribed = !*subscribed;
            }
            if (incomplete_message && hovered) {
                ImGui::SetTooltip(incomplete_message);
            }
            };


        ImGui::SameLine(offset += short_text_width);
        if (show_zaishen_missions_in_window) {
            idx = GetZaishenMissionIdx(&unix);
            write_daily_info(&subscribed_zaishen_missions[idx], GetZaishenMission(unix), true);
            ImGui::SameLine(offset += zm_width);
        }
        if (show_zaishen_bounty_in_window) {
            idx = GetZaishenBountyIdx(&unix);
            write_daily_info(&subscribed_zaishen_bounties[idx], GetZaishenBounty(unix), false);
            ImGui::SameLine(offset += zb_width);
        }
        if (show_zaishen_combat_in_window) {
            idx = GetZaishenCombatIdx(&unix);
            write_daily_info(&subscribed_zaishen_combats[idx], GetZaishenCombat(unix), false);
            ImGui::SameLine(offset += zc_width);
        }
        if (show_zaishen_vanquishes_in_window) {
            idx = GetZaishenVanquishIdx(&unix);
            write_daily_info(&subscribed_zaishen_vanquishes[idx], GetZaishenVanquish(unix), true);
            ImGui::SameLine(offset += zv_width);
        }
        if (show_wanted_quests_in_window) {
            idx = GetWantedByShiningBladeIdx(&unix);
            write_daily_info(&subscribed_wanted_quests[idx], GetWantedByShiningBlade(unix), false);
            ImGui::SameLine(offset += ws_width);
        }
        if (show_nicholas_in_window) {
            ImGui::TextUnformatted(GetNicholasTheTraveller(unix)->GetQuestName());
            ImGui::SameLine(offset += nicholas_width);
        }
        if (show_weekly_bonus_pve_in_window) {
            idx = GetWeeklyBonusPvEIdx(&unix);
            write_daily_info(&subscribed_weekly_bonus_pve[idx], &pve_weekly_bonus_cycles[idx], false);
            ImGui::SameLine(offset += wbe_width);
        }
        if (show_weekly_bonus_pvp_in_window) {
            idx = GetWeeklyBonusPvPIdx(&unix);
            write_daily_info(&subscribed_weekly_bonus_pvp[idx], &pvp_weekly_bonus_cycles[idx], false);
            ImGui::SameLine(offset += long_text_width);
        }
        ImGui::NewLine();
        unix += 86400;
    }
    ImGui::EndChild();
    ImGui::TextDisabled("Click on a daily quest to get notified when its coming up. Subscribed quests are highlighted in ");
    ImGui::SameLine(0, 0);
    ImGui::TextColored(subscribed_color, "blue");
    ImGui::SameLine(0, 0);
    ImGui::TextDisabled(".");

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
    ImGui::PopItemWidth();
    ImGui::Text("Quests to show in Daily Quests window:");
    ImGui::Indent();
    ImGui::StartSpacedElements(200.f);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Zaishen Bounty", &show_zaishen_bounty_in_window);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Zaishen Combat", &show_zaishen_combat_in_window);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Zaishen Mission", &show_zaishen_missions_in_window);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Zaishen Vanquish", &show_zaishen_vanquishes_in_window);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Wanted by Shining Blade", &show_wanted_quests_in_window);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Nicholas The Traveler", &show_nicholas_in_window);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Weekly Bonus (PvE)", &show_weekly_bonus_pve_in_window);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Weekly Bonus (PvP)", &show_weekly_bonus_pvp_in_window);

    ImGui::Unindent();
}

void DailyQuests::LoadSettings(ToolboxIni* ini)
{
    ToolboxWindow::LoadSettings(ini);

    LOAD_BOOL(show_zaishen_bounty_in_window);
    LOAD_BOOL(show_zaishen_combat_in_window);
    LOAD_BOOL(show_zaishen_missions_in_window);
    LOAD_BOOL(show_zaishen_vanquishes_in_window);
    LOAD_BOOL(show_wanted_quests_in_window);
    LOAD_BOOL(show_nicholas_in_window);
    LOAD_BOOL(show_weekly_bonus_pve_in_window);
    LOAD_BOOL(show_weekly_bonus_pvp_in_window);

    const char* zms = ini->GetValue(Name(), VAR_NAME(subscribed_zaishen_missions), "0");
    const std::bitset<ZAISHEN_MISSION_COUNT> zmb(zms);
    for (auto i = 0u; i < zmb.size(); i++) {
        subscribed_zaishen_missions[i] = zmb[i] == 1;
    }

    const char* zbs = ini->GetValue(Name(), VAR_NAME(subscribed_zaishen_bounties), "0");
    const std::bitset<ZAISHEN_BOUNTY_COUNT> zbb(zbs);
    for (auto i = 0u; i < zbb.size(); i++) {
        subscribed_zaishen_bounties[i] = zbb[i] == 1;
    }

    const char* zcs = ini->GetValue(Name(), VAR_NAME(subscribed_zaishen_combats), "0");
    const std::bitset<ZAISHEN_COMBAT_COUNT> zcb(zcs);
    for (auto i = 0u; i < zcb.size(); i++) {
        subscribed_zaishen_combats[i] = zcb[i] == 1;
    }

    const char* zvs = ini->GetValue(Name(), VAR_NAME(subscribed_zaishen_vanquishes), "0");
    const std::bitset<ZAISHEN_VANQUISH_COUNT> zvb(zvs);
    for (auto i = 0u; i < zvb.size(); i++) {
        subscribed_zaishen_vanquishes[i] = zvb[i] == 1;
    }

    const char* wss = ini->GetValue(Name(), VAR_NAME(subscribed_wanted_quests), "0");
    const std::bitset<WANTED_COUNT> wsb(wss);
    for (auto i = 0u; i < wsb.size(); i++) {
        subscribed_wanted_quests[i] = wsb[i] == 1;
    }

    const char* wbes = ini->GetValue(Name(), VAR_NAME(subscribed_weekly_bonus_pve), "0");
    const std::bitset<WEEKLY_BONUS_PVE_COUNT> wbeb(wbes);
    for (auto i = 0u; i < wbeb.size(); i++) {
        subscribed_weekly_bonus_pve[i] = wbeb[i] == 1;
    }

    const char* wbps = ini->GetValue(Name(), VAR_NAME(subscribed_weekly_bonus_pvp), "0");
    const std::bitset<WEEKLY_BONUS_PVP_COUNT> wbpb(wbps);
    for (auto i = 0u; i < wbpb.size(); i++) {
        subscribed_weekly_bonus_pvp[i] = wbpb[i] == 1;
    }
}

void DailyQuests::SaveSettings(ToolboxIni* ini)
{
    ToolboxWindow::SaveSettings(ini);

    SAVE_BOOL(show_zaishen_bounty_in_window);
    SAVE_BOOL(show_zaishen_combat_in_window);
    SAVE_BOOL(show_zaishen_missions_in_window);
    SAVE_BOOL(show_zaishen_vanquishes_in_window);
    SAVE_BOOL(show_wanted_quests_in_window);
    SAVE_BOOL(show_nicholas_in_window);
    SAVE_BOOL(show_weekly_bonus_pve_in_window);
    SAVE_BOOL(show_weekly_bonus_pvp_in_window);
    std::bitset<ZAISHEN_MISSION_COUNT> zmb;
    for (auto i = 0u; i < zmb.size(); i++) {
        zmb[i] = subscribed_zaishen_missions[i] ? 1 : 0;
    }
    ini->SetValue(Name(), VAR_NAME(subscribed_zaishen_missions), zmb.to_string().c_str());

    std::bitset<ZAISHEN_BOUNTY_COUNT> zbb;
    for (auto i = 0u; i < zbb.size(); i++) {
        zbb[i] = subscribed_zaishen_bounties[i] ? 1 : 0;
    }
    ini->SetValue(Name(), VAR_NAME(subscribed_zaishen_bounties), zbb.to_string().c_str());

    std::bitset<ZAISHEN_COMBAT_COUNT> zcb;
    for (auto i = 0u; i < zcb.size(); i++) {
        zcb[i] = subscribed_zaishen_combats[i] ? 1 : 0;
    }
    ini->SetValue(Name(), VAR_NAME(subscribed_zaishen_combats), zcb.to_string().c_str());

    std::bitset<ZAISHEN_VANQUISH_COUNT> zvb;
    for (auto i = 0u; i < zvb.size(); i++) {
        zvb[i] = subscribed_zaishen_vanquishes[i] ? 1 : 0;
    }
    ini->SetValue(Name(), VAR_NAME(subscribed_zaishen_vanquishes), zvb.to_string().c_str());

    std::bitset<WANTED_COUNT> wsb;
    for (auto i = 0u; i < wsb.size(); i++) {
        wsb[i] = subscribed_wanted_quests[i] ? 1 : 0;
    }
    ini->SetValue(Name(), VAR_NAME(subscribed_wanted_quests), wsb.to_string().c_str());

    std::bitset<WEEKLY_BONUS_PVE_COUNT> wbeb;
    for (auto i = 0u; i < wbeb.size(); i++) {
        wbeb[i] = subscribed_weekly_bonus_pve[i] ? 1 : 0;
    }
    ini->SetValue(Name(), VAR_NAME(subscribed_weekly_bonus_pve), wbeb.to_string().c_str());

    std::bitset<WEEKLY_BONUS_PVP_COUNT> wbpb;
    for (auto i = 0u; i < wbpb.size(); i++) {
        wbpb[i] = subscribed_weekly_bonus_pvp[i] ? 1 : 0;
    }
    ini->SetValue(Name(), VAR_NAME(subscribed_weekly_bonus_pvp), wbpb.to_string().c_str());
}

void DailyQuests::Initialize()
{
    ToolboxWindow::Initialize();

    if (zaishen_bounty_cycles.empty()) {
        // TODO: Find the encoded names and maps for these
        for (auto hard_coded_name : hard_coded_zaishen_bounty_names) {
            const auto wrapped = std::format(L"\x108\x107{}\x1", GuiUtils::StringToWString(hard_coded_name));
            zaishen_bounty_cycles.push_back({ GW::Constants::MapID::None,wrapped.c_str() });
        }
    }

    if (wanted_by_shining_blade_cycles.empty()) {
        // TODO: Find the encoded names and maps for these
        for (auto hard_coded_name : hard_coded_wanted_by_shining_blade_names) {
            const auto wrapped = std::format(L"\x108\x107{}\x1", GuiUtils::StringToWString(hard_coded_name));
            wanted_by_shining_blade_cycles.push_back({ GW::Constants::MapID::None,wrapped.c_str() });
        }
    }

    if (vanguard_cycles.empty()) {
        // TODO: Find the encoded names and maps for these
        for (auto hard_coded_name : hard_coded_vanguard_names) {
            const auto wrapped = std::format(L"\x108\x107{}\x1", GuiUtils::StringToWString(hard_coded_name));
            vanguard_cycles.push_back({ GW::Constants::MapID::None,wrapped.c_str() });
        }
    }

    if (nicholas_sandford_cycles.empty()) {
        // TODO: Find the encoded names and maps for these
        for (auto hard_coded_name : hard_coded_nicholas_sandford_names) {
            const auto wrapped = std::format(L"\x108\x107{}\x1", GuiUtils::StringToWString(hard_coded_name));
            nicholas_sandford_cycles.push_back({ GW::Constants::MapID::None,wrapped.c_str() });
        }
    }

    // Trigger string decodes
    for (auto& it : wanted_by_shining_blade_cycles) { it.GetQuestName(); }
    for (auto& it : vanguard_cycles) { it.GetQuestName(); }
    for (auto& it : nicholas_sandford_cycles) { it.GetQuestName(); }

    for (auto& it : nicholas_cycles) { it.GetQuestName(); }
    for (auto& it : zaishen_bounty_cycles) { it.GetQuestName(); }
    for (auto& it : zaishen_combat_cycles) { it.GetQuestName(); }
    for (auto& it : zaishen_vanquish_cycles) { it.GetQuestName(); }
    for (auto& it : zaishen_mission_cycles) { it.GetQuestName(); }
    for (auto& it : pvp_weekly_bonus_cycles) { it.GetQuestName(); }
    for (auto& it : pve_weekly_bonus_cycles) { it.GetQuestName(); }

    chat_commands = {
        {L"zm", CmdZaishenMission},
        {L"zb", CmdZaishenBounty},
        {L"zc", CmdZaishenCombat},
        {L"zv", CmdZaishenVanquish},
        {L"vanguard", CmdVanguard},
        {L"wanted", CmdWantedByShiningBlade},
        {L"nicholas", CmdNicholas},
        {L"weekly", CmdWeeklyBonus},
        {L"today", [](GW::HookStatus*,const wchar_t*, const int, const LPWSTR*) -> void {
            if (GetIsPreSearing()) {
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
        {L"daily", [](GW::HookStatus*,const wchar_t*, const int, const LPWSTR*) -> void {
            GW::Chat::SendChat('/', "today");
        }},
        {L"dailies", [](GW::HookStatus*,const wchar_t*, const int, const LPWSTR*) -> void {
            GW::Chat::SendChat('/', "today");
        }},
        {L"tomorrow", [](GW::HookStatus*,const wchar_t*, const int, const LPWSTR*) -> void {
            if (GetIsPreSearing()) {
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
        }}
    };
    for (auto& it : chat_commands) {
        GW::Chat::CreateCommand(it.first, it.second);
    }
}

void DailyQuests::Terminate() {
    ToolboxWindow::Terminate();

    for (auto& it : wanted_by_shining_blade_cycles) { it.Terminate(); }
    for (auto& it : vanguard_cycles) { it.Terminate(); }
    for (auto& it : nicholas_sandford_cycles) { it.Terminate(); }

    for (auto& it : nicholas_cycles) { it.Terminate(); }
    for (auto& it : zaishen_bounty_cycles) { it.Terminate(); }
    for (auto& it : zaishen_combat_cycles) { it.Terminate(); }
    for (auto& it : zaishen_vanquish_cycles) { it.Terminate(); }
    for (auto& it : zaishen_mission_cycles) { it.Terminate(); }
    for (auto& it : pvp_weekly_bonus_cycles) { it.Terminate(); }
    for (auto& it : pve_weekly_bonus_cycles) { it.Terminate(); }

    for (auto& it : region_names) {
        delete it.second;
    }
    region_names.clear();

    for (auto& it : chat_commands) {
        GW::Chat::DeleteCommand(it.first);
    }
}

void DailyQuests::Update(const float)
{
    if (pending_quest_take && GetQuestLogInfo() && *pending_quest_take->GetQuestName()) {
        auto has_quest = GetQuestByName(pending_quest_take->GetQuestName());
        if (!has_quest) {
            TravelWindow::Instance().Travel(pending_quest_take->GetQuestGiverOutpost());
        }
        else if(has_quest->IsCompleted()){
            TravelWindow::Instance().Travel(pending_quest_take->GetQuestGiverOutpost());
        }
        else {
            TravelWindow::Instance().TravelNearest(pending_quest_take->map_id);
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
                Log::Info("%s is the Zaishen Mission %s", zaishen_mission_cycles[quest_idx].GetQuestName(), date_str);
            }
            if (subscribed_zaishen_bounties[quest_idx = GetZaishenBountyIdx(&unix)]) {
                Log::Info("%s is the Zaishen Bounty %s", zaishen_bounty_cycles[quest_idx].GetQuestName(), date_str);
            }
            if (subscribed_zaishen_combats[quest_idx = GetZaishenCombatIdx(&unix)]) {
                Log::Info("%s is the Zaishen Combat %s", zaishen_combat_cycles[quest_idx].GetQuestName(), date_str);
            }
            if (subscribed_zaishen_vanquishes[quest_idx = GetZaishenVanquishIdx(&unix)]) {
                Log::Info("%s is the Zaishen Vanquish %s", zaishen_vanquish_cycles[quest_idx].GetQuestName(), date_str);
            }
            if (subscribed_wanted_quests[quest_idx = GetWantedByShiningBladeIdx(&unix)]) {
                Log::Info("%s is Wanted by the Shining Blade %s", wanted_by_shining_blade_cycles[quest_idx].GetQuestName(), date_str);
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
                Log::Info("%s is the Weekly PvE Bonus %s", pve_weekly_bonus_cycles[quest_idx].GetQuestName(), date_str);
            }
            if (subscribed_weekly_bonus_pvp[quest_idx = GetWeeklyBonusPvPIdx(&unix)]) {
                Log::Info("%s is the Weekly PvP Bonus %s", pvp_weekly_bonus_cycles[quest_idx].GetQuestName(), date_str);
            }
            unix += 604800;
        }
    }
}

DailyQuests::QuestData::QuestData(GW::Constants::MapID map_id, const wchar_t* enc_name) : map_id(map_id) {
    if (enc_name)
        this->enc_name = enc_name;
}
DailyQuests::QuestData::~QuestData() {
    ASSERT(!name_translated && !name_english);
}
void DailyQuests::QuestData::Terminate() {
    if (name_translated) delete name_translated;
    name_translated = nullptr;
    if (name_english) delete name_english;
    name_english = nullptr;
}
void DailyQuests::QuestData::Decode() {
    if (name_translated && name_english)
        return;
    name_translated = new GuiUtils::EncString(nullptr, false);
    name_english = new GuiUtils::EncString(nullptr, false);
    name_english->language(GW::Constants::Language::English);

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
const char* DailyQuests::QuestData::GetQuestName() {
    Decode();
    return name_translated->string().c_str();
}
const std::wstring& DailyQuests::QuestData::GetWikiName() {
    Decode();
    return name_english->wstring();
}
void DailyQuests::QuestData::Travel() {
    GW::Map::Travel(TravelWindow::GetNearestOutpost(map_id));
}
const char* DailyQuests::QuestData::GetMapName() {
    return Resources::GetMapName(map_id)->string().c_str();
}
const std::string& DailyQuests::QuestData::GetRegionName() {
    const auto region_name = Resources::GetRegionName(map_id);
    auto found = region_names.find(region_name);
    if (found == region_names.end()) {
        region_names[region_name] = new GuiUtils::EncString(region_name);
        found = region_names.find(region_name);
    }
    return found->second->string();
}
DailyQuests::NicholasCycleData::NicholasCycleData(const wchar_t* enc_name, uint32_t quantity, GW::Constants::MapID map_id)
    : QuestData(map_id, enc_name), quantity(quantity) { }
void DailyQuests::NicholasCycleData::Decode() {
    if (name_translated)
        return;
    QuestData::Decode();
    const auto with_qty = std::format(L"\xa35\x101{}\x10a{}\x1", (wchar_t)(quantity + 0x100), enc_name);
    name_translated->reset(with_qty.c_str());
}

const GW::Constants::MapID DailyQuests::QuestData::GetQuestGiverOutpost() { return GW::Constants::MapID::None; }

DailyQuests::NicholasCycleData* DailyQuests::GetNicholasItemInfo(const wchar_t* item_name_encoded) {
    for (auto& nicholas_item : nicholas_cycles) {
        if (nicholas_item.enc_name == item_name_encoded) {
            return &nicholas_item;
        }
    }
    return nullptr;
}
DailyQuests::NicholasCycleData* DailyQuests::GetNicholasTheTraveller(time_t unix) {
    if (!unix)
        unix = time(nullptr);
    return &nicholas_cycles[GetNicholasTheTravellerIdx(&unix)];
}
DailyQuests::QuestData* DailyQuests::GetZaishenBounty(time_t unix) {
    if (!unix)
        unix = time(nullptr);
    return &zaishen_bounty_cycles[GetZaishenBountyIdx(&unix)];
}
DailyQuests::QuestData* DailyQuests::GetZaishenMission(time_t unix) {
    if (!unix)
        unix = time(nullptr);
    return &zaishen_mission_cycles[GetZaishenMissionIdx(&unix)];
}
DailyQuests::QuestData* DailyQuests::GetZaishenCombat(time_t unix) {
    if (!unix)
        unix = time(nullptr);
    return &zaishen_combat_cycles[GetZaishenCombatIdx(&unix)];
}
DailyQuests::QuestData* DailyQuests::GetZaishenVanquish(time_t unix) {
    if (!unix)
        unix = time(nullptr);
    return &zaishen_vanquish_cycles[GetZaishenVanquishIdx(&unix)];
}
DailyQuests::QuestData* DailyQuests::GetNicholasSandford(time_t unix) {
    if (!unix)
        unix = time(nullptr);
    return &nicholas_sandford_cycles[GetNicholasSandfordIdx(&unix)];
}
DailyQuests::QuestData* DailyQuests::GetVanguardQuest(time_t unix) {
    if (!unix)
        unix = time(nullptr);
    return &vanguard_cycles[GetVanguardIdx(&unix)];
}
DailyQuests::QuestData* DailyQuests::GetWantedByShiningBlade(time_t unix) {
    if (!unix)
        unix = time(nullptr);
    return &wanted_by_shining_blade_cycles[GetWantedByShiningBladeIdx(&unix)];
}
DailyQuests::QuestData* DailyQuests::GetWeeklyPvEBonus(time_t unix) {
    if (!unix)
        unix = time(nullptr);
    return &pve_weekly_bonus_cycles[GetWeeklyPvEBonusIdx(&unix)];
}
DailyQuests::QuestData* DailyQuests::GetWeeklyPvPBonus(time_t unix) {
    if (!unix)
        unix = time(nullptr);
    return &pvp_weekly_bonus_cycles[GetWeeklyPvPBonusIdx(&unix)];
}

