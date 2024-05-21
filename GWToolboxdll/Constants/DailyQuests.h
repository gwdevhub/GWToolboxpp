#pragma once

#include <GWCA/Constants/Maps.h>

namespace GUIUtils {
    class EncString;
}

using GW::Constants::MapID;
using GW::Region;

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

enum class GameRegion {
    BATTLE_ISLES,
    THE_MISTS,
    PRE_SEARING,
    ASCALON,
    NORTHERN_SHIVERPEAKS,
    KRYTA,
    MAGUUMA_JUNGLE,
    CRYSTAL_DESERT,
    SOUTHERN_SHIVERPEAKS,
    RING_OF_FIRE_ISLANDS,
    SHING_JEA,
    KAINENG_CITY,
    ECHOVALD_FOREST,
    JADE_SEA,
    RAISU_PALACE,
    ISTAN,
    KOURNA,
    VABBI,
    DESOLATION,
    REALM_OF_TORMENT,
    FAR_SHIVERPEAKS,
    CHARR_HOMELANDS,
    TARNISHED_COAST,
    DEPTHS_OF_TYRIA,
};

struct NicholasCycleData {
    uint32_t model_id;
    uint32_t quantity;
    GuiUtils::EncString name_enc;
    MapID map_id;
    GameRegion region;
};

// TODO: Replace english text with encoded strings as much as possible

