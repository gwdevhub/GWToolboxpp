#include "stdafx.h"

#include <GWCA/GameEntities/Map.h>

#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <Utils/GuiUtils.h>
#include <GWToolbox.h>
#include <Logger.h>

#include <Windows/DailyQuestsWindow.h>


namespace {
    static const char* vanguard_cycles[9] = {
    "Bandits",
    "Utini Wupwup",
    "Ascalonian Noble",
    "Undead",
    "Blazefiend Griefblade",
    "Farmer Hamnet",
    "Charr",
    "Countess Nadya",
    "Footman Tate"
    };
    static const char* nicholas_sandford_cycles[52] = {
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
    static const char* nicholas_region_cycles[137] = {
        "Ascalon",
        "Southern Shiverpeaks",
        "The Desolation",
        "Tarnished Coast",
        "Kaineng City",
        "Southern Shiverpeaks",
        "Kourna",
        "Ring of Fire Islands",
        "Shing Jea Island",
        "Istan",
        "Charr Homelands",
        "Kryta",
        "The Jade Sea",
        "Maguuma Jungle",
        "Ascalon",
        "The Desolation",
        "Kryta",
        "The Jade Sea",
        "Far Shiverpeaks",
        "Charr Homelands",
        "Shing Jea Island",
        "Vabbi",
        "Kaineng City",
        "Northern Shiverpeaks",
        "Ascalon",
        "The Jade Sea",
        "Crystal Desert",
        "Far Shiverpeaks",
        "Southern Shiverpeaks",
        "Kourna",
        "Maguuma Jungle",
        "Vabbi",
        "Echovald Forest",
        "Kryta",
        "Vabbi",
        "Shing Jea Island",
        "Kaineng City",
        "Crystal Desert",
        "Southern Shiverpeaks",
        "Istan",
        "Echovald Forest",
        "Tarnished Coast",
        "Crystal Desert",
        "Southern Shiverpeaks",
        "Kaineng City",
        "Southern Shiverpeaks",
        "Istan",
        "Echovald Forest",
        "Kryta",
        "Tarnished Coast",
        "Vabbi",
        "The Desolation",
        "Southern Shiverpeaks",
        "Kaineng City",
        "Kourna",
        "Crystal Desert",
        "Southern Shiverpeaks",
        "The Jade Sea",
        "Kryta",
        "Far Shiverpeaks",
        "Shing Jea Island",
        "Northern Shiverpeaks",
        "Ascalon",
        "Vabbi",
        "Kaineng City",
        "Kryta",
        "Echovald Forest",
        "Far Shiverpeaks",
        "Maguuma Jungle",
        "Istan",
        "Kryta",
        "Kaineng City",
        "Crystal Desert",
        "Maguuma Jungle",
        "Echovald Forest",
        "Istan",
        "Vabbi",
        "Tarnished Coast",
        "Crystal Desert",
        "Kaineng City",
        "Northern Shiverpeaks",
        "The Desolation",
        "Kryta",
        "Vabbi",
        "Crystal Desert",
        "Shing Jea Island",
        "Maguuma Jungle",
        "Southern Shiverpeaks",
        "Kourna",
        "Vabbi",
        "Maguuma Jungle",
        "Shing Jea Island",
        "Northern Shiverpeaks",
        "The Desolation",
        "The Jade Sea",
        "Kourna",
        "Kryta",
        "Kryta",
        "Echovald Forest",
        "Ascalon",
        "The Desolation",
        "The Jade Sea",
        "Kourna",
        "Tarnished Coast",
        "Southern Shiverpeaks",
        "Maguuma Jungle",
        "Shing Jea Island",
        "Istan",
        "Kaineng City",
        "Vabbi",
        "Ascalon",
        "The Jade Sea",
        "Far Shiverpeaks",
        "Kourna",
        "Southern Shiverpeaks",
        "Kaineng City",
        "Kourna",
        "Ascalon",
        "Echovald Forest",
        "Southern Shiverpeaks",
        "Tarnished Coast",
        "The Desolation",
        "Shing Jea Island",
        "The Desolation",
        "Kryta",
        "Kaineng City",
        "Vabbi",
        "Charr Homelands",
        "Ascalon",
        "The Jade Sea",
        "Kryta",
        "Kourna",
        "Maguuma Jungle",
        "Istan",
        "Far Shiverpeaks",
        "Northern Shiverpeaks",
        "Ascalon"
    };
    static const uint32_t nicholas_quantity_cycles[137] = {
        3,
        3,
        2,
        2,
        3,
        2,
        1,
        1,
        5,
        3,
        2,
        1,
        3,
        1,
        1,
        1,
        2,
        3,
        3,
        3,
        5,
        2,
        3,
        3,
        3,
        2,
        1,
        1,
        3,
        1,
        3,
        1,
        1,
        3,
        1,
        1,
        3,
        2,
        2,
        2,
        1,
        3,
        2,
        3,
        3,
        1,
        1,
        3,
        2,
        5,
        2,
        1,
        3,
        3,
        1,
        2,
        2,
        1,
        2,
        2,
        3,
        3,
        1,
        3,
        3,
        1,
        1,
        3,
        1,
        1,
        2,
        3,
        2,
        2,
        1,
        3,
        1,
        2,
        1,
        2,
        2,
        1,
        2,
        1,
        2,
        5,
        3,
        2,
        5,
        2,
        1,
        5,
        3,
        1,
        3,
        3,
        3,
        3,
        5,
        3,
        1,
        1,
        6,
        2,
        2,
        2,
        3,
        1,
        3,
        1,
        3,
        1,
        1,
        2,
        5,
        3,
        3,
        3,
        3,
        3,
        3,
        2,
        3,
        3,
        2,
        3,
        1,
        2,
        3,
        2,
        3,
        2,
        2,
        3,
        3,
        3,
        3
    };
    static const char* nicholas_item_cycles[137] = {
        "Red Iris Flowers", // 0x271E 0xDBDF 0xBBD8 0x34CB 
        "Feathered Avicara Scalps", // 0x294f
        "Margonite Masks",
        "Quetzal Crests",
        "Plague Idols",
        "Azure Remains",
        "Mandragor Root Cake",
        "Mahgo Claw",
        "Mantid Pincers",
        "Sentient Seeds",
        "Stone Grawl Necklaces",
        "Herring",
        "Naga Skins",
        "Gloom Seed",
        "Charr Hide",
        "Ruby Djinn Essence",
        "Thorny Carapaces",
        "Bone Charms",
        "Modniir Manes",
        "Superb Charr Carvings",
        "Rolls of Parchment",
        "Roaring Ether Claws",
        "Branches of Juni Berries",
        "Shiverpeak Manes",
        "Fetid Carapaces",
        "Moon Shells",
        "Massive Jawbone",
        "Chromatic Scale",
        "Mursaat Tokens",
        "Sentient Lodestone",
        "Jungle Troll Tusks",
        "Sapphire Djinn Essence",
        "Stone Carving",
        "Feathered Caromi Scalps",
        "Pillaged Goods",
        "Gold Crimson Skull Coin",
        "Jade Bracelets",
        "Minotaur Horns",
        "Frosted Griffon Wings",
        "Silver Bullion Coins",
        "Truffle",
        "Skelk Claws",
        "Dessicated Hydra Claws",
        "Frigid Hearts",
        "Celestial Essences",
        "Phantom Residue",
        "Drake Kabob",
        "Amber Chunks",
        "Glowing Hearts",
        "Saurian Bones",
        "Behemoth Hides",
        "Luminous Stone",
        "Intricate Grawl Necklaces",
        "Jadeite Shards",
        "Gold Doubloon",
        "Shriveled Eyes",
        "Icy Lodestones",
        "Keen Oni Talon",
        "Hardened Humps",
        "Piles of Elemental Dust",
        "Naga Hides",
        "Spiritwood Planks",
        "Stormy Eye",
        "Skree Wings",
        "Soul Stones",
        "Spiked Crest",
        "Dragon Root",
        "Berserker Horns",
        "Behemoth Jaw",
        "Bowl of Skalefin Soup",
        "Forest Minotaur Horns",
        "Putrid Cysts",
        "Jade Mandibles",
        "Maguuma Manes",
        "Skull Juju",
        "Mandragor Swamproots",
        "Bottle of Vabbian Wine",
        "Weaver Legs",
        "Topaz Crest",
        "Rot Wallow Tusks",
        "Frostfire Fangs",
        "Demonic Relic",
        "Abnormal Seeds",
        "Diamond Djinn Essence",
        "Forgotten Seals",
        "Copper Crimson Skull Coins",
        "Mossy Mandibles",
        "Enslavement Stones",
        "Elonian Leather Squares",
        "Cobalt Talons",
        "Maguuma Spider Web",
        "Forgotten Trinket Boxes",
        "Icy Humps",
        "Sandblasted Lodestone",
        "Black Pearls",
        "Insect Carapaces",
        "Mergoyle Skulls",
        "Decayed Orr Emblems",
        "Tempered Glass Vials",
        "Scorched Lodestones",
        "Water Djinn Essence",
        "Guardian Moss",
        "Dwarven Ales",
        "Amphibian Tongues",
        "Alpine Seeds",
        "Tangled Seeds",
        "Stolen Supplies",
        "Pahnai Salad",
        "Vermin Hides",
        "Roaring Ether Heart",
        "Leathery Claws",
        "Azure Crest",
        "Jotun Pelt",
        "Heket Tongues",
        "Mountain Troll Tusks",
        "Vials of Ink",
        "Kournan Pendants",
        "Singed Gargoyle Skulls",
        "Dredge Incisors",
        "Stone Summit Badges",
        "Krait Skins",
        "Inscribed Shards",
        "Feathered Scalps",
        "Mummy Wrappings",
        "Shadowy Remnants",
        "Ancient Kappa Shells",
        "Geode",
        "Fibrous Mandragor Roots",
        "Gruesome Ribcages",
        "Kraken Eyes",
        "Bog Skale Fins",
        "Sentient Spores",
        "Ancient Eyes",
        "Copper Shillings",
        "Frigid Mandragor Husks",
        "Bolts of Linen",
        "Charr Carvings"
    };
    static const char* nicholas_location_cycles[137] = {
        "Regent Valley",
        "Mineral Springs",
        "Poisoned Outcrops",
        "Alcazia Tangle",
        "Wajjun Bazaar",
        "Dreadnought's Drift",
        "Arkjok Ward",
        "Perdition Rock",
        "Saoshang Trail",
        "Fahranur, The First City",
        "Sacnoth Valley",
        "Twin Serpent Lakes",
        "Mount Qinkai",
        "The Falls",
        "The Breach",
        "The Alkali Pan",
        "Majesty's Rest",
        "Rhea's Crater",
        "Varajar Fells",
        "Dalada Uplands",
        "Zen Daijun",
        "Garden of Seborhin",
        "Bukdek Byway",
        "Deldrimor Bowl",
        "Eastern Frontier",
        "Gyala Hatchery",
        "The Arid Sea",
        "Ice Cliff Chasms",
        "Ice Floe",
        "Bahdok Caverns",
        "Tangle Root",
        "Resplendent Makuun",
        "Arborstone",
        "North Kryta Province",
        "Holdings of Chokhin",
        "Haiju Lagoon",
        "Tahnnakai Temple",
        "Prophet's Path",
        "Snake Dance",
        "Mehtani Keys",
        "Morostav Trail",
        "Verdant Cascades",
        "The Scar",
        "Spearhead Peak",
        "Nahpui Quarter",
        "Lornar's Pass",
        "Issnur Isles",
        "Ferndale",
        "Stingray Strand",
        "Riven Earth",
        "Wilderness of Bahdza",
        "Crystal Overlook",
        "Witman's Folly",
        "Shadow's Passage",
        "Barbarous Shore",
        "Skyward Reach",
        "Icedome",
        "Silent Surf",
        "Nebo Terrace",
        "Drakkar Lake",
        "Panjiang Peninsula",
        "Griffon's Mouth",
        "Pockmark Flats",
        "Forum Highlands",
        "Raisu Palace",
        "Tears of the Fallen",
        "Drazach Thicket",
        "Jaga Moraine",
        "Mamnoon Lagoon",
        "Zehlon Reach",
        "Kessex Peak",
        "Sunjiang District",
        "Salt Flats",
        "Silverwood",
        "The Eternal Grove",
        "Lahtenda Bog",
        "Vehtendi Valley",
        "Magus Stones",
        "Diviner's Ascent",
        "Pongmei Valley",
        "Anvil Rock",
        "The Ruptured Heart",
        "Talmark Wilderness",
        "The Hidden City of Ahdashim",
        "Vulture Drifts",
        "Kinya Province",
        "Ettin's Back",
        "Grenth's Footprint",
        "Jahai Bluffs",
        "Vehjin Mines",
        "Reed Bog",
        "Minister Cho's Estate",
        "Iron Horse Mine",
        "The Shattered Ravines",
        "Archipelagos",
        "Marga Coast",
        "Watchtower Coast",
        "Cursed Lands",
        "Mourning Veil Falls",
        "Old Ascalon",
        "Turai's Procession",
        "Maishang Hills",
        "The Floodplain of Mahnkelon",
        "Sparkfly Swamp",
        "Frozen Forest",
        "Dry Top",
        "Jaya Bluffs",
        "Plains of Jarin",
        "Xaquang Skyway",
        "The Mirror of Lyss",
        "Ascalon Foothills",
        "Unwaking Waters",
        "Bjora Marches",
        "Dejarin Estate",
        "Talus Chute",
        "Shenzun Tunnels",
        "Gandara, the Moon Fortress",
        "Diessa Lowlands",
        "Melandru's Hope",
        "Tasca's Demise",
        "Arbor Bay",
        "Joko's Domain",
        "Sunqua Vale",
        "The Sulfurous Wastes",
        "The Black Curtain",
        "The Undercity",
        "Yatendi Canyons",
        "Grothmar Wardowns",
        "Dragon's Gullet",
        "Boreas Seabed",
        "Scoundrel's Rise",
        "Sunward Marches",
        "Sage Lands",
        "Cliffs of Dohjok",
        "Norrhart Domains",
        "Traveler's Vale",
        "Flame Temple Corridor"
    };
    static const char* zaishen_bounty_cycles[DailyQuests::zb_cnt] = {
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
    static const char* zaishen_combat_cycles[DailyQuests::zc_cnt] = {
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
    static const char* zaishen_mission_cycles[DailyQuests::zm_cnt] = {
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
    static const char* zaishen_vanquish_cycles[DailyQuests::zv_cnt] = {
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
    static const char* wanted_by_shining_blade_cycles[DailyQuests::ws_cnt] = {
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
    static const char* pve_weekly_bonus_cycles[DailyQuests::wbe_cnt] = {
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
    static const char* pve_weekly_bonus_descriptions[9] = {
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
    static const char* pvp_weekly_bonus_cycles[DailyQuests::wbp_cnt] = {
        "Random Arenas",
        "Guild Versus Guild",
        "Competitive Mission",
        "Heroes' Ascent",
        "Codex Arena",
        "Alliance Battle"
    };
    static const char* pvp_weekly_bonus_descriptions[6] = {
        "Double Balthazar faction and Gladiator title points in Random Arenas",
        "Double Balthazar faction and Champion title points in GvG",
        "Double Balthazar and Imperial faction in the Jade Quarry and Fort Aspenwood",
        "Double Balthazar faction and Hero title points in Heroes' Ascent",
        "Double Balthazar faction and Codex title points in Codex Arena",
        "Double Balthazar and Imperial faction in Alliance Battles"
    };

    bool subscriptions_changed = false;
    bool checked_subscriptions = false;
    time_t start_time;

    const wchar_t* DateString(time_t* unix) {
        std::tm* now = std::localtime(unix);
        static wchar_t buf[12];
        swprintf(buf, sizeof(buf), L"%d-%02d-%02d", now->tm_year + 1900, now->tm_mon + 1, now->tm_mday);
        return buf;
    }
    uint32_t GetZaishenBounty(time_t* unix) {
        return static_cast<uint32_t>((*unix - 1244736000) / 86400 % 66);
    }
    uint32_t GetWeeklyBonusPvE(time_t* unix) {
        return static_cast<uint32_t>((*unix - 1368457200) / 604800 % 9);
    }
    uint32_t GetWeeklyBonusPvP(time_t* unix) {
        return static_cast<uint32_t>((*unix - 1368457200) / 604800 % 6);
    }
    uint32_t GetZaishenCombat(time_t* unix) {
        return static_cast<uint32_t>((*unix - 1256227200) / 86400 % 28);
    }
    uint32_t GetZaishenMission(time_t* unix) {
        return static_cast<uint32_t>((*unix - 1299168000) / 86400 % 69);
    }
    uint32_t GetZaishenVanquish(time_t* unix) {
        return static_cast<uint32_t>((*unix - 1299168000) / 86400 % 136);
    }
    void PrintDaily(const wchar_t* label, const char* value, time_t unix, bool as_wiki_link = true) {
        bool show_date = unix != time(nullptr);
        wchar_t buf[139];
        if (show_date) {
            swprintf(buf, _countof(buf), as_wiki_link ? L"%s, %s: <a=1>\x200B%S</a>" : L"%s, %s: <a=1>%S</a>", label, DateString(&unix), value);
        }
        else {
            swprintf(buf, _countof(buf), as_wiki_link ? L"%s: <a=1>\x200B%S</a>" : L"%s: %S", label, value);
        }
        GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, buf, nullptr, true);
    }
}

// Find the "week start" for this timestamp.
time_t GetWeeklyRotationTime(time_t* unix) {
    return (time_t)(floor((*unix - 1368457200) / 604800) * 604800) + 1368457200;
}
time_t GetNextWeeklyRotationTime() {
    time_t unix = time(nullptr);
    return GetWeeklyRotationTime(&unix) + 604800;
}
const char* GetNicholasSandfordLocation(time_t* unix) {
    uint32_t cycle_index = static_cast<uint32_t>((*unix - 1239260400) / 86400 % 52);
    return nicholas_sandford_cycles[cycle_index];
}
uint32_t GetNicholasItemQuantity(time_t* unix) {
    uint32_t cycle_index = static_cast<uint32_t>((*unix - 1323097200) / 604800 % 137);
    return nicholas_quantity_cycles[cycle_index];
}
const char* GetNicholasLocation(time_t* unix) {
    uint32_t cycle_index = static_cast<uint32_t>((*unix - 1323097200) / 604800 % 137);
    return nicholas_location_cycles[cycle_index];
}
const char* GetNicholasItemName(time_t* unix) {
    uint32_t cycle_index = static_cast<uint32_t>((*unix - 1323097200) / 604800 % 137);
    return nicholas_item_cycles[cycle_index];
}
uint32_t GetWantedByShiningBlade(time_t* unix) {
    return static_cast<uint32_t>((*unix - 1276012800) / 86400 % 21);
}
const char* GetVanguardQuest(time_t* unix) {
    uint32_t cycle_index = static_cast<uint32_t>((*unix - 1299168000) / 86400 % 9);
    return vanguard_cycles[cycle_index];
}
bool GetIsPreSearing() {
    GW::AreaInfo* i = GW::Map::GetCurrentMapInfo();
    return i && i->region == GW::Region::Region_Presearing;
}
void DailyQuests::Draw(IDirect3DDevice9*) {
    if (!visible) return;

    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags()))
        return ImGui::End();
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
    if(show_zaishen_bounty_in_window) {
        ImGui::Text("Zaishen Bounty");
        ImGui::SameLine(offset += zb_width);
    }
    if(show_zaishen_combat_in_window) {
        ImGui::Text("Zaishen Combat");
        ImGui::SameLine(offset += zc_width);
    }
    if(show_zaishen_vanquishes_in_window) {
        ImGui::Text("Zaishen Vanquish");
        ImGui::SameLine(offset += zv_width);
    }
    if(show_wanted_quests_in_window) {
        ImGui::Text("Wanted");
        ImGui::SameLine(offset += ws_width);
    }
    if(show_nicholas_in_window) {
        ImGui::Text("Nicholas the Traveler");
        ImGui::SameLine(offset += nicholas_width);
    }
    if(show_weekly_bonus_pve_in_window) {
        ImGui::Text("Weekly Bonus PvE");
        ImGui::SameLine(offset += wbe_width);
    }
    if(show_weekly_bonus_pvp_in_window) {
        ImGui::Text("Weekly Bonus PvP");
        ImGui::SameLine(offset += long_text_width);
    }
    ImGui::NewLine();
    ImGui::Separator();
    ImGui::BeginChild("dailies_scroll", ImVec2(0, (-1 * (20.0f * ImGui::GetIO().FontGlobalScale)) - ImGui::GetStyle().ItemInnerSpacing.y));
    time_t unix = time(nullptr);
    uint32_t idx = 0;
    ImColor sCol(102, 187, 238, 255);
    ImColor wCol(255, 255, 255, 255);
    for (size_t i = 0; i < (size_t)daily_quest_window_count; i++) {
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
        if (show_zaishen_missions_in_window) {
            idx = GetZaishenMission(&unix);
            ImGui::TextColored(subscribed_zaishen_missions[idx] ? sCol : wCol, zaishen_mission_cycles[idx]);
            if (ImGui::IsItemClicked())
                subscribed_zaishen_missions[idx] = !subscribed_zaishen_missions[idx];
            ImGui::SameLine(offset += zm_width);
        }
        if (show_zaishen_bounty_in_window) {
            idx = GetZaishenBounty(&unix);
            ImGui::TextColored(subscribed_zaishen_bounties[idx] ? sCol : wCol, zaishen_bounty_cycles[idx]);
            if (ImGui::IsItemClicked())
                subscribed_zaishen_bounties[idx] = !subscribed_zaishen_bounties[idx];
            ImGui::SameLine(offset += zb_width);
        }
        if (show_zaishen_combat_in_window) {
            idx = GetZaishenCombat(&unix);
            ImGui::TextColored(subscribed_zaishen_combats[idx] ? sCol : wCol, zaishen_combat_cycles[idx]);
            if (ImGui::IsItemClicked())
                subscribed_zaishen_combats[idx] = !subscribed_zaishen_combats[idx];
            ImGui::SameLine(offset += zc_width);
        }
        if (show_zaishen_vanquishes_in_window) {
            idx = GetZaishenVanquish(&unix);
            ImGui::TextColored(subscribed_zaishen_vanquishes[idx] ? sCol : wCol, zaishen_vanquish_cycles[idx]);
            if (ImGui::IsItemClicked())
                subscribed_zaishen_vanquishes[idx] = !subscribed_zaishen_vanquishes[idx];
            ImGui::SameLine(offset += zv_width);
        }
        if (show_wanted_quests_in_window) {
            idx = GetWantedByShiningBlade(&unix);
            ImGui::TextColored(subscribed_wanted_quests[idx] ? sCol : wCol, wanted_by_shining_blade_cycles[idx]);
            if (ImGui::IsItemClicked())
                subscribed_wanted_quests[idx] = !subscribed_wanted_quests[idx];
            ImGui::SameLine(offset += ws_width);
        }
        if (show_nicholas_in_window) {
            ImGui::Text("%d %s", GetNicholasItemQuantity(&unix), GetNicholasItemName(&unix));
            ImGui::SameLine(offset += nicholas_width);
        }
        if (show_weekly_bonus_pve_in_window) {
            idx = GetWeeklyBonusPvE(&unix);
            ImGui::TextColored(subscribed_weekly_bonus_pve[idx] ? sCol : wCol, pve_weekly_bonus_cycles[idx]);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(pve_weekly_bonus_descriptions[idx]);
            if(ImGui::IsItemClicked())
                subscribed_weekly_bonus_pve[idx] = !subscribed_weekly_bonus_pve[idx];
            ImGui::SameLine(offset += wbe_width);
        }
        if (show_weekly_bonus_pvp_in_window) {
            idx = GetWeeklyBonusPvP(&unix);
            ImGui::TextColored(subscribed_weekly_bonus_pvp[idx] ? sCol : wCol, pvp_weekly_bonus_cycles[idx]);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(pvp_weekly_bonus_descriptions[idx]);
            if (ImGui::IsItemClicked())
                subscribed_weekly_bonus_pvp[idx] = !subscribed_weekly_bonus_pvp[idx];
            ImGui::SameLine(offset += long_text_width);
        }
        ImGui::NewLine();
        unix += 86400;
    }
    ImGui::EndChild();
    ImGui::TextDisabled("Click on a daily quest to get notified when its coming up. Subscribed quests are highlighted in ");
    ImGui::SameLine(0,0);
    ImGui::TextColored(sCol, "blue");
    ImGui::SameLine(0, 0);
    ImGui::TextDisabled(".");
    return ImGui::End();
}
void DailyQuests::DrawHelp() {
    if (!ImGui::TreeNodeEx("Daily Quest Chat Commands", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth))
        return;
    ImGui::Text("You can create a 'Send Chat' hotkey to perform any command.");
    ImGui::Bullet(); ImGui::Text("'/zb' prints current zaishen bounty.");
    ImGui::Bullet(); ImGui::Text("'/zm' prints current zaishen mission.");
    ImGui::Bullet(); ImGui::Text("'/zv' prints current zaishen vanquish.");
    ImGui::Bullet(); ImGui::Text("'/zc' prints current zaishen combat.");
    ImGui::Bullet(); ImGui::Text("'/vanguard' prints current pre-searing vanguard quest.");
    ImGui::Bullet(); ImGui::Text("'/wanted' prints current shining blade bounty.");
    ImGui::Bullet(); ImGui::Text("'/nicholas' prints current nicholas location.");
    ImGui::Bullet(); ImGui::Text("'/weekly' prints current weekly bonus.");
    ImGui::Bullet(); ImGui::Text("'/today' prints current daily activities.");
    ImGui::Bullet(); ImGui::Text("'/tomorrow' prints tomorrow's daily activities.");
    ImGui::TreePop();
}
void DailyQuests::DrawSettingInternal() {
    ToolboxWindow::DrawSettingInternal();
    ImGui::PushItemWidth(200.f * ImGui::FontScale());
    ImGui::InputInt("Show daily quests for the next N days", &daily_quest_window_count);
    ImGui::PopItemWidth();
    ImGui::Text("Quests to show in Daily Quests window:");
    ImGui::Indent();
    ImGui::StartSpacedElements(200.f);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Zaishen Bounty", &show_zaishen_bounty_in_window);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Zaishen Combat", &show_zaishen_combat_in_window);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Zaishen Mission", &show_zaishen_missions_in_window);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Zaishen Vanquish", &show_zaishen_vanquishes_in_window);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Wanted by Shining Blade", &show_wanted_quests_in_window);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Nicholas The Traveler", &show_nicholas_in_window);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Weekly Bonus (PvE)", &show_weekly_bonus_pve_in_window);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Weekly Bonus (PvP)", &show_weekly_bonus_pvp_in_window);
    
    ImGui::Unindent();
}
void DailyQuests::LoadSettings(ToolboxIni* ini) {
    ToolboxWindow::LoadSettings(ini);

    show_zaishen_bounty_in_window = ini->GetBoolValue(Name(), VAR_NAME(show_zaishen_bounty_in_window), show_zaishen_bounty_in_window);
    show_zaishen_combat_in_window = ini->GetBoolValue(Name(), VAR_NAME(show_zaishen_combat_in_window), show_zaishen_combat_in_window);
    show_zaishen_missions_in_window = ini->GetBoolValue(Name(), VAR_NAME(show_zaishen_missions_in_window), show_zaishen_missions_in_window);
    show_zaishen_vanquishes_in_window = ini->GetBoolValue(Name(), VAR_NAME(show_zaishen_vanquishes_in_window), show_zaishen_vanquishes_in_window);
    show_wanted_quests_in_window = ini->GetBoolValue(Name(), VAR_NAME(show_wanted_quests_in_window), show_wanted_quests_in_window);
    show_nicholas_in_window = ini->GetBoolValue(Name(), VAR_NAME(show_nicholas_in_window), show_nicholas_in_window);
    show_weekly_bonus_pve_in_window = ini->GetBoolValue(Name(), VAR_NAME(show_weekly_bonus_pve_in_window), show_weekly_bonus_pve_in_window);
    show_weekly_bonus_pvp_in_window = ini->GetBoolValue(Name(), VAR_NAME(show_weekly_bonus_pvp_in_window), show_weekly_bonus_pvp_in_window);

    const char* zms = ini->GetValue(Name(), VAR_NAME(subscribed_zaishen_missions), "0");
    std::bitset<zm_cnt> zmb(zms);
    for (unsigned int i = 0; i < zmb.size(); ++i) {
        subscribed_zaishen_missions[i] = zmb[i] == 1;
    }

    const char* zbs = ini->GetValue(Name(), VAR_NAME(subscribed_zaishen_bounties), "0");
    std::bitset<zb_cnt> zbb(zbs);
    for (unsigned int i = 0; i < zbb.size(); ++i) {
        subscribed_zaishen_bounties[i] = zbb[i] == 1;
    }

    const char* zcs = ini->GetValue(Name(), VAR_NAME(subscribed_zaishen_combats), "0");
    std::bitset<zc_cnt> zcb(zcs);
    for (unsigned int i = 0; i < zcb.size(); ++i) {
        subscribed_zaishen_combats[i] = zcb[i] == 1;
    }

    const char* zvs = ini->GetValue(Name(), VAR_NAME(subscribed_zaishen_vanquishes), "0");
    std::bitset<zv_cnt> zvb(zvs);
    for (unsigned int i = 0; i < zvb.size(); ++i) {
        subscribed_zaishen_vanquishes[i] = zvb[i] == 1;
    }

    const char* wss = ini->GetValue(Name(), VAR_NAME(subscribed_wanted_quests), "0");
    std::bitset<ws_cnt> wsb(wss);
    for (unsigned int i = 0; i < wsb.size(); ++i) {
        subscribed_wanted_quests[i] = wsb[i] == 1;
    }

    const char* wbes = ini->GetValue(Name(), VAR_NAME(subscribed_weekly_bonus_pve), "0");
    std::bitset<wbe_cnt> wbeb(wbes);
    for (unsigned int i = 0; i < wbeb.size(); ++i) {
        subscribed_weekly_bonus_pve[i] = wbeb[i] == 1;
    }

    const char* wbps = ini->GetValue(Name(), VAR_NAME(subscribed_weekly_bonus_pvp), "0");
    std::bitset<wbp_cnt> wbpb(wbps);
    for (unsigned int i = 0; i < wbpb.size(); ++i) {
        subscribed_weekly_bonus_pvp[i] = wbpb[i] == 1;
    }
}
void DailyQuests::SaveSettings(ToolboxIni* ini) {
    ToolboxWindow::SaveSettings(ini);

    ini->SetBoolValue(Name(), VAR_NAME(show_zaishen_bounty_in_window), show_zaishen_bounty_in_window);
    ini->SetBoolValue(Name(), VAR_NAME(show_zaishen_combat_in_window), show_zaishen_combat_in_window);
    ini->SetBoolValue(Name(), VAR_NAME(show_zaishen_missions_in_window), show_zaishen_missions_in_window);
    ini->SetBoolValue(Name(), VAR_NAME(show_zaishen_vanquishes_in_window), show_zaishen_vanquishes_in_window);
    ini->SetBoolValue(Name(), VAR_NAME(show_wanted_quests_in_window), show_wanted_quests_in_window);
    ini->SetBoolValue(Name(), VAR_NAME(show_nicholas_in_window), show_nicholas_in_window);
    ini->SetBoolValue(Name(), VAR_NAME(show_weekly_bonus_pve_in_window), show_weekly_bonus_pve_in_window);
    ini->SetBoolValue(Name(), VAR_NAME(show_weekly_bonus_pvp_in_window), show_weekly_bonus_pvp_in_window);
    std::bitset<zm_cnt> zmb;
    for (unsigned int i = 0; i < zmb.size(); ++i) {
        zmb[i] = subscribed_zaishen_missions[i] ? 1 : 0;
    }
    ini->SetValue(Name(), VAR_NAME(subscribed_zaishen_missions), zmb.to_string().c_str());

    std::bitset<zb_cnt> zbb;
    for (unsigned int i = 0; i < zbb.size(); ++i) {
        zbb[i] = subscribed_zaishen_bounties[i] ? 1 : 0;
    }
    ini->SetValue(Name(), VAR_NAME(subscribed_zaishen_bounties), zbb.to_string().c_str());

    std::bitset<zc_cnt> zcb;
    for (unsigned int i = 0; i < zcb.size(); ++i) {
        zcb[i] = subscribed_zaishen_combats[i] ? 1 : 0;
    }
    ini->SetValue(Name(), VAR_NAME(subscribed_zaishen_combats), zcb.to_string().c_str());

    std::bitset<zv_cnt> zvb;
    for (unsigned int i = 0; i < zvb.size(); ++i) {
        zvb[i] = subscribed_zaishen_vanquishes[i] ? 1 : 0;
    }
    ini->SetValue(Name(), VAR_NAME(subscribed_zaishen_vanquishes), zvb.to_string().c_str());

    std::bitset<ws_cnt> wsb;
    for (unsigned int i = 0; i < wsb.size(); ++i) {
        wsb[i] = subscribed_wanted_quests[i] ? 1 : 0;
    }
    ini->SetValue(Name(), VAR_NAME(subscribed_wanted_quests), wsb.to_string().c_str());

    std::bitset<wbe_cnt> wbeb;
    for (unsigned int i = 0; i < wbeb.size(); ++i) {
        wbeb[i] = subscribed_weekly_bonus_pve[i] ? 1 : 0;
    }
    ini->SetValue(Name(), VAR_NAME(subscribed_weekly_bonus_pve), wbeb.to_string().c_str());

    std::bitset<wbp_cnt> wbpb;
    for (unsigned int i = 0; i < wbpb.size(); ++i) {
        wbpb[i] = subscribed_weekly_bonus_pvp[i] ? 1 : 0;
    }
    ini->SetValue(Name(), VAR_NAME(subscribed_weekly_bonus_pvp), wbpb.to_string().c_str());
}

void DailyQuests::Initialize() {
    ToolboxWindow::Initialize();

    GW::Chat::CreateCommand(L"zm", DailyQuests::CmdZaishenMission);
    GW::Chat::CreateCommand(L"zb", DailyQuests::CmdZaishenBounty);
    GW::Chat::CreateCommand(L"zc", DailyQuests::CmdZaishenCombat);
    GW::Chat::CreateCommand(L"zv", DailyQuests::CmdZaishenVanquish);
    GW::Chat::CreateCommand(L"vanguard", DailyQuests::CmdVanguard);
    GW::Chat::CreateCommand(L"wanted", DailyQuests::CmdWantedByShiningBlade);
    GW::Chat::CreateCommand(L"nicholas", DailyQuests::CmdNicholas);
    GW::Chat::CreateCommand(L"weekly", DailyQuests::CmdWeeklyBonus);
    GW::Chat::CreateCommand(L"today", [](const wchar_t* message, int argc, LPWSTR* argv) -> void {
        UNREFERENCED_PARAMETER(message);
        UNREFERENCED_PARAMETER(argc);
        UNREFERENCED_PARAMETER(argv);
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
    });
    GW::Chat::CreateCommand(L"daily", [](const wchar_t* message, int argc, LPWSTR* argv) -> void {
        UNREFERENCED_PARAMETER(message);
        UNREFERENCED_PARAMETER(argc);
        UNREFERENCED_PARAMETER(argv);
        GW::Chat::SendChat('/', "today");
    });
    GW::Chat::CreateCommand(L"dailies", [](const wchar_t* message, int argc, LPWSTR* argv) -> void {
        UNREFERENCED_PARAMETER(message);
        UNREFERENCED_PARAMETER(argc);
        UNREFERENCED_PARAMETER(argv);
        GW::Chat::SendChat('/', "today");
    });
    GW::Chat::CreateCommand(L"tomorrow", [](const wchar_t* message, int argc, LPWSTR* argv) -> void {
        UNREFERENCED_PARAMETER(message);
        UNREFERENCED_PARAMETER(argc);
        UNREFERENCED_PARAMETER(argv);
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
    });
}

void DailyQuests::Update(float delta) {
    UNREFERENCED_PARAMETER(delta);
    if (subscriptions_changed)
        checked_subscriptions = false;
    if (checked_subscriptions)
        return;
    subscriptions_changed = false;
    if (!start_time)
        start_time = time(nullptr);
    if (GW::Map::GetIsMapLoaded() && (time(nullptr) - start_time) > 1) {
        checked_subscriptions = true;
        // Check daily quests for the next 6 days, and send a message if found. Only runs once when TB is opened.
        time_t now = time(nullptr);
        time_t unix = now + 0;
        uint32_t quest_idx;
        for (unsigned int i = 0; i < subscriptions_lookahead_days; i++) {
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
            if (subscribed_zaishen_missions[quest_idx = GetZaishenMission(&unix)])
                Log::Info("%s is the Zaishen Mission %s", zaishen_mission_cycles[quest_idx], date_str);
            if (subscribed_zaishen_bounties[quest_idx = GetZaishenBounty(&unix)])
                Log::Info("%s is the Zaishen Bounty %s", zaishen_bounty_cycles[quest_idx], date_str);
            if (subscribed_zaishen_combats[quest_idx = GetZaishenCombat(&unix)])
                Log::Info("%s is the Zaishen Combat %s", zaishen_combat_cycles[quest_idx], date_str);
            if (subscribed_zaishen_vanquishes[quest_idx = GetZaishenVanquish(&unix)])
                Log::Info("%s is the Zaishen Vanquish %s", zaishen_vanquish_cycles[quest_idx], date_str);
            if (subscribed_wanted_quests[quest_idx = GetWantedByShiningBlade(&unix)])
                Log::Info("%s is Wanted by the Shining Blade %s", wanted_by_shining_blade_cycles[quest_idx], date_str);
            unix += 86400;
        }

        // Check weekly bonuses / special events
        unix = GetWeeklyRotationTime(&now);
        for (unsigned int i = 0; i < 2; i++) {
            char date_str[32];
            switch (i) {
                case 0: std::strftime(date_str, 32, "until %R on %A", std::localtime(&unix)); break;
                default: std::strftime(date_str, 32, "on %A at %R", std::localtime(&unix)); break;
            }
            if (subscribed_weekly_bonus_pve[quest_idx = GetWeeklyBonusPvE(&unix)])
                Log::Info("%s is the Weekly PvE Bonus %s", pve_weekly_bonus_cycles[quest_idx], date_str);
            if (subscribed_weekly_bonus_pvp[quest_idx = GetWeeklyBonusPvP(&unix)])
                Log::Info("%s is the Weekly PvP Bonus %s", pvp_weekly_bonus_cycles[quest_idx], date_str);
            unix += 604800;
        }
    }
}

void DailyQuests::CmdWeeklyBonus(const wchar_t* , int argc, LPWSTR* argv) {
    time_t now = time(nullptr);
    if (argc > 1 && !wcscmp(argv[1], L"tomorrow"))
        now += 86400;
    PrintDaily(L"Weekly Bonus PvE", pve_weekly_bonus_cycles[GetWeeklyBonusPvE(&now)], now);
    PrintDaily(L"Weekly Bonus PvP", pvp_weekly_bonus_cycles[GetWeeklyBonusPvP(&now)], now);
}
void DailyQuests::CmdZaishenBounty(const wchar_t *, int argc, LPWSTR *argv) {
    time_t now = time(nullptr);
    if (argc > 1 && !wcscmp(argv[1], L"tomorrow"))
        now += 86400;
    PrintDaily(L"Zaishen Bounty", zaishen_bounty_cycles[GetZaishenBounty(&now)], now);
}
void DailyQuests::CmdZaishenMission(const wchar_t *, int argc, LPWSTR *argv) {
    time_t now = time(nullptr);
    if (argc > 1 && !wcscmp(argv[1], L"tomorrow"))
        now += 86400;
    PrintDaily(L"Zaishen Mission", zaishen_mission_cycles[GetZaishenMission(&now)], now);
}
void DailyQuests::CmdZaishenVanquish(const wchar_t *, int argc, LPWSTR *argv) {
    time_t now = time(nullptr);
    if (argc > 1 && !wcscmp(argv[1], L"tomorrow"))
        now += 86400;
    PrintDaily(L"Zaishen Vanquish", zaishen_vanquish_cycles[GetZaishenVanquish(&now)], now);
}
void DailyQuests::CmdZaishenCombat(const wchar_t *, int argc, LPWSTR *argv) {
    time_t now = time(nullptr);
    if (argc > 1 && !wcscmp(argv[1], L"tomorrow"))
        now += 86400;
    PrintDaily(L"Zaishen Combat", zaishen_combat_cycles[GetZaishenCombat(&now)], now);
}
void DailyQuests::CmdWantedByShiningBlade(const wchar_t *, int argc, LPWSTR *argv) {
    time_t now = time(nullptr);
    if (argc > 1 && !wcscmp(argv[1], L"tomorrow"))
        now += 86400;
    PrintDaily(L"Wanted", wanted_by_shining_blade_cycles[GetWantedByShiningBlade(&now)], now);
}
void DailyQuests::CmdVanguard(const wchar_t *, int argc, LPWSTR *argv) {
    time_t now = time(nullptr);
    if (argc > 1 && !wcscmp(argv[1], L"tomorrow"))
        now += 86400;
    PrintDaily(L"Vanguard Quest", GetVanguardQuest(&now), now);
}
void DailyQuests::CmdNicholas(const wchar_t *, int argc, LPWSTR *argv) {
    time_t now = time(nullptr);
    if (argc > 1 && !wcscmp(argv[1], L"tomorrow"))
        now += 86400;
    char buf[128];
    if (GetIsPreSearing()) {
        snprintf(buf, _countof(buf), "5 %s", GetNicholasSandfordLocation(&now));
        PrintDaily(L"Nicholas Sandford", buf, now,false);
    }
    else {
        snprintf(buf, _countof(buf), "%d %s in %s", GetNicholasItemQuantity(&now), GetNicholasItemName(&now), GetNicholasLocation(&now));
        PrintDaily(L"Nicholas the Traveler", buf, now,false);
    }
}
