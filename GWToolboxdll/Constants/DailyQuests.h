#pragma once

#include <GWCA/Constants/Maps.h>

namespace GuiUtils {
    class EncString;
}

using GW::Constants::MapID;

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

class NicholasCycleData final {
private:
    GuiUtils::EncString* _map_name;
public:
    GuiUtils::EncString* name_english;
    GuiUtils::EncString* name_translated;
    const int quantity;
    const MapID map_id;

    NicholasCycleData(const wchar_t* enc_name, int quantity, MapID map_id);
    ~NicholasCycleData();

    GuiUtils::EncString* map_name();
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

inline const NicholasCycleData nicholas_post_cycles[NICHOLAS_POST_COUNT] = {
    { L"\x271E\xDBDF\xBBD8\x34CB",       3, MapID::Regent_Valley                   }, // Red Iris Flowers
    { L"\x294F",                         3, MapID::Mineral_Springs                 }, // Feathered Avicara Scalps
    { L"\x8101\x43E8\xBEBD\xF09A\x4455", 2, MapID::Poisoned_Outcrops               }, // Margonite Masks
    { L"\x8102\x26DC\xADF9\x8D4B\x6575", 2, MapID::Alcazia_Tangle                  }, // Quetzal Crests
    { L"\x56D3\xA490\xE607\x11B6",       3, MapID::Wajjun_Bazaar                   }, // Plague Idols
    { L"\x294D",                         2, MapID::Dreadnoughts_Drift              }, // Azure Remains
    { L"\x8101\x52E9\xA18F\x8FE1\x3EE9", 1, MapID::Arkjok_Ward                     }, // Mandragor Root Cake
    { L"\x2960",                         1, MapID::Perdition_Rock                  }, // Mahgo Claw
    { L"\x56EF\xD1D8\xC773\x2C26",       5, MapID::Saoshang_Trail                  }, // Mantid Pincers
    { L"\x8101\x43DE\xD124\xA4D9\x7D4A", 3, MapID::Fahranur_The_First_City         }, // Sentient Seeds
    { L"\x8102\x26EA\x8A6F\xD31C\x31DD", 2, MapID::Sacnoth_Valley                  }, // Stone Grawl Necklaces
    { L"\x8102\x26D1",                   1, MapID::Twin_Serpent_Lakes              } , // Herring
    { L"\x5702\xA954\x959D\x51B8",       3, MapID::Mount_Qinkai                    }, // Naga Skins
    { L"\x296A",                         1, MapID::The_Falls                       }, // Gloom Seed
    { L"\x2882",                         1, MapID::The_Breach                      }, // Charr Hide
    { L"\x8101\x43F3\xF5F8\xC245\x41F2", 1, MapID::The_Alkali_Pan                  }, // Ruby Djinn Essence
    { L"\x2930",                         2, MapID::Majestys_Rest                   }, // Thorny Carapaces
    { L"\x56DD\xC82C\xB7E0\x3EB9",       3, MapID::Rheas_Crater                    }, // Bone Charms
    { L"\x8102\x26E0\xA884\xE2D3\x7E01", 3, MapID::Varajar_Fells                   }, // Modniir Manes
    { L"\x8102\x26E9\x96D3\x8E81\x64D1", 3, MapID::Dalada_Uplands                  }, // Superb Charr Carvings
    { L"\x22EE\xF65A\x86E6\x1C6C",       5, MapID::Zen_Daijun_explorable           }, // Rolls of Parchment
    { L"\x8101\x5208\xA22C\xC074\x2373", 2, MapID::Garden_of_Seborhin              }, // Roaring Ether Claws
    { L"\x8101\x5721",                   3, MapID::Bukdek_Byway                    }, // Branches of Juni Berries
    { L"\x2945",                         3, MapID::Deldrimor_Bowl                  }, // Shiverpeak Manes
    { L"\x293C",                         3, MapID::Eastern_Frontier                }, // Fetid Carapaces
    { L"\x6CCD\xC6FD\xA37B\x3529",       2, MapID::Gyala_Hatchery                  }, // Moon Shells
    { L"\x2921",                         1, MapID::The_Arid_Sea                    }, // Massive Jawbone
    { L"\x8102\x26FA\x8E00\xEA86\x3A1D", 1, MapID::Ice_Cliff_Chasms                }, // Chromatic Scale
    { L"\x292B",                         3, MapID::Ice_Floe                        }, // Mursaat Tokens
    { L"\x8101\x43FA\xA429\xC255\x23C4", 1, MapID::Bahdok_Caverns                  }, // Sentient Lodestone
    { L"\x2934",                         3, MapID::Tangle_Root                     }, // Jungle Troll Tusks
    { L"\x8101\x57DD\xF97D\xB5AD\x21FF", 1, MapID::Resplendent_Makuun              }, // Sapphire Djinn Essence
    { L"\x56E6\xB928\x9FA2\x43E1",       1, MapID::Arborstone_explorable           }, // Stone Carving
    { L"\x2919",                         3, MapID::North_Kryta_Province            }, // Feathered Caromi Scalps
    { L"\x8101\x52EE\xBF76\xE319\x2B39", 1, MapID::Holdings_of_Chokhin             }, // Pillaged Goods
    { L"\x56D5\x8B0F\xAB5B\x8A6",        1, MapID::Haiju_Lagoon                    }, // Gold Crimson Skull Coin
    { L"\x56D7\xDD87\x8A67\x167D",       3, MapID::Tahnnakai_Temple_explorable     }, // Jade Bracelets
    { L"\x2924",                         2, MapID::Prophets_Path                   }, // Minotaur Horns
    { L"\x294A",                         2, MapID::Snake_Dance                     }, // Frosted Griffon Wings
    { L"\x8101\x43E6\xBE4C\xE956\x780",  2, MapID::Mehtani_Keys                    }, // Silver Bullion Coins
    { L"\x56DF\xFEE4\xCA2D\x27A",        1, MapID::Morostav_Trail                  }, // Truffle
    { L"\x8102\x26DD\x85C5\xD98F\x5CCB", 3, MapID::Verdant_Cascades                }, // Skelk Claws
    { L"\x2923",                         2, MapID::The_Scar                        }, // Dessicated Hydra Claws
    { L"\x294B",                         3, MapID::Spearhead_Peak                  }, // Frigid Hearts
    { L"\x570A\x9453\x84A6\x64D4",       3, MapID::Nahpui_Quarter_explorable       }, // Celestial Essences
    { L"\x2937",                         1, MapID::Lornars_Pass                    }, // Phantom Residue
    { L"\x8101\x42D1\xFB15\xD39E\x5A26", 1, MapID::Issnur_Isles                    }, // Drake Kabob
    { L"\x55D0\xF8B7\xB108\x6018",       3, MapID::Ferndale                        }, // Amber Chunks
    { L"\x2914",                         2, MapID::Stingray_Strand                 }, // Glowing Hearts
    { L"\x8102\x26D8\xB5B9\x9AF6\x42D6", 5, MapID::Riven_Earth                     }, // Saurian Bones
    { L"\x8101\x5207\xEBD7\xB733\x2E27", 2, MapID::Wilderness_of_Bahdza            }, // Behemoth Hides
    { L"\x8101\x4E35\xD63F\xCAB4\xDD1",  1, MapID::Crystal_Overlook                }, // Luminous Stone
    { L"\x2950",                         3, MapID::Witmans_Folly                   }, // Intricate Grawl Necklaces
    { L"\x55D1\xD189\x845A\x7164",       3, MapID::Shadows_Passage                 }, // Jadeite Shards
    { L"\x8101\x43E5\xA891\xA83A\x426D", 1, MapID::Barbarous_Shore                 }, // Gold Doubloon
    { L"\x291B",                         2, MapID::Skyward_Reach                   }, // Shriveled Eyes
    { L"\x28EF",                         2, MapID::Icedome                         }, // Icy Lodestones
    { L"\x5701\xD258\xC958\x506F",       1, MapID::Silent_Surf                     }, // Keen Oni Talon
    { L"\x2910",                         2, MapID::Nebo_Terrace                    }, // Hardened Humps
    { L"\x8102\x26E7\xC330\xC111\x4058", 2, MapID::Drakkar_Lake                    }, // Piles of Elemental Dust
    { L"\x56F2\x876E\xEACB\x730",        3, MapID::Panjiang_Peninsula              }, // Naga Hides
    { L"\x22F3\xA11C\xC924\x5E15",       3, MapID::Griffons_Mouth                  }, // Spiritwood Planks
    { L"\x293A",                         1, MapID::Pockmark_Flats                  }, // Stormy Eye
    { L"\x8101\x43F0\xFF3B\x8E3E\x20B1", 3, MapID::Forum_Highlands                 }, // Skree Wings
    { L"\x5706\xC61F\xF23D\x3C4",        3, MapID::Raisu_Palace                    }, // Soul Stones
    { L"\x290F",                         1, MapID::Tears_of_the_Fallen             }, // Spiked Crest
    { L"\x56E5\x922D\xCF17\x7258",       1, MapID::Drazach_Thicket                 }, // Dragon Root
    { L"\x8102\x26E3\xB76F\xE56C\x1A2",  3, MapID::Jaga_Moraine                    }, // Berserker Horns
    { L"\x292E",                         1, MapID::Mamnoon_Lagoon                  }, // Behemoth Jaw
    { L"\x8101\x42D2\xE08B\xB81A\x604",  1, MapID::Zehlon_Reach                    }, // Bowl of Skalefin Soup
    { L"\x2915",                         2, MapID::Kessex_Peak                     }, // Forest Minotaur Horns
    { L"\x56ED\xE607\x9B27\x7299",       3, MapID::Sunjiang_District_explorable    }, // Putrid Cysts
    { L"\x2926",                         2, MapID::Salt_Flats                      }, // Jade Mandibles
    { L"\x292F",                         2, MapID::Silverwood                      }, // Maguuma Manes
    { L"\x56E0\xFBEB\xA429\x7B5",        1, MapID::The_Eternal_Grove               }, // Skull Juju
    { L"\x8101\x5840\xB4F5\xB2A7\x5E0F", 3, MapID::Lahtenda_Bog                    }, // Mandragor Swamproots
    { L"\x8101\x52EA",                   1, MapID::Vehtendi_Valley                 }, // Bottle of Vabbian Wine
    { L"\x8102\x26DA\x950E\x82F1\xA3D",  2, MapID::Magus_Stones                    }, // Weaver Legs
    { L"\x291F",                         1, MapID::Diviners_Ascent                 }, // Topaz Crest
    { L"\x56FC\xD503\x9D77\x730C",       2, MapID::Pongmei_Valley                  }, // Rot Wallow Tusks
    { L"\x2946",                         2, MapID::Anvil_Rock                      }, // Frostfire Fangs
    { L"\x8101\x43E7\xD854\xC981\x54DD", 1, MapID::The_Ruptured_Heart              }, // Demonic Relic
    { L"\x2917",                         2, MapID::Talmark_Wilderness              }, // Abnormal Seeds
    { L"\x8101\x43EA\xE72E\xAA23\x3C54", 1, MapID::The_Hidden_City_of_Ahdashim     }, // Diamond Djinn Essence
    { L"\x2928",                         2, MapID::Vulture_Drifts                  }, // Forgotten Seals
    { L"\x56D4\x8663\xA244\x50F5",       5, MapID::Kinya_Province                  }, // Copper Crimson Skull Coins
    { L"\x2932",                         3, MapID::Ettins_Back                     }, // Mossy Mandibles
    { L"\x2954",                         2, MapID::Grenths_Footprint               }, // Enslavement Stones
    { L"\x22E6\xE8F4\xA898\x75CB",       5, MapID::Jahai_Bluffs                    }, // Elonian Leather Squares
    { L"\x8101\x43EC\x8335\xBAA8\x153C", 2, MapID::Vehjin_Mines                    }, // Cobalt Talons
    { L"\x288C",                         1, MapID::Reed_Bog                        }, // Maguuma Spider Web
    { L"\x56EB\xB8B7\xF734\x2985",       5, MapID::Minister_Chos_Estate_explorable }, // Forgotten Trinket Boxes
    { L"\x2947",                         3, MapID::Iron_Horse_Mine                 }, // Icy Humps
    { L"\x8101\x43D2\x8CB3\xFC99\x602F", 1, MapID::The_Shattered_Ravines           }, // Sandblasted Lodestone
    { L"\x56FB\xA16B\x9DAD\x62B6",       3, MapID::Archipelagos                    }, // Black Pearls
    { L"\x8101\x43F7\xFD85\x9D52\x6DFA", 3, MapID::Marga_Coast                     }, // Insect Carapaces
    { L"\x2911",                         3, MapID::Watchtower_Coast                }, // Mergoyle Skulls
    { L"\x2957",                         3, MapID::Cursed_Lands                    }, // Decayed Orr Emblems
    { L"\x22E2\xCE9B\x8771\x7DC7",       5, MapID::Mourning_Veil_Falls             }, // Tempered Glass Vials
    { L"\x2943",                         3, MapID::Old_Ascalon                     }, // Scorched Lodestones
    { L"\x8101\x583C\xD7B3\xDD92\x598F", 1, MapID::Turais_Procession               }, // Water Djinn Essence
    { L"\x5703\xE1CC\xFE29\x4525",       1, MapID::Maishang_Hills                  }, // Guardian Moss
    { L"\x22C1",                         6, MapID::The_Floodplain_of_Mahnkelon     }, // Dwarven Ales
    { L"\x8102\x26D9\xABE9\x9082\x4999", 2, MapID::Sparkfly_Swamp                  }, // Amphibian Tongues
    { L"\x294E",                         2, MapID::Frozen_Forest                   }, // Alpine Seeds
    { L"\x2931",                         2, MapID::Dry_Top                         }, // Tangled Seeds
    { L"\x56FA\xE3AB\xA19E\x5D6A",       3, MapID::Jaya_Bluffs                     }, // Stolen Supplies
    { L"\x8101\x42D3\xD9E7\xD4E3\x259E", 1, MapID::Plains_of_Jarin                 }, // Pahnai Salad
    { L"\x5707\xF70E\xCAA2\x5CC5",       3, MapID::Xaquang_Skyway                  }, // Vermin Hides
    { L"\x8101\x52ED\x86E9\xCEF3\x69D3", 1, MapID::The_Mirror_of_Lyss              }, // Roaring Ether Heart
    { L"\x2941",                         3, MapID::Ascalon_Foothills               }, // Leathery Claws
    { L"\x56FE\xF2B0\x8B62\x116A",       1, MapID::Unwaking_Waters                 }, // Azure Crest
    { L"\x8102\x26E2\xC8E7\x8B1F\x716A", 1, MapID::Bjora_Marches                   }, // Jotun Pelt
    { L"\x8101\x583E\xE3F5\x87A2\x194F", 2, MapID::Dejarin_Estate                  }, // Heket Tongues
    { L"\x2951",                         5, MapID::Talus_Chute                     }, // Mountain Troll Tusks
    { L"\x22E7\xC1DA\xF2C1\x452A",       3, MapID::Shenzun_Tunnels                 }, // Vials of Ink
    { L"\x8101\x43E9\xDBD0\xA0C6\x4AF1", 3, MapID::Gandara_the_Moon_Fortress       }, // Kournan Pendants
    { L"\x293D",                         3, MapID::Diessa_Lowlands                 }, // Singed Gargoyle Skulls
    { L"\x56E4\xDF8C\xAD76\x3958",       3, MapID::Melandrus_Hope                  }, // Dredge Incisors
    { L"\x2955",                         3, MapID::Tascas_Demise                   }, // Stone Summit Badges
    { L"\x8102\x26F4\xE764\xC908\x52E2", 3, MapID::Arbor_Bay                       }, // Krait Skins
    { L"\x8101\x43D0\x843D\x98D1\x775C", 2, MapID::Jokos_Domain                    }, // Inscribed Shards
    { L"\x56F6\xB464\x9A9E\x11EF",       3, MapID::Sunqua_Vale                     }, // Feathered Scalps
    { L"\x8101\x43EB\xF92D\xD469\x73A8", 3, MapID::The_Sulfurous_Wastes            }, // Mummy Wrappings
    { L"\x2916",                         2, MapID::The_Black_Curtain               }, // Shadowy Remnants
    { L"\x570B\xFE7B\xBD8A\x7CF4",       3, MapID::The_Undercity                   }, // Ancient Kappa Shells
    { L"\x8101\x5206\x8286\xFEFA\x191C", 1, MapID::Yatendi_Canyons                 }, // Geode
    { L"\x8102\x26E8\xC0DB\xD26E\x4711", 2, MapID::Grothmar_Wardowns               }, // Fibrous Mandragor Roots
    { L"\x293F",                         3, MapID::Dragons_Gullet                  }, // Gruesome Ribcages
    { L"\x56FD\xC65F\xF6F1\x26B4",       2, MapID::Boreas_Seabed_explorable        }, // Kraken Eyes
    { L"\x2918",                         3, MapID::Scoundrels_Rise                 }, // Bog Skale Fins
    { L"\x8101\x583D\xB904\xF476\x59A7", 2, MapID::Sunward_Marches                 }, // Sentient Spores
    { L"\x292D",                         2, MapID::Sage_Lands                      }, // Ancient Eyes
    { L"\x8101\x43E4\x8D6E\x83E5\x4C07", 3, MapID::Cliffs_of_Dohjok                }, // Copper Shillings
    { L"\x8102\x26DF\xF8E8\x8ACB\x58B4", 3, MapID::Norrhart_Domains                }, // Frigid Mandragor Husks
    { L"\x22D5\x8371\x8ED5\x56B4",       3, MapID::Travelers_Vale                  }, // Bolts of Linen
    { L"\x28EE",                         3, MapID::Flame_Temple_Corridor           }, // Charr Carvings
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
inline const char* zaishen_combat_cycles[ZAISHEN_COMBAT_COUNT] = {
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
inline const char* zaishen_mission_cycles[ZAISHEN_MISSION_COUNT] = {
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
inline const char* zaishen_vanquish_cycles[ZAISHEN_VANQUISH_COUNT] = {
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
inline const char* wanted_by_shining_blade_cycles[WANTED_COUNT] = {
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
inline const char* pve_weekly_bonus_cycles[WEEKLY_BONUS_PVE_COUNT] = {
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
inline const char* pve_weekly_bonus_descriptions[9] = {
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
inline const char* pvp_weekly_bonus_cycles[WEEKLY_BONUS_PVP_COUNT] = {
    "Random Arenas",
    "Guild Versus Guild",
    "Competitive Mission",
    "Heroes' Ascent",
    "Codex Arena",
    "Alliance Battle"
};

// TODO: enc strings?  should probably be some for login screen bonus info
inline const char* pvp_weekly_bonus_descriptions[6] = {
    "Double Balthazar faction and Gladiator title points in Random Arenas",
    "Double Balthazar faction and Champion title points in GvG",
    "Double Balthazar and Imperial faction in the Jade Quarry and Fort Aspenwood",
    "Double Balthazar faction and Hero title points in Heroes' Ascent",
    "Double Balthazar faction and Codex title points in Codex Arena",
    "Double Balthazar and Imperial faction in Alliance Battles"
};
