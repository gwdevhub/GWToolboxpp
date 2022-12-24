#pragma once
#include <GWCA/Constants/Constants.h>

namespace CompletionWindow_Constants {

    using namespace GW::Constants;

    const char* campaign_names[] = {"Core", "Prophecies", "Factions", "Nightfall", "Eye Of The North", "Dungeons"};

    const char* CampaignName(const Campaign camp) { return campaign_names[static_cast<uint8_t>(camp)]; }
    const char* hero_names[] = {"", "Norgu", "Goren", "Tahlkora", "Master of Whispers", "Acolyte Jin", "Koss", "Dunkoro", "Acolyte Sousuke", "Melonni", "Zhed Shadowhoof", "General Morgahn", "Margrid the Sly", "Zenmai", "Olias", "Razah", "M.O.X.",
        "Keiran Thackeray", "Jora", "Pyre Fierceshot", "Anton", "Livia", "Hayda", "Kahmu", "Gwen", "Xandra", "Vekk", "Ogden Stonehealer", "", "", "", "", "", "", "", "", "Miku", "Zei Ri"};

    const wchar_t* encoded_festival_hat_names[] = {
        // Halloween
        L"\x8102\x5C2B\xB7F4\xC976\x5CE1", // Charr hat
        L"\x8101\x5EBF\xE584\x8367\x5830", // Furious pumpkin crown
        L"\x8102\x6E5B\x9CE4\xF433\x7437", // Furrocious ears
        L"\x8102\x34F7\xD06F\x9B03\x1D89",        // Mummy mask
        L"\x8102\x34F6\x9A67\xE2B0\x19A6",       // Lupine mask
        L"\x245D\xCD3B\xC2E1\x2AC3",       // Pumpkin crown
        L"\x8102\x7F39\xEDE6\xF160\x552",  // Reapers hood
        L"\x8102\x34F8\xE9FE\x9F4D\x4B4A",    // Scarecrow Mask
        L"\x8102\x5C2A\xBF3B\x9294\x717F", // Skeleton face paint
        L"\x8102\x6E5C\xE744\xE51D\x549D", // Spectercles
        L"\x8102\x7F3A\xD2B3\xE31C\x8F1",  // Tricorne
        L"\x8101\x5EC0\xD174\xBB32\x1697", // Wicked hat
        L"\x8102\x4997\x8F3A\xB679\x3196", // Zombie face paint
        // Wintersday
        L"\x8102\x6E5D",                   // Demonic horns
        L"\x8102\x6E16",                   // Divine halo
        L"\x8103\x1E6\xCA8E\xFEBE\x13F1",  // Festive winter hood
        L"\x8101\x60FF\xD57C\xEE89\x1B58", // Freezie crown
        L"\x8101\x6100\xE654\xF2D6\x4A8",  // Great horns of grenth
        L"\x8102\x34F3\xA758\x90C4\x16B6",       // Grentch Cap
        L"\x2481\x821F\xD73C\x4CD2",       // Horns of Grenth
        L"\x8102\x34F1\xDF5B\xCBEB\x51B3",         // Ice crown
        L"\x8102\x5EA2\xF571\xCFCB\x35A6", // Ice shard crest
        L"\x8101\x60FE\xDE91\x9A0C\x158B",       // Jesters cap
        L"\x8102\x34F4\xB9A7\x8ED1\x23ED",         // Rudi mask
        L"\x8102\x5EA3\xBF84\xA60F\xC33",  // Snow crystal crest
        L"\x8103\x1E9\xDD16\x9987\x490C",  // Stylish black scarf
        L"\x8103\x1E7\xDD75\xA264\x4968",  // Stylish red striped scarf
        L"\x8103\x1E8\xDBFA\xB129\x3687",  // Stylish white striped scarf
        L"\x8101\x6101\xCAC9\xC13F\x5086", // Stylish yule cap
        L"\x8102\x34F2\xBED1\xD377\x12A0",      // Wreath crown
        L"\x2482\x81E2\xEE52\x471F",       // Yule cap
        // Dragon Festival
        L"\x8102\x216A\xF512\xCE87\x46F",     // Demon mask
        L"\x8101\x151F\x918F\xB47E\x36CA",  // Dragon mask
        L"\x8102\x4669\xAC2E\xFF9A\x29A4",        // Grasping mask
        L"\x8102\x5A95\xB979\xA70E\x1639",    // Imperial dragon mask
        L"\x8101\x66FE\xE888\xCFB3\x1F77",    // Lion mask
        L"\x8102\x7699\x94D7\xAEA5\x2431", // Mirthful dragon mask
        L"\x8102\x6901\x8F3A\xC47B\x4E1A",  // Sinister dragon mask
        L"\x8101\x3DE\xE3E9\xAFAA\x152"  // Tengu mask
    };
    constexpr size_t wintersday_index = 13;      // Index in the encoded_festival_hat_names array where wintersday hats start
    constexpr size_t dragon_festival_index = 31; // Index in the encoded_festival_hat_names array where wintersday hats start

