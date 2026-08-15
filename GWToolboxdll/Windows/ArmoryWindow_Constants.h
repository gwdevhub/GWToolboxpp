#pragma once

#include <GWCA/Constants/Constants.h>

namespace GW {
    struct ItemData;
}

namespace GWArmory {

    using namespace GW::Constants;

    // Arg3:
    //  - Costume = 0x20000006
    //  - Armor =   0x20110007 (3)
    //  - Chaos glove = 0x20110407 (38)

    constexpr uint32_t headpiece_dummy_interaction = 0x20001206;


    constexpr ImVec4 palette[] = {
        {0.f, 0.f, 1.f, 1.f},       // Blue
        {0.f, 0.75f, 0.f, 1.f},     // Green
        {0.5f, 0.f, 0.5f, 1.f},     // Purple
        {1.f, 0.f, 0.f, 1.f},       // Red
        {1.f, 1.f, 0.f, 1.f},       // Yellow
        {0.5f, 0.25f, 0.f, 1.f},    // Brown
        {1.f, 0.65f, 0.f, 1.f},     // Orange
        {0.75f, 0.75f, 0.75f, 1.f}, // Silver
        {0.f, 0.f, 0.f, 1.f},       // Black
        {0.5f, 0.5f, 0.5f, 1.f},    // Gray
        {1.f, 1.f, 1.f, 1.f},       // White
        {0.95f, 0.5f, 0.95f, 1.f},  // Pink
    };

    enum ItemSlot : uint8_t {
        RightHand,
        LeftHand,
        Chestpiece,
        Leggings,
        Headpiece,
        Boots,
        Gloves,
        CostumeBody,
        CostumeHead,
        None,
        Unknown = 0xff
    };

    struct Armor {
        const char* label = 0;
        uint32_t model_file_id = 0;
        Profession profession = Profession::None;
        ItemType type = ItemType::Unknown;
        Campaign campaign = Campaign::BonusMissionPack;
        uint8_t dye_tint = 0;
        uint32_t interaction = 0x20000006;
        Armor(const char* _label, uint32_t _model_file_id, Profession _profession, ItemType _type, Campaign _campaign, uint8_t _dye_tint, uint32_t _interaction = 0) :
            label(_label), model_file_id(_model_file_id), profession(_profession), type(_type), campaign(_campaign), dye_tint(_dye_tint) {
            if (_interaction)
                interaction = _interaction;
        }
        Armor(uint32_t _model_file_id, ItemType _type, uint8_t _dye_tint) :
            model_file_id(_model_file_id), type(_type), dye_tint(_dye_tint) {}
    };

    struct ComboListState {
        std::vector<Armor*> pieces{};
        int current_piece_index = -1;
        Armor* current_piece = nullptr;
    };

    Armor unequipped_armors[] = {
        // Empty slots
        { "NoHead", 0, Profession::None, ItemType::Headpiece, Campaign::Core, 0 },
        { "NoChest", 0, Profession::None, ItemType::Chestpiece, Campaign::Core, 0 },
        { "NoGloves", 0, Profession::None, ItemType::Gloves, Campaign::Core, 0 },
        { "NoLegs", 0, Profession::None, ItemType::Leggings, Campaign::Core, 0 },
        { "NoBoots", 0, Profession::None, ItemType::Boots, Campaign::Core, 0 },
        { "NoCostumeHead", 0, Profession::None, ItemType::Costume_Headpiece, Campaign::Core, 0 },
        { "NoCostume", 0, Profession::None, ItemType::Costume, Campaign::Core, 0 },
        { "NoLeftHand", 0, Profession::None, ItemType::Offhand, Campaign::Core, 0 },
        { "NoRightHand", 0, Profession::None, ItemType::Axe, Campaign::Core, 0 },
    };