// TODO: enc quest names?  quest ids?
inline const char* VANGUARD_CYCLES[VANGUARD_COUNT] = {
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

// TODO: enc names
inline const char* NICHOLAS_PRE_CYCLES[NICHOLAS_PRE_COUNT] = {
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

// TODO: enc names, map ids, add item type field as model id is not unique across types
inline const NicholasCycleData nicholas_post_cycles[NICHOLAS_POST_COUNT] = {
    { 2994, 3, L"\x271E\xDBDF\xBBD8\x34CB", MapID::Regent_Valley, GameRegion::ASCALON, }, // Red Iris Flowers
    { 498, 3, L"\x294F", MapID::Mineral_Springs, GameRegion::SOUTHERN_SHIVERPEAKS, }, // Feathered Avicara Scalps
    { 1581, 2, L"\x8101\x43E8\xBEBD\xF09A\x4455", MapID::Poisoned_Outcrops, GameRegion::DESOLATION, }, // Margonite Masks
    { 27039, 2, L"\x8102\x26DC\xADF9\x8D4B\x6575", MapID::Alcazia_Tangle, GameRegion::TARNISHED_COAST, }, // Quetzal Crests
    { 805, 3, L"\x56D3\xA490\xE607\x11B6", MapID::Wajjun_Bazaar, GameRegion::KAINENG_CITY, }, // Plague Idols
    { 496, 2, L"\x294D", MapID::Dreadnoughts_Drift, GameRegion::SOUTHERN_SHIVERPEAKS, }, // Azure Remains
    { 19170, 1, L"\x8101\x52E9\xA18F\x8FE1\x3EE9", MapID::Arkjok_Ward, GameRegion::KOURNA, }, // Mandragor Root Cake
    { 513, 1, L"\x2960", MapID::Perdition_Rock, GameRegion::RING_OF_FIRE_ISLANDS, }, // Mahgo Claw
    { 829, 5, L"\x56EF\xD1D8\xC773\x2C26", MapID::Saoshang_Trail, GameRegion::SHING_JEA, }, // Mantid Pincers
    { 1601, 3, L"\x8101\x43DE\xD124\xA4D9\x7D4A", MapID::Fahranur_The_First_City, GameRegion::ISTAN, }, // Sentient Seeds
    { 27053, 2, L"\x8102\x26EA\x8A6F\xD31C\x31DD", MapID::Sacnoth_Valley, GameRegion::CHARR_HOMELANDS, }, // Stone Grawl Necklaces
    { 26502, 1, L"\x8102\x26D1", MapID::Twin_Serpent_Lakes, GameRegion::KRYTA, }, // Herring
    { 848, 3, L"\x5702\xA954\x959D\x51B8", MapID::Mount_Qinkai, GameRegion::JADE_SEA, }, // Naga Skins
    { 523, 1, L"\x296A", MapID::The_Falls, GameRegion::MAGUUMA_JUNGLE, }, // Gloom Seed
    { 225, 1, L"\x2882", MapID::The_Breach, GameRegion::ASCALON, }, // Charr Hide
    { 19187, 1, L"\x8101\x43F3\xF5F8\xC245\x41F2", MapID::The_Alkali_Pan, GameRegion::DESOLATION, }, // Ruby Djinn Essence
    { 467, 2, L"\x2930", MapID::Majestys_Rest, GameRegion::KRYTA, }, // Thorny Carapaces
    { 811, 3, L"\x56DD\xC82C\xB7E0\x3EB9", MapID::Rheas_Crater, GameRegion::JADE_SEA, }, // Bone Charms
    { 27043, 3, L"\x8102\x26E0\xA884\xE2D3\x7E01", MapID::Varajar_Fells, GameRegion::FAR_SHIVERPEAKS, }, // Modniir Manes
    { 27052, 3, L"\x8102\x26E9\x96D3\x8E81\x64D1", MapID::Dalada_Uplands, GameRegion::CHARR_HOMELANDS, }, // Superb Charr Carvings
    { 951, 5, L"\x22EE\xF65A\x86E6\x1C6C", MapID::Zen_Daijun_explorable, GameRegion::SHING_JEA, }, // Rolls of Parchment
    { 1679, 2, L"\x8101\x5208\xA22C\xC074\x2373", MapID::Garden_of_Seborhin, GameRegion::VABBI, }, // Roaring Ether Claws
    { 19194, 3, L"\x8101\x5721", MapID::Bukdek_Byway, GameRegion::KAINENG_CITY, }, // Branches of Juni Berries
    { 488, 3, L"\x2945", MapID::Deldrimor_Bowl, GameRegion::NORTHERN_SHIVERPEAKS, }, // Shiverpeak Manes
    { 479, 3, L"\x293C", MapID::Eastern_Frontier, GameRegion::ASCALON, }, // Fetid Carapaces
    { 1009, 2, L"\x6CCD\xC6FD\xA37B\x3529", MapID::Gyala_Hatchery, GameRegion::JADE_SEA, }, // Moon Shells
    { 452, 1, L"\x2921", MapID::The_Arid_Sea, GameRegion::CRYSTAL_DESERT, }, // Massive Jawbone
    { 27069, 1, L"\x8102\x26FA\x8E00\xEA86\x3A1D", MapID::Ice_Cliff_Chasms, GameRegion::FAR_SHIVERPEAKS, }, // Chromatic Scale
    { 462, 3, L"\x292B", MapID::Ice_Floe, GameRegion::SOUTHERN_SHIVERPEAKS, }, // Mursaat Tokens
    { 1619, 1, L"\x8101\x43FA\xA429\xC255\x23C4", MapID::Bahdok_Caverns, GameRegion::KOURNA, }, // Sentient Lodestone
    { 471, 3, L"\x2934", MapID::Tangle_Root, GameRegion::MAGUUMA_JUNGLE, }, // Jungle Troll Tusks
    { 19188, 1, L"\x8101\x57DD\xF97D\xB5AD\x21FF", MapID::Resplendent_Makuun, GameRegion::VABBI, }, // Sapphire Djinn Essence
    { 820, 1, L"\x56E6\xB928\x9FA2\x43E1", MapID::Arborstone_explorable, GameRegion::ECHOVALD_FOREST, }, // Stone Carving
    { 444, 3, L"\x2919", MapID::North_Kryta_Province, GameRegion::KRYTA, }, // Feathered Caromi Scalps
    { 1663, 1, L"\x8101\x52EE\xBF76\xE319\x2B39", MapID::Holdings_of_Chokhin, GameRegion::VABBI, }, // Pillaged Goods
    { 807, 1, L"\x56D5\x8B0F\xAB5B\x8A6", MapID::Haiju_Lagoon, GameRegion::SHING_JEA, }, // Gold Crimson Skull Coin
    { 809, 3, L"\x56D7\xDD87\x8A67\x167D", MapID::Tahnnakai_Temple_explorable, GameRegion::KAINENG_CITY, }, // Jade Bracelets
    { 455, 2, L"\x2924", MapID::Prophets_Path, GameRegion::CRYSTAL_DESERT, }, // Minotaur Horns
    { 493, 2, L"\x294A", MapID::Snake_Dance, GameRegion::SOUTHERN_SHIVERPEAKS, }, // Frosted Griffon Wings
    { 1579, 2, L"\x8101\x43E6\xBE4C\xE956\x780", MapID::Mehtani_Keys, GameRegion::ISTAN, }, // Silver Bullion Coins
    { 813, 1, L"\x56DF\xFEE4\xCA2D\x27A", MapID::Morostav_Trail, GameRegion::ECHOVALD_FOREST, }, // Truffle
    { 27040, 3, L"\x8102\x26DD\x85C5\xD98F\x5CCB", MapID::Verdant_Cascades, GameRegion::TARNISHED_COAST, }, // Skelk Claws
    { 454, 2, L"\x2923", MapID::The_Scar, GameRegion::CRYSTAL_DESERT, }, // Dessicated Hydra Claws
    { 494, 3, L"\x294B", MapID::Spearhead_Peak, GameRegion::SOUTHERN_SHIVERPEAKS, }, // Frigid Hearts
    { 855, 3, L"\x570A\x9453\x84A6\x64D4", MapID::Nahpui_Quarter_explorable, GameRegion::KAINENG_CITY, }, // Celestial Essences
    { 474, 1, L"\x2937", MapID::Lornars_Pass, GameRegion::SOUTHERN_SHIVERPEAKS, }, // Phantom Residue
    { 17060, 1, L"\x8101\x42D1\xFB15\xD39E\x5A26", MapID::Issnur_Isles, GameRegion::ISTAN, }, // Drake Kabob
    { 6532, 3, L"\x55D0\xF8B7\xB108\x6018", MapID::Ferndale, GameRegion::ECHOVALD_FOREST, }, // Amber Chunks
    { 439, 2, L"\x2914", MapID::Stingray_Strand, GameRegion::KRYTA, }, // Glowing Hearts
    { 27035, 5, L"\x8102\x26D8\xB5B9\x9AF6\x42D6", MapID::Riven_Earth, GameRegion::TARNISHED_COAST, }, // Saurian Bones
    { 1675, 2, L"\x8101\x5207\xEBD7\xB733\x2E27", MapID::Wilderness_of_Bahdza, GameRegion::VABBI, }, // Behemoth Hides
    { 1660, 1, L"\x8101\x4E35\xD63F\xCAB4\xDD1", MapID::Crystal_Overlook, GameRegion::DESOLATION, }, // Luminous Stone
    { 499, 3, L"\x2950", MapID::Witmans_Folly, GameRegion::SOUTHERN_SHIVERPEAKS, }, // Intricate Grawl Necklaces
    { 6533, 3, L"\x55D1\xD189\x845A\x7164", MapID::Shadows_Passage, GameRegion::KAINENG_CITY, }, // Jadeite Shards
    { 1578, 1, L"\x8101\x43E5\xA891\xA83A\x426D", MapID::Barbarous_Shore, GameRegion::KOURNA, }, // Gold Doubloon
    { 446, 2, L"\x291B", MapID::Skyward_Reach, GameRegion::CRYSTAL_DESERT, }, // Shriveled Eyes
    { 424, 2, L"\x28EF", MapID::Icedome, GameRegion::SOUTHERN_SHIVERPEAKS, }, // Icy Lodestones
    { 847, 1, L"\x5701\xD258\xC958\x506F", MapID::Silent_Surf, GameRegion::JADE_SEA, }, // Keen Oni Talon
    { 435, 2, L"\x2910", MapID::Nebo_Terrace, GameRegion::KRYTA, }, // Hardened Humps
    { 27050, 2, L"\x8102\x26E7\xC330\xC111\x4058", MapID::Drakkar_Lake, GameRegion::FAR_SHIVERPEAKS, }, // Piles of Elemental Dust
    { 832, 3, L"\x56F2\x876E\xEACB\x730", MapID::Panjiang_Peninsula, GameRegion::SHING_JEA, }, // Naga Hides
    { 956, 3, L"\x22F3\xA11C\xC924\x5E15", MapID::Griffons_Mouth, GameRegion::NORTHERN_SHIVERPEAKS, }, // Spiritwood Planks
    { 477, 1, L"\x293A", MapID::Pockmark_Flats, GameRegion::ASCALON, }, // Stormy Eye
    { 1610, 3, L"\x8101\x43F0\xFF3B\x8E3E\x20B1", MapID::Forum_Highlands, GameRegion::VABBI, }, // Skree Wings
    { 852, 3, L"\x5706\xC61F\xF23D\x3C4", MapID::Raisu_Palace, GameRegion::KAINENG_CITY, }, // Soul Stones
    { 434, 1, L"\x290F", MapID::Tears_of_the_Fallen, GameRegion::KRYTA, }, // Spiked Crest
    { 819, 1, L"\x56E5\x922D\xCF17\x7258", MapID::Drazach_Thicket, GameRegion::ECHOVALD_FOREST, }, // Dragon Root
    { 27046, 3, L"\x8102\x26E3\xB76F\xE56C\x1A2", MapID::Jaga_Moraine, GameRegion::FAR_SHIVERPEAKS, }, // Berserker Horns
    { 465, 1, L"\x292E", MapID::Mamnoon_Lagoon, GameRegion::MAGUUMA_JUNGLE, }, // Behemoth Jaw
    { 17061, 1, L"\x8101\x42D2\xE08B\xB81A\x604", MapID::Zehlon_Reach, GameRegion::ISTAN, }, // Bowl of Skalefin Soup
    { 440, 2, L"\x2915", MapID::Kessex_Peak, GameRegion::KRYTA, }, // Forest Minotaur Horns
    { 827, 3, L"\x56ED\xE607\x9B27\x7299", MapID::Sunjiang_District_explorable, GameRegion::KAINENG_CITY, }, // Putrid Cysts
    { 457, 2, L"\x2926", MapID::Salt_Flats, GameRegion::CRYSTAL_DESERT, }, // Jade Mandibles
    { 466, 2, L"\x292F", MapID::Silverwood, GameRegion::MAGUUMA_JUNGLE, }, // Maguuma Manes
    { 814, 1, L"\x56E0\xFBEB\xA429\x7B5", MapID::The_Eternal_Grove, GameRegion::ECHOVALD_FOREST, }, // Skull Juju
    { 1671, 3, L"\x8101\x5840\xB4F5\xB2A7\x5E0F", MapID::Lahtenda_Bog, GameRegion::ISTAN, }, // Mandragor Swamproots
    { 19173, 1, L"\x8101\x52EA", MapID::Vehtendi_Valley, GameRegion::VABBI, }, // Bottle of Vabbian Wine
    { 27037, 2, L"\x8102\x26DA\x950E\x82F1\xA3D", MapID::Magus_Stones, GameRegion::TARNISHED_COAST, }, // Weaver Legs
    { 450, 1, L"\x291F", MapID::Diviners_Ascent, GameRegion::CRYSTAL_DESERT, }, // Topaz Crest
    { 842, 2, L"\x56FC\xD503\x9D77\x730C", MapID::Pongmei_Valley, GameRegion::KAINENG_CITY, }, // Rot Wallow Tusks
    { 489, 2, L"\x2946", MapID::Anvil_Rock, GameRegion::NORTHERN_SHIVERPEAKS, }, // Frostfire Fangs
    { 1580, 1, L"\x8101\x43E7\xD854\xC981\x54DD", MapID::The_Ruptured_Heart, GameRegion::DESOLATION, }, // Demonic Relic
    { 442, 2, L"\x2917", MapID::Talmark_Wilderness, GameRegion::KRYTA, }, // Abnormal Seeds
    { 19186, 1, L"\x8101\x43EA\xE72E\xAA23\x3C54", MapID::The_Hidden_City_of_Ahdashim, GameRegion::VABBI, }, // Diamond Djinn Essence
    { 459, 2, L"\x2928", MapID::Vulture_Drifts, GameRegion::CRYSTAL_DESERT, }, // Forgotten Seals
    { 806, 5, L"\x56D4\x8663\xA244\x50F5", MapID::Kinya_Province, GameRegion::SHING_JEA, }, // Copper Crimson Skull Coins
    { 469, 3, L"\x2932", MapID::Ettins_Back, GameRegion::MAGUUMA_JUNGLE, }, // Mossy Mandibles
    { 532, 2, L"\x2954", MapID::Grenths_Footprint, GameRegion::SOUTHERN_SHIVERPEAKS, }, // Enslavement Stones
    { 943, 5, L"\x22E6\xE8F4\xA898\x75CB", MapID::Jahai_Bluffs, GameRegion::KOURNA, }, // Elonian Leather Squares
    { 1609, 2, L"\x8101\x43EC\x8335\xBAA8\x153C", MapID::Vehjin_Mines, GameRegion::VABBI, }, // Cobalt Talons
    { 234, 1, L"\x288C", MapID::Reed_Bog, GameRegion::MAGUUMA_JUNGLE, }, // Maguuma Spider Web
    { 825, 5, L"\x56EB\xB8B7\xF734\x2985", MapID::Minister_Chos_Estate_explorable, GameRegion::SHING_JEA, }, // Forgotten Trinket Boxes
    { 490, 3, L"\x2947", MapID::Iron_Horse_Mine, GameRegion::NORTHERN_SHIVERPEAKS, }, // Icy Humps
    { 1584, 1, L"\x8101\x43D2\x8CB3\xFC99\x602F", MapID::The_Shattered_Ravines, GameRegion::DESOLATION, }, // Sandblasted Lodestone
    { 841, 3, L"\x56FB\xA16B\x9DAD\x62B6", MapID::Archipelagos, GameRegion::JADE_SEA, }, // Black Pearls
    { 1617, 3, L"\x8101\x43F7\xFD85\x9D52\x6DFA", MapID::Marga_Coast, GameRegion::KOURNA, }, // Insect Carapaces
    { 463, 3, L"\x2911", MapID::Watchtower_Coast, GameRegion::KRYTA, }, // Mergoyle Skulls
    { 504, 3, L"\x2957", MapID::Cursed_Lands, GameRegion::KRYTA, }, // Decayed Orr Emblems
    { 939, 5, L"\x22E2\xCE9B\x8771\x7DC7", MapID::Mourning_Veil_Falls, GameRegion::ECHOVALD_FOREST, }, // Tempered Glass Vials
    { 486, 3, L"\x2943", MapID::Old_Ascalon, GameRegion::ASCALON, }, // Scorched Lodestones
    { 19189, 1, L"\x8101\x583C\xD7B3\xDD92\x598F", MapID::Turais_Procession, GameRegion::DESOLATION, }, // Water Djinn Essence
    { 849, 1, L"\x5703\xE1CC\xFE29\x4525", MapID::Maishang_Hills, GameRegion::JADE_SEA, }, // Guardian Moss
    { 5585, 6, L"\x22C1", MapID::The_Floodplain_of_Mahnkelon, GameRegion::KOURNA, }, // Dwarven Ales
    { 27036, 2, L"\x8102\x26D9\xABE9\x9082\x4999", MapID::Sparkfly_Swamp, GameRegion::TARNISHED_COAST, }, // Amphibian Tongues
    { 497, 2, L"\x294E", MapID::Frozen_Forest, GameRegion::SOUTHERN_SHIVERPEAKS, }, // Alpine Seeds
    { 468, 2, L"\x2931", MapID::Dry_Top, GameRegion::MAGUUMA_JUNGLE, }, // Tangled Seeds
    { 840, 3, L"\x56FA\xE3AB\xA19E\x5D6A", MapID::Jaya_Bluffs, GameRegion::SHING_JEA, }, // Stolen Supplies
    { 17062, 1, L"\x8101\x42D3\xD9E7\xD4E3\x259E", MapID::Plains_of_Jarin, GameRegion::ISTAN, }, // Pahnai Salad
    { 853, 3, L"\x5707\xF70E\xCAA2\x5CC5", MapID::Xaquang_Skyway, GameRegion::KAINENG_CITY, }, // Vermin Hides
    { 1662, 1, L"\x8101\x52ED\x86E9\xCEF3\x69D3", MapID::The_Mirror_of_Lyss, GameRegion::VABBI, }, // Roaring Ether Heart
    { 484, 3, L"\x2941", MapID::Ascalon_Foothills, GameRegion::ASCALON, }, // Leathery Claws
    { 844, 1, L"\x56FE\xF2B0\x8B62\x116A", MapID::Unwaking_Waters, GameRegion::JADE_SEA, }, // Azure Crest
    { 27045, 1, L"\x8102\x26E2\xC8E7\x8B1F\x716A", MapID::Bjora_Marches, GameRegion::FAR_SHIVERPEAKS, }, // Jotun Pelt
    { 19199, 2, L"\x8101\x583E\xE3F5\x87A2\x194F", MapID::Dejarin_Estate, GameRegion::KOURNA, }, // Heket Tongues
    { 500, 5, L"\x2951", MapID::Talus_Chute, GameRegion::SOUTHERN_SHIVERPEAKS, }, // Mountain Troll Tusks
    { 944, 3, L"\x22E7\xC1DA\xF2C1\x452A", MapID::Shenzun_Tunnels, GameRegion::KAINENG_CITY, }, // Vials of Ink
    { 1582, 3, L"\x8101\x43E9\xDBD0\xA0C6\x4AF1", MapID::Gandara_the_Moon_Fortress, GameRegion::KOURNA, }, // Kournan Pendants
    { 480, 3, L"\x293D", MapID::Diessa_Lowlands, GameRegion::ASCALON, }, // Singed Gargoyle Skulls
    { 818, 3, L"\x56E4\xDF8C\xAD76\x3958", MapID::Melandrus_Hope, GameRegion::ECHOVALD_FOREST, }, // Dredge Incisors
    { 502, 3, L"\x2955", MapID::Tascas_Demise, GameRegion::SOUTHERN_SHIVERPEAKS, }, // Stone Summit Badges
    { 27729, 3, L"\x8102\x26F4\xE764\xC908\x52E2", MapID::Arbor_Bay, GameRegion::TARNISHED_COAST, }, // Krait Skins
    { 1587, 2, L"\x8101\x43D0\x843D\x98D1\x775C", MapID::Jokos_Domain, GameRegion::DESOLATION, }, // Inscribed Shards
    { 836, 3, L"\x56F6\xB464\x9A9E\x11EF", MapID::Sunqua_Vale, GameRegion::SHING_JEA, }, // Feathered Scalps
    { 1583, 3, L"\x8101\x43EB\xF92D\xD469\x73A8", MapID::The_Sulfurous_Wastes, GameRegion::DESOLATION, }, // Mummy Wrappings
    { 441, 2, L"\x2916", MapID::The_Black_Curtain, GameRegion::KRYTA, }, // Shadowy Remnants
    { 856, 3, L"\x570B\xFE7B\xBD8A\x7CF4", MapID::The_Undercity, GameRegion::KAINENG_CITY, }, // Ancient Kappa Shells
    { 1681, 1, L"\x8101\x5206\x8286\xFEFA\x191C", MapID::Yatendi_Canyons, GameRegion::VABBI, }, // Geode
    { 27051, 2, L"\x8102\x26E8\xC0DB\xD26E\x4711", MapID::Grothmar_Wardowns, GameRegion::CHARR_HOMELANDS, }, // Fibrous Mandragor Roots
    { 482, 3, L"\x293F", MapID::Dragons_Gullet, GameRegion::ASCALON, }, // Gruesome Ribcages
    { 843, 2, L"\x56FD\xC65F\xF6F1\x26B4", MapID::Boreas_Seabed_explorable, GameRegion::JADE_SEA, }, // Kraken Eyes
    { 443, 3, L"\x2918", MapID::Scoundrels_Rise, GameRegion::KRYTA, }, // Bog Skale Fins
    { 19198, 2, L"\x8101\x583D\xB904\xF476\x59A7", MapID::Sunward_Marches, GameRegion::KOURNA, }, // Sentient Spores
    { 464, 2, L"\x292D", MapID::Sage_Lands, GameRegion::MAGUUMA_JUNGLE, }, // Ancient Eyes
    { 1577, 3, L"\x8101\x43E4\x8D6E\x83E5\x4C07", MapID::Cliffs_of_Dohjok, GameRegion::ISTAN, }, // Copper Shillings
    { 27042, 3, L"\x8102\x26DF\xF8E8\x8ACB\x58B4", MapID::Norrhart_Domains, GameRegion::FAR_SHIVERPEAKS, }, // Frigid Mandragor Husks
    { 926, 3, L"\x22D5\x8371\x8ED5\x56B4", MapID::Travelers_Vale, GameRegion::NORTHERN_SHIVERPEAKS, }, // Bolts of Linen
    { 423, 3, L"\x28EE", MapID::Flame_Temple_Corridor, GameRegion::ASCALON }, // Charr Carvings
};

// TODO: enc names
inline const char* zaishen_bounty_cycles[ZAISHEN_BOUNTY_COUNT] = {
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

// TODO: enc names
const char* zaishen_combat_cycles[ZAISHEN_COMBAT_COUNT] = {
    "Jade Quarry",
    "Codex Arena",
    "Heroes' Ascent",
    "Guild Versus Guild",
    "Alliance Battles",
    "Heroes' Ascent",
    "Guild Versus Guild",
    "Codex Arena",
    "Fort Aspenwood",
    "Jade Quarry",
    "Random Arena",
    "Codex Arena",
    "Guild Versus Guild",
    "Jade Quarry",
    "Alliance Battles",
    "Heroes' Ascent",
    "Random Arena",
    "Fort Aspenwood",
    "Jade Quarry",
    "Random Arena",
    "Fort Aspenwood",
    "Heroes' Ascent",
    "Alliance Battles",
    "Guild Versus Guild",
    "Codex Arena",
    "Random Arena",
    "Fort Aspenwood",
    "Alliance Battles"
};

// TODO: enc names/mission ids/something
const char* zaishen_mission_cycles[ZAISHEN_MISSION_COUNT] = {
    "Augury Rock",
    "Grand Court of Sebelkeh",
    "Ice Caves of Sorrow",
    "Raisu Palace",
    "Gate of Desolation",
    "Thirsty River",
    "Blacktide Den",
    "Against the Charr",
    "Abaddon's Mouth",
    "Nundu Bay",
    "Divinity Coast",
    "Zen Daijun",
    "Pogahn Passage",
    "Tahnnakai Temple",
    "The Great Northern Wall",
    "Dasha Vestibule",
    "The Wilds",
    "Unwaking Waters",
    "Chahbek Village",
    "Aurora Glade",
    "A Time for Heroes",
    "Consulate Docks",
    "Ring of Fire",
    "Nahpui Quarter",
    "The Dragon's Lair",
    "Dzagonur Bastion",
    "D'Alessio Seaboard",
    "Assault on the Stronghold",
    "The Eternal Grove",
    "Sanctum Cay",
    "Rilohn Refuge",
    "Warband of Brothers",
    "Borlis Pass",
    "Imperial Sanctum",
    "Moddok Crevice",
    "Nolani Academy",
    "Destruction's Depths",
    "Venta Cemetery",
    "Fort Ranik",
    "A Gate Too Far",
    "Minister Cho's Estate",
    "Thunderhead Keep",
    "Tihark Orchard",
    "Finding the Bloodstone",
    "Dunes of Despair",
    "Vizunah Square",
    "Jokanur Diggings",
    "Iron Mines of Moladune",
    "Kodonur Crossroads",
    "G.O.L.E.M.",
    "Arborstone",
    "Gates of Kryta",
    "Gate of Madness",
    "The Elusive Golemancer",
    "Riverside Province",
    "Boreas Seabed",
    "Ruins of Morah",
    "Hell's Precipice",
    "Ruins of Surmia",
    "Curse of the Nornbear",
    "Sunjiang District",
    "Elona Reach",
    "Gate of Pain",
    "Blood Washes Blood",
    "Bloodstone Fen",
    "Jennur's Horde",
    "Gyala Hatchery",
    "Abaddon's Gate",
    "The Frost Gate"
};

// TODO: enc names/map ids/something
const char* zaishen_vanquish_cycles[ZAISHEN_VANQUISH_COUNT] = {
    "Jaya Bluffs",
    "Holdings of Chokhin",
    "Ice Cliff Chasms",
    "Griffon's Mouth",
    "Kinya Province",
    "Issnur Isles",
    "Jaga Moraine",
    "Ice Floe",
    "Maishang Hills",
    "Jahai Bluffs",
    "Riven Earth",
    "Icedome",
    "Minister Cho's Estate",
    "Mehtani Keys",
    "Sacnoth Valley",
    "Iron Horse Mine",
    "Morostav Trail",
    "Plains of Jarin",
    "Sparkfly Swamp",
    "Kessex Peak",
    "Mourning Veil Falls",
    "The Alkali Pan",
    "Varajar Fells",
    "Lornar's Pass",
    "Pongmei Valley",
    "The Floodplain of Mahnkelon",
    "Verdant Cascades",
    "Majesty's Rest",
    "Raisu Palace",
    "The Hidden City of Ahdashim",
    "Rhea's Crater",
    "Mamnoon Lagoon",
    "Shadow's Passage",
    "The Mirror of Lyss",
    "Saoshang Trail",
    "Nebo Terrace",
    "Shenzun Tunnels",
    "The Ruptured Heart",
    "Salt Flats",
    "North Kryta Province",
    "Silent Surf",
    "The Shattered Ravines",
    "Scoundrel's Rise",
    "Old Ascalon",
    "Sunjiang District",
    "The Sulfurous Wastes",
    "Magus Stones",
    "Perdition Rock",
    "Sunqua Vale",
    "Turai's Procession",
    "Norrhart Domains",
    "Pockmark Flats",
    "Tahnnakai Temple",
    "Vehjin Mines",
    "Poisoned Outcrops",
    "Prophet's Path",
    "The Eternal Grove",
    "Tasca's Demise",
    "Resplendent Makuun",
    "Reed Bog",
    "Unwaking Waters",
    "Stingray Strand",
    "Sunward Marches",
    "Regent Valley",
    "Wajjun Bazaar",
    "Yatendi Canyons",
    "Twin Serpent Lakes",
    "Sage Lands",
    "Xaquang Skyway",
    "Zehlon Reach",
    "Tangle Root",
    "Silverwood",
    "Zen Daijun",
    "The Arid Sea",
    "Nahpui Quarter",
    "Skyward Reach",
    "The Scar",
    "The Black Curtain",
    "Panjiang Peninsula",
    "Snake Dance",
    "Traveler's Vale",
    "The Breach",
    "Lahtenda Bog",
    "Spearhead Peak",
    "Mount Qinkai",
    "Marga Coast",
    "Melandru's Hope",
    "The Falls",
    "Joko's Domain",
    "Vulture Drifts",
    "Wilderness of Bahdza",
    "Talmark Wilderness",
    "Vehtendi Valley",
    "Talus Chute",
    "Mineral Springs",
    "Anvil Rock",
    "Arborstone",
    "Witman's Folly",
    "Arkjok Ward",
    "Ascalon Foothills",
    "Bahdok Caverns",
    "Cursed Lands",
    "Alcazia Tangle",
    "Archipelagos",
    "Eastern Frontier",
    "Dejarin Estate",
    "Watchtower Coast",
    "Arbor Bay",
    "Barbarous Shore",
    "Deldrimor Bowl",
    "Boreas Seabed",
    "Cliffs of Dohjok",
    "Diessa Lowlands",
    "Bukdek Byway",
    "Bjora Marches",
    "Crystal Overlook",
    "Diviner's Ascent",
    "Dalada Uplands",
    "Drazach Thicket",
    "Fahranur, the First City",
    "Dragon's Gullet",
    "Ferndale",
    "Forum Highlands",
    "Dreadnought's Drift",
    "Drakkar Lake",
    "Dry Top",
    "Tears of the Fallen",
    "Gyala Hatchery",
    "Ettin's Back",
    "Gandara, the Moon Fortress",
    "Grothmar Wardowns",
    "Flame Temple Corridor",
    "Haiju Lagoon",
    "Frozen Forest",
    "Garden of Seborhin",
    "Grenth's Footprint"
};

// TODO: enc names etc
const char* wanted_by_shining_blade_cycles[WANTED_COUNT] = {
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

// TODO: enc strings?  should probably be some for login screen bonus info
const char* pve_weekly_bonus_cycles[WEEKLY_BONUS_PVE_COUNT] = {
    "Extra Luck",
    "Elonian Support",
    "Zaishen Bounty",
    "Factions Elite",
    "Northern Support",
    "Zaishen Mission",
    "Pantheon",
    "Faction Support",
    "Zaishen Vanquishing"
};

// TODO: enc strings?  should probably be some for login screen bonus info
const char* pve_weekly_bonus_descriptions[9] = {
    "Keys and lockpicks drop at four times the usual rate and double Lucky and Unlucky title points",
    "Double Sunspear and Lightbringer points",
    "Double copper Zaishen Coin rewards for Zaishen bounties",
    "The Deep and Urgoz's Warren can be entered from Kaineng Center",
    "Double Asura, Deldrimor, Ebon Vanguard, or Norn reputation points",
    "Double copper Zaishen Coin rewards for Zaishen missions",
    "Free passage to the Underworld and the Fissure of Woe",
    "Double Kurzick and Luxon title track points for exchanging faction",
    "Double copper Zaishen Coin rewards for Zaishen vanquishes"
};

// TODO: enc strings?  should probably be some for login screen bonus info
const char* pvp_weekly_bonus_cycles[WEEKLY_BONUS_PVP_COUNT] = {
    "Random Arenas",
    "Guild Versus Guild",
    "Competitive Mission",
    "Heroes' Ascent",
    "Codex Arena",
    "Alliance Battle"
};

// TODO: enc strings?  should probably be some for login screen bonus info
const char* pvp_weekly_bonus_descriptions[6] = {
    "Double Balthazar faction and Gladiator title points in Random Arenas",
    "Double Balthazar faction and Champion title points in GvG",
    "Double Balthazar and Imperial faction in the Jade Quarry and Fort Aspenwood",
    "Double Balthazar faction and Hero title points in Heroes' Ascent",
    "Double Balthazar faction and Codex title points in Codex Arena",
    "Double Balthazar and Imperial faction in Alliance Battles"
};