    // This array is keyed in the order of armors listed in HallOfMonumentsModule::Detail enum
    // i.e. index 0 is Elite Canthan Armor.
    const wchar_t* encoded_armor_names[] = {
        L"\x108\x107"
        "Elite Canthan Armor\x1", // Elite Canthan Armor
        L"\x108\x107"
        "Elite Exotic Armor\x1", // Elite Exotic Armor
        L"\x108\x107"
        "Elite Kurzick Armor\x1", // Elite Kurzick Armor
        L"\x108\x107"
        "Elite Luxon Armor\x1", // Elite Luxon Armor
        L"\x108\x107"
        "Imperial Ascended Armor\x1", // Imperial Ascended Armor
        L"\x108\x107"
        "Ancient Armor\x1", // Ancient Armor
        L"\x108\x107"
        "Elite Sunspear Armor\x1", // Elite Sunspear Armor
        L"\x108\x107"
        "Vabbian Armor\x1", // Vabbian Armor
        L"\x108\x107"
        "Primeval Armor\x1", // Primeval Armor
        L"\x108\x107"
        "Asuran Armor\x1", // Asuran Armor
        L"\x108\x107"
        "Norn Armor\x1", // Norn Armor
        L"\x108\x107"
        "Silver Eagle Armor\x1", // Silver Eagle Armor
        L"\x108\x107"
        "Monument Armor\x1", // Monument Armor
        L"\x108\x107"
        "Obsidian Armor\x1", // Obsidian Armor
        L"\x108\x107"
        "Granite Citadel Elite Armor\x1", // Granite Citadel Elite Armor
        L"\x108\x107"
        "Granite Citadel Exclusive Armor\x1", // Granite Citadel Exclusive Armor
        L"\x108\x107"
        "Granite Citadel Ascended Armor\x1", // Granite Citadel Ascended Armor
        L"\x108\x107"
        "Marhan's Grotto Elite Armor\x1", // Marhans Grottol Ascended Armor
        L"\x108\x107"
        "Marhan's Grotto Exclusive Armor\x1", // Marhans Grotto Ascended Armor
        L"\x108\x107"
        "Marhan's Grotto Ascended Armor\x1", // Marhans Grotto Ascended Armor
    };
    static_assert(_countof(encoded_armor_names) == (size_t)ResilienceDetail::Count);