    Armor warrior_armors[] = {
        // Core
        {"Obsidian Helm", 0x20D, Profession::Warrior, ItemType::Headpiece, Campaign::Core, 3},
        {"Obsidian Cuirass", 0x20B, Profession::Warrior, ItemType::Chestpiece, Campaign::Core, 3},
        {"Obsidian Gauntlets", 0x20C, Profession::Warrior, ItemType::Gloves, Campaign::Core, 3},
        {"Obsidian Leggings", 0x20E, Profession::Warrior, ItemType::Leggings, Campaign::Core, 3},
        {"Obsidian Boots", 0x20A, Profession::Warrior, ItemType::Boots, Campaign::Core, 3},
        // Prophecies
        {"Ascalon Helm", 0x05D, Profession::Warrior, ItemType::Headpiece, Campaign::Prophecies, 19},
        {"Ascalon Cuirass", 0x05B, Profession::Warrior, ItemType::Chestpiece, Campaign::Prophecies, 19},
        {"Ascalon Gauntlets", 0x05C, Profession::Warrior, ItemType::Gloves, Campaign::Prophecies, 19},
        {"Ascalon Leggings", 0x05E, Profession::Warrior, ItemType::Leggings, Campaign::Prophecies, 19},
        {"Ascalon Boots", 0x05A, Profession::Warrior, ItemType::Boots, Campaign::Prophecies, 19},
        {"Krytan Helm (collector)", 0x28B, Profession::Warrior, ItemType::Headpiece, Campaign::Prophecies, 3},
        {"Krytan Cuirass (collector)", 0x289, Profession::Warrior, ItemType::Chestpiece, Campaign::Prophecies, 3},
        {"Krytan Gauntlets (collector)", 0x28A, Profession::Warrior, ItemType::Gloves, Campaign::Prophecies, 3},
        {"Krytan Leggings (collector)", 0x28C, Profession::Warrior, ItemType::Leggings, Campaign::Prophecies, 3},
        {"Krytan Boots (collector)", 0x288, Profession::Warrior, ItemType::Boots, Campaign::Prophecies, 3},
        {"Tyrian Helm", 0x03A, Profession::Warrior, ItemType::Headpiece, Campaign::Prophecies, 19},
        {"Tyrian Cuirass", 0x038, Profession::Warrior, ItemType::Chestpiece, Campaign::Prophecies, 19},
        {"Tyrian Gauntlets", 0x039, Profession::Warrior, ItemType::Gloves, Campaign::Prophecies, 19},
        {"Tyrian Leggings", 0x03B, Profession::Warrior, ItemType::Leggings, Campaign::Prophecies, 19},
        {"Tyrian Boots", 0x037, Profession::Warrior, ItemType::Boots, Campaign::Prophecies, 19},
        {"Charr Hide Helm", 0x035, Profession::Warrior, ItemType::Headpiece, Campaign::Prophecies, 3},
        {"Charr Hide Cuirass", 0x033, Profession::Warrior, ItemType::Chestpiece, Campaign::Prophecies, 3},
        {"Charr Hide Gauntlets", 0x034, Profession::Warrior, ItemType::Gloves, Campaign::Prophecies, 3},
        {"Charr Hide Leggings", 0x036, Profession::Warrior, ItemType::Leggings, Campaign::Prophecies, 3},
        {"Charr Hide Boots", 0x032, Profession::Warrior, ItemType::Boots, Campaign::Prophecies, 3},
        {"Gladiator Helm", 0x044, Profession::Warrior, ItemType::Headpiece, Campaign::Prophecies, 3},
        {"Gladiator Hauberk", 0x042, Profession::Warrior, ItemType::Chestpiece, Campaign::Prophecies, 3},
        {"Gladiator Gauntlets", 0x043, Profession::Warrior, ItemType::Gloves, Campaign::Prophecies, 3},
        {"Gladiator Leggings", 0x045, Profession::Warrior, ItemType::Leggings, Campaign::Prophecies, 3},
        {"Gladiator Boots", 0x041, Profession::Warrior, ItemType::Boots, Campaign::Prophecies, 3},
        {"Wyvern Helm", 0x03F, Profession::Warrior, ItemType::Headpiece, Campaign::Prophecies, 3},
        {"Wyvern Hauberk", 0x03D, Profession::Warrior, ItemType::Chestpiece, Campaign::Prophecies, 3},
        {"Wyvern Gauntlets", 0x03E, Profession::Warrior, ItemType::Gloves, Campaign::Prophecies, 3},
        {"Wyvern Leggings", 0x040, Profession::Warrior, ItemType::Leggings, Campaign::Prophecies, 3},
        {"Wyvern Boots", 0x03C, Profession::Warrior, ItemType::Boots, Campaign::Prophecies, 3},
        {"Platemail Helm", 0x053, Profession::Warrior, ItemType::Headpiece, Campaign::Prophecies, 3},
        {"Platemail Cuirass", 0x051, Profession::Warrior, ItemType::Chestpiece, Campaign::Prophecies, 3},
        {"Platemail Gauntlets", 0x052, Profession::Warrior, ItemType::Gloves, Campaign::Prophecies, 3},
        {"Platemail Leggings", 0x054, Profession::Warrior, ItemType::Leggings, Campaign::Prophecies, 3},
        {"Platemail Boots", 0x050, Profession::Warrior, ItemType::Boots, Campaign::Prophecies, 3},
        {"Templar Helm", 0x049, Profession::Warrior, ItemType::Headpiece, Campaign::Prophecies, 3},
        {"Templar Cuirass", 0x047, Profession::Warrior, ItemType::Chestpiece, Campaign::Prophecies, 3},
        {"Templar Gauntlets", 0x048, Profession::Warrior, ItemType::Gloves, Campaign::Prophecies, 3},
        {"Templar Leggings", 0x04A, Profession::Warrior, ItemType::Leggings, Campaign::Prophecies, 3},
        {"Templar Boots", 0x046, Profession::Warrior, ItemType::Boots, Campaign::Prophecies, 3},
        {"Elite Charr Hide Helm", 0x247, Profession::Warrior, ItemType::Headpiece, Campaign::Prophecies, 3},
        {"Elite Charr Hide Cuirass", 0x245, Profession::Warrior, ItemType::Chestpiece, Campaign::Prophecies, 3},
        {"Elite Charr Hide Gauntlets", 0x246, Profession::Warrior, ItemType::Gloves, Campaign::Prophecies, 3},
        {"Elite Charr Hide Leggings", 0x248, Profession::Warrior, ItemType::Leggings, Campaign::Prophecies, 3},
        {"Elite Charr Hide Boots", 0x244, Profession::Warrior, ItemType::Boots, Campaign::Prophecies, 3},
        {"Elite Gladiator Helm", 0x2EA, Profession::Warrior, ItemType::Headpiece, Campaign::Prophecies, 3},
        {"Elite Gladiator Hauberk", 0x2E8, Profession::Warrior, ItemType::Chestpiece, Campaign::Prophecies, 3},
        {"Elite Gladiator Gauntlets", 0x2E9, Profession::Warrior, ItemType::Gloves, Campaign::Prophecies, 3},
        {"Elite Gladiator Leggings", 0x2EB, Profession::Warrior, ItemType::Leggings, Campaign::Prophecies, 3},
        {"Elite Gladiator Boots", 0x2E7, Profession::Warrior, ItemType::Boots, Campaign::Prophecies, 3},
        {"Elite Dragon Helm", 0x2BC, Profession::Warrior, ItemType::Headpiece, Campaign::Prophecies, 3},
        {"Elite Dragon Hauberk", 0x2BA, Profession::Warrior, ItemType::Chestpiece, Campaign::Prophecies, 3},
        {"Elite Dragon Gauntlets", 0x2BB, Profession::Warrior, ItemType::Gloves, Campaign::Prophecies, 3},
        {"Elite Dragon Leggings", 0x2BD, Profession::Warrior, ItemType::Leggings, Campaign::Prophecies, 3},
        {"Elite Dragon Boots", 0x2B9, Profession::Warrior, ItemType::Boots, Campaign::Prophecies, 3},
        {"Elite Platemail Helm", 0x2C5, Profession::Warrior, ItemType::Headpiece, Campaign::Prophecies, 3},
        {"Elite Platemail Cuirass", 0x2C3, Profession::Warrior, ItemType::Chestpiece, Campaign::Prophecies, 3},
        {"Elite Platemail Gauntlets", 0x2C4, Profession::Warrior, ItemType::Gloves, Campaign::Prophecies, 3},
        {"Elite Platemail Leggings", 0x2C6, Profession::Warrior, ItemType::Leggings, Campaign::Prophecies, 3},
        {"Elite Platemail Boots", 0x2C2, Profession::Warrior, ItemType::Boots, Campaign::Prophecies, 3},
        {"Elite Templar Helm", 0x2EF, Profession::Warrior, ItemType::Headpiece, Campaign::Prophecies, 3},
        {"Elite Templar Cuirass", 0x2ED, Profession::Warrior, ItemType::Chestpiece, Campaign::Prophecies, 3},
        {"Elite Templar Gauntlets", 0x2EE, Profession::Warrior, ItemType::Gloves, Campaign::Prophecies, 3},
        {"Elite Templar Leggings", 0x2F0, Profession::Warrior, ItemType::Leggings, Campaign::Prophecies, 3},
        {"Elite Templar Boots", 0x2EC, Profession::Warrior, ItemType::Boots, Campaign::Prophecies, 3},
        // Factions
        {"Shing Jea Helm", 0x0B4, Profession::Warrior, ItemType::Headpiece, Campaign::Factions, 3},
        {"Shing Jea Cuirass", 0x0B2, Profession::Warrior, ItemType::Chestpiece, Campaign::Factions, 3},
        {"Shing Jea Gauntlets", 0x0B3, Profession::Warrior, ItemType::Gloves, Campaign::Factions, 3},
        {"Shing Jea Leggings", 0x0B5, Profession::Warrior, ItemType::Leggings, Campaign::Factions, 3},
        {"Shing Jea Boots", 0x0B1, Profession::Warrior, ItemType::Boots, Campaign::Factions, 3},
        {"Canthan Helm", 0x0B9, Profession::Warrior, ItemType::Headpiece, Campaign::Factions, 3},
        {"Canthan Cuirass", 0x0B7, Profession::Warrior, ItemType::Chestpiece, Campaign::Factions, 3},
        {"Canthan Gauntlets", 0x0B8, Profession::Warrior, ItemType::Gloves, Campaign::Factions, 3},
        {"Canthan Leggings", 0x0BA, Profession::Warrior, ItemType::Leggings, Campaign::Factions, 3},
        {"Canthan Boots", 0x0B6, Profession::Warrior, ItemType::Boots, Campaign::Factions, 3},
        {"Kurzick Helm", 0x162, Profession::Warrior, ItemType::Headpiece, Campaign::Factions, 3},
        {"Kurzick Cuirass", 0x113, Profession::Warrior, ItemType::Chestpiece, Campaign::Factions, 3},
        {"Kurzick Gauntlets", 0x161, Profession::Warrior, ItemType::Gloves, Campaign::Factions, 3},
        {"Kurzick Leggings", 0x163, Profession::Warrior, ItemType::Leggings, Campaign::Factions, 3},
        {"Kurzick Boots", 0x112, Profession::Warrior, ItemType::Boots, Campaign::Factions, 3},
        {"Luxon Helm", 0x10B, Profession::Warrior, ItemType::Headpiece, Campaign::Factions, 3},
        {"Luxon Cuirass", 0x109, Profession::Warrior, ItemType::Chestpiece, Campaign::Factions, 3},
        {"Luxon Gauntlets", 0x10A, Profession::Warrior, ItemType::Gloves, Campaign::Factions, 3},
        {"Luxon Leggings", 0x10C, Profession::Warrior, ItemType::Leggings, Campaign::Factions, 3},
        {"Luxon Boots", 0x108, Profession::Warrior, ItemType::Boots, Campaign::Factions, 3},
        {"Elite Canthan Helm", 0x0BE, Profession::Warrior, ItemType::Headpiece, Campaign::Factions, 3},
        {"Elite Canthan Cuirass", 0x0BC, Profession::Warrior, ItemType::Chestpiece, Campaign::Factions, 3},
        {"Elite Canthan Gauntlets", 0x0BD, Profession::Warrior, ItemType::Gloves, Campaign::Factions, 3},
        {"Elite Canthan Leggings", 0x0BF, Profession::Warrior, ItemType::Leggings, Campaign::Factions, 3},
        {"Elite Canthan Boots", 0x0BB, Profession::Warrior, ItemType::Boots, Campaign::Factions, 3},
        {"Elite Kurzick Helm", 0x167, Profession::Warrior, ItemType::Headpiece, Campaign::Factions, 3},
        {"Elite Kurzick Cuirass", 0x165, Profession::Warrior, ItemType::Chestpiece, Campaign::Factions, 3},
        {"Elite Kurzick Gauntlets", 0x166, Profession::Warrior, ItemType::Gloves, Campaign::Factions, 3},
        {"Elite Kurzick Leggings", 0x168, Profession::Warrior, ItemType::Leggings, Campaign::Factions, 3},
        {"Elite Kurzick Boots", 0x164, Profession::Warrior, ItemType::Boots, Campaign::Factions, 3},
        {"Elite Luxon Helm", 0x110, Profession::Warrior, ItemType::Headpiece, Campaign::Factions, 3},
        {"Elite Luxon Cuirass", 0x10E, Profession::Warrior, ItemType::Chestpiece, Campaign::Factions, 3},
        {"Elite Luxon Gauntlets", 0x10F, Profession::Warrior, ItemType::Gloves, Campaign::Factions, 3},
        {"Elite Luxon Leggings", 0x111, Profession::Warrior, ItemType::Leggings, Campaign::Factions, 3},
        {"Elite Luxon Boots", 0x10D, Profession::Warrior, ItemType::Boots, Campaign::Factions, 3},
        // Nightfall
        {"Istani Helm", 0x53F, Profession::Warrior, ItemType::Headpiece, Campaign::Nightfall, 19},
        {"Istani Cuirass", 0x541, Profession::Warrior, ItemType::Chestpiece, Campaign::Nightfall, 19},
        {"Istani Gauntlets", 0x542, Profession::Warrior, ItemType::Gloves, Campaign::Nightfall, 19},
        {"Istani Leggings", 0x543, Profession::Warrior, ItemType::Leggings, Campaign::Nightfall, 19},
        {"Istani Boots", 0x540, Profession::Warrior, ItemType::Boots, Campaign::Nightfall, 19},
        {"Sunspear Helm", 0x544, Profession::Warrior, ItemType::Headpiece, Campaign::Nightfall, 3},
        {"Sunspear Cuirass", 0x546, Profession::Warrior, ItemType::Chestpiece, Campaign::Nightfall, 3},
        {"Sunspear Gauntlets", 0x547, Profession::Warrior, ItemType::Gloves, Campaign::Nightfall, 3},
        {"Sunspear Leggings", 0x548, Profession::Warrior, ItemType::Leggings, Campaign::Nightfall, 3},
        {"Sunspear Boots", 0x545, Profession::Warrior, ItemType::Boots, Campaign::Nightfall, 3},
        {"Elite Sunspear Helm", 0x549, Profession::Warrior, ItemType::Headpiece, Campaign::Nightfall, 3},
        {"Elite Sunspear Cuirass", 0x54B, Profession::Warrior, ItemType::Chestpiece, Campaign::Nightfall, 3},
        {"Elite Sunspear Gauntlets", 0x54C, Profession::Warrior, ItemType::Gloves, Campaign::Nightfall, 3},
        {"Elite Sunspear Leggings", 0x54D, Profession::Warrior, ItemType::Leggings, Campaign::Nightfall, 3},
        {"Elite Sunspear Boots", 0x54A, Profession::Warrior, ItemType::Boots, Campaign::Nightfall, 3},
        {"Vabbian Helm", 0x54E, Profession::Warrior, ItemType::Headpiece, Campaign::Nightfall, 6},
        {"Vabbian Cuirass", 0x550, Profession::Warrior, ItemType::Chestpiece, Campaign::Nightfall, 6},
        {"Vabbian Gauntlets", 0x551, Profession::Warrior, ItemType::Gloves, Campaign::Nightfall, 6},
        {"Vabbian Leggings", 0x552, Profession::Warrior, ItemType::Leggings, Campaign::Nightfall, 6},
        {"Vabbian Boots", 0x54F, Profession::Warrior, ItemType::Boots, Campaign::Nightfall, 6},
        {"Ancient Helm", 0x553, Profession::Warrior, ItemType::Headpiece, Campaign::Nightfall, 3},
        {"Ancient Cuirass", 0x555, Profession::Warrior, ItemType::Chestpiece, Campaign::Nightfall, 3},
        {"Ancient Gauntlets", 0x556, Profession::Warrior, ItemType::Gloves, Campaign::Nightfall, 3},
        {"Ancient Leggings", 0x557, Profession::Warrior, ItemType::Leggings, Campaign::Nightfall, 3},
        {"Ancient Boots", 0x554, Profession::Warrior, ItemType::Boots, Campaign::Nightfall, 3},
        {"Primeval Helm", 0x558, Profession::Warrior, ItemType::Headpiece, Campaign::Nightfall, 3},
        {"Primeval Cuirass", 0x55A, Profession::Warrior, ItemType::Chestpiece, Campaign::Nightfall, 3},
        {"Primeval Gauntlets", 0x55B, Profession::Warrior, ItemType::Gloves, Campaign::Nightfall, 3},
        {"Primeval Leggings", 0x55C, Profession::Warrior, ItemType::Leggings, Campaign::Nightfall, 3},
        {"Primeval Boots", 0x559, Profession::Warrior, ItemType::Boots, Campaign::Nightfall, 3},
        // Eye of the North
        {"Norn Helm", 0x874, Profession::Warrior, ItemType::Headpiece, Campaign::EyeOfTheNorth, 48},
        {"Norn Cuirass", 0x8B3, Profession::Warrior, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 48},
        {"Norn Gauntlets", 0x8D9, Profession::Warrior, ItemType::Gloves, Campaign::EyeOfTheNorth, 48},
        {"Norn Leggings", 0x8E4, Profession::Warrior, ItemType::Leggings, Campaign::EyeOfTheNorth, 48},
        {"Norn Boots", 0x88D, Profession::Warrior, ItemType::Boots, Campaign::EyeOfTheNorth, 48},
        {"Monument Helm", 0x868, Profession::Warrior, ItemType::Headpiece, Campaign::EyeOfTheNorth, 48},
        {"Monument Cuirass", 0x86A, Profession::Warrior, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 48},
        {"Monument Gauntlets", 0x86B, Profession::Warrior, ItemType::Gloves, Campaign::EyeOfTheNorth, 48},
        {"Monument Leggings", 0x86C, Profession::Warrior, ItemType::Leggings, Campaign::EyeOfTheNorth, 48},
        {"Monument Boots", 0x869, Profession::Warrior, ItemType::Boots, Campaign::EyeOfTheNorth, 48},
        {"Asuran Helm", 0x7E1, Profession::Warrior, ItemType::Headpiece, Campaign::EyeOfTheNorth, 48},
        {"Asuran Cuirass", 0x827, Profession::Warrior, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 48},
        {"Asuran Gauntlets", 0x828, Profession::Warrior, ItemType::Gloves, Campaign::EyeOfTheNorth, 48},
        {"Asuran Leggings", 0x829, Profession::Warrior, ItemType::Leggings, Campaign::EyeOfTheNorth, 48},
        {"Asuran Boots", 0x826, Profession::Warrior, ItemType::Boots, Campaign::EyeOfTheNorth, 48},
        {"Silver Eagle Headpiece", 0x907, Profession::Warrior, ItemType::Headpiece, Campaign::EyeOfTheNorth, 48},
        {"Silver Eagle Vest", 0x909, Profession::Warrior, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 48},
        {"Silver Eagle Gloves", 0x90A, Profession::Warrior, ItemType::Gloves, Campaign::EyeOfTheNorth, 48},
        {"Silver Eagle Leggings", 0x90B, Profession::Warrior, ItemType::Leggings, Campaign::EyeOfTheNorth, 48},
        {"Silver Eagle Boots", 0x908, Profession::Warrior, ItemType::Boots, Campaign::EyeOfTheNorth, 48},
        {"Heavy Breastplate", 0x871, Profession::Warrior, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 48},
        {"Ironfist Gauntlets", 0x906, Profession::Warrior, ItemType::Gloves, Campaign::EyeOfTheNorth, 48},
        {"Stalwart Leggings", 0x872, Profession::Warrior, ItemType::Leggings, Campaign::EyeOfTheNorth, 48},
        {"Sturdy Boots", 0x873, Profession::Warrior, ItemType::Boots, Campaign::EyeOfTheNorth, 48},
        {"Chaos Gloves", 0x7DD, Profession::Warrior, ItemType::Gloves, Campaign::EyeOfTheNorth, 38},
        {"Destroyer Gauntlets", 0x853, Profession::Warrior, ItemType::Gloves, Campaign::EyeOfTheNorth, 6},
        {"Dragon Gauntlets", 0x86D, Profession::Warrior, ItemType::Gloves, Campaign::EyeOfTheNorth, 6},
        {"Glacial Gauntlets", 0x85D, Profession::Warrior, ItemType::Gloves, Campaign::EyeOfTheNorth, 6},
        {"Stone Gauntlets", 0x86E, Profession::Warrior, ItemType::Gloves, Campaign::EyeOfTheNorth, 6},
        {"Bandana", 0x7E6, Profession::Warrior, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Blindfold", 0x80D, Profession::Warrior, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Crown", 0x80E, Profession::Warrior, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Dread Mask", 0x82B, Profession::Warrior, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Highlander Woad", 0x86F, Profession::Warrior, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Mask of the Mo Zing", 0x7EB, Profession::Warrior, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Norn Woad", 0x870, Profession::Warrior, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Slim Spectacles", 0x835, Profession::Warrior, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Spectacles", 0x836, Profession::Warrior, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Tinted Spectacles", 0x837, Profession::Warrior, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
    };

    Armor ranger_armors[] = {
        // Core
        {"Obsidian Vest", 0x210, Profession::Ranger, ItemType::Chestpiece, Campaign::Core, 7},
        {"Obsidian Gloves", 0x211, Profession::Ranger, ItemType::Gloves, Campaign::Core, 7},
        {"Obsidian Leggings", 0x212, Profession::Ranger, ItemType::Leggings, Campaign::Core, 7},
        {"Obsidian Boots", 0x20F, Profession::Ranger, ItemType::Boots, Campaign::Core, 7},
        // Prophecies
        {"Simple Mask", 0x08F, Profession::Ranger, ItemType::Headpiece, Campaign::Prophecies, 7},
        {"Hunter Mask", 0x08E, Profession::Ranger, ItemType::Headpiece, Campaign::Prophecies, 7},
        {"Tamer Mask", 0x091, Profession::Ranger, ItemType::Headpiece, Campaign::Prophecies, 7},
        {"Traveler Mask", 0x090, Profession::Ranger, ItemType::Headpiece, Campaign::Prophecies, 7},
        {"Ascalon Vest", 0x095, Profession::Ranger, ItemType::Chestpiece, Campaign::Prophecies, 7},
        {"Ascalon Gloves", 0x096, Profession::Ranger, ItemType::Gloves, Campaign::Prophecies, 7},
        {"Ascalon Leggings", 0x097, Profession::Ranger, ItemType::Leggings, Campaign::Prophecies, 7},
        {"Ascalon Boots", 0x094, Profession::Ranger, ItemType::Boots, Campaign::Prophecies, 7},
        {"Krytan Mask", 0x092, Profession::Ranger, ItemType::Headpiece, Campaign::Prophecies, 7},
        {"Krytan Vest (collector)", 0x28E, Profession::Ranger, ItemType::Chestpiece, Campaign::Prophecies, 7},
        {"Krytan Gloves (collector)", 0x28F, Profession::Ranger, ItemType::Gloves, Campaign::Prophecies, 7},
        {"Krytan Leggings (collector)", 0x290, Profession::Ranger, ItemType::Leggings, Campaign::Prophecies, 7},
        {"Krytan Boots (collector)", 0x28D, Profession::Ranger, ItemType::Boots, Campaign::Prophecies, 7},
        {"Tyrian Vest", 0x0AD, Profession::Ranger, ItemType::Chestpiece, Campaign::Prophecies, 7},
        {"Tyrian Gloves", 0x0AE, Profession::Ranger, ItemType::Gloves, Campaign::Prophecies, 7},
        {"Tyrian Leggings", 0x0AF, Profession::Ranger, ItemType::Leggings, Campaign::Prophecies, 7},
        {"Tyrian Boots", 0x0AC, Profession::Ranger, ItemType::Boots, Campaign::Prophecies, 7},
        {"Fur-Lined Mask", 0x093, Profession::Ranger, ItemType::Headpiece, Campaign::Prophecies, 7},
        {"Fur-Lined Vest", 0x0A1, Profession::Ranger, ItemType::Chestpiece, Campaign::Prophecies, 7},
        {"Fur-Lined Gloves", 0x0A2, Profession::Ranger, ItemType::Gloves, Campaign::Prophecies, 7},
        {"Fur-Lined Leggings", 0x0A3, Profession::Ranger, ItemType::Leggings, Campaign::Prophecies, 7},
        {"Fur-Lined Boots", 0x0A0, Profession::Ranger, ItemType::Boots, Campaign::Prophecies, 7},
        {"Drakescale Vest", 0x0A9, Profession::Ranger, ItemType::Chestpiece, Campaign::Prophecies, 7},
        {"Drakescale Gloves", 0x0AA, Profession::Ranger, ItemType::Gloves, Campaign::Prophecies, 7},
        {"Drakescale Leggings", 0x0AB, Profession::Ranger, ItemType::Leggings, Campaign::Prophecies, 7},
        {"Drakescale Boots", 0x0A8, Profession::Ranger, ItemType::Boots, Campaign::Prophecies, 7},
        {"Druid Vest", 0x09D, Profession::Ranger, ItemType::Chestpiece, Campaign::Prophecies, 7},
        {"Druid Gloves", 0x09E, Profession::Ranger, ItemType::Gloves, Campaign::Prophecies, 7},
        {"Druid Leggings", 0x09F, Profession::Ranger, ItemType::Leggings, Campaign::Prophecies, 7},
        {"Druid Boots", 0x09C, Profession::Ranger, ItemType::Boots, Campaign::Prophecies, 7},
        {"Studded Leather Vest", 0x0A5, Profession::Ranger, ItemType::Chestpiece, Campaign::Prophecies, 7},
        {"Studded Leather Gloves", 0x0A6, Profession::Ranger, ItemType::Gloves, Campaign::Prophecies, 7},
        {"Studded Leather Leggings", 0x0A7, Profession::Ranger, ItemType::Leggings, Campaign::Prophecies, 7},
        {"Studded Leather Boots", 0x0A4, Profession::Ranger, ItemType::Boots, Campaign::Prophecies, 7},
        {"Elite Fur-Lined Vest", 0x27D, Profession::Ranger, ItemType::Chestpiece, Campaign::Prophecies, 7},
        {"Elite Fur-Lined Gloves", 0x27E, Profession::Ranger, ItemType::Gloves, Campaign::Prophecies, 7},
        {"Elite Fur-Lined Leggings", 0x27F, Profession::Ranger, ItemType::Leggings, Campaign::Prophecies, 7},
        {"Elite Fur-Lined Boots", 0x27C, Profession::Ranger, ItemType::Boots, Campaign::Prophecies, 7},
        {"Elite Drakescale Vest", 0x2C8, Profession::Ranger, ItemType::Chestpiece, Campaign::Prophecies, 7},
        {"Elite Drakescale Gloves", 0x2C9, Profession::Ranger, ItemType::Gloves, Campaign::Prophecies, 7},
        {"Elite Drakescale Leggings", 0x2CA, Profession::Ranger, ItemType::Leggings, Campaign::Prophecies, 7},
        {"Elite Drakescale Boots", 0x2C7, Profession::Ranger, ItemType::Boots, Campaign::Prophecies, 7},
        {"Elite Druid Vest", 0x2F2, Profession::Ranger, ItemType::Chestpiece, Campaign::Prophecies, 7},
        {"Elite Druid Gloves", 0x2F3, Profession::Ranger, ItemType::Gloves, Campaign::Prophecies, 7},
        {"Elite Druid Leggings", 0x2F4, Profession::Ranger, ItemType::Leggings, Campaign::Prophecies, 7},
        {"Elite Druid Boots", 0x2F1, Profession::Ranger, ItemType::Boots, Campaign::Prophecies, 7},
        {"Elite Studded Leather Vest", 0x270, Profession::Ranger, ItemType::Chestpiece, Campaign::Prophecies, 7},
        {"Elite Studded Leather Gloves", 0x271, Profession::Ranger, ItemType::Gloves, Campaign::Prophecies, 7},
        {"Elite Studded Leather Leggings", 0x272, Profession::Ranger, ItemType::Leggings, Campaign::Prophecies, 7},
        {"Elite Studded Leather Boots", 0x26F, Profession::Ranger, ItemType::Boots, Campaign::Prophecies, 7},
        // Factions
        {"Shing Jea Mask", 0x458, Profession::Ranger, ItemType::Headpiece, Campaign::Factions, 22},
        {"Shing Jea Vest", 0x170, Profession::Ranger, ItemType::Chestpiece, Campaign::Factions, 22},
        {"Shing Jea Gloves", 0x1B6, Profession::Ranger, ItemType::Gloves, Campaign::Factions, 22},
        {"Shing Jea Leggings", 0x1B7, Profession::Ranger, ItemType::Leggings, Campaign::Factions, 22},
        {"Shing Jea Boots", 0x16F, Profession::Ranger, ItemType::Boots, Campaign::Factions, 22},
        {"Canthan Mask", 0x459, Profession::Ranger, ItemType::Headpiece, Campaign::Factions, 22},
        {"Canthan Vest", 0x1B9, Profession::Ranger, ItemType::Chestpiece, Campaign::Factions, 22},
        {"Canthan Gloves", 0x1F8, Profession::Ranger, ItemType::Gloves, Campaign::Factions, 22},
        {"Canthan Leggings", 0x1F9, Profession::Ranger, ItemType::Leggings, Campaign::Factions, 22},
        {"Canthan Boots", 0x1B8, Profession::Ranger, ItemType::Boots, Campaign::Factions, 22},
        {"Kurzick Mask", 0x45D, Profession::Ranger, ItemType::Headpiece, Campaign::Factions, 22},
        {"Kurzick Vest", 0x301, Profession::Ranger, ItemType::Chestpiece, Campaign::Factions, 22},
        {"Kurzick Gloves", 0x302, Profession::Ranger, ItemType::Gloves, Campaign::Factions, 22},
        {"Kurzick Leggings", 0x303, Profession::Ranger, ItemType::Leggings, Campaign::Factions, 22},
        {"Kurzick Boots", 0x300, Profession::Ranger, ItemType::Boots, Campaign::Factions, 22},
        {"Luxon Mask", 0x45B, Profession::Ranger, ItemType::Headpiece, Campaign::Factions, 22},
        {"Luxon Vest", 0x1FF, Profession::Ranger, ItemType::Chestpiece, Campaign::Factions, 22},
        {"Luxon Gloves", 0x200, Profession::Ranger, ItemType::Gloves, Campaign::Factions, 22},
        {"Luxon Leggings", 0x201, Profession::Ranger, ItemType::Leggings, Campaign::Factions, 22},
        {"Luxon Boots", 0x1FE, Profession::Ranger, ItemType::Boots, Campaign::Factions, 22},
        {"Elite Canthan Mask", 0x45A, Profession::Ranger, ItemType::Headpiece, Campaign::Factions, 22},
        {"Elite Canthan Vest", 0x1FB, Profession::Ranger, ItemType::Chestpiece, Campaign::Factions, 22},
        {"Elite Canthan Gloves", 0x1FC, Profession::Ranger, ItemType::Gloves, Campaign::Factions, 22},
        {"Elite Canthan Leggings", 0x1FD, Profession::Ranger, ItemType::Leggings, Campaign::Factions, 22},
        {"Elite Canthan Boots", 0x1FA, Profession::Ranger, ItemType::Boots, Campaign::Factions, 22},
        {"Elite Kurzick Mask", 0x45E, Profession::Ranger, ItemType::Headpiece, Campaign::Factions, 22},
        {"Elite Kurzick Vest", 0x305, Profession::Ranger, ItemType::Chestpiece, Campaign::Factions, 22},
        {"Elite Kurzick Gloves", 0x306, Profession::Ranger, ItemType::Gloves, Campaign::Factions, 22},
        {"Elite Kurzick Leggings", 0x307, Profession::Ranger, ItemType::Leggings, Campaign::Factions, 22},
        {"Elite Kurzick Boots", 0x304, Profession::Ranger, ItemType::Boots, Campaign::Factions, 22},
        {"Elite Luxon Mask", 0x45C, Profession::Ranger, ItemType::Headpiece, Campaign::Factions, 22},
        {"Elite Luxon Vest", 0x203, Profession::Ranger, ItemType::Chestpiece, Campaign::Factions, 22},
        {"Elite Luxon Gloves", 0x2FE, Profession::Ranger, ItemType::Gloves, Campaign::Factions, 22},
        {"Elite Luxon Leggings", 0x2FF, Profession::Ranger, ItemType::Leggings, Campaign::Factions, 22},
        {"Elite Luxon Boots", 0x202, Profession::Ranger, ItemType::Boots, Campaign::Factions, 22},
        // Nightfall
        {"Istani Mask", 0x570, Profession::Ranger, ItemType::Headpiece, Campaign::Nightfall, 23},
        {"Istani Vest", 0x572, Profession::Ranger, ItemType::Chestpiece, Campaign::Nightfall, 23},
        {"Istani Gloves", 0x573, Profession::Ranger, ItemType::Gloves, Campaign::Nightfall, 23},
        {"Istani Leggings", 0x574, Profession::Ranger, ItemType::Leggings, Campaign::Nightfall, 23},
        {"Istani Boots", 0x571, Profession::Ranger, ItemType::Boots, Campaign::Nightfall, 23},
        {"Sunspear Mask", 0x575, Profession::Ranger, ItemType::Headpiece, Campaign::Nightfall, 23},
        {"Sunspear Vest", 0x577, Profession::Ranger, ItemType::Chestpiece, Campaign::Nightfall, 23},
        {"Sunspear Gloves", 0x578, Profession::Ranger, ItemType::Gloves, Campaign::Nightfall, 23},
        {"Sunspear Leggings", 0x579, Profession::Ranger, ItemType::Leggings, Campaign::Nightfall, 23},
        {"Sunspear Boots", 0x576, Profession::Ranger, ItemType::Boots, Campaign::Nightfall, 23},
        {"Elite Sunspear Mask", 0x57A, Profession::Ranger, ItemType::Headpiece, Campaign::Nightfall, 23},
        {"Elite Sunspear Vest", 0x57C, Profession::Ranger, ItemType::Chestpiece, Campaign::Nightfall, 23},
        {"Elite Sunspear Gloves", 0x57D, Profession::Ranger, ItemType::Gloves, Campaign::Nightfall, 23},
        {"Elite Sunspear Leggings", 0x57E, Profession::Ranger, ItemType::Leggings, Campaign::Nightfall, 23},
        {"Elite Sunspear Boots", 0x57B, Profession::Ranger, ItemType::Boots, Campaign::Nightfall, 23},
        {"Vabbian Mask", 0x57F, Profession::Ranger, ItemType::Headpiece, Campaign::Nightfall, 23},
        {"Vabbian Vest", 0x581, Profession::Ranger, ItemType::Chestpiece, Campaign::Nightfall, 23},
        {"Vabbian Gloves", 0x582, Profession::Ranger, ItemType::Gloves, Campaign::Nightfall, 23},
        {"Vabbian Leggings", 0x583, Profession::Ranger, ItemType::Leggings, Campaign::Nightfall, 23},
        {"Vabbian Boots", 0x580, Profession::Ranger, ItemType::Boots, Campaign::Nightfall, 23},
        {"Ancient Mask", 0x584, Profession::Ranger, ItemType::Headpiece, Campaign::Nightfall, 23},
        {"Ancient Vest", 0x586, Profession::Ranger, ItemType::Chestpiece, Campaign::Nightfall, 23},
        {"Ancient Gloves", 0x587, Profession::Ranger, ItemType::Gloves, Campaign::Nightfall, 23},
        {"Ancient Leggings", 0x588, Profession::Ranger, ItemType::Leggings, Campaign::Nightfall, 23},
        {"Ancient Boots", 0x585, Profession::Ranger, ItemType::Boots, Campaign::Nightfall, 23},
        {"Primeval Mask", 0x589, Profession::Ranger, ItemType::Headpiece, Campaign::Nightfall, 23},
        {"Primeval Vest", 0x58B, Profession::Ranger, ItemType::Chestpiece, Campaign::Nightfall, 23},
        {"Primeval Gloves", 0x58C, Profession::Ranger, ItemType::Gloves, Campaign::Nightfall, 23},
        {"Primeval Leggings", 0x58D, Profession::Ranger, ItemType::Leggings, Campaign::Nightfall, 23},
        {"Primeval Boots", 0x58A, Profession::Ranger, ItemType::Boots, Campaign::Nightfall, 23},
        // EotN
        {"Norn Mask", 0x7CA, Profession::Ranger, ItemType::Headpiece, Campaign::EyeOfTheNorth, 41},
        {"Norn Vest", 0x7CC, Profession::Ranger, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 41},
        {"Norn Gloves", 0x7CD, Profession::Ranger, ItemType::Gloves, Campaign::EyeOfTheNorth, 41},
        {"Norn Leggings", 0x7CE, Profession::Ranger, ItemType::Leggings, Campaign::EyeOfTheNorth, 41},
        {"Norn Boots", 0x7CB, Profession::Ranger, ItemType::Boots, Campaign::EyeOfTheNorth, 41},
        {"Asuran Mask", 0x875, Profession::Ranger, ItemType::Headpiece, Campaign::EyeOfTheNorth, 41},
        {"Asuran Vest", 0x877, Profession::Ranger, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 41},
        {"Asuran Gloves", 0x878, Profession::Ranger, ItemType::Gloves, Campaign::EyeOfTheNorth, 41},
        {"Asuran Leggings", 0x880, Profession::Ranger, ItemType::Leggings, Campaign::EyeOfTheNorth, 41},
        {"Asuran Boots", 0x876, Profession::Ranger, ItemType::Boots, Campaign::EyeOfTheNorth, 41},
        {"Monument Mask", 0x881, Profession::Ranger, ItemType::Headpiece, Campaign::EyeOfTheNorth, 41},
        {"Monument Vest", 0x883, Profession::Ranger, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 41},
        {"Monument Gloves", 0x88B, Profession::Ranger, ItemType::Gloves, Campaign::EyeOfTheNorth, 41},
        {"Monument Leggings", 0x88C, Profession::Ranger, ItemType::Leggings, Campaign::EyeOfTheNorth, 41},
        {"Monument Boots", 0x882, Profession::Ranger, ItemType::Boots, Campaign::EyeOfTheNorth, 41},
        {"Leather Long Coat", 0x87D, Profession::Ranger, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 41},
        {"Sterling Leggings", 0x87E, Profession::Ranger, ItemType::Leggings, Campaign::EyeOfTheNorth, 41},
        {"Embroidered Boots", 0x87F, Profession::Ranger, ItemType::Boots, Campaign::EyeOfTheNorth, 41},
        {"Bandana", 0x7EC, Profession::Ranger, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Blindfold", 0x80F, Profession::Ranger, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Crown", 0x810, Profession::Ranger, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Dread Mask", 0x82C, Profession::Ranger, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Highlander Woad", 0x87B, Profession::Ranger, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Mask of the Mo Zing", 0x7EE, Profession::Ranger, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Norn Woad", 0x87C, Profession::Ranger, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Slim Spectacles", 0x838, Profession::Ranger, ItemType::Headpiece, Campaign::EyeOfTheNorth, 4},
        {"Spectacles", 0x839, Profession::Ranger, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Tinted Spectacles", 0x83A, Profession::Ranger, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Chaos Gloves", 0x7DE, Profession::Ranger, ItemType::Gloves, Campaign::EyeOfTheNorth, 38},
        {"Destroyer Gauntlets", 0x854, Profession::Ranger, ItemType::Gloves, Campaign::EyeOfTheNorth, 6},
        {"Dragon Gauntlets", 0x879, Profession::Ranger, ItemType::Gloves, Campaign::EyeOfTheNorth, 6},
        {"Glacial Gauntlets", 0x85E, Profession::Ranger, ItemType::Gloves, Campaign::EyeOfTheNorth, 6},
        {"Stone Gauntlets", 0x87A, Profession::Ranger, ItemType::Gloves, Campaign::EyeOfTheNorth, 6},
    };

    Armor monk_armors[] = {
        // Core
        {"Obsidian Vestments", 0x214, Profession::Monk, ItemType::Chestpiece, Campaign::Core, 1},
        {"Obsidian Handwraps", 0x215, Profession::Monk, ItemType::Gloves, Campaign::Core, 1},
        {"Obsidian Pants", 0x216, Profession::Monk, ItemType::Leggings, Campaign::Core, 1},
        {"Obsidian Sandals", 0x213, Profession::Monk, ItemType::Boots, Campaign::Core, 1},
        // Prophecies
        {"Ascalon Vestments", 0x0E7, Profession::Monk, ItemType::Chestpiece, Campaign::Prophecies, 1},
        {"Ascalon Handwraps", 0x0E8, Profession::Monk, ItemType::Gloves, Campaign::Prophecies, 1},
        {"Ascalon Pants", 0x0E9, Profession::Monk, ItemType::Leggings, Campaign::Prophecies, 1},
        {"Ascalon Sandals", 0x0E6, Profession::Monk, ItemType::Boots, Campaign::Prophecies, 1},
        {"Krytan Vestments", 0x292, Profession::Monk, ItemType::Chestpiece, Campaign::Prophecies, 1},
        {"Krytan Handwraps", 0x293, Profession::Monk, ItemType::Gloves, Campaign::Prophecies, 1},
        {"Krytan Pants", 0x294, Profession::Monk, ItemType::Leggings, Campaign::Prophecies, 1},
        {"Krytan Sandals", 0x291, Profession::Monk, ItemType::Boots, Campaign::Prophecies, 1},
        {"Tyrian Vestments", 0x0F8, Profession::Monk, ItemType::Chestpiece, Campaign::Prophecies, 1},
        {"Tyrian Handwraps", 0x0F9, Profession::Monk, ItemType::Gloves, Campaign::Prophecies, 1},
        {"Tyrian Pants", 0x0FA, Profession::Monk, ItemType::Leggings, Campaign::Prophecies, 1},
        {"Tyrian Sandals", 0x0F7, Profession::Monk, ItemType::Boots, Campaign::Prophecies, 15},
        {"Woven Vestments", 0x0FC, Profession::Monk, ItemType::Chestpiece, Campaign::Prophecies, 5},
        {"Woven Handwraps", 0x0FD, Profession::Monk, ItemType::Gloves, Campaign::Prophecies, 5},
        {"Woven Pants", 0x0FE, Profession::Monk, ItemType::Leggings, Campaign::Prophecies, 5},
        {"Woven Sandals", 0x0FB, Profession::Monk, ItemType::Boots, Campaign::Prophecies, 5},
        {"Censor Vestments", 0x100, Profession::Monk, ItemType::Chestpiece, Campaign::Prophecies, 1},
        {"Censor Handwraps", 0x101, Profession::Monk, ItemType::Gloves, Campaign::Prophecies, 1},
        {"Censor Pants", 0x102, Profession::Monk, ItemType::Leggings, Campaign::Prophecies, 1},
        {"Censor Sandals", 0x0FF, Profession::Monk, ItemType::Boots, Campaign::Prophecies, 1},
        {"Sacred Vestments", 0x0EF, Profession::Monk, ItemType::Chestpiece, Campaign::Prophecies, 1},
        {"Sacred Handwraps", 0x0F0, Profession::Monk, ItemType::Gloves, Campaign::Prophecies, 1},
        {"Sacred Pants", 0x0F1, Profession::Monk, ItemType::Leggings, Campaign::Prophecies, 1},
        {"Sacred Sandals", 0x0EE, Profession::Monk, ItemType::Boots, Campaign::Prophecies, 1},
        {"Dragon Scalp Design", 0x0F5, Profession::Monk, ItemType::Headpiece, Campaign::Prophecies, 10},
        {"Dragon Chest Design", 0x0F3, Profession::Monk, ItemType::Chestpiece, Campaign::Prophecies, 10},
        {"Dragon Arm Design", 0x0F4, Profession::Monk, ItemType::Gloves, Campaign::Prophecies, 10},
        {"Dragon Leg Design", 0x0F6, Profession::Monk, ItemType::Leggings, Campaign::Prophecies, 10},
        {"Dragon Foot Design", 0x0F2, Profession::Monk, ItemType::Boots, Campaign::Prophecies, 10},
        {"Star Scalp Design", 0x106, Profession::Monk, ItemType::Headpiece, Campaign::Prophecies, 10},
        {"Star Chest Design", 0x104, Profession::Monk, ItemType::Chestpiece, Campaign::Prophecies, 10},
        {"Star Arm Design", 0x105, Profession::Monk, ItemType::Gloves, Campaign::Prophecies, 10},
        {"Star Leg Design", 0x107, Profession::Monk, ItemType::Leggings, Campaign::Prophecies, 10},
        {"Star Foot Design", 0x103, Profession::Monk, ItemType::Boots, Campaign::Prophecies, 10},
        {"Elite Woven Vestments", 0x24C, Profession::Monk, ItemType::Chestpiece, Campaign::Prophecies, 15},
        {"Elite Woven Handwraps", 0x24D, Profession::Monk, ItemType::Gloves, Campaign::Prophecies, 15},
        {"Elite Woven Pants", 0x24E, Profession::Monk, ItemType::Leggings, Campaign::Prophecies, 15},
        {"Elite Woven Sandals", 0x24B, Profession::Monk, ItemType::Boots, Campaign::Prophecies, 15},
        {"Elite Judge Vestments", 0x250, Profession::Monk, ItemType::Chestpiece, Campaign::Prophecies, 1},
        {"Elite Judge Handwraps", 0x251, Profession::Monk, ItemType::Gloves, Campaign::Prophecies, 1},
        {"Elite Judge Pants", 0x252, Profession::Monk, ItemType::Leggings, Campaign::Prophecies, 1},
        {"Elite Judge Sandals", 0x24F, Profession::Monk, ItemType::Boots, Campaign::Prophecies, 1},
        {"Elite Saintly Vestments", 0x2D8, Profession::Monk, ItemType::Chestpiece, Campaign::Prophecies, 1},
        {"Elite Saintly Handwraps", 0x2D9, Profession::Monk, ItemType::Gloves, Campaign::Prophecies, 1},
        {"Elite Saintly Pants", 0x2DA, Profession::Monk, ItemType::Leggings, Campaign::Prophecies, 1},
        {"Elite Saintly Sandals", 0x2D7, Profession::Monk, ItemType::Boots, Campaign::Prophecies, 1},
        {"Flowing Scalp Design", 0x2CE, Profession::Monk, ItemType::Headpiece, Campaign::Prophecies, 10},
        {"Flowing Chest Design", 0x2CC, Profession::Monk, ItemType::Chestpiece, Campaign::Prophecies, 10},
        {"Flowing Arm Design", 0x2CD, Profession::Monk, ItemType::Gloves, Campaign::Prophecies, 10},
        {"Flowing Leg Design", 0x2CF, Profession::Monk, ItemType::Leggings, Campaign::Prophecies, 10},
        {"Flowing Foot Design", 0x2CB, Profession::Monk, ItemType::Boots, Campaign::Prophecies, 10},
        {"Labyrinthine Scalp Design", 0x2F8, Profession::Monk, ItemType::Headpiece, Campaign::Prophecies, 10},
        {"Labyrinthine Chest Design", 0x2F6, Profession::Monk, ItemType::Chestpiece, Campaign::Prophecies, 10},
        {"Labyrinthine Arm Design", 0x2F7, Profession::Monk, ItemType::Gloves, Campaign::Prophecies, 10},
        {"Labyrinthine Leg Design", 0x2F9, Profession::Monk, ItemType::Leggings, Campaign::Prophecies, 10},
        {"Labyrinthine Foot Design", 0x2F5, Profession::Monk, ItemType::Boots, Campaign::Prophecies, 10},
        // Factions
        {"Shing Jea Vestments", 0x30E, Profession::Monk, ItemType::Chestpiece, Campaign::Factions, 16},
        {"Shing Jea Handwraps", 0x30F, Profession::Monk, ItemType::Gloves, Campaign::Factions, 16},
        {"Shing Jea Pants", 0x310, Profession::Monk, ItemType::Leggings, Campaign::Factions, 16},
        {"Shing Jea Sandals", 0x30D, Profession::Monk, ItemType::Boots, Campaign::Factions, 16},
        {"Canthan Vestments", 0x312, Profession::Monk, ItemType::Chestpiece, Campaign::Factions, 16},
        {"Canthan Handwraps", 0x313, Profession::Monk, ItemType::Gloves, Campaign::Factions, 16},
        {"Canthan Pants", 0x314, Profession::Monk, ItemType::Leggings, Campaign::Factions, 16},
        {"Canthan Sandals", 0x311, Profession::Monk, ItemType::Boots, Campaign::Factions, 16},
        {"Kurzick Vestments", 0x322, Profession::Monk, ItemType::Chestpiece, Campaign::Factions, 16},
        {"Kurzick Handwraps", 0x323, Profession::Monk, ItemType::Gloves, Campaign::Factions, 16},
        {"Kurzick Pants", 0x324, Profession::Monk, ItemType::Leggings, Campaign::Factions, 16},
        {"Kurzick Sandals", 0x321, Profession::Monk, ItemType::Boots, Campaign::Factions, 16},
        {"Luxon Vestments", 0x31A, Profession::Monk, ItemType::Chestpiece, Campaign::Factions, 16},
        {"Luxon Handwraps", 0x31B, Profession::Monk, ItemType::Gloves, Campaign::Factions, 16},
        {"Luxon Pants", 0x31C, Profession::Monk, ItemType::Leggings, Campaign::Factions, 16},
        {"Luxon Sandals", 0x319, Profession::Monk, ItemType::Boots, Campaign::Factions, 16},
        {"Elite Canthan Vestments", 0x316, Profession::Monk, ItemType::Chestpiece, Campaign::Factions, 16},
        {"Elite Canthan Handwraps", 0x317, Profession::Monk, ItemType::Gloves, Campaign::Factions, 16},
        {"Elite Canthan Pants", 0x318, Profession::Monk, ItemType::Leggings, Campaign::Factions, 16},
        {"Elite Canthan Sandals", 0x315, Profession::Monk, ItemType::Boots, Campaign::Factions, 16},
        {"Elite Kurzick Vestments", 0x326, Profession::Monk, ItemType::Chestpiece, Campaign::Factions, 16},
        {"Elite Kurzick Handwraps", 0x327, Profession::Monk, ItemType::Gloves, Campaign::Factions, 16},
        {"Elite Kurzick Pants", 0x328, Profession::Monk, ItemType::Leggings, Campaign::Factions, 16},
        {"Elite Kurzick Sandals", 0x325, Profession::Monk, ItemType::Boots, Campaign::Factions, 16},
        {"Elite Luxon Vestments", 0x31E, Profession::Monk, ItemType::Chestpiece, Campaign::Factions, 16},
        {"Elite Luxon Handwraps", 0x31F, Profession::Monk, ItemType::Gloves, Campaign::Factions, 16},
        {"Elite Luxon Pants", 0x320, Profession::Monk, ItemType::Leggings, Campaign::Factions, 16},
        {"Elite Luxon Sandals", 0x31D, Profession::Monk, ItemType::Boots, Campaign::Factions, 16},
        {"Shing Jea Scalp", 0x47B, Profession::Monk, ItemType::Headpiece, Campaign::Factions, 10},
        {"Canthan Scalp", 0x47C, Profession::Monk, ItemType::Headpiece, Campaign::Factions, 10},
        {"Kurzick Scalp", 0x480, Profession::Monk, ItemType::Headpiece, Campaign::Factions, 10},
        {"Luxon Scalp", 0x47E, Profession::Monk, ItemType::Headpiece, Campaign::Factions, 10},
        {"Elite Canthan Scalp", 0x47D, Profession::Monk, ItemType::Headpiece, Campaign::Factions, 10},
        {"Elite Kurzick Scalp", 0x481, Profession::Monk, ItemType::Headpiece, Campaign::Factions, 10},
        {"Elite Luxon Scalp", 0x47F, Profession::Monk, ItemType::Headpiece, Campaign::Factions, 10},
        // Nightfall
        {"Istani Scalp Design", 0x59E, Profession::Monk, ItemType::Headpiece, Campaign::Nightfall, 10},
        {"Istani Vestments", 0x5A0, Profession::Monk, ItemType::Chestpiece, Campaign::Nightfall, 5},
        {"Istani Handwraps", 0x5A1, Profession::Monk, ItemType::Gloves, Campaign::Nightfall, 5},
        {"Istani Pants", 0x5A2, Profession::Monk, ItemType::Leggings, Campaign::Nightfall, 5},
        {"Istani Sandals", 0x59F, Profession::Monk, ItemType::Boots, Campaign::Nightfall, 5},
        {"Sunspear Scalp Design", 0x5A3, Profession::Monk, ItemType::Headpiece, Campaign::Nightfall, 5},
        {"Sunspear Vestments", 0x5A5, Profession::Monk, ItemType::Chestpiece, Campaign::Nightfall, 17},
        {"Sunspear Handwraps", 0x5A6, Profession::Monk, ItemType::Gloves, Campaign::Nightfall, 17},
        {"Sunspear Pants", 0x5A7, Profession::Monk, ItemType::Leggings, Campaign::Nightfall, 17},
        {"Sunspear Sandals", 0x5A4, Profession::Monk, ItemType::Boots, Campaign::Nightfall, 17},
        {"Elite Sunspear Scalp Design", 0x5A8, Profession::Monk, ItemType::Headpiece, Campaign::Nightfall, 18},
        {"Elite Sunspear Vestments", 0x5AA, Profession::Monk, ItemType::Chestpiece, Campaign::Nightfall, 18},
        {"Elite Sunspear Handwraps", 0x5AB, Profession::Monk, ItemType::Gloves, Campaign::Nightfall, 18},
        {"Elite Sunspear Pants", 0x5AC, Profession::Monk, ItemType::Leggings, Campaign::Nightfall, 18},
        {"Elite Sunspear Sandals", 0x5A9, Profession::Monk, ItemType::Boots, Campaign::Nightfall, 18},
        {"Vabbian Scalp Design", 0x5AD, Profession::Monk, ItemType::Headpiece, Campaign::Nightfall, 12},
        {"Vabbian Vestments", 0x5AF, Profession::Monk, ItemType::Chestpiece, Campaign::Nightfall, 12},
        {"Vabbian Handwraps", 0x5B0, Profession::Monk, ItemType::Gloves, Campaign::Nightfall, 12},
        {"Vabbian Pants", 0x5B1, Profession::Monk, ItemType::Leggings, Campaign::Nightfall, 12},
        {"Vabbian Sandals", 0x5AE, Profession::Monk, ItemType::Boots, Campaign::Nightfall, 12},
        {"Ancient Scalp Design", 0x5B2, Profession::Monk, ItemType::Headpiece, Campaign::Nightfall, 18},
        {"Ancient Vestments", 0x5B4, Profession::Monk, ItemType::Chestpiece, Campaign::Nightfall, 18},
        {"Ancient Handwraps", 0x5B5, Profession::Monk, ItemType::Gloves, Campaign::Nightfall, 18},
        {"Ancient Pants", 0x5B6, Profession::Monk, ItemType::Leggings, Campaign::Nightfall, 18},
        {"Ancient Sandals", 0x5B3, Profession::Monk, ItemType::Boots, Campaign::Nightfall, 18},
        {"Primeval Scalp Design", 0x5B7, Profession::Monk, ItemType::Headpiece, Campaign::Nightfall, 5},
        {"Primeval Vestments", 0x5B9, Profession::Monk, ItemType::Chestpiece, Campaign::Nightfall, 18},
        {"Primeval Handwraps", 0x5BA, Profession::Monk, ItemType::Gloves, Campaign::Nightfall, 18},
        {"Primeval Pants", 0x5BB, Profession::Monk, ItemType::Leggings, Campaign::Nightfall, 18},
        {"Primeval Sandals", 0x5B8, Profession::Monk, ItemType::Boots, Campaign::Nightfall, 18},
        // EotN
        {"Norn Headpiece", 0x88E, Profession::Monk, ItemType::Headpiece, Campaign::EyeOfTheNorth, 10},
        {"Norn Vest", 0x7E8, Profession::Monk, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 42},
        {"Norn Gloves", 0x7E9, Profession::Monk, ItemType::Gloves, Campaign::EyeOfTheNorth, 42},
        {"Norn Leggings", 0x7EA, Profession::Monk, ItemType::Leggings, Campaign::EyeOfTheNorth, 42},
        {"Norn Boots", 0x7E7, Profession::Monk, ItemType::Boots, Campaign::EyeOfTheNorth, 42},
        {"Asuran Headpiece", 0x88F, Profession::Monk, ItemType::Headpiece, Campaign::EyeOfTheNorth, 10},
        {"Asuran Vest", 0x89D, Profession::Monk, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 42},
        {"Asuran Gloves", 0x89E, Profession::Monk, ItemType::Gloves, Campaign::EyeOfTheNorth, 38},
        {"Asuran Leggings", 0x89F, Profession::Monk, ItemType::Leggings, Campaign::EyeOfTheNorth, 42},
        {"Asuran Boots", 0x89C, Profession::Monk, ItemType::Boots, Campaign::EyeOfTheNorth, 42},
        {"Monument Headpiece", 0x8A0, Profession::Monk, ItemType::Headpiece, Campaign::EyeOfTheNorth, 10},
        {"Monument Vest", 0x8A9, Profession::Monk, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 42},
        {"Monument Gloves", 0x8AA, Profession::Monk, ItemType::Gloves, Campaign::EyeOfTheNorth, 42},
        {"Monument Leggings", 0x8AB, Profession::Monk, ItemType::Leggings, Campaign::EyeOfTheNorth, 42},
        {"Monument Boots", 0x8A8, Profession::Monk, ItemType::Boots, Campaign::EyeOfTheNorth, 42},
        {"Gilded Vestments", 0x888, Profession::Monk, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 42},
        {"Adorned Handwraps", 0x90C, Profession::Monk, ItemType::Gloves, Campaign::EyeOfTheNorth, 42},
        {"Aureate Pants", 0x889, Profession::Monk, ItemType::Leggings, Campaign::EyeOfTheNorth, 42},
        {"Ocher Sandals", 0x88A, Profession::Monk, ItemType::Boots, Campaign::EyeOfTheNorth, 42},
        {"Blindfold", 0x811, Profession::Monk, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Crown", 0x812, Profession::Monk, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Dread Mask", 0x82D, Profession::Monk, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Norn Woad", 0x887, Profession::Monk, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Highlander Woad", 0x886, Profession::Monk, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Slim Spectacles", 0x83B, Profession::Monk, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Spectacles", 0x83C, Profession::Monk, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Tinted Spectacles", 0x83D, Profession::Monk, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Bandana", 0x7EF, Profession::Monk, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Mask of the Mo Zing", 0x7F0, Profession::Monk, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Chaos Gloves", 0x7DF, Profession::Monk, ItemType::Gloves, Campaign::EyeOfTheNorth, 16},
        {"Destroyer Gauntlets", 0x855, Profession::Monk, ItemType::Gloves, Campaign::EyeOfTheNorth, 6},
        {"Dragon Gauntlets", 0x884, Profession::Monk, ItemType::Gloves, Campaign::EyeOfTheNorth, 6},
        {"Glacial Gauntlets", 0x85F, Profession::Monk, ItemType::Gloves, Campaign::EyeOfTheNorth, 6},
        {"Stone Gauntlets", 0x885, Profession::Monk, ItemType::Gloves, Campaign::EyeOfTheNorth, 6},
    };

    Armor necromancer_armors[] = {
        // Core
        {"Obsidian Tunic", 0x223, Profession::Necromancer, ItemType::Chestpiece, Campaign::Core, 20},
        {"Obsidian Gloves", 0x219, Profession::Necromancer, ItemType::Gloves, Campaign::Core, 20},
        {"Obsidian Leggings", 0x21A, Profession::Necromancer, ItemType::Leggings, Campaign::Core, 20},
        {"Obsidian Boots", 0x217, Profession::Necromancer, ItemType::Boots, Campaign::Core, 20},
        // Prophecies
        {"Ragged Scar Pattern", 0x158, Profession::Necromancer, ItemType::Headpiece, Campaign::Prophecies, 20},
        {"Ascalon Tunic", 0x140, Profession::Necromancer, ItemType::Chestpiece, Campaign::Prophecies, 20},
        {"Ascalon Gloves", 0x141, Profession::Necromancer, ItemType::Gloves, Campaign::Prophecies, 20},
        {"Ascalon Leggings", 0x142, Profession::Necromancer, ItemType::Leggings, Campaign::Prophecies, 20},
        {"Ascalon Boots", 0x13F, Profession::Necromancer, ItemType::Boots, Campaign::Prophecies, 20},
        {"Krytan Tunic", 0x296, Profession::Necromancer, ItemType::Chestpiece, Campaign::Prophecies, 20},
        {"Krytan Gloves", 0x297, Profession::Necromancer, ItemType::Gloves, Campaign::Prophecies, 20},
        {"Krytan Leggings", 0x298, Profession::Necromancer, ItemType::Leggings, Campaign::Prophecies, 20},
        {"Krytan Boots", 0x295, Profession::Necromancer, ItemType::Boots, Campaign::Prophecies, 20},
        {"Tyrian Tunic", 0x15A, Profession::Necromancer, ItemType::Chestpiece, Campaign::Prophecies, 20},
        {"Tyrian Gloves", 0x15B, Profession::Necromancer, ItemType::Gloves, Campaign::Prophecies, 20},
        {"Tyrian Leggings", 0x15C, Profession::Necromancer, ItemType::Leggings, Campaign::Prophecies, 20},
        {"Tyrian Boots", 0x159, Profession::Necromancer, ItemType::Boots, Campaign::Prophecies, 20},
        {"Cabal Tunic", 0x148, Profession::Necromancer, ItemType::Chestpiece, Campaign::Prophecies, 20},
        {"Cabal Gloves", 0x149, Profession::Necromancer, ItemType::Gloves, Campaign::Prophecies, 20},
        {"Cabal Leggings", 0x14A, Profession::Necromancer, ItemType::Leggings, Campaign::Prophecies, 20},
        {"Cabal Boots", 0x147, Profession::Necromancer, ItemType::Boots, Campaign::Prophecies, 20},
        {"Fanatic Tunic", 0x150, Profession::Necromancer, ItemType::Chestpiece, Campaign::Prophecies, 20},
        {"Fanatic Gloves", 0x151, Profession::Necromancer, ItemType::Gloves, Campaign::Prophecies, 20},
        {"Fanatic Leggings", 0x152, Profession::Necromancer, ItemType::Leggings, Campaign::Prophecies, 20},
        {"Fanatic Boots", 0x14F, Profession::Necromancer, ItemType::Boots, Campaign::Prophecies, 20},
        {"Necrotic Tunic", 0x144, Profession::Necromancer, ItemType::Chestpiece, Campaign::Prophecies, 20},
        {"Necrotic Gloves", 0x145, Profession::Necromancer, ItemType::Gloves, Campaign::Prophecies, 20},
        {"Necrotic Leggings", 0x146, Profession::Necromancer, ItemType::Leggings, Campaign::Prophecies, 20},
        {"Necrotic Boots", 0x143, Profession::Necromancer, ItemType::Boots, Campaign::Prophecies, 20},
        {"Chest Scar Pattern", 0x154, Profession::Necromancer, ItemType::Chestpiece, Campaign::Prophecies, 36},
        {"Arm Scar Pattern", 0x155, Profession::Necromancer, ItemType::Gloves, Campaign::Prophecies, 36},
        {"Leg Scar Pattern", 0x156, Profession::Necromancer, ItemType::Leggings, Campaign::Prophecies, 36},
        {"Foot Scar Pattern", 0x153, Profession::Necromancer, ItemType::Boots, Campaign::Prophecies, 36},
        {"Profane Tunic", 0x14C, Profession::Necromancer, ItemType::Chestpiece, Campaign::Prophecies, 20},
        {"Profane Gloves", 0x14D, Profession::Necromancer, ItemType::Gloves, Campaign::Prophecies, 20},
        {"Profane Leggings", 0x14E, Profession::Necromancer, ItemType::Leggings, Campaign::Prophecies, 20},
        {"Profane Boots", 0x14B, Profession::Necromancer, ItemType::Boots, Campaign::Prophecies, 20},
        {"Elite Cabal Tunic", 0x254, Profession::Necromancer, ItemType::Chestpiece, Campaign::Prophecies, 20},
        {"Elite Cabal Gloves", 0x255, Profession::Necromancer, ItemType::Gloves, Campaign::Prophecies, 20},
        {"Elite Cabal Leggings", 0x256, Profession::Necromancer, ItemType::Leggings, Campaign::Prophecies, 20},
        {"Elite Cabal Boots", 0x253, Profession::Necromancer, ItemType::Boots, Campaign::Prophecies, 20},
        {"Elite Cultist Tunic", 0x2A4, Profession::Necromancer, ItemType::Chestpiece, Campaign::Prophecies, 20},
        {"Elite Cultist Gloves", 0x2A5, Profession::Necromancer, ItemType::Gloves, Campaign::Prophecies, 20},
        {"Elite Cultist Leggings", 0x2A6, Profession::Necromancer, ItemType::Leggings, Campaign::Prophecies, 20},
        {"Elite Cultist Boots", 0x2A3, Profession::Necromancer, ItemType::Boots, Campaign::Prophecies, 20},
        {"Elite Necrotic Tunic", 0x2DC, Profession::Necromancer, ItemType::Chestpiece, Campaign::Prophecies, 20},
        {"Elite Necrotic Gloves", 0x2DD, Profession::Necromancer, ItemType::Gloves, Campaign::Prophecies, 20},
        {"Elite Necrotic Leggings", 0x2DE, Profession::Necromancer, ItemType::Leggings, Campaign::Prophecies, 20},
        {"Elite Necrotic Boots", 0x2DB, Profession::Necromancer, ItemType::Boots, Campaign::Prophecies, 20},
        {"Chest Elite Scar Pattern", 0x2B2, Profession::Necromancer, ItemType::Chestpiece, Campaign::Prophecies, 20},
        {"Arm Elite Scar Pattern", 0x2B3, Profession::Necromancer, ItemType::Gloves, Campaign::Prophecies, 20},
        {"Leg Elite Scar Pattern", 0x2B4, Profession::Necromancer, ItemType::Leggings, Campaign::Prophecies, 20},
        {"Foot Elite Scar Pattern", 0x2B1, Profession::Necromancer, ItemType::Boots, Campaign::Prophecies, 20},
        {"Elite Profane Tunic", 0x027, Profession::Necromancer, ItemType::Chestpiece, Campaign::Prophecies, 20},
        {"Elite Profane Gloves", 0x181, Profession::Necromancer, ItemType::Gloves, Campaign::Prophecies, 20},
        {"Elite Profane Leggings", 0x18C, Profession::Necromancer, ItemType::Leggings, Campaign::Prophecies, 20},
        {"Elite Profane Boots", 0x00A, Profession::Necromancer, ItemType::Boots, Campaign::Prophecies, 20},
        // Factions
        {"Shing Jea Scar Pattern", 0x49C, Profession::Necromancer, ItemType::Headpiece, Campaign::Factions, 20},
        {"Shing Jea Tunic", 0x331, Profession::Necromancer, ItemType::Chestpiece, Campaign::Factions, 20},
        {"Shing Jea Gloves", 0x332, Profession::Necromancer, ItemType::Gloves, Campaign::Factions, 20},
        {"Shing Jea Leggings", 0x333, Profession::Necromancer, ItemType::Leggings, Campaign::Factions, 20},
        {"Shing Jea Boots", 0x330, Profession::Necromancer, ItemType::Boots, Campaign::Factions, 20},
        {"Canthan Scar Pattern", 0x49D, Profession::Necromancer, ItemType::Headpiece, Campaign::Factions, 20},
        {"Canthan Tunic", 0x335, Profession::Necromancer, ItemType::Chestpiece, Campaign::Factions, 20},
        {"Canthan Gloves", 0x336, Profession::Necromancer, ItemType::Gloves, Campaign::Factions, 20},
        {"Canthan Leggings", 0x337, Profession::Necromancer, ItemType::Leggings, Campaign::Factions, 20},
        {"Canthan Boots", 0x334, Profession::Necromancer, ItemType::Boots, Campaign::Factions, 20},
        {"Kurzick Scar Pattern", 0x4A1, Profession::Necromancer, ItemType::Headpiece, Campaign::Factions, 20},
        {"Kurzick Tunic", 0x345, Profession::Necromancer, ItemType::Chestpiece, Campaign::Factions, 20},
        {"Kurzick Gloves", 0x346, Profession::Necromancer, ItemType::Gloves, Campaign::Factions, 20},
        {"Kurzick Leggings", 0x347, Profession::Necromancer, ItemType::Leggings, Campaign::Factions, 20},
        {"Kurzick Boots", 0x344, Profession::Necromancer, ItemType::Boots, Campaign::Factions, 20},
        {"Luxon Scar Pattern", 0x49F, Profession::Necromancer, ItemType::Headpiece, Campaign::Factions, 20},
        {"Luxon Tunic", 0x33D, Profession::Necromancer, ItemType::Chestpiece, Campaign::Factions, 20},
        {"Luxon Gloves", 0x33E, Profession::Necromancer, ItemType::Gloves, Campaign::Factions, 20},
        {"Luxon Leggings", 0x33F, Profession::Necromancer, ItemType::Leggings, Campaign::Factions, 20},
        {"Luxon Boots", 0x33C, Profession::Necromancer, ItemType::Boots, Campaign::Factions, 20},
        {"Elite Canthan Scar Pattern", 0x49E, Profession::Necromancer, ItemType::Headpiece, Campaign::Factions, 20},
        {"Elite Canthan Tunic", 0x339, Profession::Necromancer, ItemType::Chestpiece, Campaign::Factions, 20},
        {"Elite Canthan Gloves", 0x33A, Profession::Necromancer, ItemType::Gloves, Campaign::Factions, 20},
        {"Elite Canthan Leggings", 0x33B, Profession::Necromancer, ItemType::Leggings, Campaign::Factions, 20},
        {"Elite Canthan Boots", 0x338, Profession::Necromancer, ItemType::Boots, Campaign::Factions, 20},
        {"Elite Kurzick Scar Pattern", 0x4A2, Profession::Necromancer, ItemType::Headpiece, Campaign::Factions, 20},
        {"Elite Kurzick Tunic", 0x349, Profession::Necromancer, ItemType::Chestpiece, Campaign::Factions, 20},
        {"Elite Kurzick Gloves", 0x34A, Profession::Necromancer, ItemType::Gloves, Campaign::Factions, 20},
        {"Elite Kurzick Leggings", 0x34B, Profession::Necromancer, ItemType::Leggings, Campaign::Factions, 20},
        {"Elite Kurzick Boots", 0x348, Profession::Necromancer, ItemType::Boots, Campaign::Factions, 20},
        {"Elite Luxon Scar Pattern", 0x4A0, Profession::Necromancer, ItemType::Headpiece, Campaign::Factions, 20},
        {"Elite Luxon Tunic", 0x341, Profession::Necromancer, ItemType::Chestpiece, Campaign::Factions, 20},
        {"Elite Luxon Gloves", 0x342, Profession::Necromancer, ItemType::Gloves, Campaign::Factions, 20},
        {"Elite Luxon Leggings", 0x343, Profession::Necromancer, ItemType::Leggings, Campaign::Factions, 20},
        {"Elite Luxon Boots", 0x340, Profession::Necromancer, ItemType::Boots, Campaign::Factions, 20},
        // Nightfall
        {"Istani Scar Pattern", 0x5CD, Profession::Necromancer, ItemType::Headpiece, Campaign::Nightfall, 6},
        {"Istani Tunic", 0x5CF, Profession::Necromancer, ItemType::Chestpiece, Campaign::Nightfall, 6},
        {"Istani Gloves", 0x5D0, Profession::Necromancer, ItemType::Gloves, Campaign::Nightfall, 6},
        {"Istani Leggings", 0x5D1, Profession::Necromancer, ItemType::Leggings, Campaign::Nightfall, 6},
        {"Istani Boots", 0x5CE, Profession::Necromancer, ItemType::Boots, Campaign::Nightfall, 6},
        {"Sunspear Scar Pattern", 0x5D2, Profession::Necromancer, ItemType::Headpiece, Campaign::Nightfall, 6},
        {"Sunspear Tunic", 0x5D4, Profession::Necromancer, ItemType::Chestpiece, Campaign::Nightfall, 21},
        {"Sunspear Gloves", 0x5D5, Profession::Necromancer, ItemType::Gloves, Campaign::Nightfall, 21},
        {"Sunspear Leggings", 0x5D6, Profession::Necromancer, ItemType::Leggings, Campaign::Nightfall, 21},
        {"Sunspear Boots", 0x5D3, Profession::Necromancer, ItemType::Boots, Campaign::Nightfall, 21},
        {"Elite Sunspear Scar Pattern", 0x5D7, Profession::Necromancer, ItemType::Headpiece, Campaign::Nightfall, 6},
        {"Elite Sunspear Tunic", 0x5D9, Profession::Necromancer, ItemType::Chestpiece, Campaign::Nightfall, 6},
        {"Elite Sunspear Gloves", 0x5DA, Profession::Necromancer, ItemType::Gloves, Campaign::Nightfall, 6},
        {"Elite Sunspear Leggings", 0x5DB, Profession::Necromancer, ItemType::Leggings, Campaign::Nightfall, 6},
        {"Elite Sunspear Boots", 0x5D8, Profession::Necromancer, ItemType::Boots, Campaign::Nightfall, 6},
        {"Vabbian Scar Pattern", 0x5DC, Profession::Necromancer, ItemType::Headpiece, Campaign::Nightfall, 6},
        {"Vabbian Tunic", 0x5DE, Profession::Necromancer, ItemType::Chestpiece, Campaign::Nightfall, 6},
        {"Vabbian Gloves", 0x5DF, Profession::Necromancer, ItemType::Gloves, Campaign::Nightfall, 6},
        {"Vabbian Leggings", 0x5E0, Profession::Necromancer, ItemType::Leggings, Campaign::Nightfall, 6},
        {"Vabbian Boots", 0x5DD, Profession::Necromancer, ItemType::Boots, Campaign::Nightfall, 6},
        {"Ancient Scar Pattern", 0x5E1, Profession::Necromancer, ItemType::Headpiece, Campaign::Nightfall, 6},
        {"Ancient Tunic", 0x5E3, Profession::Necromancer, ItemType::Chestpiece, Campaign::Nightfall, 6},
        {"Ancient Gloves", 0x5E4, Profession::Necromancer, ItemType::Gloves, Campaign::Nightfall, 6},
        {"Ancient Leggings", 0x5E5, Profession::Necromancer, ItemType::Leggings, Campaign::Nightfall, 6},
        {"Ancient Boots", 0x5E2, Profession::Necromancer, ItemType::Boots, Campaign::Nightfall, 6},
        {"Primeval Scar Pattern", 0x5E6, Profession::Necromancer, ItemType::Headpiece, Campaign::Nightfall, 6},
        {"Primeval Tunic", 0x5E8, Profession::Necromancer, ItemType::Chestpiece, Campaign::Nightfall, 21},
        {"Primeval Gloves", 0x5E9, Profession::Necromancer, ItemType::Gloves, Campaign::Nightfall, 21},
        {"Primeval Leggings", 0x5EA, Profession::Necromancer, ItemType::Leggings, Campaign::Nightfall, 21},
        {"Primeval Boots", 0x5E7, Profession::Necromancer, ItemType::Boots, Campaign::Nightfall, 21},
        // EotN
        {"Norn Scar Pattern", 0x7ED, Profession::Necromancer, ItemType::Headpiece, Campaign::EyeOfTheNorth, 47},
        {"Norn Tunic", 0x7D0, Profession::Necromancer, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 47},
        {"Norn Gloves", 0x7D1, Profession::Necromancer, ItemType::Gloves, Campaign::EyeOfTheNorth, 47},
        {"Norn Leggings", 0x7D2, Profession::Necromancer, ItemType::Leggings, Campaign::EyeOfTheNorth, 47},
        {"Norn Boots", 0x7CF, Profession::Necromancer, ItemType::Boots, Campaign::EyeOfTheNorth, 47},
        {"Asuran Scar Pattern", 0x8B4, Profession::Necromancer, ItemType::Headpiece, Campaign::EyeOfTheNorth, 47},
        {"Asuran Tunic", 0x8B6, Profession::Necromancer, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 47},
        {"Asuran Gloves", 0x8B7, Profession::Necromancer, ItemType::Gloves, Campaign::EyeOfTheNorth, 47},
        {"Asuran Leggings", 0x8BF, Profession::Necromancer, ItemType::Leggings, Campaign::EyeOfTheNorth, 47},
        {"Asuran Boots", 0x8B5, Profession::Necromancer, ItemType::Boots, Campaign::EyeOfTheNorth, 47},
        {"Monument Scar Pattern", 0x8C0, Profession::Necromancer, ItemType::Headpiece, Campaign::EyeOfTheNorth, 47},
        {"Monument Tunic", 0x8C2, Profession::Necromancer, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 47},
        {"Monument Gloves", 0x8C3, Profession::Necromancer, ItemType::Gloves, Campaign::EyeOfTheNorth, 47},
        {"Monument Leggings", 0x8D8, Profession::Necromancer, ItemType::Leggings, Campaign::EyeOfTheNorth, 47},
        {"Monument Boots", 0x8C1, Profession::Necromancer, ItemType::Boots, Campaign::EyeOfTheNorth, 47},
        {"Gloomcrest Tunic", 0x894, Profession::Necromancer, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 47},
        {"Grim Gloves", 0x90E, Profession::Necromancer, ItemType::Gloves, Campaign::EyeOfTheNorth, 47},
        {"Deathlace Leggings", 0x895, Profession::Necromancer, ItemType::Leggings, Campaign::EyeOfTheNorth, 47},
        {"Demonhorn Boots", 0x896, Profession::Necromancer, ItemType::Boots, Campaign::EyeOfTheNorth, 47},
        {"Chaos Gloves", 0x7E0, Profession::Necromancer, ItemType::Gloves, Campaign::EyeOfTheNorth, 38},
        {"Destroyer Gauntlets", 0x856, Profession::Necromancer, ItemType::Gloves, Campaign::EyeOfTheNorth, 6},
        {"Dragon Gauntlets", 0x890, Profession::Necromancer, ItemType::Gloves, Campaign::EyeOfTheNorth, 6},
        {"Glacial Gauntlets", 0x860, Profession::Necromancer, ItemType::Gloves, Campaign::EyeOfTheNorth, 6},
        {"Stone Gauntlets", 0x891, Profession::Necromancer, ItemType::Gloves, Campaign::EyeOfTheNorth, 6},
        {"Bandana", 0x7F1, Profession::Necromancer, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Blindfold", 0x813, Profession::Necromancer, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Crown", 0x814, Profession::Necromancer, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Dread Mask", 0x82E, Profession::Necromancer, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Highlander Woad", 0x892, Profession::Necromancer, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Mask of the Mo Zing", 0x7F2, Profession::Necromancer, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Norn Woad", 0x893, Profession::Necromancer, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Slim Spectacles", 0x83E, Profession::Necromancer, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Spectacles", 0x83F, Profession::Necromancer, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Tinted Spectacles", 0x840, Profession::Necromancer, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6}
    };

    Armor mesmer_armors[] = {
        // Core
        {"Obsidian Attire", 0x21C, Profession::Mesmer, ItemType::Chestpiece, Campaign::Core, 33},
        {"Obsidian Gloves", 0x21D, Profession::Mesmer, ItemType::Gloves, Campaign::Core, 33},
        {"Obsidian Hose", 0x21E, Profession::Mesmer, ItemType::Leggings, Campaign::Core, 33},
        {"Obsidian Footwear", 0x21B, Profession::Mesmer, ItemType::Boots, Campaign::Core, 33},
        // Prophecies
        {"Costume Mask", 0x195, Profession::Mesmer, ItemType::Headpiece, Campaign::Prophecies, 33},
        {"Discreet Mask", 0x196, Profession::Mesmer, ItemType::Headpiece, Campaign::Prophecies, 33},
        {"Imposing Mask", 0x197, Profession::Mesmer, ItemType::Headpiece, Campaign::Prophecies, 33},
        {"Sleek Mask", 0x198, Profession::Mesmer, ItemType::Headpiece, Campaign::Prophecies, 33},
        {"Animal Mask", 0x199, Profession::Mesmer, ItemType::Headpiece, Campaign::Prophecies, 33},
        {"Ascalon Attire", 0x19B, Profession::Mesmer, ItemType::Chestpiece, Campaign::Prophecies, 33},
        {"Ascalon Gloves", 0x19C, Profession::Mesmer, ItemType::Gloves, Campaign::Prophecies, 33},
        {"Ascalon Hose", 0x19D, Profession::Mesmer, ItemType::Leggings, Campaign::Prophecies, 33},
        {"Ascalon Footwear", 0x19A, Profession::Mesmer, ItemType::Boots, Campaign::Prophecies, 33},
        {"Krytan Attire", 0x29A, Profession::Mesmer, ItemType::Chestpiece, Campaign::Prophecies, 33},
        {"Krytan Gloves", 0x29B, Profession::Mesmer, ItemType::Gloves, Campaign::Prophecies, 33},
        {"Krytan Hose", 0x29C, Profession::Mesmer, ItemType::Leggings, Campaign::Prophecies, 33},
        {"Krytan Footwear", 0x299, Profession::Mesmer, ItemType::Boots, Campaign::Prophecies, 33},
        {"Tyrian Attire", 0x1B3, Profession::Mesmer, ItemType::Chestpiece, Campaign::Prophecies, 33},
        {"Tyrian Gloves", 0x1B4, Profession::Mesmer, ItemType::Gloves, Campaign::Prophecies, 33},
        {"Tyrian Hose", 0x1B5, Profession::Mesmer, ItemType::Leggings, Campaign::Prophecies, 33},
        {"Tyrian Footwear", 0x1B2, Profession::Mesmer, ItemType::Boots, Campaign::Prophecies, 33},
        {"Rogue Attire", 0x1AB, Profession::Mesmer, ItemType::Chestpiece, Campaign::Prophecies, 33},
        {"Rogue Gloves", 0x1AC, Profession::Mesmer, ItemType::Gloves, Campaign::Prophecies, 33},
        {"Rogue Hose", 0x1AD, Profession::Mesmer, ItemType::Leggings, Campaign::Prophecies, 33},
        {"Rogue Footwear", 0x1AA, Profession::Mesmer, ItemType::Boots, Campaign::Prophecies, 33},
        {"Courtly Attire", 0x19F, Profession::Mesmer, ItemType::Chestpiece, Campaign::Prophecies, 34},
        {"Courtly Gloves", 0x1A0, Profession::Mesmer, ItemType::Gloves, Campaign::Prophecies, 34},
        {"Courtly Hose", 0x1A1, Profession::Mesmer, ItemType::Leggings, Campaign::Prophecies, 34},
        {"Courtly Footwear", 0x19E, Profession::Mesmer, ItemType::Boots, Campaign::Prophecies, 34},
        {"Performer Attire", 0x1A7, Profession::Mesmer, ItemType::Chestpiece, Campaign::Prophecies, 33},
        {"Performer Gloves", 0x1A8, Profession::Mesmer, ItemType::Gloves, Campaign::Prophecies, 33},
        {"Performer Hose", 0x1A9, Profession::Mesmer, ItemType::Leggings, Campaign::Prophecies, 33},
        {"Performer Footwear", 0x1A6, Profession::Mesmer, ItemType::Boots, Campaign::Prophecies, 33},
        {"Enchanter Attire", 0x1A3, Profession::Mesmer, ItemType::Chestpiece, Campaign::Prophecies, 33},
        {"Enchanter Gloves", 0x1A4, Profession::Mesmer, ItemType::Gloves, Campaign::Prophecies, 33},
        {"Enchanter Hose", 0x1A5, Profession::Mesmer, ItemType::Leggings, Campaign::Prophecies, 33},
        {"Enchanter Footwear", 0x1A2, Profession::Mesmer, ItemType::Boots, Campaign::Prophecies, 33},
        {"Elite Rogue Attire", 0x25E, Profession::Mesmer, ItemType::Chestpiece, Campaign::Prophecies, 33},
        {"Elite Rogue Gloves", 0x25F, Profession::Mesmer, ItemType::Gloves, Campaign::Prophecies, 33},
        {"Elite Rogue Hose", 0x260, Profession::Mesmer, ItemType::Leggings, Campaign::Prophecies, 33},
        {"Elite Rogue Footwear", 0x25D, Profession::Mesmer, ItemType::Boots, Campaign::Prophecies, 33},
        {"Elite Noble Attire", 0x281, Profession::Mesmer, ItemType::Chestpiece, Campaign::Prophecies, 33},
        {"Elite Noble Gloves", 0x282, Profession::Mesmer, ItemType::Gloves, Campaign::Prophecies, 33},
        {"Elite Noble Hose", 0x283, Profession::Mesmer, ItemType::Leggings, Campaign::Prophecies, 33},
        {"Elite Noble Footwear", 0x280, Profession::Mesmer, ItemType::Boots, Campaign::Prophecies, 33},
        {"Elite Elegant Attire", 0x25A, Profession::Mesmer, ItemType::Chestpiece, Campaign::Prophecies, 33},
        {"Elite Elegant Gloves", 0x25B, Profession::Mesmer, ItemType::Gloves, Campaign::Prophecies, 33},
        {"Elite Elegant Hose", 0x25C, Profession::Mesmer, ItemType::Leggings, Campaign::Prophecies, 33},
        {"Elite Elegant Footwear", 0x259, Profession::Mesmer, ItemType::Boots, Campaign::Prophecies, 33},
        {"Elite Enchanter Attire", 0x2E0, Profession::Mesmer, ItemType::Chestpiece, Campaign::Prophecies, 33},
        {"Elite Enchanter Gloves", 0x2E1, Profession::Mesmer, ItemType::Gloves, Campaign::Prophecies, 33},
        {"Elite Enchanter Hose", 0x2E2, Profession::Mesmer, ItemType::Leggings, Campaign::Prophecies, 33},
        {"Elite Enchanter Footwear", 0x2DF, Profession::Mesmer, ItemType::Boots, Campaign::Prophecies, 33},
        // Factions
        {"Shing Jea Mask", 0x4B8, Profession::Mesmer, ItemType::Headpiece, Campaign::Factions, 32},
        {"Shing Jea Attire", 0x355, Profession::Mesmer, ItemType::Chestpiece, Campaign::Factions, 32},
        {"Shing Jea Gloves", 0x356, Profession::Mesmer, ItemType::Gloves, Campaign::Factions, 32},
        {"Shing Jea Hose", 0x357, Profession::Mesmer, ItemType::Leggings, Campaign::Factions, 32},
        {"Shing Jea Footwear", 0x354, Profession::Mesmer, ItemType::Boots, Campaign::Factions, 32},
        {"Canthan Mask", 0x4B9, Profession::Mesmer, ItemType::Headpiece, Campaign::Factions, 32},
        {"Canthan Attire", 0x359, Profession::Mesmer, ItemType::Chestpiece, Campaign::Factions, 32},
        {"Canthan Gloves", 0x35A, Profession::Mesmer, ItemType::Gloves, Campaign::Factions, 32},
        {"Canthan Hose", 0x35B, Profession::Mesmer, ItemType::Leggings, Campaign::Factions, 32},
        {"Canthan Footwear", 0x358, Profession::Mesmer, ItemType::Boots, Campaign::Factions, 32},
        {"Kurzick Mask", 0x4BD, Profession::Mesmer, ItemType::Headpiece, Campaign::Factions, 32},
        {"Kurzick Attire", 0x369, Profession::Mesmer, ItemType::Chestpiece, Campaign::Factions, 32},
        {"Kurzick Gloves", 0x36A, Profession::Mesmer, ItemType::Gloves, Campaign::Factions, 32},
        {"Kurzick Hose", 0x36B, Profession::Mesmer, ItemType::Leggings, Campaign::Factions, 32},
        {"Kurzick Footwear", 0x368, Profession::Mesmer, ItemType::Boots, Campaign::Factions, 32},
        {"Luxon Mask", 0x4BB, Profession::Mesmer, ItemType::Headpiece, Campaign::Factions, 32},
        {"Luxon Attire", 0x361, Profession::Mesmer, ItemType::Chestpiece, Campaign::Factions, 32},
        {"Luxon Gloves", 0x362, Profession::Mesmer, ItemType::Gloves, Campaign::Factions, 32},
        {"Luxon Hose", 0x363, Profession::Mesmer, ItemType::Leggings, Campaign::Factions, 32},
        {"Luxon Footwear", 0x360, Profession::Mesmer, ItemType::Boots, Campaign::Factions, 32},
        {"Elite Canthan Mask", 0x4BA, Profession::Mesmer, ItemType::Headpiece, Campaign::Factions, 32},
        {"Elite Canthan Attire", 0x35D, Profession::Mesmer, ItemType::Chestpiece, Campaign::Factions, 32},
        {"Elite Canthan Gloves", 0x35E, Profession::Mesmer, ItemType::Gloves, Campaign::Factions, 32},
        {"Elite Canthan Hose", 0x35F, Profession::Mesmer, ItemType::Leggings, Campaign::Factions, 32},
        {"Elite Canthan Footwear", 0x35C, Profession::Mesmer, ItemType::Boots, Campaign::Factions, 32},
        {"Elite Kurzick Mask", 0x4BE, Profession::Mesmer, ItemType::Headpiece, Campaign::Factions, 32},
        {"Elite Kurzick Attire", 0x36D, Profession::Mesmer, ItemType::Chestpiece, Campaign::Factions, 32},
        {"Elite Kurzick Gloves", 0x36E, Profession::Mesmer, ItemType::Gloves, Campaign::Factions, 32},
        {"Elite Kurzick Hose", 0x36F, Profession::Mesmer, ItemType::Leggings, Campaign::Factions, 32},
        {"Elite Kurzick Footwear", 0x36C, Profession::Mesmer, ItemType::Boots, Campaign::Factions, 32},
        {"Elite Luxon Mask", 0x4BC, Profession::Mesmer, ItemType::Headpiece, Campaign::Factions, 32},
        {"Elite Luxon Attire", 0x365, Profession::Mesmer, ItemType::Chestpiece, Campaign::Factions, 32},
        {"Elite Luxon Gloves", 0x366, Profession::Mesmer, ItemType::Gloves, Campaign::Factions, 32},
        {"Elite Luxon Hose", 0x367, Profession::Mesmer, ItemType::Leggings, Campaign::Factions, 32},
        {"Elite Luxon Footwear", 0x364, Profession::Mesmer, ItemType::Boots, Campaign::Factions, 32},
        // Nightfall
        {"Istani Mask", 0x6C9, Profession::Mesmer, ItemType::Headpiece, Campaign::Nightfall, 9},
        {"Istani Attire", 0x6CB, Profession::Mesmer, ItemType::Chestpiece, Campaign::Nightfall, 9},
        {"Istani Gloves", 0x6CC, Profession::Mesmer, ItemType::Gloves, Campaign::Nightfall, 9},
        {"Istani Hose", 0x6CD, Profession::Mesmer, ItemType::Leggings, Campaign::Nightfall, 9},
        {"Istani Footwear", 0x6CA, Profession::Mesmer, ItemType::Boots, Campaign::Nightfall, 9},
        {"Sunspear Mask", 0x6CE, Profession::Mesmer, ItemType::Headpiece, Campaign::Nightfall, 34},
        {"Sunspear Attire", 0x6D0, Profession::Mesmer, ItemType::Chestpiece, Campaign::Nightfall, 34},
        {"Sunspear Gloves", 0x6D1, Profession::Mesmer, ItemType::Gloves, Campaign::Nightfall, 34},
        {"Sunspear Hose", 0x6D2, Profession::Mesmer, ItemType::Leggings, Campaign::Nightfall, 34},
        {"Sunspear Footwear", 0x6CF, Profession::Mesmer, ItemType::Boots, Campaign::Nightfall, 34},
        {"Elite Sunspear Mask", 0x6D3, Profession::Mesmer, ItemType::Headpiece, Campaign::Nightfall, 34},
        {"Elite Sunspear Attire", 0x6D5, Profession::Mesmer, ItemType::Chestpiece, Campaign::Nightfall, 34},
        {"Elite Sunspear Gloves", 0x6D6, Profession::Mesmer, ItemType::Gloves, Campaign::Nightfall, 34},
        {"Elite Sunspear Hose", 0x6D7, Profession::Mesmer, ItemType::Leggings, Campaign::Nightfall, 34},
        {"Elite Sunspear Footwear", 0x6D4, Profession::Mesmer, ItemType::Boots, Campaign::Nightfall, 34},
        {"Vabbian Mask", 0x6D8, Profession::Mesmer, ItemType::Headpiece, Campaign::Nightfall, 9},
        {"Vabbian Attire", 0x6DA, Profession::Mesmer, ItemType::Chestpiece, Campaign::Nightfall, 9},
        {"Vabbian Gloves", 0x6DB, Profession::Mesmer, ItemType::Gloves, Campaign::Nightfall, 9},
        {"Vabbian Hose", 0x6DC, Profession::Mesmer, ItemType::Leggings, Campaign::Nightfall, 9},
        {"Vabbian Footwear", 0x6D9, Profession::Mesmer, ItemType::Boots, Campaign::Nightfall, 9},
        {"Ancient Mask", 0x6DD, Profession::Mesmer, ItemType::Headpiece, Campaign::Nightfall, 9},
        {"Ancient Attire", 0x6DF, Profession::Mesmer, ItemType::Chestpiece, Campaign::Nightfall, 9},
        {"Ancient Gloves", 0x6E0, Profession::Mesmer, ItemType::Gloves, Campaign::Nightfall, 9},
        {"Ancient Hose", 0x6E1, Profession::Mesmer, ItemType::Leggings, Campaign::Nightfall, 9},
        {"Ancient Footwear", 0x6DE, Profession::Mesmer, ItemType::Boots, Campaign::Nightfall, 9},
        {"Primeval Mask", 0x6E2, Profession::Mesmer, ItemType::Headpiece, Campaign::Nightfall, 9},
        {"Primeval Attire", 0x6E4, Profession::Mesmer, ItemType::Chestpiece, Campaign::Nightfall, 9},
        {"Primeval Gloves", 0x6E5, Profession::Mesmer, ItemType::Gloves, Campaign::Nightfall, 9},
        {"Primeval Hose", 0x6E6, Profession::Mesmer, ItemType::Leggings, Campaign::Nightfall, 9},
        {"Primeval Footwear", 0x6E3, Profession::Mesmer, ItemType::Boots, Campaign::Nightfall, 9},
        // EotN
        {"Norn Mask", 0x897, Profession::Mesmer, ItemType::Headpiece, Campaign::EyeOfTheNorth, 40},
        {"Norn Attire", 0x899, Profession::Mesmer, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 40},
        {"Norn Gloves", 0x89A, Profession::Mesmer, ItemType::Gloves, Campaign::EyeOfTheNorth, 40},
        {"Norn Hose", 0x89B, Profession::Mesmer, ItemType::Leggings, Campaign::EyeOfTheNorth, 40},
        {"Norn Footwear", 0x898, Profession::Mesmer, ItemType::Boots, Campaign::EyeOfTheNorth, 40},
        {"Asuran Mask", 0x8DA, Profession::Mesmer, ItemType::Headpiece, Campaign::EyeOfTheNorth, 40},
        {"Asuran Attire", 0x8DC, Profession::Mesmer, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 40},
        {"Asuran Gloves", 0x8DD, Profession::Mesmer, ItemType::Gloves, Campaign::EyeOfTheNorth, 40},
        {"Asuran Hose", 0x8DE, Profession::Mesmer, ItemType::Leggings, Campaign::EyeOfTheNorth, 40},
        {"Asuran Footwear", 0x8DB, Profession::Mesmer, ItemType::Boots, Campaign::EyeOfTheNorth, 40},
        {"Monument Mask", 0x8DF, Profession::Mesmer, ItemType::Headpiece, Campaign::EyeOfTheNorth, 40},
        {"Monument Attire", 0x8E1, Profession::Mesmer, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 40},
        {"Monument Gloves", 0x8E2, Profession::Mesmer, ItemType::Gloves, Campaign::EyeOfTheNorth, 40},
        {"Monument Hose", 0x8E3, Profession::Mesmer, ItemType::Leggings, Campaign::EyeOfTheNorth, 40},
        {"Monument Footwear", 0x8E0, Profession::Mesmer, ItemType::Boots, Campaign::EyeOfTheNorth, 40},
        {"Elegant Long Coat", 0x8A5, Profession::Mesmer, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 40},
        {"Studded Hose", 0x8A6, Profession::Mesmer, ItemType::Leggings, Campaign::EyeOfTheNorth, 40},
        {"Silken Footwear", 0x8A7, Profession::Mesmer, ItemType::Boots, Campaign::EyeOfTheNorth, 40},
        {"Chaos Gloves", 0x7E2, Profession::Mesmer, ItemType::Gloves, Campaign::EyeOfTheNorth, 38},
        {"Destroyer Gauntlets", 0x857, Profession::Mesmer, ItemType::Gloves, Campaign::EyeOfTheNorth, 6},
        {"Dragon Gauntlets", 0x8A1, Profession::Mesmer, ItemType::Gloves, Campaign::EyeOfTheNorth, 6},
        {"Glacial Gauntlets", 0x861, Profession::Mesmer, ItemType::Gloves, Campaign::EyeOfTheNorth, 6},
        {"Stone Gauntlets", 0x8A2, Profession::Mesmer, ItemType::Gloves, Campaign::EyeOfTheNorth, 6},
        {"Bandana", 0x7F3, Profession::Mesmer, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Blindfold", 0x815, Profession::Mesmer, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Crown", 0x816, Profession::Mesmer, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Dread Mask", 0x82F, Profession::Mesmer, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Highlander Woad", 0x8A3, Profession::Mesmer, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Mask of the Mo Zing", 0x7F4, Profession::Mesmer, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Norn Woad", 0x8A4, Profession::Mesmer, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Slim Spectacles", 0x841, Profession::Mesmer, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Spectacles", 0x842, Profession::Mesmer, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Tinted Spectacles", 0x843, Profession::Mesmer, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6}
    };

    Armor elementalist_armors[] = {
        // Core
        {"Obsidian Robes", 0x220, Profession::Elementalist, ItemType::Chestpiece, Campaign::Core, 25},
        {"Obsidian Gloves", 0x221, Profession::Elementalist, ItemType::Gloves, Campaign::Core, 25},
        {"Obsidian Leggings", 0x222, Profession::Elementalist, ItemType::Leggings, Campaign::Core, 25},
        {"Obsidian Shoes", 0x21F, Profession::Elementalist, ItemType::Boots, Campaign::Core, 25},
        // Prophecies, Nightfall, and Eye of the North
        {"Flame Eye", 0xD18A, Profession::Elementalist, ItemType::Headpiece, Campaign::Prophecies, 0x9,0x20110203},
        {"All-Seeing Eye", 0xD3C3, Profession::Elementalist, ItemType::Headpiece, Campaign::Prophecies, 0x9,0x20110203},
        {"Stone Eye", 0xD184, Profession::Elementalist, ItemType::Headpiece, Campaign::Prophecies, 0x9,0x20110203},
        {"Storm Eye", 0xD181, Profession::Elementalist, ItemType::Headpiece, Campaign::Prophecies, 0x9,0x20110203},
        {"Third Eye", 0xD187, Profession::Elementalist, ItemType::Headpiece, Campaign::Prophecies, 0x9,0x20110203},
        {"Glacial Eye", 0xD18D, Profession::Elementalist, ItemType::Headpiece, Campaign::Prophecies, 0x9,0x20110203},
        // Prophecies
        {"Ascalon Robes", 0x1F1, Profession::Elementalist, ItemType::Chestpiece, Campaign::Prophecies, 8},
        {"Ascalon Gloves", 0x1F2, Profession::Elementalist, ItemType::Gloves, Campaign::Prophecies, 8},
        {"Ascalon Leggings", 0x1F3, Profession::Elementalist, ItemType::Leggings, Campaign::Prophecies, 8},
        {"Ascalon Shoes", 0x1F0, Profession::Elementalist, ItemType::Boots, Campaign::Prophecies, 8},
        {"Krytan Robes (collector)", 0x1E9, Profession::Elementalist, ItemType::Chestpiece, Campaign::Prophecies, 8},
        {"Krytan Gloves (collector)", 0x1EA, Profession::Elementalist, ItemType::Gloves, Campaign::Prophecies, 8},
        {"Krytan Leggings (collector)", 0x1EB, Profession::Elementalist, ItemType::Leggings, Campaign::Prophecies, 8},
        {"Krytan Shoes (collector)", 0x1E8, Profession::Elementalist, ItemType::Boots, Campaign::Prophecies, 8},
        {"Tyrian Robes", 0x1ED, Profession::Elementalist, ItemType::Chestpiece, Campaign::Prophecies, 8},
        {"Tyrian Gloves", 0x1EE, Profession::Elementalist, ItemType::Gloves, Campaign::Prophecies, 8},
        {"Tyrian Leggings", 0x1EF, Profession::Elementalist, ItemType::Leggings, Campaign::Prophecies, 8},
        {"Tyrian Shoes", 0x1EC, Profession::Elementalist, ItemType::Boots, Campaign::Prophecies, 8},
        {"Flameforged Robes", 0x1E5, Profession::Elementalist, ItemType::Chestpiece, Campaign::Prophecies, 8},
        {"Flameforged Gloves", 0x1E6, Profession::Elementalist, ItemType::Gloves, Campaign::Prophecies, 8},
        {"Flameforged Leggings", 0x1E7, Profession::Elementalist, ItemType::Leggings, Campaign::Prophecies, 8},
        {"Flameforged Shoes", 0x1E4, Profession::Elementalist, ItemType::Boots, Campaign::Prophecies, 8},
        {"Iceforged Robes", 0x1F5, Profession::Elementalist, ItemType::Chestpiece, Campaign::Prophecies, 8},
        {"Iceforged Gloves", 0x1F6, Profession::Elementalist, ItemType::Gloves, Campaign::Prophecies, 8},
        {"Iceforged Leggings", 0x1F7, Profession::Elementalist, ItemType::Leggings, Campaign::Prophecies, 8},
        {"Iceforged Shoes", 0x1F4, Profession::Elementalist, ItemType::Boots, Campaign::Prophecies, 8},
        {"Stoneforged Robes", 0x1E1, Profession::Elementalist, ItemType::Chestpiece, Campaign::Prophecies, 8},
        {"Stoneforged Gloves", 0x1E2, Profession::Elementalist, ItemType::Gloves, Campaign::Prophecies, 8},
        {"Stoneforged Leggings", 0x1E3, Profession::Elementalist, ItemType::Leggings, Campaign::Prophecies, 8},
        {"Stoneforged Shoes", 0x1E0, Profession::Elementalist, ItemType::Boots, Campaign::Prophecies, 8},
        {"Stormforged Robes", 0x1DD, Profession::Elementalist, ItemType::Chestpiece, Campaign::Prophecies, 8},
        {"Stormforged Gloves", 0x1DE, Profession::Elementalist, ItemType::Gloves, Campaign::Prophecies, 8},
        {"Stormforged Leggings", 0x1DF, Profession::Elementalist, ItemType::Leggings, Campaign::Prophecies, 8},
        {"Stormforged Shoes", 0x1DC, Profession::Elementalist, ItemType::Boots, Campaign::Prophecies, 8},
        {"Elite Flameforged Robes", 0x266, Profession::Elementalist, ItemType::Chestpiece, Campaign::Prophecies, 8},
        {"Elite Flameforged Gloves", 0x267, Profession::Elementalist, ItemType::Gloves, Campaign::Prophecies, 8},
        {"Elite Flameforged Leggings", 0x268, Profession::Elementalist, ItemType::Leggings, Campaign::Prophecies, 8},
        {"Elite Flameforged Shoes", 0x265, Profession::Elementalist, ItemType::Boots, Campaign::Prophecies, 8},
        {"Elite Iceforged Robes", 0x285, Profession::Elementalist, ItemType::Chestpiece, Campaign::Prophecies, 31},
        {"Elite Iceforged Gloves", 0x286, Profession::Elementalist, ItemType::Gloves, Campaign::Prophecies, 31},
        {"Elite Iceforged Leggings", 0x287, Profession::Elementalist, ItemType::Leggings, Campaign::Prophecies, 31},
        {"Elite Iceforged Shoes", 0x284, Profession::Elementalist, ItemType::Boots, Campaign::Prophecies, 31},
        {"Elite Stoneforged Robes", 0x262, Profession::Elementalist, ItemType::Chestpiece, Campaign::Prophecies, 31},
        {"Elite Stoneforged Gloves", 0x263, Profession::Elementalist, ItemType::Gloves, Campaign::Prophecies, 31},
        {"Elite Stoneforged Leggings", 0x264, Profession::Elementalist, ItemType::Leggings, Campaign::Prophecies, 31},
        {"Elite Stoneforged Shoes", 0x261, Profession::Elementalist, ItemType::Boots, Campaign::Prophecies, 31},
        {"Elite Stormforged Robes", 0x2E4, Profession::Elementalist, ItemType::Chestpiece, Campaign::Prophecies, 31},
        {"Elite Stormforged Gloves", 0x2E5, Profession::Elementalist, ItemType::Gloves, Campaign::Prophecies, 31},
        {"Elite Stormforged Leggings", 0x2E6, Profession::Elementalist, ItemType::Leggings, Campaign::Prophecies, 31},
        {"Elite Stormforged Shoes", 0x2E3, Profession::Elementalist, ItemType::Boots, Campaign::Prophecies, 31},
        // Factions
        {"Storm Aura", 0x2ADF2, Profession::Elementalist, ItemType::Headpiece, Campaign::Factions, 8, 0x20000203},
        {"Stone Aura", 0x2ADF5, Profession::Elementalist, ItemType::Headpiece, Campaign::Factions, 8, 0x20000203},
        {"Flame Aura", 0x2ADF7, Profession::Elementalist, ItemType::Headpiece, Campaign::Factions, 8, 0x20000203},
        {"Glacial Aura", 0x2ADFB, Profession::Elementalist, ItemType::Headpiece, Campaign::Factions, 8, 0x20000203},
        {"Everlasting Aura", 0x2ADF6, Profession::Elementalist, ItemType::Headpiece, Campaign::Factions, 8, 0x20000203},
        {"Elementalist's Aura", 0x2ADF8, Profession::Elementalist, ItemType::Headpiece, Campaign::Factions, 8, 0x20000203},
        {"Shing Jea Robes", 0x378, Profession::Elementalist, ItemType::Chestpiece, Campaign::Factions, 29},
        {"Shing Jea Gloves", 0x379, Profession::Elementalist, ItemType::Gloves, Campaign::Factions, 29},
        {"Shing Jea Leggings", 0x37A, Profession::Elementalist, ItemType::Leggings, Campaign::Factions, 29},
        {"Shing Jea Shoes", 0x377, Profession::Elementalist, ItemType::Boots, Campaign::Factions, 29},
        {"Canthan Robes", 0x37C, Profession::Elementalist, ItemType::Chestpiece, Campaign::Factions, 28},
        {"Canthan Gloves", 0x37D, Profession::Elementalist, ItemType::Gloves, Campaign::Factions, 28},
        {"Canthan Leggings", 0x37E, Profession::Elementalist, ItemType::Leggings, Campaign::Factions, 28},
        {"Canthan Shoes", 0x37B, Profession::Elementalist, ItemType::Boots, Campaign::Factions, 28},
        {"Luxon Robes", 0x384, Profession::Elementalist, ItemType::Chestpiece, Campaign::Factions, 28},
        {"Luxon Gloves", 0x385, Profession::Elementalist, ItemType::Gloves, Campaign::Factions, 28},
        {"Luxon Leggings", 0x386, Profession::Elementalist, ItemType::Leggings, Campaign::Factions, 28},
        {"Luxon Shoes", 0x383, Profession::Elementalist, ItemType::Boots, Campaign::Factions, 28},
        {"Kurzick Robes", 0x38C, Profession::Elementalist, ItemType::Chestpiece, Campaign::Factions, 29},
        {"Kurzick Gloves", 0x38D, Profession::Elementalist, ItemType::Gloves, Campaign::Factions, 29},
        {"Kurzick Leggings", 0x38E, Profession::Elementalist, ItemType::Leggings, Campaign::Factions, 29},
        {"Kurzick Shoes", 0x38B, Profession::Elementalist, ItemType::Boots, Campaign::Factions, 29},
        {"Canthan Robes", 0x380, Profession::Elementalist, ItemType::Chestpiece, Campaign::Factions, 28},
        {"Canthan Gloves", 0x381, Profession::Elementalist, ItemType::Gloves, Campaign::Factions, 28},
        {"Canthan Leggings", 0x382, Profession::Elementalist, ItemType::Leggings, Campaign::Factions, 28},
        {"Canthan Shoes", 0x37F, Profession::Elementalist, ItemType::Boots, Campaign::Factions, 28},
        {"Elite Kurzick Robes", 0x390, Profession::Elementalist, ItemType::Chestpiece, Campaign::Factions, 30},
        {"Elite Kurzick Gloves", 0x391, Profession::Elementalist, ItemType::Gloves, Campaign::Factions, 30},
        {"Elite Kurzick Leggings", 0x392, Profession::Elementalist, ItemType::Leggings, Campaign::Factions, 30},
        {"Elite Kurzick Shoes", 0x38F, Profession::Elementalist, ItemType::Boots, Campaign::Factions, 30},
        {"Elite Luxon Robes", 0x388, Profession::Elementalist, ItemType::Chestpiece, Campaign::Factions, 8},
        {"Elite Luxon Gloves", 0x389, Profession::Elementalist, ItemType::Gloves, Campaign::Factions, 8},
        {"Elite Luxon Leggings", 0x38A, Profession::Elementalist, ItemType::Leggings, Campaign::Factions, 8},
        {"Elite Luxon Shoes", 0x387, Profession::Elementalist, ItemType::Boots, Campaign::Factions, 8},
        // Nightfall
        {"Vabbian Storm Eye", 0x3E2F1, Profession::Elementalist, ItemType::Headpiece, Campaign::Nightfall, 8,0x20000603},
        {"Vabbian Stone Eye", 0x3E2F6, Profession::Elementalist, ItemType::Headpiece, Campaign::Nightfall, 8,0x20000603},
        {"Vabbian Everlasting Eye", 0x3E2FB, Profession::Elementalist, ItemType::Headpiece, Campaign::Nightfall, 8,0x20000603},
        {"Vabbian Flame Eye", 0x3E2FE, Profession::Elementalist, ItemType::Headpiece, Campaign::Nightfall, 8,0x20000603},
        {"Vabbian Glacial Eye", 0x3E301, Profession::Elementalist, ItemType::Headpiece, Campaign::Nightfall, 8,0x20000603},

        {"Istani Robes", 0x611, Profession::Elementalist, ItemType::Chestpiece, Campaign::Nightfall, 8},
        {"Istani Gloves", 0x612, Profession::Elementalist, ItemType::Gloves, Campaign::Nightfall, 8},
        {"Istani Leggings", 0x613, Profession::Elementalist, ItemType::Leggings, Campaign::Nightfall, 8},
        {"Istani Shoes", 0x610, Profession::Elementalist, ItemType::Boots, Campaign::Nightfall, 8},
        {"Sunspear Robes", 0x615, Profession::Elementalist, ItemType::Chestpiece, Campaign::Nightfall, 29},
        {"Sunspear Gloves", 0x616, Profession::Elementalist, ItemType::Gloves, Campaign::Nightfall, 29},
        {"Sunspear Leggings", 0x617, Profession::Elementalist, ItemType::Leggings, Campaign::Nightfall, 29},
        {"Sunspear Shoes", 0x614, Profession::Elementalist, ItemType::Boots, Campaign::Nightfall, 29},
        {"Elite Sunspear Robes", 0x619, Profession::Elementalist, ItemType::Chestpiece, Campaign::Nightfall, 8},
        {"Elite Sunspear Gloves", 0x61A, Profession::Elementalist, ItemType::Gloves, Campaign::Nightfall, 8},
        {"Elite Sunspear Leggings", 0x61B, Profession::Elementalist, ItemType::Leggings, Campaign::Nightfall, 8},
        {"Elite Sunspear Shoes", 0x618, Profession::Elementalist, ItemType::Boots, Campaign::Nightfall, 8},
        {"Vabbian Robes", 0x61D, Profession::Elementalist, ItemType::Chestpiece, Campaign::Nightfall, 26},
        {"Vabbian Gloves", 0x61E, Profession::Elementalist, ItemType::Gloves, Campaign::Nightfall, 26},
        {"Vabbian Leggings", 0x61F, Profession::Elementalist, ItemType::Leggings, Campaign::Nightfall, 26},
        {"Vabbian Shoes", 0x61C, Profession::Elementalist, ItemType::Boots, Campaign::Nightfall, 26},
        {"Ancient Robes", 0x621, Profession::Elementalist, ItemType::Chestpiece, Campaign::Nightfall, 26},
        {"Ancient Gloves", 0x622, Profession::Elementalist, ItemType::Gloves, Campaign::Nightfall, 26},
        {"Ancient Leggings", 0x623, Profession::Elementalist, ItemType::Leggings, Campaign::Nightfall, 26},
        {"Ancient Shoes", 0x620, Profession::Elementalist, ItemType::Boots, Campaign::Nightfall, 26},
        {"Primeval Robes", 0x625, Profession::Elementalist, ItemType::Chestpiece, Campaign::Nightfall, 24},
        {"Primeval Gloves", 0x626, Profession::Elementalist, ItemType::Gloves, Campaign::Nightfall, 24},
        {"Primeval Leggings", 0x627, Profession::Elementalist, ItemType::Leggings, Campaign::Nightfall, 24},
        {"Primeval Shoes", 0x624, Profession::Elementalist, ItemType::Boots, Campaign::Nightfall, 24},
        // EotN
        {"Thaumaturgic Robes", 0x8EA, Profession::Elementalist, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 43},
        {"Yeoryios Gloves", 0x90D, Profession::Elementalist, ItemType::Gloves, Campaign::EyeOfTheNorth, 43},
        {"Exquisite Leggings", 0x8EB, Profession::Elementalist, ItemType::Leggings, Campaign::EyeOfTheNorth, 43},
        {"Majestic Shoes", 0x8EC, Profession::Elementalist, ItemType::Boots, Campaign::EyeOfTheNorth, 43},
        {"Norn Robes", 0x8ED, Profession::Elementalist, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 43},
        {"Norn Gloves", 0x8F3, Profession::Elementalist, ItemType::Gloves, Campaign::EyeOfTheNorth, 43},
        {"Norn Leggings", 0x8F9, Profession::Elementalist, ItemType::Leggings, Campaign::EyeOfTheNorth, 43},
        {"Norn Shoes", 0x8FE, Profession::Elementalist, ItemType::Boots, Campaign::EyeOfTheNorth, 43},
        {"Monument Robes", 0x8E6, Profession::Elementalist, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 43},
        {"Monument Gloves", 0x8E7, Profession::Elementalist, ItemType::Gloves, Campaign::EyeOfTheNorth, 43},
        {"Monument Leggings", 0x8E8, Profession::Elementalist, ItemType::Leggings, Campaign::EyeOfTheNorth, 43},
        {"Monument Shoes", 0x8E9, Profession::Elementalist, ItemType::Boots, Campaign::EyeOfTheNorth, 43},
        {"Asuran Robes", 0x8B0, Profession::Elementalist, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 43},
        {"Asuran Gloves", 0x8B1, Profession::Elementalist, ItemType::Gloves, Campaign::EyeOfTheNorth, 43},
        {"Asuran Leggings", 0x8B2, Profession::Elementalist, ItemType::Leggings, Campaign::EyeOfTheNorth, 43},
        {"Asuran Shoes", 0x8E5, Profession::Elementalist, ItemType::Boots, Campaign::EyeOfTheNorth, 43},

        {"Chaos Gloves", 0x7E3, Profession::Elementalist, ItemType::Gloves, Campaign::EyeOfTheNorth, 38},
        {"Destroyer Gauntlets", 0x858, Profession::Elementalist, ItemType::Gloves, Campaign::EyeOfTheNorth, 43},
        {"Dragon Gauntlets", 0x8AC, Profession::Elementalist, ItemType::Gloves, Campaign::EyeOfTheNorth, 43},
        {"Glacial Gauntlets", 0x862, Profession::Elementalist, ItemType::Gloves, Campaign::EyeOfTheNorth, 8},
        {"Stone Gauntlets", 0x8AD, Profession::Elementalist, ItemType::Gloves, Campaign::EyeOfTheNorth, 6},
        {"Bandana", 0x7F5, Profession::Elementalist, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Blindfold", 0x817, Profession::Elementalist, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Crown", 0x818, Profession::Elementalist, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Dread Mask", 0x830, Profession::Elementalist, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Highlander Woad", 0x8AE, Profession::Elementalist, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Mask of the Mo Zing", 0x7F6, Profession::Elementalist, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Norn Woad", 0x8AF, Profession::Elementalist, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Slim Spectacles", 0x844, Profession::Elementalist, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Spectacles", 0x845, Profession::Elementalist, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Tinted Spectacles", 0x846, Profession::Elementalist, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
    };

    Armor assassin_armors[] = {
        // Core
        {"Obsidian Mask", 0x3D8, Profession::Assassin, ItemType::Headpiece, Campaign::Core, 11},
        {"Obsidian Guise", 0x3DA, Profession::Assassin, ItemType::Chestpiece, Campaign::Core, 11},
        {"Obsidian Gloves", 0x3DB, Profession::Assassin, ItemType::Gloves, Campaign::Core, 11},
        {"Obsidian Leggings", 0x3DC, Profession::Assassin, ItemType::Leggings, Campaign::Core, 11},
        {"Obsidian Shoes", 0x3D9, Profession::Assassin, ItemType::Boots, Campaign::Core, 11},
        // Faction
        {"Shing Jea Mask", 0x3B0, Profession::Assassin, ItemType::Headpiece, Campaign::Factions, 11},
        {"Shing Jea Guise", 0x3B2, Profession::Assassin, ItemType::Chestpiece, Campaign::Factions, 11},
        {"Shing Jea Gloves", 0x3B3, Profession::Assassin, ItemType::Gloves, Campaign::Factions, 11},
        {"Shing Jea Leggings", 0x3B4, Profession::Assassin, ItemType::Leggings, Campaign::Factions, 11},
        {"Shing Jea Shoes", 0x3B1, Profession::Assassin, ItemType::Boots, Campaign::Factions, 11},
        {"Seitung Mask", 0x3D3, Profession::Assassin, ItemType::Headpiece, Campaign::Factions, 11},
        {"Seitung Guise", 0x3D5, Profession::Assassin, ItemType::Chestpiece, Campaign::Factions, 11},
        {"Seitung Gloves", 0x3D6, Profession::Assassin, ItemType::Gloves, Campaign::Factions, 11},
        {"Seitung Leggings", 0x3D7, Profession::Assassin, ItemType::Leggings, Campaign::Factions, 11},
        {"Seitung Shoes", 0x3D4, Profession::Assassin, ItemType::Boots, Campaign::Factions, 11},
        {"Canthan Mask", 0x3B5, Profession::Assassin, ItemType::Headpiece, Campaign::Factions, 11},
        {"Canthan Guise", 0x3B7, Profession::Assassin, ItemType::Chestpiece, Campaign::Factions, 11},
        {"Canthan Gloves", 0x3B8, Profession::Assassin, ItemType::Gloves, Campaign::Factions, 11},
        {"Canthan Leggings", 0x3B9, Profession::Assassin, ItemType::Leggings, Campaign::Factions, 11},
        {"Canthan Shoes", 0x3B6, Profession::Assassin, ItemType::Boots, Campaign::Factions, 11},
        {"Exotic Mask", 0x4EB, Profession::Assassin, ItemType::Headpiece, Campaign::Factions, 11},
        {"Exotic Guise", 0x4ED, Profession::Assassin, ItemType::Chestpiece, Campaign::Factions, 11},
        {"Exotic Gloves", 0x4EE, Profession::Assassin, ItemType::Gloves, Campaign::Factions, 11},
        {"Exotic Leggings", 0x4EF, Profession::Assassin, ItemType::Leggings, Campaign::Factions, 11},
        {"Exotic Shoes", 0x4EC, Profession::Assassin, ItemType::Boots, Campaign::Factions, 11},
        {"Imperial Mask", 0x4E1, Profession::Assassin, ItemType::Headpiece, Campaign::Factions, 11},
        {"Imperial Guise", 0x4E3, Profession::Assassin, ItemType::Chestpiece, Campaign::Factions, 11},
        {"Imperial Gloves", 0x4E4, Profession::Assassin, ItemType::Gloves, Campaign::Factions, 11},
        {"Imperial Leggings", 0x4E5, Profession::Assassin, ItemType::Leggings, Campaign::Factions, 11},
        {"Imperial Shoes", 0x4E2, Profession::Assassin, ItemType::Boots, Campaign::Factions, 11},
        {"Kurzick Mask", 0x3C9, Profession::Assassin, ItemType::Headpiece, Campaign::Factions, 11},
        {"Kurzick Guise", 0x3CB, Profession::Assassin, ItemType::Chestpiece, Campaign::Factions, 11},
        {"Kurzick Gloves", 0x3CC, Profession::Assassin, ItemType::Gloves, Campaign::Factions, 11},
        {"Kurzick Leggings", 0x3CD, Profession::Assassin, ItemType::Leggings, Campaign::Factions, 11},
        {"Kurzick Shoes", 0x3CA, Profession::Assassin, ItemType::Boots, Campaign::Factions, 11},
        {"Luxon Mask", 0x3BF, Profession::Assassin, ItemType::Headpiece, Campaign::Factions, 11},
        {"Luxon Guise", 0x3C1, Profession::Assassin, ItemType::Chestpiece, Campaign::Factions, 11},
        {"Luxon Gloves", 0x3C2, Profession::Assassin, ItemType::Gloves, Campaign::Factions, 11},
        {"Luxon Leggings", 0x3C3, Profession::Assassin, ItemType::Leggings, Campaign::Factions, 11},
        {"Luxon Shoes", 0x3C0, Profession::Assassin, ItemType::Boots, Campaign::Factions, 11},
        {"Elite Canthan Mask", 0x3BA, Profession::Assassin, ItemType::Headpiece, Campaign::Factions, 11},
        {"Elite Canthan Guise", 0x3BC, Profession::Assassin, ItemType::Chestpiece, Campaign::Factions, 11},
        {"Elite Canthan Gloves", 0x3BD, Profession::Assassin, ItemType::Gloves, Campaign::Factions, 11},
        {"Elite Canthan Leggings", 0x3BE, Profession::Assassin, ItemType::Leggings, Campaign::Factions, 11},
        {"Elite Canthan Shoes", 0x3BB, Profession::Assassin, ItemType::Boots, Campaign::Factions, 11},
        {"Elite Exotic Mask", 0x4F0, Profession::Assassin, ItemType::Headpiece, Campaign::Factions, 11},
        {"Elite Exotic Guise", 0x4F2, Profession::Assassin, ItemType::Chestpiece, Campaign::Factions, 11},
        {"Elite Exotic Gloves", 0x4F3, Profession::Assassin, ItemType::Gloves, Campaign::Factions, 11},
        {"Elite Exotic Leggings", 0x4F4, Profession::Assassin, ItemType::Leggings, Campaign::Factions, 11},
        {"Elite Exotic Shoes", 0x4F1, Profession::Assassin, ItemType::Boots, Campaign::Factions, 11},
        {"Elite Imperial Mask", 0x4E6, Profession::Assassin, ItemType::Headpiece, Campaign::Factions, 11},
        {"Elite Imperial Guise", 0x4E8, Profession::Assassin, ItemType::Chestpiece, Campaign::Factions, 11},
        {"Elite Imperial Gloves", 0x4E9, Profession::Assassin, ItemType::Gloves, Campaign::Factions, 11},
        {"Elite Imperial Leggings", 0x4EA, Profession::Assassin, ItemType::Leggings, Campaign::Factions, 11},
        {"Elite Imperial Shoes", 0x4E7, Profession::Assassin, ItemType::Boots, Campaign::Factions, 11},
        {"Elite Kurzick Mask", 0x3CE, Profession::Assassin, ItemType::Headpiece, Campaign::Factions, 11},
        {"Elite Kurzick Guise", 0x3D0, Profession::Assassin, ItemType::Chestpiece, Campaign::Factions, 11},
        {"Elite Kurzick Gloves", 0x3D1, Profession::Assassin, ItemType::Gloves, Campaign::Factions, 11},
        {"Elite Kurzick Leggings", 0x3D2, Profession::Assassin, ItemType::Leggings, Campaign::Factions, 11},
        {"Elite Kurzick Shoes", 0x3CF, Profession::Assassin, ItemType::Boots, Campaign::Factions, 11},
        {"Elite Luxon Mask", 0x3C4, Profession::Assassin, ItemType::Headpiece, Campaign::Factions, 11},
        {"Elite Luxon Guise", 0x3C6, Profession::Assassin, ItemType::Chestpiece, Campaign::Factions, 11},
        {"Elite Luxon Gloves", 0x3C7, Profession::Assassin, ItemType::Gloves, Campaign::Factions, 11},
        {"Elite Luxon Leggings", 0x3C8, Profession::Assassin, ItemType::Leggings, Campaign::Factions, 11},
        {"Elite Luxon Shoes", 0x3C5, Profession::Assassin, ItemType::Boots, Campaign::Factions, 11},
        // Nightfall
        {"Vabbian Mask", 0x704, Profession::Assassin, ItemType::Headpiece, Campaign::Nightfall, 37},
        {"Vabbian Guise", 0x706, Profession::Assassin, ItemType::Chestpiece, Campaign::Nightfall, 37},
        {"Vabbian Gloves", 0x707, Profession::Assassin, ItemType::Gloves, Campaign::Nightfall, 37},
        {"Vabbian Leggings", 0x708, Profession::Assassin, ItemType::Leggings, Campaign::Nightfall, 37},
        {"Vabbian Shoes", 0x705, Profession::Assassin, ItemType::Boots, Campaign::Nightfall, 37},
        {"Ancient Mask", 0x709, Profession::Assassin, ItemType::Headpiece, Campaign::Nightfall, 11},
        {"Ancient Guise", 0x70B, Profession::Assassin, ItemType::Chestpiece, Campaign::Nightfall, 11},
        {"Ancient Gloves", 0x70C, Profession::Assassin, ItemType::Gloves, Campaign::Nightfall, 11},
        {"Ancient Leggings", 0x70D, Profession::Assassin, ItemType::Leggings, Campaign::Nightfall, 11},
        {"Ancient Shoes", 0x70A, Profession::Assassin, ItemType::Boots, Campaign::Nightfall, 11},
        // EotN
        {"Norn Mask", 0x904, Profession::Assassin, ItemType::Headpiece, Campaign::EyeOfTheNorth, 49},
        {"Norn Guise", 0x7C2, Profession::Assassin, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 49},
        {"Norn Gloves", 0x7C3, Profession::Assassin, ItemType::Gloves, Campaign::EyeOfTheNorth, 49},
        {"Norn Leggings", 0x7C4, Profession::Assassin, ItemType::Leggings, Campaign::EyeOfTheNorth, 49},
        {"Norn Shoes", 0x7C1, Profession::Assassin, ItemType::Boots, Campaign::EyeOfTheNorth, 49},
        {"Asuran Mask", 0x7F7, Profession::Assassin, ItemType::Headpiece, Campaign::EyeOfTheNorth, 49},
        {"Asuran Guise", 0x7F9, Profession::Assassin, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 49},
        {"Asuran Gloves", 0x7FA, Profession::Assassin, ItemType::Gloves, Campaign::EyeOfTheNorth, 49},
        {"Asuran Leggings", 0x7FB, Profession::Assassin, ItemType::Leggings, Campaign::EyeOfTheNorth, 49},
        {"Asuran Shoes", 0x7F8, Profession::Assassin, ItemType::Boots, Campaign::EyeOfTheNorth, 49},
        {"Monument Mask", 0x8EE, Profession::Assassin, ItemType::Headpiece, Campaign::EyeOfTheNorth, 49},
        {"Monument Guise", 0x8F0, Profession::Assassin, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 49},
        {"Monument Gloves", 0x8F1, Profession::Assassin, ItemType::Gloves, Campaign::EyeOfTheNorth, 49},
        {"Monument Leggings", 0x8F2, Profession::Assassin, ItemType::Leggings, Campaign::EyeOfTheNorth, 49},
        {"Monument Shoes", 0x8EF, Profession::Assassin, ItemType::Boots, Campaign::EyeOfTheNorth, 49},
        {"Spiked Guise", 0x8BC, Profession::Assassin, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 49},
        {"Bladed Gloves", 0x905, Profession::Assassin, ItemType::Gloves, Campaign::EyeOfTheNorth, 49},
        {"Oni Leggings  ", 0x8BD, Profession::Assassin, ItemType::Leggings, Campaign::EyeOfTheNorth, 49},
        {"Winged Shoes", 0x8BE, Profession::Assassin, ItemType::Boots, Campaign::EyeOfTheNorth, 49},
        {"Chaos Gloves", 0x7E4, Profession::Assassin, ItemType::Gloves, Campaign::EyeOfTheNorth, 38},
        {"Destroyer Gauntlets", 0x859, Profession::Assassin, ItemType::Gloves, Campaign::EyeOfTheNorth, 43},
        {"Dragon Gauntlets", 0x8B8, Profession::Assassin, ItemType::Gloves, Campaign::EyeOfTheNorth, 6},
        {"Glacial Gauntlets", 0x863, Profession::Assassin, ItemType::Gloves, Campaign::EyeOfTheNorth, 8},
        {"Stone Gauntlets", 0x8B9, Profession::Assassin, ItemType::Gloves, Campaign::EyeOfTheNorth, 6},
        {"Bandana", 0x7FC, Profession::Assassin, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Blindfold", 0x819, Profession::Assassin, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Crown", 0x81A, Profession::Assassin, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Dread Mask", 0x831, Profession::Assassin, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Highlander Woad", 0x8BA, Profession::Assassin, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Mask of the Mo Zing", 0x7FD, Profession::Assassin, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Norn Woad", 0x8BB, Profession::Assassin, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Slim Spectacles", 0x847, Profession::Assassin, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Spectacles", 0x848, Profession::Assassin, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Tinted Spectacles", 0x849, Profession::Assassin, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
    };

    Armor ritualist_armors[] = {
        // Core
        {"Obsidian Headwrap", 0x42B, Profession::Ritualist, ItemType::Headpiece, Campaign::Core, 12},
        {"Obsidian Raiment", 0x42D, Profession::Ritualist, ItemType::Chestpiece, Campaign::Core, 12},
        {"Obsidian Bangles", 0x42E, Profession::Ritualist, ItemType::Gloves, Campaign::Core, 12},
        {"Obsidian Leggings", 0x42F, Profession::Ritualist, ItemType::Leggings, Campaign::Core, 12},
        {"Obsidian Shoes", 0x42C, Profession::Ritualist, ItemType::Boots, Campaign::Core, 12},
        // Factions
        {"Shing Jea Headwrap", 0x403, Profession::Ritualist, ItemType::Headpiece, Campaign::Factions, 12},
        {"Shing Jea Raiment", 0x405, Profession::Ritualist, ItemType::Chestpiece, Campaign::Factions, 12},
        {"Shing Jea Bangles", 0x406, Profession::Ritualist, ItemType::Gloves, Campaign::Factions, 12},
        {"Shing Jea Leggings", 0x407, Profession::Ritualist, ItemType::Leggings, Campaign::Factions, 12},
        {"Shing Jea Shoes", 0x404, Profession::Ritualist, ItemType::Boots, Campaign::Factions, 12},
        {"Canthan Headwrap", 0x408, Profession::Ritualist, ItemType::Headpiece, Campaign::Factions, 12},
        {"Canthan Raiment", 0x40A, Profession::Ritualist, ItemType::Chestpiece, Campaign::Factions, 12},
        {"Canthan Bangles", 0x40B, Profession::Ritualist, ItemType::Gloves, Campaign::Factions, 12},
        {"Canthan Leggings", 0x40C, Profession::Ritualist, ItemType::Leggings, Campaign::Factions, 12},
        {"Canthan Shoes", 0x409, Profession::Ritualist, ItemType::Boots, Campaign::Factions, 12},
        {"Seitung Headwrap", 0x426, Profession::Ritualist, ItemType::Headpiece, Campaign::Factions, 12},
        {"Seitung Raiment", 0x428, Profession::Ritualist, ItemType::Chestpiece, Campaign::Factions, 12},
        {"Seitung Bangles", 0x429, Profession::Ritualist, ItemType::Gloves, Campaign::Factions, 12},
        {"Seitung Leggings", 0x42A, Profession::Ritualist, ItemType::Leggings, Campaign::Factions, 12},
        {"Seitung Shoes", 0x427, Profession::Ritualist, ItemType::Boots, Campaign::Factions, 12},
        {"Exotic Headwrap", 0x508, Profession::Ritualist, ItemType::Headpiece, Campaign::Factions, 12},
        {"Exotic Raiment", 0x50A, Profession::Ritualist, ItemType::Chestpiece, Campaign::Factions, 12},
        {"Exotic Bangles", 0x50B, Profession::Ritualist, ItemType::Gloves, Campaign::Factions, 12},
        {"Exotic Leggings", 0x50C, Profession::Ritualist, ItemType::Leggings, Campaign::Factions, 12},
        {"Exotic Shoes", 0x509, Profession::Ritualist, ItemType::Boots, Campaign::Factions, 12},
        {"Imperial Headwrap", 0x4FE, Profession::Ritualist, ItemType::Headpiece, Campaign::Factions, 12},
        {"Imperial Raiment", 0x500, Profession::Ritualist, ItemType::Chestpiece, Campaign::Factions, 12},
        {"Imperial Bangles", 0x501, Profession::Ritualist, ItemType::Gloves, Campaign::Factions, 12},
        {"Imperial Leggings", 0x502, Profession::Ritualist, ItemType::Leggings, Campaign::Factions, 12},
        {"Imperial Shoes", 0x4FF, Profession::Ritualist, ItemType::Boots, Campaign::Factions, 12},
        {"Kurzick Headwrap", 0x41C, Profession::Ritualist, ItemType::Headpiece, Campaign::Factions, 12},
        {"Kurzick Raiment", 0x41E, Profession::Ritualist, ItemType::Chestpiece, Campaign::Factions, 12},
        {"Kurzick Bangles", 0x41F, Profession::Ritualist, ItemType::Gloves, Campaign::Factions, 12},
        {"Kurzick Leggings", 0x420, Profession::Ritualist, ItemType::Leggings, Campaign::Factions, 12},
        {"Kurzick Shoes", 0x41D, Profession::Ritualist, ItemType::Boots, Campaign::Factions, 12},
        {"Luxon Headwrap", 0x412, Profession::Ritualist, ItemType::Headpiece, Campaign::Factions, 12},
        {"Luxon Raiment", 0x414, Profession::Ritualist, ItemType::Chestpiece, Campaign::Factions, 12},
        {"Luxon Bangles", 0x415, Profession::Ritualist, ItemType::Gloves, Campaign::Factions, 12},
        {"Luxon Leggings", 0x416, Profession::Ritualist, ItemType::Leggings, Campaign::Factions, 12},
        {"Luxon Shoes", 0x413, Profession::Ritualist, ItemType::Boots, Campaign::Factions, 12},
        {"Elite Canthan Headwrap", 0x40D, Profession::Ritualist, ItemType::Headpiece, Campaign::Factions, 12},
        {"Elite Canthan Raiment", 0x40F, Profession::Ritualist, ItemType::Chestpiece, Campaign::Factions, 12},
        {"Elite Canthan Bangles", 0x410, Profession::Ritualist, ItemType::Gloves, Campaign::Factions, 12},
        {"Elite Canthan Leggings", 0x411, Profession::Ritualist, ItemType::Leggings, Campaign::Factions, 12},
        {"Elite Canthan Shoes", 0x40E, Profession::Ritualist, ItemType::Boots, Campaign::Factions, 12},
        {"Elite Exotic Headwrap", 0x50D, Profession::Ritualist, ItemType::Headpiece, Campaign::Factions, 12},
        {"Elite Exotic Raiment", 0x50F, Profession::Ritualist, ItemType::Chestpiece, Campaign::Factions, 12},
        {"Elite Exotic Bangles", 0x510, Profession::Ritualist, ItemType::Gloves, Campaign::Factions, 12},
        {"Elite Exotic Leggings", 0x511, Profession::Ritualist, ItemType::Leggings, Campaign::Factions, 12},
        {"Elite Exotic Shoes", 0x50E, Profession::Ritualist, ItemType::Boots, Campaign::Factions, 12},
        {"Elite Imperial Headwrap", 0x503, Profession::Ritualist, ItemType::Headpiece, Campaign::Factions, 12},
        {"Elite Imperial Raiment", 0x505, Profession::Ritualist, ItemType::Chestpiece, Campaign::Factions, 12},
        {"Elite Imperial Bangles", 0x506, Profession::Ritualist, ItemType::Gloves, Campaign::Factions, 12},
        {"Elite Imperial Leggings", 0x507, Profession::Ritualist, ItemType::Leggings, Campaign::Factions, 12},
        {"Elite Imperial Shoes", 0x504, Profession::Ritualist, ItemType::Boots, Campaign::Factions, 12},
        {"Elite Kurzick Headwrap", 0x421, Profession::Ritualist, ItemType::Headpiece, Campaign::Factions, 12},
        {"Elite Kurzick Raiment", 0x423, Profession::Ritualist, ItemType::Chestpiece, Campaign::Factions, 12},
        {"Elite Kurzick Bangles", 0x424, Profession::Ritualist, ItemType::Gloves, Campaign::Factions, 12},
        {"Elite Kurzick Leggings", 0x425, Profession::Ritualist, ItemType::Leggings, Campaign::Factions, 12},
        {"Elite Kurzick Shoes", 0x422, Profession::Ritualist, ItemType::Boots, Campaign::Factions, 12},
        {"Elite Luxon Headwrap", 0x417, Profession::Ritualist, ItemType::Headpiece, Campaign::Factions, 12},
        {"Elite Luxon Raiment", 0x419, Profession::Ritualist, ItemType::Chestpiece, Campaign::Factions, 12},
        {"Elite Luxon Bangles", 0x41A, Profession::Ritualist, ItemType::Gloves, Campaign::Factions, 12},
        {"Elite Luxon Leggings", 0x41B, Profession::Ritualist, ItemType::Leggings, Campaign::Factions, 12},
        {"Elite Luxon Shoes", 0x418, Profession::Ritualist, ItemType::Boots, Campaign::Factions, 12},
        // Nightfall
        {"Vabbian Headwrap", 0x70E, Profession::Ritualist, ItemType::Headpiece, Campaign::Nightfall, 12},
        {"Vabbian Raiment", 0x710, Profession::Ritualist, ItemType::Chestpiece, Campaign::Nightfall, 12},
        {"Vabbian Bangles", 0x711, Profession::Ritualist, ItemType::Gloves, Campaign::Nightfall, 12},
        {"Vabbian Leggings", 0x712, Profession::Ritualist, ItemType::Leggings, Campaign::Nightfall, 12},
        {"Vabbian Shoes", 0x70F, Profession::Ritualist, ItemType::Boots, Campaign::Nightfall, 12},
        {"Ancient Headwrap", 0x713, Profession::Ritualist, ItemType::Headpiece, Campaign::Nightfall, 12},
        {"Ancient Raiment", 0x715, Profession::Ritualist, ItemType::Chestpiece, Campaign::Nightfall, 12},
        {"Ancient Bangles", 0x716, Profession::Ritualist, ItemType::Gloves, Campaign::Nightfall, 12},
        {"Ancient Leggings", 0x717, Profession::Ritualist, ItemType::Leggings, Campaign::Nightfall, 12},
        {"Ancient Shoes", 0x714, Profession::Ritualist, ItemType::Boots, Campaign::Nightfall, 12},
        // EotN
        {"Norn Headwrap", 0x7D3, Profession::Ritualist, ItemType::Headpiece, Campaign::EyeOfTheNorth, 46},
        {"Norn Raiment", 0x7D5, Profession::Ritualist, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 46},
        {"Norn Bangles", 0x7D6, Profession::Ritualist, ItemType::Gloves, Campaign::EyeOfTheNorth, 46},
        {"Norn Leggings", 0x7D7, Profession::Ritualist, ItemType::Leggings, Campaign::EyeOfTheNorth, 46},
        {"Norn Shoes", 0x7D4, Profession::Ritualist, ItemType::Boots, Campaign::EyeOfTheNorth, 46},
        {"Asuran Headwrap", 0x7C5, Profession::Ritualist, ItemType::Headpiece, Campaign::EyeOfTheNorth, 46},
        {"Asuran Raiment", 0x7C7, Profession::Ritualist, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 46},
        {"Asuran Bangles", 0x7C8, Profession::Ritualist, ItemType::Gloves, Campaign::EyeOfTheNorth, 46},
        {"Asuran Leggings", 0x7C9, Profession::Ritualist, ItemType::Leggings, Campaign::EyeOfTheNorth, 46},
        {"Asuran Shoes", 0x7C6, Profession::Ritualist, ItemType::Boots, Campaign::EyeOfTheNorth, 46},
        {"Monument Headwrap", 0x8F4, Profession::Ritualist, ItemType::Headpiece, Campaign::EyeOfTheNorth, 46},
        {"Monument Raiment", 0x8F6, Profession::Ritualist, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 46},
        {"Monument Bangles", 0x8F7, Profession::Ritualist, ItemType::Gloves, Campaign::EyeOfTheNorth, 46},
        {"Monument Leggings", 0x8F8, Profession::Ritualist, ItemType::Leggings, Campaign::EyeOfTheNorth, 46},
        {"Monument Shoes", 0x8F5, Profession::Ritualist, ItemType::Boots, Campaign::EyeOfTheNorth, 46},
        {"Opulent Raiment", 0x8C8, Profession::Ritualist, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 46},
        {"Scale Leggings", 0x8C9, Profession::Ritualist, ItemType::Leggings, Campaign::EyeOfTheNorth, 46},
        {"Beaded Shoes", 0x8CA, Profession::Ritualist, ItemType::Boots, Campaign::EyeOfTheNorth, 46},
        {"Chaos Gloves", 0x7E5, Profession::Ritualist, ItemType::Gloves, Campaign::EyeOfTheNorth, 38},
        {"Destroyer Gauntlets", 0x85A, Profession::Ritualist, ItemType::Gloves, Campaign::EyeOfTheNorth, 6},
        {"Dragon Gauntlets", 0x8C4, Profession::Ritualist, ItemType::Gloves, Campaign::EyeOfTheNorth, 6},
        {"Glacial Gauntlets", 0x864, Profession::Ritualist, ItemType::Gloves, Campaign::EyeOfTheNorth, 6},
        {"Stone Gauntlets", 0x8C5, Profession::Ritualist, ItemType::Gloves, Campaign::EyeOfTheNorth, 6},
        {"Bandana", 0x7FE, Profession::Ritualist, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Blindfold", 0x81B, Profession::Ritualist, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Crown", 0x81C, Profession::Ritualist, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Dread Mask", 0x832, Profession::Ritualist, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Highlander Woad", 0x8C6, Profession::Ritualist, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Mask of the Mo Zing", 0x7FF, Profession::Ritualist, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Norn Woad", 0x8C7, Profession::Ritualist, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Slim Spectacles", 0x84A, Profession::Ritualist, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Spectacles", 0x84B, Profession::Ritualist, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Tinted Spectacles", 0x84C, Profession::Ritualist, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6}
    };

    Armor paragon_armors[] = {
        // Core
        {"Obsidian Raiment", 0x65C, Profession::Paragon, ItemType::Chestpiece, Campaign::Core, 13},
        {"Obsidian Armguards", 0x65D, Profession::Paragon, ItemType::Gloves, Campaign::Core, 13},
        {"Obsidian Leggings", 0x65E, Profession::Paragon, ItemType::Leggings, Campaign::Core, 13},
        {"Obsidian Sandals", 0x65B, Profession::Paragon, ItemType::Boots, Campaign::Core, 13},
        // Nightfall
        {"Exalted Vabbian Crest", 0x38434, Profession::Paragon, ItemType::Headpiece, Campaign::Nightfall, 0x9,0x20000603},
        {"Vocal Vabbian Crest", 0x322C1, Profession::Elementalist, ItemType::Headpiece, Campaign::Nightfall, 0x9,0x20000603},
        {"Command Vabbian Crest", 0x38440, Profession::Elementalist, ItemType::Headpiece, Campaign::Nightfall, 0x9,0x20000603},
        {"Spear Vabbian Crest", 0x38437, Profession::Elementalist, ItemType::Headpiece, Campaign::Nightfall, 0x9,0x20000603},
        {"Exalted Elonian Crest", 0x3843D, Profession::Paragon, ItemType::Headpiece, Campaign::Nightfall, 0x9,0x20000603},
        {"Vocal Elonian Crest", 0x3842B, Profession::Elementalist, ItemType::Headpiece, Campaign::Nightfall, 0x9,0x20000603},
        {"Command Elonian Crest", 0x3843A, Profession::Elementalist, ItemType::Headpiece, Campaign::Nightfall, 0x9,0x20000603},
        {"Spear Elonian Crest", 0x38431, Profession::Elementalist, ItemType::Headpiece, Campaign::Nightfall, 0x9,0x20000603},

        {"Istani Raiment", 0x640, Profession::Paragon, ItemType::Chestpiece, Campaign::Nightfall, 13},
        {"Istani Armguards", 0x641, Profession::Paragon, ItemType::Gloves, Campaign::Nightfall, 13},
        {"Istani Leggings", 0x642, Profession::Paragon, ItemType::Leggings, Campaign::Nightfall, 13},
        {"Istani Sandals", 0x63F, Profession::Paragon, ItemType::Boots, Campaign::Nightfall, 13},
        {"Sunspear Raiment", 0x644, Profession::Paragon, ItemType::Chestpiece, Campaign::Nightfall, 13},
        {"Sunspear Armguards", 0x645, Profession::Paragon, ItemType::Gloves, Campaign::Nightfall, 13},
        {"Sunspear Leggings", 0x646, Profession::Paragon, ItemType::Leggings, Campaign::Nightfall, 13},
        {"Sunspear Sandals", 0x643, Profession::Paragon, ItemType::Boots, Campaign::Nightfall, 13},
        {"Elonian Raiment", 0x658, Profession::Paragon, ItemType::Chestpiece, Campaign::Nightfall, 13},
        {"Elonian Armguards", 0x659, Profession::Paragon, ItemType::Gloves, Campaign::Nightfall, 13},
        {"Elonian Leggings", 0x65A, Profession::Paragon, ItemType::Leggings, Campaign::Nightfall, 13},
        {"Elonian Sandals", 0x657, Profession::Paragon, ItemType::Boots, Campaign::Nightfall, 13},
        {"Elite Sunspear Raiment", 0x648, Profession::Paragon, ItemType::Chestpiece, Campaign::Nightfall, 13},
        {"Elite Sunspear Armguards", 0x649, Profession::Paragon, ItemType::Gloves, Campaign::Nightfall, 13},
        {"Elite Sunspear Leggings", 0x64A, Profession::Paragon, ItemType::Leggings, Campaign::Nightfall, 13},
        {"Elite Sunspear Sandals", 0x647, Profession::Paragon, ItemType::Boots, Campaign::Nightfall, 13},
        {"Vabbian Raiment", 0x64C, Profession::Paragon, ItemType::Chestpiece, Campaign::Nightfall, 13},
        {"Vabbian Armguards", 0x64D, Profession::Paragon, ItemType::Gloves, Campaign::Nightfall, 13},
        {"Vabbian Leggings", 0x64E, Profession::Paragon, ItemType::Leggings, Campaign::Nightfall, 13},
        {"Vabbian Sandals", 0x64B, Profession::Paragon, ItemType::Boots, Campaign::Nightfall, 13},
        {"Ancient Raiment", 0x650, Profession::Paragon, ItemType::Chestpiece, Campaign::Nightfall, 13},
        {"Ancient Armguards", 0x651, Profession::Paragon, ItemType::Gloves, Campaign::Nightfall, 13},
        {"Ancient Leggings", 0x652, Profession::Paragon, ItemType::Leggings, Campaign::Nightfall, 13},
        {"Ancient Sandals", 0x64F, Profession::Paragon, ItemType::Boots, Campaign::Nightfall, 13},
        {"Primeval Raiment", 0x654, Profession::Paragon, ItemType::Chestpiece, Campaign::Nightfall, 13},
        {"Primeval Armguards", 0x655, Profession::Paragon, ItemType::Gloves, Campaign::Nightfall, 13},
        {"Primeval Leggings", 0x656, Profession::Paragon, ItemType::Leggings, Campaign::Nightfall, 13},
        {"Primeval Sandals", 0x653, Profession::Paragon, ItemType::Boots, Campaign::Nightfall, 13},
        // EotN
        {"Norn Raiment", 0x7D9, Profession::Paragon, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 45},
        {"Norn Armguards", 0x7DA, Profession::Paragon, ItemType::Gloves, Campaign::EyeOfTheNorth, 45},
        {"Norn Leggings", 0x7DB, Profession::Paragon, ItemType::Leggings, Campaign::EyeOfTheNorth, 45},
        {"Norn Sandals", 0x7D8, Profession::Paragon, ItemType::Boots, Campaign::EyeOfTheNorth, 45},
        {"Asuran Raiment", 0x801, Profession::Paragon, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 45},
        {"Asuran Armguards", 0x802, Profession::Paragon, ItemType::Gloves, Campaign::EyeOfTheNorth, 45},
        {"Asuran Leggings", 0x803, Profession::Paragon, ItemType::Leggings, Campaign::EyeOfTheNorth, 45},
        {"Asuran Sandals", 0x800, Profession::Paragon, ItemType::Boots, Campaign::EyeOfTheNorth, 45},
        {"Winged Raiment", 0x8CF, Profession::Paragon, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 45},
        {"Sashed Leggings", 0x8D0, Profession::Paragon, ItemType::Leggings, Campaign::EyeOfTheNorth, 45},
        {"Banded Sandals", 0x8D1, Profession::Paragon, ItemType::Boots, Campaign::EyeOfTheNorth, 45},
        {"Monument Raiment", 0x8FB, Profession::Paragon, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 45},
        {"Monument Armguards", 0x8FC, Profession::Paragon, ItemType::Gloves, Campaign::EyeOfTheNorth, 45},
        {"Monument Leggings", 0x8FD, Profession::Paragon, ItemType::Leggings, Campaign::EyeOfTheNorth, 45},
        {"Monument Sandals", 0x8FA, Profession::Paragon, ItemType::Boots, Campaign::EyeOfTheNorth, 45},
        {"Chaos Gloves", 0x807, Profession::Paragon, ItemType::Gloves, Campaign::EyeOfTheNorth, 38},
        {"Destroyer Gauntlets", 0x85B, Profession::Paragon, ItemType::Gloves, Campaign::EyeOfTheNorth, 6},
        {"Dragon Gauntlets", 0x8CB, Profession::Paragon, ItemType::Gloves, Campaign::EyeOfTheNorth, 43},
        {"Glacial Gauntlets", 0x865, Profession::Paragon, ItemType::Gloves, Campaign::EyeOfTheNorth, 6},
        {"Stone Gauntlets", 0x8CC, Profession::Paragon, ItemType::Gloves, Campaign::EyeOfTheNorth, 8},
        {"Bandana", 0x804, Profession::Paragon, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Blindfold", 0x81D, Profession::Paragon, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Crown", 0x81E, Profession::Paragon, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Dread Mask", 0x833, Profession::Paragon, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Highlander Woad", 0x8CD, Profession::Paragon, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Mask of the Mo Zing", 0x805, Profession::Paragon, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Norn Woad", 0x8CE, Profession::Paragon, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Slim Spectacles", 0x84D, Profession::Paragon, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Spectacles", 0x84E, Profession::Paragon, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Tinted Spectacles", 0x84F, Profession::Paragon, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
    };

    Armor dervish_armors[] = {
        // Core
        {"Obsidian Hood", 0x696, Profession::Dervish, ItemType::Headpiece, Campaign::Core, 14},
        {"Obsidian Robes", 0x698, Profession::Dervish, ItemType::Chestpiece, Campaign::Core, 14},
        {"Obsidian Vambraces", 0x699, Profession::Dervish, ItemType::Gloves, Campaign::Core, 14},
        {"Obsidian Leggings", 0x69A, Profession::Dervish, ItemType::Leggings, Campaign::Core, 14},
        {"Obsidian Shoes", 0x697, Profession::Dervish, ItemType::Boots, Campaign::Core, 14},
        // Nightfall
        {"Istani Hood", 0x673, Profession::Dervish, ItemType::Headpiece, Campaign::Nightfall, 14},
        {"Istani Robes", 0x675, Profession::Dervish, ItemType::Chestpiece, Campaign::Nightfall, 14},
        {"Istani Vambraces", 0x676, Profession::Dervish, ItemType::Gloves, Campaign::Nightfall, 14},
        {"Istani Leggings", 0x677, Profession::Dervish, ItemType::Leggings, Campaign::Nightfall, 14},
        {"Istani Shoes", 0x674, Profession::Dervish, ItemType::Boots, Campaign::Nightfall, 14},
        {"Sunspear Hood", 0x678, Profession::Dervish, ItemType::Headpiece, Campaign::Nightfall, 14},
        {"Sunspear Robes", 0x67A, Profession::Dervish, ItemType::Chestpiece, Campaign::Nightfall, 14},
        {"Sunspear Vambraces", 0x67B, Profession::Dervish, ItemType::Gloves, Campaign::Nightfall, 14},
        {"Sunspear Leggings", 0x67C, Profession::Dervish, ItemType::Leggings, Campaign::Nightfall, 14},
        {"Sunspear Shoes", 0x679, Profession::Dervish, ItemType::Boots, Campaign::Nightfall, 14},
        {"Elonian Hood", 0x691, Profession::Dervish, ItemType::Headpiece, Campaign::Nightfall, 14},
        {"Elonian Robes", 0x693, Profession::Dervish, ItemType::Chestpiece, Campaign::Nightfall, 14},
        {"Elonian Vambraces", 0x694, Profession::Dervish, ItemType::Gloves, Campaign::Nightfall, 14},
        {"Elonian Leggings", 0x695, Profession::Dervish, ItemType::Leggings, Campaign::Nightfall, 14},
        {"Elonian Shoes", 0x692, Profession::Dervish, ItemType::Boots, Campaign::Nightfall, 14},
        {"Elite Sunspear Hood", 0x67D, Profession::Dervish, ItemType::Headpiece, Campaign::Nightfall, 14},
        {"Elite Sunspear Robes", 0x67F, Profession::Dervish, ItemType::Chestpiece, Campaign::Nightfall, 14},
        {"Elite Sunspear Vambraces", 0x680, Profession::Dervish, ItemType::Gloves, Campaign::Nightfall, 14},
        {"Elite Sunspear Leggings", 0x681, Profession::Dervish, ItemType::Leggings, Campaign::Nightfall, 14},
        {"Elite Sunspear Shoes", 0x67E, Profession::Dervish, ItemType::Boots, Campaign::Nightfall, 14},
        {"Vabbian Hood", 0x682, Profession::Dervish, ItemType::Headpiece, Campaign::Nightfall, 14},
        {"Vabbian Robes", 0x684, Profession::Dervish, ItemType::Chestpiece, Campaign::Nightfall, 14},
        {"Vabbian Vambraces", 0x685, Profession::Dervish, ItemType::Gloves, Campaign::Nightfall, 14},
        {"Vabbian Leggings", 0x686, Profession::Dervish, ItemType::Leggings, Campaign::Nightfall, 14},
        {"Vabbian Shoes", 0x683, Profession::Dervish, ItemType::Boots, Campaign::Nightfall, 14},
        {"Ancient Hood", 0x687, Profession::Dervish, ItemType::Headpiece, Campaign::Nightfall, 14},
        {"Ancient Robes", 0x689, Profession::Dervish, ItemType::Chestpiece, Campaign::Nightfall, 14},
        {"Ancient Vambraces", 0x68A, Profession::Dervish, ItemType::Gloves, Campaign::Nightfall, 14},
        {"Ancient Leggings", 0x68B, Profession::Dervish, ItemType::Leggings, Campaign::Nightfall, 14},
        {"Ancient Shoes", 0x688, Profession::Dervish, ItemType::Boots, Campaign::Nightfall, 14},
        {"Primeval Hood", 0x68C, Profession::Dervish, ItemType::Headpiece, Campaign::Nightfall, 14},
        {"Primeval Robes", 0x68E, Profession::Dervish, ItemType::Chestpiece, Campaign::Nightfall, 14},
        {"Primeval Vambraces", 0x68F, Profession::Dervish, ItemType::Gloves, Campaign::Nightfall, 14},
        {"Primeval Leggings", 0x690, Profession::Dervish, ItemType::Leggings, Campaign::Nightfall, 14},
        {"Primeval Shoes", 0x68D, Profession::Dervish, ItemType::Boots, Campaign::Nightfall, 14},
        // EotN
        {"Norn Hood", 0x7DC, Profession::Dervish, ItemType::Headpiece, Campaign::EyeOfTheNorth, 44},
        {"Norn Robes", 0x809, Profession::Dervish, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 44},
        {"Norn Vambraces", 0x80A, Profession::Dervish, ItemType::Gloves, Campaign::EyeOfTheNorth, 44},
        {"Norn Leggings", 0x821, Profession::Dervish, ItemType::Leggings, Campaign::EyeOfTheNorth, 44},
        {"Norn Shoes", 0x808, Profession::Dervish, ItemType::Boots, Campaign::EyeOfTheNorth, 44},
        {"Asuran Hood", 0x806, Profession::Dervish, ItemType::Headpiece, Campaign::EyeOfTheNorth, 44},
        {"Asuran Robes", 0x823, Profession::Dervish, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 44},
        {"Asuran Vambraces", 0x824, Profession::Dervish, ItemType::Gloves, Campaign::EyeOfTheNorth, 44},
        {"Asuran Leggings", 0x825, Profession::Dervish, ItemType::Leggings, Campaign::EyeOfTheNorth, 44},
        {"Asuran Shoes", 0x822, Profession::Dervish, ItemType::Boots, Campaign::EyeOfTheNorth, 44},
        {"Monument Hood", 0x8FF, Profession::Dervish, ItemType::Headpiece, Campaign::EyeOfTheNorth, 44},
        {"Monument Robes", 0x901, Profession::Dervish, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 44},
        {"Monument Vambraces", 0x902, Profession::Dervish, ItemType::Gloves, Campaign::EyeOfTheNorth, 44},
        {"Monument Leggings", 0x903, Profession::Dervish, ItemType::Leggings, Campaign::EyeOfTheNorth, 44},
        {"Monument Shoes", 0x900, Profession::Dervish, ItemType::Boots, Campaign::EyeOfTheNorth, 44},
        {"Light Breastplate", 0x8D6, Profession::Dervish, ItemType::Chestpiece, Campaign::EyeOfTheNorth, 44},
        {"Hooked Leggings", 0x8D7, Profession::Dervish, ItemType::Leggings, Campaign::EyeOfTheNorth, 44},
        {"Padded Sandals", 0x867, Profession::Dervish, ItemType::Boots, Campaign::EyeOfTheNorth, 44},
        {"Chaos Gloves", 0x82A, Profession::Dervish, ItemType::Gloves, Campaign::EyeOfTheNorth, 38},
        {"Destroyer Gauntlets", 0x85C, Profession::Dervish, ItemType::Gloves, Campaign::EyeOfTheNorth, 6},
        {"Dragon Gauntlets", 0x8D2, Profession::Dervish, ItemType::Gloves, Campaign::EyeOfTheNorth, 6},
        {"Glacial Gauntlets", 0x866, Profession::Dervish, ItemType::Gloves, Campaign::EyeOfTheNorth, 6},
        {"Stone Gauntlets", 0x8D5, Profession::Dervish, ItemType::Gloves, Campaign::EyeOfTheNorth, 6},
        {"Bandana", 0x80B, Profession::Dervish, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Blindfold", 0x81F, Profession::Dervish, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Crown", 0x820, Profession::Dervish, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Dread Mask", 0x834, Profession::Dervish, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Mask of the Mo Zing", 0x80C, Profession::Dervish, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Highlander Woad", 0x8D3, Profession::Dervish, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Norn Woad", 0x8D4, Profession::Dervish, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Slim Spectacles", 0x850, Profession::Dervish, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Spectacles", 0x851, Profession::Dervish, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6},
        {"Tinted Spectacles", 0x852, Profession::Dervish, ItemType::Headpiece, Campaign::EyeOfTheNorth, 6}
    };
    Armor costume_heads[] = {
        {"Grenth's Visage", 0x9e3, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},
        {"Dwayna's Diadem", 0x9e8, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},
        {"Shining Blade Cowl", 0xa5e, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 35},
        {"White Mantle Mitre", 0xa59, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 35},
        //{"0xac7", 0xac7, GW::Constants::Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},
        {"Wedding Headpiece", 0xacc, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},
        //{"0xad1", 0xad1, GW::Constants::Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},
        {"Formal Headwear", 0xad6, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},
        {"Dapper Top Hat", 0xadb, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},
        {"Cowl of the Lich", 0xae0, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 47},
        {"Aspect of the Lich", 0xae1, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 47},
        {"Lunatic Court Helm", 0xaeb, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},

        {"Horned Helm of Balthazar", 0xaf4, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},
        {"Menaldru's Helm", 0xaf9, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},
        {"Ravenheart's Gaze", 0xdd7, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 46},
        {"Wraith Veil", 0xddc, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 46},
        {"Facets of Lyssa", 0xe54, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},
        {"True Sight of Kormir", 0xe4f, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},
        {"Pumpkin Crown", 0x004, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 9},
        {"Horns of Grenth", 0x26d, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 9},

        {"Yule Cap", 0x1c9, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 9},
        {"Tengu Mask", 0x517, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 9},
        {"Dragon Mask", 0x51f, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 9},
        {"Furious Pumpkin Crown (broken effect)", 0x75e, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},
        {"Wicked Hat", 0x75f, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},
        {"Great Horns of Grenth", 0x775, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},
        {"Jester's Cap", 0x778, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 9},
        {"Stylish Yule Cap", 0x776, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},

        {"Freezie Crown", 0x777, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},
        {"Lion Mask", 0x7ad, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 3},
        {"Demon Mask", 0x7b7, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 6},
        {"Scarecrow Mask", 0x910, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 6},
        {"Mummy Mash", 0x90f, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},
        {"Ice Crown", 0x923, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0}, // @Cleanup: dye tint for this?
        {"Wreath Crown", 0x924, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0}, // @Cleanup: dye tint for this?
        {"Grasping Mask", 0x937, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0}, // @Cleanup: dye tint for this?

        {"Zombie Face Paint", 0x94b, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},
        {"Lupine Mask", 0x94c, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},
        {"Grentch Cap", 0x95f, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},
        {"Rudi Mask", 0x960, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},
        {"Imperial Dragon Mask", 0x973, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 3},
        {"Skeleton Face Paint", 0x97d, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},
        {"Charr Hat", 0x9af, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},
        {"Ice Shard Crest", 0x9e1, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},

        {"Snow Crystal Crest", 0x9e2, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},
        {"Sinister Dragon Mask", 0xabd, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0}, // @Cleanup: dye tint for this?
        {"Spectercles", 0xd15, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},
        {"Furrocious Ears", 0xd16, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},
        {"Demonic Horns", 0xd17, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},
        {"Divine Halo (broken effect)", 0xb01, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},
        {"Wisdom of the Dragon", 0xd69, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},
        {"Aegis of Unity Headpiece", 0xd6e, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0}, // @Cleanup: proper name?

        {"Mirthful Dragon Mask", 0xd73, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},
        {"Reaper's Hood", 0xde1, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},
        {"Tricorne", 0xde2, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},
        {"Festive Winter Hood", 0xe59, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},
        {"Stylish Red Striped Scarf", 0xe5a, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},
        {"Stylish White Striped Scarf", 0xe5b, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0},
        {"Stylish Black Scarf", 0xe5c, Profession::None, ItemType::Costume_Headpiece, Campaign::BonusMissionPack, 0}
    };
    Armor costumes[] = {

        {"Dapper Tuxedo", 0xadd, Profession::None, ItemType::Costume, Campaign::BonusMissionPack, 0},
        {"Raiment of the Lich", 0xae3, Profession::None, ItemType::Costume, Campaign::BonusMissionPack, 47},
        {"Lunatic Court Finery", 0xaed, Profession::None, ItemType::Costume, Campaign::BonusMissionPack, 46},
        {"Headless Lunatic Court Finery", 0xaf1, Profession::None, ItemType::Costume, Campaign::BonusMissionPack, 0},
        {"Ravenheart Witchwear", 0xdd9, Profession::None, ItemType::Costume, Campaign::BonusMissionPack, 46},
        {"Vale Wraith", 0xdde, Profession::None, ItemType::Costume, Campaign::BonusMissionPack, 0},

        {"Grenth's Regalia", 0x9e5, Profession::None, ItemType::Costume, Campaign::BonusMissionPack, 0},
        {"Dwayna's Regalia", 0x9ea, Profession::None, ItemType::Costume, Campaign::BonusMissionPack, 0},
        {"Agent of Balthazar", 0xaf6, Profession::None, ItemType::Costume, Campaign::BonusMissionPack, 0},
        {"Disciple of Melandru", 0xafb, Profession::None, ItemType::Costume, Campaign::BonusMissionPack, 0},
        {"Vision of Lyssa", 0xe56, Profession::None, ItemType::Costume, Campaign::BonusMissionPack, 0},
        {"Augur of Kormir", 0xe51, Profession::None, ItemType::Costume, Campaign::BonusMissionPack, 0},

        // {"0xac9", 0xac9, GW::Constants::Profession::None, ItemType::Costume, Campaign::BonusMissionPack, 0}, // Grenth Dupe
        {"Traditional Wedding Couple Attire", 0xace, Profession::None, ItemType::Costume, Campaign::BonusMissionPack, 0},
        {"Wedding Couple Attire", 0xd39, Profession::None, ItemType::Costume, Campaign::BonusMissionPack, 0},
        // {"0xad3", 0xad3, GW::Constants::Profession::None, ItemType::Costume, Campaign::BonusMissionPack, 0}, // Grenth Dupe
        {"Formal Attire", 0xad8, Profession::None, ItemType::Costume, Campaign::BonusMissionPack, 0},

        {"Shining Blade Uniform", 0xa60, Profession::None, ItemType::Costume, Campaign::BonusMissionPack, 35},
        {"White Mantle Robes", 0xa5b, Profession::None, ItemType::Costume, Campaign::BonusMissionPack, 21},

        {"Dragonguard", 0xd6b, Profession::None, ItemType::Costume, Campaign::BonusMissionPack, 0},
        {"Aegis of Unity", 0xd70, Profession::None, ItemType::Costume, Campaign::BonusMissionPack, 0},
    };
    Armor weapons[] = {
        // Axes
        {"Chaos Axe", 0x213B9, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Eaglecrest Axe", 0x5370C, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Envoy Axe", 0x5A525, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Fiery Blade Axe", 0x3850F, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Icy Blade Axe", 0x3852D, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Pyroclastic Axe", 0x49A6A, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x2E130441},
        {"Serpentine Reaver", 0x53709, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Tormented Axe", 0x43481, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Candy Cane Axe", 0x26AA3, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Gothic Dual Axe", 0x2B0CD, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Mallyx's Reaver", 0x38528, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000},

        // Bows
        {"Black Hawk's Lust", 0x26940, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Drago's Flatbow", 0x2598A, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Dryad Bow", 0x5370F, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Eternal Bow", 0x213B0, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Flatbow", 0x24A4, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 4, 0x22330001},
        {"Scorpion Bow", 0x2546F, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Scorpion's Lust", 0x2693D, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Silverwing Bow", 0x49A90, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Storm Bow", 0x213D5, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Tormented Longbow", 0x43496, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Tormented Shortbow", 0x43499, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Balthazar's Flatbow", 0x5C93E, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Bramble Flatbow", 0x2B0F1, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Candy Cane Bow", 0x26AA4, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Half Moon", 0x18213, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Ivory Bow", 0x17C3D, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Keiran's Bow", 0x2B0E2, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Oppressor's Recurve Bow", 0x57C20, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Turtle Shell Longbow", 0x49A83, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000},

        // Daggers
        {"Tormented Daggers", 0x4348A, Profession::None, ItemType::Daggers, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Arrowblade Daggers", 0x49A9A, Profession::None, ItemType::Daggers, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Butterfly Knives", 0x28433, Profession::None, ItemType::Daggers, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Celestial Daggers", 0x2AF95, Profession::None, ItemType::Daggers, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Crude Daggers", 0x49AA4, Profession::None, ItemType::Daggers, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Decade Daggers \"High Noon\"", 0x3848B, Profession::None, ItemType::Daggers, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Peppermint Daggers", 0x43B8C, Profession::None, ItemType::Daggers, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Stilletos", 0x2AF0D, Profession::None, ItemType::Daggers, Campaign::BonusMissionPack, 3, 0x20000000},

        // Foci
        {"Bone Idol", 0x2B136, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Celestial Compass", 0x5371d, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Chimeric Prism", 0x235AE, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Deldrimor Focus", 0x5263B, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Forgotten Fan", 0x2B11F, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Grim Cesta", 0x1553D, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 0, 0x20000000},
        {"Heaven's Arch", 0x25487, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Inscribed Chakram (metal)", 0x15DB2, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 0, 0x20000000},
        {"Jeweled Chalice", 0x15DD8, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x2C020000},
        {"Jug", 0x2B14B, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Luminescent Lantern", 0x385CD, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Paper Fan / The Windcatcher", 0x2B125, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Paper Lantern", 0x2B13C, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Plagueborn Focus", 0x28408, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 0, 0x20000000},
        {"Rose Focus", 0x49A60, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Storm Ember", 0x25484, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Tiger's Pride", 0x26948, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Tormented Focus", 0x4349C, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 0, 0x28020000},
        {"Aureate Chalice", 0x18223, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Bleached Skull", 0x2B139, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Bogroot Focus", 0x385BA, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Channeling Focus", 0x2B146, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Destroyer Focus", 0x49CE0, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Gingerbread Focus", 0x26B48, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Gwen's Flute", 0x1B57D, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Jeweled Chakram", 0x15DB5, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Majestic Focus", 0x385C0, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Oppressor's Focus", 0x57791, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Restoration Focus", 0x2AEAA, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"The Bison Cup", 0x499FD, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Zehtuka's Horn", 0x49A5B, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000},

        // Hammers
        {"Anniversary Hammer \"Verdict\"", 0x2AFB1, Profession::None, ItemType::Hammer, Campaign::BonusMissionPack, 3, 0x2A221201},
        {"Kanaxai's Mallet", 0x2B16F, Profession::None, ItemType::Hammer, Campaign::BonusMissionPack, 0, 0x2231C210},
        {"Tormented Maul", 0x43484, Profession::None, ItemType::Hammer, Campaign::BonusMissionPack, 0, 0x20000000},
        {"Candy Cane Hammer", 0x26AA8, Profession::None, ItemType::Hammer, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Copper Crusher", 0x385EA, Profession::None, ItemType::Hammer, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Destroyer Maul", 0x49CE3, Profession::None, ItemType::Hammer, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Ruby Maul", 0x38603, Profession::None, ItemType::Hammer, Campaign::BonusMissionPack, 3, 0x20000000},

        // Scythes
        {"Banana Scythe", 0x3868D, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Bronze Scythe", 0x386B0, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 2, 0x2E330001},
        {"Decade Scythe \"Funeral Fang\"", 0x38686, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x22330011},
        {"Dhuum's Soul Reaper", 0x564F2, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Envoy Scythe", 0x5A524, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Soulbreaker", 0x3247B, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Tormented Scythe", 0x4348D, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Wintergreen Scythe", 0x40E5D, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 0, 0x2A331601},
        {"Peppermint Scythe", 0x43B8E, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000},

        // Shields
        {"Amber Aegis", 0x2AFC6, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20020041},
        {"Amethyst Aegis", 0x49B23, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x2C120441},
        {"Aureate Aegis", 0x386F2, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Bladed Shield", 0x2AF29, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Canthan Targe", 0x24ECD, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Celestial Shield", 0x2AFC9, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Crude Shield", 0x3C59, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Darkwing Defender", 0x3870B, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Diamond Aegis", 0x2AF2E, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Draconic Aegis", 0x53742, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x2C120641},
        {"Echovald Shield", 0x2B231, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Embossed Aegis", 0x2B227, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x28020001},
        {"Eternal Shield", 0x21406, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Exalted Aegis", 0x2B215, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Guardian of the Hunt", 0x2B218, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Iridescent Aegis", 0x2AF33, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Istani Shield", 0x38729, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Japan 1st Anniversary Shield", 0x4402F, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Kappa Shield", 0x2AFD3, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Magmas Shield", 0x21412, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Mursaat Shield", 0x5274D, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Ominous Aegis", 0x2EBDE, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Outcast Shield", 0x2AEBC, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Shadow Shield", 0x2140B, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Spider's Gluttony", 0x26962, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Tormented Shield", 0x43493, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Wintergreen Shield", 0x40E60, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Zodiac Shield", 0x2B1B6, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x2C020001},
        {"Destroyer Shield", 0x47A03, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Gingerbread Shield", 0x26AAE, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Great Conch", 0x49B28, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Oppressor's Shield", 0x57778, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Peppermint Shield", 0x26AB3, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Reefclaw's Refuge", 0x9F24, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000},

        // Spears
        {"Spirit of the Forgotten", 0x38762, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 0, 0x2210C611},
        {"Sunspear", 0x32485, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Tormented Spear", 0x43490, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Voltaic Spear", 0x49B32, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 3, 0x2E130441},
        {"Destroyer Spear", 0x49CED, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Peppermint Spear", 0x43B8F, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Tureksin's Spear", 0x38753, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 3, 0x20000000},

        // Staves
        {"Bedlam Staff", 0x387B2, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Bo Staff", 0x2AE68, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Bone Dragon Staff", 0x53756, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Dragon's Envy", 0x26968, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Envoy Staff", 0x2AE80, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Earthbound Staff/Magmus' Staff", 0x28405, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Firebrand", 0x5D84, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Holy Staff", 0x1B5D1, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Hourglass Staff", 0x49A51, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Inscribed Staff", 0x9F29, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, 
        {"Jade Staff", 0x28424, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Onyx Staff", 0x49B94, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 4, 0x2E330441},
        {"Plagueborn Staff", 0x2841f, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x2E330441},

        {"Platinum Staff", 0x2B256, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x2E330441},
        {"Snake's Envy", 0x26965, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Spiritbinder", 0x2ADF1, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Tormented Staff", 0x434A2, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Togo's Staff / Channeling Staff", 0x2AE0C, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 2, 0x2A200401},
        {"Asuran Staff", 0x49C71, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Bone Staff", 0x17955, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Candy Cane Staff", 0x26AB4, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Clairvoyant Staff", 0x2B27B, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Fuchsia Staff", 0x49B85, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Lotus Staff", 0x2B245, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Portal Staff", 0x387A8, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Raven Staff", 0x21420, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Shadow Staff", 0x21423, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Suntouched Staff", 0x49BC2, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000},

        // Swords
        {"Butterfly Sword", 0x3843, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 0, 0x20000000},
        {"Colossal Scimitar", 0x387F8, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Crystalline Sword", 0x383E, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Embersteel Blade", 0x49C17, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x2E134441},
        {"Envoy Sword", 0x2AE83, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Eternal Blade", 0x17B20, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Fellblade", 0x2142D, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x22030041},
        {"Glacial Blade", 0x49A56, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Icy Dragon Sword", 0x26ABA, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Jade Sword", 0x2B2A9, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x2E130441},
        {"Obsidian Edge", 0x53A92, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Oni Blade", 0x2AFF8, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Plagueborn Sword", 0x2B002, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Shiro's Sword", 0x2AE62, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Tormented Sword", 0x43487, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Turai's Sword", 0x5266A, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Vampiric Dragon Sword", 0x55777, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x2A024201},
        {"Vertebreaker", 0x2B2A4, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Wintergreen Sword", 0x40E63, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 0, 0x2A021600},
        {"Candy Cane Sword", 0x26AB5, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Elemental Sword", 0x38807, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Etched Sword", 0x49BCE, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Mammoth Blade", 0x49C3A, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Oppressor's Sword", 0x57796, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Stoneblade", 0x49C30, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000},

        // Wands
        {"Amber Wand", 0x28415, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 2, 0x22020041},
        {"Channeling Rod", 0x2B208, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 2, 0x22020041},
        {"Frog Scepter", 0x53733, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x2E120641},
        {"Holy Rod", 0x172B8, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 0, 0x22000001},
        {"Jade Wand", 0x28412, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 2, 0x22020041},
        {"Jellyfish Wand", 0x2B1B8, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Koi Scepter", 0x2B1BD, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Peacock's Wrath", 0x2695A, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Platinum Wand", 0x2B19F, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 2, 0x2E120441},
        {"Plagueborn Scepter", 0x2840d, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Quicksilver", 0x2548A, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 0, 0x2E020001},
        {"Tormented Scepter", 0x4349F, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 0, 0x2A020001},
        {"Unicorn's Wrath", 0x26957, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Wayward Wand", 0x2B17E, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Wintergreen Wand", 0x40E5C, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 0, 0x2A021600},
        {"Zodiac Scepter", 0x2841e, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Bogroot Rod", 0x49ADB, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Bone Spiral Rod", 0x2B1E5, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Brightclaw", 0x2B1EF, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Candy Cane Wand", 0x26AA9, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Cane", 0x16D68, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Destroyer Scepter", 0x49CE6, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Droknar's Scepter", 0x50950, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Hypnotic Scepter", 0x2B1B3, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Imposing Scepter", 0x38632, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Oppressor's Scepter", 0x57C1D, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Restoration Scepter", 0x38671, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000},
        {"Ritualist Cane", 0x2B203, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000},

        // -------------------------------------------------------------------
        // Models discovered by scanning Gw.dat for ffna type-2 files that match
        // the chunk layout of the weapons above (0xFA0 + 0xFA1 + 0xFA5).
        // Labels are placeholder hex file ids, to be renamed as they are
        // identified in game. ItemType is inferred from the 0xFA0 rig hash and
        // is only reliable where marked "rig-pure"; the rest are best guesses
        // and put the model in the wrong dropdown at worst.
        // -------------------------------------------------------------------

        // Discovered Axe (52)
        {"0x2B0AA", 0x2B0AA, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B0AF", 0x2B0AF, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B0B9", 0x2B0B9, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B0BE", 0x2B0BE, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B0C3", 0x2B0C3, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B0D2", 0x2B0D2, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B0D7", 0x2B0D7, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x32447", 0x32447, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x32480", 0x32480, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x37340", 0x37340, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x3735F", 0x3735F, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x3736E", 0x3736E, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x373A5", 0x373A5, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x373C6", 0x373C6, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x373ED", 0x373ED, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38444", 0x38444, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38449", 0x38449, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x3844C", 0x3844C, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38453", 0x38453, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38454", 0x38454, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x3845C", 0x3845C, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x3847E", 0x3847E, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x384F6", 0x384F6, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x384FB", 0x384FB, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38500", 0x38500, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38505", 0x38505, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x3850A", 0x3850A, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38514", 0x38514, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38519", 0x38519, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x3851E", 0x3851E, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38523", 0x38523, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38532", 0x38532, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38537", 0x38537, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38565", 0x38565, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38566", 0x38566, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38569", 0x38569, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38583", 0x38583, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38586", 0x38586, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38589", 0x38589, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x43474", 0x43474, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x499D4", 0x499D4, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xA16264A1
        {"0x499F0", 0x499F0, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xA16264A1
        {"0x499F1", 0x499F1, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xA16264A1
        {"0x49A65", 0x49A65, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49A6F", 0x49A6F, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49A74", 0x49A74, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49A79", 0x49A79, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49EB2", 0x49EB2, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xA16264A1
        {"0x5063E", 0x5063E, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xA16264A1
        {"0x5789F", 0x5789F, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xA16264A1
        {"0x5A523", 0x5A523, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xA16264A1
        {"0x5A52B", 0x5A52B, Profession::None, ItemType::Axe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xA16264A1

        // Discovered Bow (61)
        {"0x2523", 0x2523, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xAF290816
        {"0x17C42", 0x17C42, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xAF290816
        {"0x25818", 0x25818, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0xC02810C9
        {"0x283A8", 0x283A8, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xAF290816
        {"0x283AA", 0x283AA, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xAF290816
        {"0x283B4", 0x283B4, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xAF290816
        {"0x283B5", 0x283B5, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xAF290816
        {"0x283B6", 0x283B6, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xAF290816
        {"0x283B7", 0x283B7, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xAF290816
        {"0x283D5", 0x283D5, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xAF290816
        {"0x283D6", 0x283D6, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xAF290816
        {"0x28435", 0x28435, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xAF290816
        {"0x28441", 0x28441, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xAF290816
        {"0x2AEA0", 0x2AEA0, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xAF290816
        {"0x2AEAD", 0x2AEAD, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xAF290816
        {"0x2AEF4", 0x2AEF4, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x7DEB68AE
        {"0x2AF7E", 0x2AF7E, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xAF290816
        {"0x2AF83", 0x2AF83, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x7DEB68AE
        {"0x2AF88", 0x2AF88, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x7DEB68AE
        {"0x2AF8D", 0x2AF8D, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xAF290816
        {"0x2B0D8", 0x2B0D8, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xAF290816
        {"0x2B0E7", 0x2B0E7, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0xC02810C9
        {"0x2B0EC", 0x2B0EC, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0xC02810C9
        {"0x2B0F2", 0x2B0F2, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x7DEB68AE
        {"0x3853C", 0x3853C, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x7DEB68AE
        {"0x38541", 0x38541, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x7DEB68AE
        {"0x38544", 0x38544, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x7DEB68AE
        {"0x38549", 0x38549, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xAF290816
        {"0x3854A", 0x3854A, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xAF290816
        {"0x3854F", 0x3854F, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xAF290816
        {"0x38554", 0x38554, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xAF290816
        {"0x38555", 0x38555, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xAF290816
        {"0x3855A", 0x3855A, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xAF290816
        {"0x3855B", 0x3855B, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xAF290816
        {"0x38560", 0x38560, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x7DEB68AE
        {"0x40E59", 0x40E59, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0xC02810C9
        {"0x43B83", 0x43B83, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0xC02810C9
        {"0x458E4", 0x458E4, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xAF290816
        {"0x49A4C", 0x49A4C, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xAF290816
        {"0x49A7E", 0x49A7E, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x7DEB68AE
        {"0x49A86", 0x49A86, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x7DEB68AE
        {"0x49A8B", 0x49A8B, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x7DEB68AE
        {"0x49C4A", 0x49C4A, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xAF290816
        {"0x49C7A", 0x49C7A, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x7DEB68AE
        {"0x49CA9", 0x49CA9, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x7DEB68AE
        {"0x49CDA", 0x49CDA, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x7DEB68AE
        {"0x50948", 0x50948, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xAF290816
        {"0x5095D", 0x5095D, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xAF290816
        {"0x5262C", 0x5262C, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x7DEB68AE
        {"0x52631", 0x52631, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x7DEB68AE
        {"0x52674", 0x52674, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x7DEB68AE
        {"0x52679", 0x52679, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x7DEB68AE
        {"0x526B0", 0x526B0, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x7DEB68AE
        {"0x526B5", 0x526B5, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x7DEB68AE
        {"0x526EC", 0x526EC, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x7DEB68AE
        {"0x526F3", 0x526F3, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x7DEB68AE
        {"0x5272A", 0x5272A, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x7DEB68AE
        {"0x5272F", 0x5272F, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x7DEB68AE
        {"0x57782", 0x57782, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x057BA0C0
        {"0x5789E", 0x5789E, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xAF290816
        {"0x5AB66", 0x5AB66, Profession::None, ItemType::Bow, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x7DEB68AE

        // Discovered Daggers (20)
        {"0x282B1", 0x282B1, Profession::None, ItemType::Daggers, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x283CA", 0x283CA, Profession::None, ItemType::Daggers, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x283DA", 0x283DA, Profession::None, ItemType::Daggers, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x283FD", 0x283FD, Profession::None, ItemType::Daggers, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x2842E", 0x2842E, Profession::None, ItemType::Daggers, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2843C", 0x2843C, Profession::None, ItemType::Daggers, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2843D", 0x2843D, Profession::None, ItemType::Daggers, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2843E", 0x2843E, Profession::None, ItemType::Daggers, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x28440", 0x28440, Profession::None, ItemType::Daggers, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2844D", 0x2844D, Profession::None, ItemType::Daggers, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x2AE95", 0x2AE95, Profession::None, ItemType::Daggers, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x2AEB2", 0x2AEB2, Profession::None, ItemType::Daggers, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x2AEF9", 0x2AEF9, Profession::None, ItemType::Daggers, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x2AF42", 0x2AF42, Profession::None, ItemType::Daggers, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x2AF4C", 0x2AF4C, Profession::None, ItemType::Daggers, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x2AF56", 0x2AF56, Profession::None, ItemType::Daggers, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49A95", 0x49A95, Profession::None, ItemType::Daggers, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49A9F", 0x49A9F, Profession::None, ItemType::Daggers, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49AA9", 0x49AA9, Profession::None, ItemType::Daggers, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49AAE", 0x49AAE, Profession::None, ItemType::Daggers, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860

        // Discovered Hammer (25)
        {"0x2B15F", 0x2B15F, Profession::None, ItemType::Hammer, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B164", 0x2B164, Profession::None, ItemType::Hammer, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B167", 0x2B167, Profession::None, ItemType::Hammer, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B16C", 0x2B16C, Profession::None, ItemType::Hammer, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B174", 0x2B174, Profession::None, ItemType::Hammer, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B183", 0x2B183, Profession::None, ItemType::Hammer, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x3858E", 0x3858E, Profession::None, ItemType::Hammer, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x385A4", 0x385A4, Profession::None, ItemType::Hammer, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x385AC", 0x385AC, Profession::None, ItemType::Hammer, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x385AF", 0x385AF, Profession::None, ItemType::Hammer, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x385B2", 0x385B2, Profession::None, ItemType::Hammer, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x385C5", 0x385C5, Profession::None, ItemType::Hammer, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x385C8", 0x385C8, Profession::None, ItemType::Hammer, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x385D5", 0x385D5, Profession::None, ItemType::Hammer, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x385D8", 0x385D8, Profession::None, ItemType::Hammer, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x385DB", 0x385DB, Profession::None, ItemType::Hammer, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x385E5", 0x385E5, Profession::None, ItemType::Hammer, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x385EF", 0x385EF, Profession::None, ItemType::Hammer, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x385F4", 0x385F4, Profession::None, ItemType::Hammer, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x385F9", 0x385F9, Profession::None, ItemType::Hammer, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x385FE", 0x385FE, Profession::None, ItemType::Hammer, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38608", 0x38608, Profession::None, ItemType::Hammer, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x3860D", 0x3860D, Profession::None, ItemType::Hammer, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38612", 0x38612, Profession::None, ItemType::Hammer, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38617", 0x38617, Profession::None, ItemType::Hammer, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766

        // Discovered Offhand (158)
        {"0x2520", 0x2520, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x9B49", 0x9B49, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x9B50", 0x9B50, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x9B55", 0x9B55, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x9B5D", 0x9B5D, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0xF7C0", 0xF7C0, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x154EE", 0x154EE, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x159D9", 0x159D9, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x16FE3", 0x16FE3, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x16FE6", 0x16FE6, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x16FEB", 0x16FEB, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x16FFF", 0x16FFF, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x17C3C", 0x17C3C, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x18228", 0x18228, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x1A1BB", 0x1A1BB, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x1A6A9", 0x1A6A9, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x1B540", 0x1B540, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x1B544", 0x1B544, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x1B57E", 0x1B57E, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x1B5A2", 0x1B5A2, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x1E1AE", 0x1E1AE, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x1E203", 0x1E203, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x1E217", 0x1E217, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x1E21C", 0x1E21C, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x1E232", 0x1E232, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x1E254", 0x1E254, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x1E25E", 0x1E25E, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x235AA", 0x235AA, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x25155", 0x25155, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2515C", 0x2515C, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2548D", 0x2548D, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2581B", 0x2581B, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2581E", 0x2581E, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2592A", 0x2592A, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x25932", 0x25932, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2595C", 0x2595C, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2595F", 0x2595F, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2596B", 0x2596B, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x25978", 0x25978, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x25980", 0x25980, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x26945", 0x26945, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x19C96481
        {"0x26AA7", 0x26AA7, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x26B4A", 0x26B4A, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x26B4D", 0x26B4D, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AE90", 0x2AE90, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AE9B", 0x2AE9B, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AEA5", 0x2AEA5, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AEB7", 0x2AEB7, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AEBF", 0x2AEBF, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AEC4", 0x2AEC4, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AEC9", 0x2AEC9, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AECE", 0x2AECE, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AED3", 0x2AED3, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AED8", 0x2AED8, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AEEF", 0x2AEEF, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AEFE", 0x2AEFE, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AF03", 0x2AF03, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AF08", 0x2AF08, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AF12", 0x2AF12, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AF13", 0x2AF13, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AF18", 0x2AF18, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AF1F", 0x2AF1F, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AF24", 0x2AF24, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B0F7", 0x2B0F7, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B0FC", 0x2B0FC, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B101", 0x2B101, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B106", 0x2B106, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B10B", 0x2B10B, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B110", 0x2B110, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B11A", 0x2B11A, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B122", 0x2B122, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B128", 0x2B128, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B12D", 0x2B12D, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B130", 0x2B130, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B133", 0x2B133, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B141", 0x2B141, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B150", 0x2B150, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B155", 0x2B155, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B15A", 0x2B15A, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38491", 0x38491, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x384EC", 0x384EC, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x384F1", 0x384F1, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x3856E", 0x3856E, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x38571", 0x38571, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x38574", 0x38574, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x38579", 0x38579, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x3857E", 0x3857E, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x38593", 0x38593, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x38598", 0x38598, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x3859D", 0x3859D, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x385A9", 0x385A9, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x385B5", 0x385B5, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x385D2", 0x385D2, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x47D6C", 0x47D6C, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x48FD0", 0x48FD0, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x48FD5", 0x48FD5, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x499BC", 0x499BC, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x499BD", 0x499BD, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x499C3", 0x499C3, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x499C7", 0x499C7, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x499CE", 0x499CE, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x499D1", 0x499D1, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x499D9", 0x499D9, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x499DC", 0x499DC, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x499DE", 0x499DE, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x499DF", 0x499DF, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x499ED", 0x499ED, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x499FF", 0x499FF, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49CB3", 0x49CB3, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49CB8", 0x49CB8, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49CBB", 0x49CBB, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49CC0", 0x49CC0, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49CCF", 0x49CCF, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49CD7", 0x49CD7, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x50367", 0x50367, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x5036E", 0x5036E, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x503B6", 0x503B6, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x5092F", 0x5092F, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x5093A", 0x5093A, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x50943", 0x50943, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x5094D", 0x5094D, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x50958", 0x50958, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x50962", 0x50962, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x5096A", 0x5096A, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x5096F", 0x5096F, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x50994", 0x50994, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x52627", 0x52627, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x52640", 0x52640, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x52645", 0x52645, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x5264A", 0x5264A, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x52659", 0x52659, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x52664", 0x52664, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x52665", 0x52665, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x5266F", 0x5266F, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x52683", 0x52683, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x52688", 0x52688, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x5268D", 0x5268D, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x52692", 0x52692, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x526A1", 0x526A1, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x526AB", 0x526AB, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x526BF", 0x526BF, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x526C4", 0x526C4, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x526C9", 0x526C9, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x526CE", 0x526CE, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x526DD", 0x526DD, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x526E2", 0x526E2, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x526E7", 0x526E7, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x526FD", 0x526FD, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x52702", 0x52702, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x52707", 0x52707, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x5270C", 0x5270C, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x5271B", 0x5271B, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x52725", 0x52725, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x52739", 0x52739, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x5273E", 0x5273E, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x52743", 0x52743, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x52748", 0x52748, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x52757", 0x52757, Profession::None, ItemType::Offhand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766

        // Discovered Scythe (55)
        {"0x38457", 0x38457, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xA16264A1
        {"0x38647", 0x38647, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xA16264A1
        {"0x38692", 0x38692, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38697", 0x38697, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x3869C", 0x3869C, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x386A1", 0x386A1, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x386A6", 0x386A6, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x386AB", 0x386AB, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xB4593E33
        {"0x386B5", 0x386B5, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x386BA", 0x386BA, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x386BF", 0x386BF, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x386C2", 0x386C2, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x386C7", 0x386C7, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x386CC", 0x386CC, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38789", 0x38789, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xA16264A1
        {"0x40E62", 0x40E62, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x40E6C", 0x40E6C, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x40E6E", 0x40E6E, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x43B71", 0x43B71, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x43B8D", 0x43B8D, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x44CA8", 0x44CA8, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x458E3", 0x458E3, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x458E9", 0x458E9, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xA16264A1
        {"0x458EB", 0x458EB, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x45902", 0x45902, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x4590B", 0x4590B, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x4590C", 0x4590C, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x4590E", 0x4590E, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x45911", 0x45911, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x45982", 0x45982, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x4598B", 0x4598B, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x45998", 0x45998, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x45999", 0x45999, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49AE5", 0x49AE5, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xB4593E33
        {"0x49AED", 0x49AED, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xB4593E33
        {"0x49AF7", 0x49AF7, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xB4593E33
        {"0x49AFF", 0x49AFF, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xB4593E33
        {"0x49C64", 0x49C64, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xB4593E33
        {"0x55653", 0x55653, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x5644B", 0x5644B, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x564F3", 0x564F3, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x564F4", 0x564F4, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x5AB63", 0x5AB63, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x5AB6C", 0x5AB6C, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x5AB6F", 0x5AB6F, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x5AB72", 0x5AB72, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x5AB79", 0x5AB79, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x5AB7F", 0x5AB7F, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x5BEE1", 0x5BEE1, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x5BEE2", 0x5BEE2, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x5C93B", 0x5C93B, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x5C947", 0x5C947, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x5C94A", 0x5C94A, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x5C94D", 0x5C94D, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xB4593E33
        {"0x5C956", 0x5C956, Profession::None, ItemType::Scythe, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766

        // Discovered Shield (65)
        {"0x24D2", 0x24D2, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x381E", 0x381E, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x3820", 0x3820, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x3824", 0x3824, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x3828", 0x3828, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x9B6B", 0x9B6B, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x9F1A", 0x9F1A, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x9F1F", 0x9F1F, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0xA047", 0xA047, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x22EE2", 0x22EE2, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x22EE7", 0x22EE7, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x22EEC", 0x22EEC, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x25490", 0x25490, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x25981", 0x25981, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x2695D", 0x2695D, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x2AE9A", 0x2AE9A, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x2AFCE", 0x2AFCE, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x2AFD8", 0x2AFD8, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x2AFDD", 0x2AFDD, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x2B21D", 0x2B21D, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x2B222", 0x2B222, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x2B22C", 0x2B22C, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x2FB2F", 0x2FB2F, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x3248D", 0x3248D, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x32492", 0x32492, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x32495", 0x32495, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x386CF", 0x386CF, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x386D4", 0x386D4, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x386D9", 0x386D9, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x386DE", 0x386DE, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x386E3", 0x386E3, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x386E8", 0x386E8, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x386ED", 0x386ED, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x386F7", 0x386F7, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x386FC", 0x386FC, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x38701", 0x38701, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x38706", 0x38706, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x38710", 0x38710, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x38715", 0x38715, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x3871A", 0x3871A, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x3871F", 0x3871F, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x38724", 0x38724, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x3872F", 0x3872F, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x38734", 0x38734, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x38739", 0x38739, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x458F6", 0x458F6, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x45915", 0x45915, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x45916", 0x45916, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x45950", 0x45950, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x459AA", 0x459AA, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x49B0D", 0x49B0D, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x49B12", 0x49B12, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x49B17", 0x49B17, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x49B1E", 0x49B1E, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x49B2B", 0x49B2B, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x49C69", 0x49C69, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x49CC5", 0x49CC5, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x50955", 0x50955, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x5264F", 0x5264F, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x52697", 0x52697, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x52711", 0x52711, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x57CA9", 0x57CA9, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x5A2CC", 0x5A2CC, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x5AE0A", 0x5AE0A, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF
        {"0x5C950", 0x5C950, Profession::None, ItemType::Shield, Campaign::BonusMissionPack, 3, 0x20000000}, // rig-pure rig=0x781953BF

        // Discovered Spear (28)
        {"0x32448", 0x32448, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x32488", 0x32488, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x3873E", 0x3873E, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x38743", 0x38743, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x38748", 0x38748, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x3874D", 0x3874D, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x38752", 0x38752, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x38758", 0x38758, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x3875D", 0x3875D, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x38767", 0x38767, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x3876C", 0x3876C, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x3876F", 0x3876F, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x38774", 0x38774, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x458FF", 0x458FF, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x459B0", 0x459B0, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49B37", 0x49B37, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49B3C", 0x49B3C, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49B41", 0x49B41, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49B46", 0x49B46, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49B4B", 0x49B4B, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49B55", 0x49B55, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49B5A", 0x49B5A, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49C99", 0x49C99, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49C9F", 0x49C9F, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49CAE", 0x49CAE, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49CD4", 0x49CD4, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49CDD", 0x49CDD, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49CF3", 0x49CF3, Profession::None, ItemType::Spear, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860

        // Discovered Staff (124)
        {"0x1B5CE", 0x1B5CE, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x63BBDC9F
        {"0x1E236", 0x1E236, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x63BBDC9F
        {"0x212E8", 0x212E8, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x212EC", 0x212EC, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x21368", 0x21368, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x213AD", 0x213AD, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x213BF", 0x213BF, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x213E4", 0x213E4, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x213F0", 0x213F0, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x213F8", 0x213F8, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x21417", 0x21417, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2141D", 0x2141D, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x63BBDC9F
        {"0x21459", 0x21459, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x21475", 0x21475, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x22EEF", 0x22EEF, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x22EF2", 0x22EF2, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x262E6", 0x262E6, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x262E9", 0x262E9, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2694B", 0x2694B, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2694E", 0x2694E, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x26954", 0x26954, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x283AD", 0x283AD, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x283AE", 0x283AE, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x283AF", 0x283AF, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x283B2", 0x283B2, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x283C1", 0x283C1, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x283C5", 0x283C5, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x283C7", 0x283C7, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x283F8", 0x283F8, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x28427", 0x28427, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x28429", 0x28429, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2A489", 0x2A489, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2A490", 0x2A490, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2A497", 0x2A497, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2A49E", 0x2A49E, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2A4A5", 0x2A4A5, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2A4AC", 0x2A4AC, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2A4B3", 0x2A4B3, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2ADAF", 0x2ADAF, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2ADB4", 0x2ADB4, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2ADC0", 0x2ADC0, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2ADE6", 0x2ADE6, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AE02", 0x2AE02, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AE07", 0x2AE07, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AE34", 0x2AE34, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AE35", 0x2AE35, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AE6D", 0x2AE6D, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AE72", 0x2AE72, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AE77", 0x2AE77, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B19A", 0x2B19A, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x63BBDC9F
        {"0x2B20D", 0x2B20D, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x63BBDC9F
        {"0x2B23B", 0x2B23B, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B240", 0x2B240, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B24A", 0x2B24A, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B251", 0x2B251, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B25B", 0x2B25B, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B260", 0x2B260, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B265", 0x2B265, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B26A", 0x2B26A, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B276", 0x2B276, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B280", 0x2B280, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B285", 0x2B285, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B28F", 0x2B28F, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B294", 0x2B294, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B299", 0x2B299, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B29E", 0x2B29E, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B29F", 0x2B29F, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B2BA", 0x2B2BA, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2EBD6", 0x2EBD6, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2F301", 0x2F301, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2F30B", 0x2F30B, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2F30F", 0x2F30F, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x30708", 0x30708, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38411", 0x38411, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x63BBDC9F
        {"0x38779", 0x38779, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x3877E", 0x3877E, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38783", 0x38783, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38788", 0x38788, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x3878E", 0x3878E, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38793", 0x38793, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38798", 0x38798, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38799", 0x38799, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x3879E", 0x3879E, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x387A3", 0x387A3, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x387AD", 0x387AD, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x387B7", 0x387B7, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x387BC", 0x387BC, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x387C1", 0x387C1, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x63BBDC9F
        {"0x387C6", 0x387C6, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x387CB", 0x387CB, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x387D0", 0x387D0, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x387D5", 0x387D5, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49B5F", 0x49B5F, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49B64", 0x49B64, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49B69", 0x49B69, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49B6E", 0x49B6E, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49B71", 0x49B71, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49B76", 0x49B76, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49B7B", 0x49B7B, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49B80", 0x49B80, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49B8A", 0x49B8A, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49B8F", 0x49B8F, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49B99", 0x49B99, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49BA0", 0x49BA0, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49BA7", 0x49BA7, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49BAE", 0x49BAE, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49BB3", 0x49BB3, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49BB8", 0x49BB8, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49BBD", 0x49BBD, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49BC8", 0x49BC8, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49C45", 0x49C45, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49C5A", 0x49C5A, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49C5F", 0x49C5F, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49C77", 0x49C77, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49C89", 0x49C89, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49C8E", 0x49C8E, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49C93", 0x49C93, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49C9E", 0x49C9E, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49CA4", 0x49CA4, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x53722", 0x53722, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x53738", 0x53738, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x54C78", 0x54C78, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x54C87", 0x54C87, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x54C94", 0x54C94, Profession::None, ItemType::Staff, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766

        // Discovered Sword (146)
        {"0x24EB", 0x24EB, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x254D", 0x254D, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x383C", 0x383C, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x3841", 0x3841, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x4215", 0x4215, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x9B72", 0x9B72, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x9B75", 0x9B75, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x9B7C", 0x9B7C, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xCA65189D
        {"0x13D19", 0x13D19, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x13D1C", 0x13D1C, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x118C24A5
        {"0x21438", 0x21438, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x22EF7", 0x22EF7, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xCA65189D
        {"0x22EFC", 0x22EFC, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xCA65189D
        {"0x22F01", 0x22F01, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xCA65189D
        {"0x22F04", 0x22F04, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xCA65189D
        {"0x22F09", 0x22F09, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xCA65189D
        {"0x22F0E", 0x22F0E, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xCA65189D
        {"0x2AE40", 0x2AE40, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AE52", 0x2AE52, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AE5D", 0x2AE5D, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AE63", 0x2AE63, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AE8B", 0x2AE8B, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xCA65189D
        {"0x2AF47", 0x2AF47, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xCA65189D
        {"0x2AF51", 0x2AF51, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xCA65189D
        {"0x2AF5B", 0x2AF5B, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AF6B", 0x2AF6B, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AF70", 0x2AF70, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AF73", 0x2AF73, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AF78", 0x2AF78, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AF92", 0x2AF92, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AF9A", 0x2AF9A, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AF9F", 0x2AF9F, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x2AFA4", 0x2AFA4, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AFA9", 0x2AFA9, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AFBB", 0x2AFBB, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AFC1", 0x2AFC1, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AFE6", 0x2AFE6, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AFEB", 0x2AFEB, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AFF3", 0x2AFF3, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2AFFD", 0x2AFFD, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xCA65189D
        {"0x2B007", 0x2B007, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x2B2AE", 0x2B2AE, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x387DA", 0x387DA, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x387DB", 0x387DB, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x387DC", 0x387DC, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x387E1", 0x387E1, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x387E6", 0x387E6, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x387E7", 0x387E7, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x387E8", 0x387E8, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x387EB", 0x387EB, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x387F0", 0x387F0, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x387F5", 0x387F5, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x387FD", 0x387FD, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x38802", 0x38802, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x3880C", 0x3880C, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x3880F", 0x3880F, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x38814", 0x38814, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x38817", 0x38817, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x3881A", 0x3881A, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x3881F", 0x3881F, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x38824", 0x38824, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x38827", 0x38827, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x3882C", 0x3882C, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x3882F", 0x3882F, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x38834", 0x38834, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x38839", 0x38839, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x3883E", 0x3883E, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xCA65189D
        {"0x38843", 0x38843, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x38848", 0x38848, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x3884D", 0x3884D, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x40E5A", 0x40E5A, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x40E61", 0x40E61, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x4347C", 0x4347C, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x43B90", 0x43B90, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xCA65189D
        {"0x45912", 0x45912, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x118C24A5
        {"0x499F8", 0x499F8, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xCA65189D
        {"0x49BCB", 0x49BCB, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49BCF", 0x49BCF, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49BD4", 0x49BD4, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49BD9", 0x49BD9, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49BDE", 0x49BDE, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49BE1", 0x49BE1, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49BE4", 0x49BE4, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49BE9", 0x49BE9, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49BEC", 0x49BEC, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49BEF", 0x49BEF, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49BF4", 0x49BF4, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49BF7", 0x49BF7, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49BFA", 0x49BFA, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49BFF", 0x49BFF, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49C04", 0x49C04, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49C09", 0x49C09, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49C0E", 0x49C0E, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49C11", 0x49C11, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49C14", 0x49C14, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49C1C", 0x49C1C, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49C21", 0x49C21, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49C26", 0x49C26, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49C2B", 0x49C2B, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49C35", 0x49C35, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49C3F", 0x49C3F, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49C40", 0x49C40, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49C4F", 0x49C4F, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49C55", 0x49C55, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49C76", 0x49C76, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49C7F", 0x49C7F, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49C84", 0x49C84, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x49CCA", 0x49CCA, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xCA65189D
        {"0x50974", 0x50974, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x5097E", 0x5097E, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x50981", 0x50981, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x52636", 0x52636, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x52654", 0x52654, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x5265E", 0x5265E, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x5267E", 0x5267E, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x5269C", 0x5269C, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x526A6", 0x526A6, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x526BA", 0x526BA, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x526D8", 0x526D8, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x526F8", 0x526F8, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x52716", 0x52716, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x52720", 0x52720, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x52734", 0x52734, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x52752", 0x52752, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x5275C", 0x5275C, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x53714", 0x53714, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x53749", 0x53749, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x57787", 0x57787, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x57799", 0x57799, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x57CAB", 0x57CAB, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x57CAC", 0x57CAC, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x57CAD", 0x57CAD, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x58607", 0x58607, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x59EF1", 0x59EF1, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x59EF2", 0x59EF2, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x59EF3", 0x59EF3, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x5A2CF", 0x5A2CF, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x5A2D0", 0x5A2D0, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x5A522", 0x5A522, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xCA65189D
        {"0x5AB69", 0x5AB69, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x5AB78", 0x5AB78, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x5AB7C", 0x5AB7C, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x5C941", 0x5C941, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x5C944", 0x5C944, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x5C953", 0x5C953, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860
        {"0x5C959", 0x5C959, Profession::None, ItemType::Sword, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x1AE75860

        // Discovered Wand (96)
        {"0x154F1", 0x154F1, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x63BBDC9F
        {"0x15504", 0x15504, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x63BBDC9F
        {"0x15DBB", 0x15DBB, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x63BBDC9F
        {"0x16FFC", 0x16FFC, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xA16264A1
        {"0x172DC", 0x172DC, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x63BBDC9F
        {"0x1A6AF", 0x1A6AF, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xA16264A1
        {"0x1E1FF", 0x1E1FF, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xA16264A1
        {"0x213E9", 0x213E9, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xA16264A1
        {"0x22ED8", 0x22ED8, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xA16264A1
        {"0x22EDD", 0x22EDD, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xA16264A1
        {"0x25495", 0x25495, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xA16264A1
        {"0x25821", 0x25821, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xA16264A1
        {"0x2597B", 0x2597B, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x63BBDC9F
        {"0x26951", 0x26951, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xA16264A1
        {"0x28439", 0x28439, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xA16264A1
        {"0x2AE7D", 0x2AE7D, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xA16264A1
        {"0x2AF65", 0x2AF65, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xA16264A1
        {"0x2AFB6", 0x2AFB6, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xA16264A1
        {"0x2B0A0", 0x2B0A0, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xA16264A1
        {"0x2B0A5", 0x2B0A5, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xA16264A1
        {"0x2B188", 0x2B188, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B18D", 0x2B18D, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B195", 0x2B195, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B1A4", 0x2B1A4, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B1A9", 0x2B1A9, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B1AE", 0x2B1AE, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B1C2", 0x2B1C2, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B1C7", 0x2B1C7, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B1CC", 0x2B1CC, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B1D1", 0x2B1D1, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B1D6", 0x2B1D6, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B1DB", 0x2B1DB, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B1E0", 0x2B1E0, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B1EA", 0x2B1EA, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B1F4", 0x2B1F4, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B1F9", 0x2B1F9, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B1FE", 0x2B1FE, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B212", 0x2B212, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x2B270", 0x2B270, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0xA16264A1
        {"0x3861C", 0x3861C, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38621", 0x38621, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38626", 0x38626, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x3862B", 0x3862B, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x3862C", 0x3862C, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x3862D", 0x3862D, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38637", 0x38637, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x3863C", 0x3863C, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38641", 0x38641, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38646", 0x38646, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x3864D", 0x3864D, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38652", 0x38652, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38657", 0x38657, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x3865C", 0x3865C, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38661", 0x38661, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38666", 0x38666, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x3866B", 0x3866B, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x3866C", 0x3866C, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38676", 0x38676, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x3867B", 0x3867B, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38680", 0x38680, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38681", 0x38681, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38687", 0x38687, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x38688", 0x38688, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x40D87", 0x40D87, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x40D88", 0x40D88, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x40E58", 0x40E58, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x40E5B", 0x40E5B, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x40EA3", 0x40EA3, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x63BBDC9F
        {"0x458E6", 0x458E6, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x63BBDC9F
        {"0x458EE", 0x458EE, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x63BBDC9F
        {"0x458F2", 0x458F2, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x63BBDC9F
        {"0x458F3", 0x458F3, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x63BBDC9F
        {"0x458F4", 0x458F4, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x63BBDC9F
        {"0x45906", 0x45906, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x63BBDC9F
        {"0x45907", 0x45907, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x63BBDC9F
        {"0x49AB3", 0x49AB3, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x63BBDC9F
        {"0x49AB8", 0x49AB8, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49ABD", 0x49ABD, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49AC2", 0x49AC2, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49AC7", 0x49AC7, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49ACC", 0x49ACC, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49AD1", 0x49AD1, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49AD6", 0x49AD6, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49AE0", 0x49AE0, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49AF2", 0x49AF2, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49AFA", 0x49AFA, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49CE9", 0x49CE9, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49CF0", 0x49CF0, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49EAD", 0x49EAD, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x49EB5", 0x49EB5, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x9E84D766
        {"0x50979", 0x50979, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x63BBDC9F
        {"0x50992", 0x50992, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x63BBDC9F
        {"0x57773", 0x57773, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x63BBDC9F
        {"0x5777D", 0x5777D, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x63BBDC9F
        {"0x57CA5", 0x57CA5, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x63BBDC9F
        {"0x57CA6", 0x57CA6, Profession::None, ItemType::Wand, Campaign::BonusMissionPack, 3, 0x20000000}, // guess rig=0x63BBDC9F

    };
}