    // This array is keyed in the order of weapons listed in HallOfMonumentsModule::Detail enum
    // i.e. index 0 is Destroyer Axe.
    const wchar_t* encoded_weapon_names[] = {
        L"\x8101\x7776\xCCA9\xBAA8\x10E0", // Destroyer Axe
        L"\x8101\x7777\xA0D7\x9027\x2458", // Destroyer Bow
        L"\x8101\x7778\xB879\xDFF6\x3310", // Destroyer Daggers
        L"\x8101\x7779\x83CC\xCECC\x5CA4", // Destroyer Focus
        L"\x8101\x777A\xDC41\x9DBE\x663A", // Destroyer Hammer
        L"\x8101\x777B\xB050\xBB40\x245B", // Destroyer Wandz
        L"\x8101\x777C\xFFD7\xE16E\x4BEE", // Destroyer Scythe
        L"\x8101\x777D\xCA7A\xB9E2\x3BDD", // Destroyer Shield
        L"\x8101\x777E\xB3DD\x830E\x4CA1", // Destroyer Spear
        L"\x8101\x777F\xCCF0\xA1E7\x2A5E", // Destroyer Staff
        L"\x8101\x7780\x8DAB\xA3C4\x48B1", // Destroyer Sword

        L"\x108\x107"
        "Tormented Axe\x1", // Tormented Axe
        L"\x108\x107"
        "Tormented Bow\x1", // Tormented Bow
        L"\x108\x107"
        "Tormented Daggers\x1", // Tormented Daggers
        L"\x108\x107"
        "Tormented Focus\x1", // Tormented Focus
        L"\x108\x107"
        "Tormented Hammer\x1", // Tormented Hammer
        L"\x108\x107"
        "Tormented Scepter\x1", // Tormented Scepter
        L"\x108\x107"
        "Tormented Scythe\x1",            // Tormented Scythe
        L"\x8102\x45E0\xDC95\xF3B4\x404", // Tormented Shield
        L"\x108\x107"
        "Tormented Spear\x1",             // Tormented Spear
        L"\x8102\x45E2\xA1E4\xBB9E\x2A8", // Tormented Staff
        L"\x108\x107"
        "Tormented Sword\x1", // Tormented Sword

        L"\x8102\xEDD\xD560\xED5C\x2578",  // Oppressor's Axe
        L"\x8102\xEDE\x945E\x98D8\x4698",  // Oppressor's Bow
        L"\x8102\x2C72\xCC78\xA2B4\x5F85", // Oppressor's Daggers
        L"\x8102\x6B5C\x9773\xD778\x3567", // Oppressor's Focus
        L"\x108\x107"
        "Oppressor's Hammer\x1",           // Oppressor's Hammer
        L"\x8102\x6B5E\x9964\xCAF9\x700D", // Oppressor's Scepter
        L"\x8102\x6B5F\x8E3F\x8145\x1956", // Oppressor's Scythe
        L"\x8102\x6B60\xFC25\xD943\x329F", // Oppressor's Shield
        L"\x8102\x6B61\xC1EA\xD1AF\x4F8",  // Oppressor's Spear
        L"\x8102\x6B62\xB5BE\xA6EE\x2937", // Oppressor's Staff
        L"\x8102\x6B63\x9222\xF8D1\x5715", // Oppressor's Sword
    };
    static_assert(_countof(encoded_weapon_names) == (size_t)ValorDetail::Count);

    // NOTE: Do NOT try to reorder this list; the keys are used to identify which minipet is which in the tracker across saves.
    const wchar_t* encoded_minipet_names[] = {

        L"\x8101\x730C",                   // Aatxe
        L"\x8102\x4509",                   // Abyssal
        L"\x8101\x682F",                   // Asura
        L"\x8102\x450A",                   // Black Beast of Aaaaarrrrrrggghhh
        L"\x8102\x2176\xA5D1\x8A87\x6C96", // Black Moa Chick
        L"\x8101\x3E8\xB1EC\xA471\xA12",   // Bone Dragon
        L"\x8102\x5387\x8E7B\xC70D\x6A66", // Brown Rabbit
        L"\x8101\x3F2\xA392\x9F0A\x2FD5",  // Burning Titan
        L"\x8102\x4515",                   // Cave Spider
        L"\x8102\x3F68\xB9DD\x9EEE\x73A7", // Celestial Dog
        L"\x8102\x3F62\xF4D2\xB3A7\x50D9", // Celestial Dragon
        L"\x8102\x3F64\x91D1\xFF08\x1406", // Celestial Horse
        L"\x8102\x3F66\xC842\x9CB3\xF11",  // Celestial Monkey
        L"\x8102\x3F5F\xEAB3\x9E25\x22C9", // Celestial Ox
        L"\x8102\x3F5D\x82A5\xCB19\x49F7", // Celestial Pig
        L"\x8102\x3F61\xD886\xC8C6\x70BD", // Celestial Rabbit
        L"\x8102\x3F5E\xA749\x8783\x5EE0", // Celestial Rat
        L"\x8102\x3F67\xA65A\xEF3A\x7E4F", // Celestial Rooster
        L"\x8102\x3F65\xF85B\xEFA1\x5929", // Celestial Sheep
        L"\x8102\x3F63\xBF56\xE485\x37B7", // Celestial Snake
        L"\x8102\x3F60\xB396\xABEF\x3CA6", // Celestial Tiger
        L"\x8102\x3272",                   // Ceratadon
        L"\x8101\x3E9\xDD98\xBEEE\x5ABA",  // Charr Shaman
        L"\x8102\x4514",                   // Cloudtouched Simian
        L"\x8101\x76DD",                   // Destroyer of Flesh
        L"\x8102\x5945",                   // Dredge Brute
        L"\x8103\x6F9",                    // Ecclesiate Xun Rao
        L"\x8101\x7303",                   // Elf
        L"\x8101\x730B",                   // Fire Imp
        L"\x8102\x450E",                   // Forest Minotaur
        L"\x8102\x450B",                   // Freezie
        L"\x8101\x3E7\xFB88\xF384\x7D78",  // Fungal Wallow
        L"\x8102\x122E",                   // Grawl
        L"\x8101\x2EA1\xAECB\x8321\x55B2", // Gray Giant
        L"\x8101\x66FD\xB774\x8AC7\x4878", // Greased Lightning
        L"\x8102\x7446",                   // Guild Lord
        L"\x8101\x7300",                   // Gwen
        L"\x8102\x5385\xB5F4\xDD41\x6DE",  // Gwen Doll
        L"\x8101\x7308",                   // Harpy Ranger
        L"\x8101\x7307",                   // Heket Warrior
        L"\x8101\x3EC\xC067\xCE23\x645C",  // Hydra
        L"\x8102\x450C",                   // Irukandji
        L"\x108\x107"
        "Miniature Island Guardian\x1",   // Island Guardian (need)
        L"\x8101\x3ED\xF9DD\xCFE6\x144F", // Jade Armor
        L"\x8101\x7309",                  // Juggernaught
        L"\x8101\x3F3\x870C\xA10D\xB74",  // Jungle Troll
        L"\x108\x107"
        "Miniature Kanaxai\x1",            // Kanaxai (need)
        L"\x8101\x3EE\xFEE1\x8908\xA80",   // Kirin
        L"\x8101\x7305",                   // Koss
        L"\x8101\x9Bc",                    // Kuunavang
        L"\x8103\xAF7\xCFC2\x99A2\x3DDC",  // Legionnaire
        L"\x8101\x7302",                   // Lich
        L"\x8101\xF81\x9D3D\xF28A\x2F4E",  // Longhair Yeti
        L"\x8101\xF7D\xBCAD\xF3B9\xC22",   // Naga Raincaller
        L"\x8101\x3EB\xFBB9\xF538\x2C8",   // Necrid Horseman
        L"\x8102\x4510",                   // Nornbear
        L"\x8102\x450D",                   // Mad King Thorn
        L"\x8102\x5CC5",                   // Mad King's Guard
        L"\x8101\x39EF\xC406\xC4C7\x7D88", // Mallyx
        L"\x8101\x7306",                   // Mandragor Imp
        L"\x8103\x6F8",                    // Minister Reiko
        L"\x8102\x450F",                   // Mursaat
        L"\x8101\xF7E\x95BD\xB2B4\x51E7",  // Oni
        L"\x8102\x4511",                   // Ooze
        L"\x8101\x7304",                   // Palawa Joko
        L"\x108\x107"
        "Miniature Panda\x1",              // Panda (need)
        L"\x8101\x66FC\xC207\xBD26\x40CB", // Pig
        L"\x8101\x3C78",                   // Polar Bear
        L"\x8101\x3EF\xE477\xD632\x3AC",   // Prince Rurik
        L"\x8102\x4512",                   // Raptor
        L"\x8102\x4513",                   // Roaring Ether
        L"\x8101\x3F0\x98B5\xB78C\x1EBD",  // Shiro
        L"\x8101\x6BB6",                   // Shiro'ken Assassin
        L"\x8101\x3F4\x9D9B\xEDB3\x3EA7",  // Siege Turtle
        L"\x8101\x3F1\xCAA4\xC7B1\x77B8",  // Temple Guardian
        L"\x8101\x730D",                   // Thorn Wolf
        L"\x8101\x5EE0",                   // Varesh
        L"\x8101\x6BB7",                   // Vizu
        L"\x8101\x7301",                   // Water Djinn
        L"\x8101\x3EA\xB3F6\xFFBA\x1293",  // Whiptail Devourer
        L"\x8102\x4516",                   // White Rabbit
        L"\x8101\x730A",                   // Wind Rider
        L"\x8102\x5944",                   // Word of Madness
        L"\x8102\x5389\xD54E\xE94E\x5120", // Yakkington
        L"\x8101\x6BB8",                   // Zhed Shadowhoof

        L"\x8102\x5946", // Terrorweb Dryder
        L"\x8102\x5947", // Abomination
        L"\x8102\x5948", // Krait Neoss
        L"\x8102\x5949", // Desert Griffon
        L"\x8102\x594A", // Kveldulf
        L"\x8102\x594B", // Quetzal Sly
        L"\x8102\x594C", // Jora
        L"\x8102\x594D", // Flowstone Elemental
        L"\x8102\x594E", // Nian
        L"\x8102\x594F", // Dagnar Stoneplate
        L"\x8102\x5950", // Flame Djinn
        L"\x8102\x5952", // Eye of Janthir

        L"\x8102\x5CC6", // Smite Crawler
        L"\x8102\x5E49", // Dhuum

        L"\x8102\x6505", // Seer
        L"\x8102\x6506", // Siege Devourer
        L"\x8102\x6507", // Shard Wolf
        L"\x8102\x6508", // Fire Drake
        L"\x8102\x6509", // Summit Giant Herder
        L"\x8102\x650A", // Ophil Nahualli
        L"\x8102\x650B", // Cobalt Scabara
        L"\x8102\x650C", // Scourge Manta
        L"\x8102\x650D", // Ventari
        L"\x8102\x650E", // Oola
        L"\x8102\x650F", // Candysmith Marley
        L"\x8102\x6510", // Zhu Hanuku
        L"\x8102\x6511", // King Adelbern
        L"\x8102\x6512", // M.O.X

        L"\x8102\x6799", // Salma
        L"\x8102\x679A", // Livia
        L"\x8102\x679B", // Evennia
        L"\x8102\x679C", // Confessor Isaiah
        L"\x8102\x679D", // Confessor Dorian
        L"\x8102\x679E", // Peacekeeper Enforcer

        L"\x8102\x7526", // High Priest Zhang
        L"\x8102\x7527", // Ghostly Priest
        L"\x8102\x7528", // Rift Warden

        L"\x8103\xA3B\xEEC0\xD3AD\x6648", // World-Famous Racing Beetle
        L"\x8101\x6730",                  // Ghostly Hero (need)
    };
}
