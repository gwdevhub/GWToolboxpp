// ReSharper disable CppUnusedIncludeDirective
#pragma once

#include "AgentIDs.h"
#include "ItemIDs.h"
#include "Maps.h"
#include "QuestIDs.h"
#include "Skills.h"

namespace GW {
    namespace Constants {

        enum class Campaign : uint32_t { Core, Prophecies, Factions, Nightfall, EyeOfTheNorth, BonusMissionPack };
        enum class Difficulty { Normal, Hard };
        enum class InstanceType { Outpost, Explorable, Loading };

        enum class Profession : uint32_t {
            None, Warrior, Ranger, Monk, Necromancer, Mesmer,
            Elementalist, Assassin, Ritualist, Paragon, Dervish
        };
        enum class ProfessionByte : uint8_t {
            None, Warrior, Ranger, Monk, Necromancer, Mesmer,
            Elementalist, Assassin, Ritualist, Paragon, Dervish
        };
        static const char* GetProfessionAcronym(Profession prof) {
            switch (prof) {
            case GW::Constants::Profession::None: return "X";
            case GW::Constants::Profession::Warrior: return "W";
            case GW::Constants::Profession::Ranger: return "R";
            case GW::Constants::Profession::Monk: return "Mo";
            case GW::Constants::Profession::Necromancer: return "N";
            case GW::Constants::Profession::Mesmer: return "Me";
            case GW::Constants::Profession::Elementalist: return "E";
            case GW::Constants::Profession::Assassin: return "A";
            case GW::Constants::Profession::Ritualist: return "Rt";
            case GW::Constants::Profession::Paragon: return "P";
            case GW::Constants::Profession::Dervish: return "D";
            default: return "";
            }
        }
        static const wchar_t* GetWProfessionAcronym(Profession prof) {
            switch (prof) {
            case GW::Constants::Profession::None: return L"X";
            case GW::Constants::Profession::Warrior: return L"W";
            case GW::Constants::Profession::Ranger: return L"R";
            case GW::Constants::Profession::Monk: return L"Mo";
            case GW::Constants::Profession::Necromancer: return L"N";
            case GW::Constants::Profession::Mesmer: return L"Me";
            case GW::Constants::Profession::Elementalist: return L"E";
            case GW::Constants::Profession::Assassin: return L"A";
            case GW::Constants::Profession::Ritualist: return L"Rt";
            case GW::Constants::Profession::Paragon: return L"P";
            case GW::Constants::Profession::Dervish: return L"D";
            default: return L"";
            }
        }

        namespace Preference {
            enum class CharSortOrder : uint32_t {
                None, Alphabetize, PvPRP
            };
        }

        enum class Attribute : uint32_t {
            FastCasting, IllusionMagic, DominationMagic, InspirationMagic,      // mesmer
            BloodMagic, DeathMagic, SoulReaping, Curses,                        // necro
            AirMagic, EarthMagic, FireMagic, WaterMagic, EnergyStorage,         // ele
            HealingPrayers, SmitingPrayers, ProtectionPrayers, DivineFavor,     // monk
            Strength, AxeMastery, HammerMastery, Swordsmanship, Tactics,        // warrior
            BeastMastery, Expertise, WildernessSurvival, Marksmanship,          // ranger
            DaggerMastery = 29, DeadlyArts, ShadowArts,                         // assassin (most)
            Communing, RestorationMagic, ChannelingMagic,                       // ritualist (most)
            CriticalStrikes, SpawningPower,                                     // sin/rit primary (gw is weird)
            SpearMastery, Command, Motivation, Leadership,                      // paragon
            ScytheMastery, WindPrayers, EarthPrayers, Mysticism,                // derv
            None = 0xff
        };
        enum class AttributeByte : uint8_t {
            FastCasting, IllusionMagic, DominationMagic, InspirationMagic,      // mesmer
            BloodMagic, DeathMagic, SoulReaping, Curses,                        // necro
            AirMagic, EarthMagic, FireMagic, WaterMagic, EnergyStorage,         // ele
            HealingPrayers, SmitingPrayers, ProtectionPrayers, DivineFavor,     // monk
            Strength, AxeMastery, HammerMastery, Swordsmanship, Tactics,        // warrior
            BeastMastery, Expertise, WildernessSurvival, Marksmanship,          // ranger
            DaggerMastery = 29, DeadlyArts, ShadowArts,                         // assassin (most)
            Communing, RestorationMagic, ChannelingMagic,                       // ritualist (most)
            CriticalStrikes, SpawningPower,                                     // sin/rit primary (gw is weird)
            SpearMastery, Command, Motivation, Leadership,                      // paragon
            ScytheMastery, WindPrayers, EarthPrayers, Mysticism,                // derv
            None = 0xff
        };

        enum class Bag : uint8_t {
            None, Backpack, Belt_Pouch, Bag_1, Bag_2, Equipment_Pack,
            Material_Storage, Unclaimed_Items, Storage_1, Storage_2,
            Storage_3, Storage_4, Storage_5, Storage_6, Storage_7,
            Storage_8, Storage_9, Storage_10, Storage_11, Storage_12,
            Storage_13, Storage_14, Equipped_Items, Max
        };
        inline Bag& operator++(Bag& bag) {
            if (bag == Bag::Max) return bag;
            bag = static_cast<Bag>(static_cast<uint8_t>(bag) + 1);
            return bag;
        }
        inline Bag operator++(Bag& bag, int) {
            const Bag cpy = bag;
            ++bag;
            return cpy;
        }

        // Order of storage panes.
        enum class StoragePane : uint8_t {
            Storage_1,Storage_2,Storage_3,Storage_4,Storage_5,
            Storage_6,Storage_7,Storage_8,Storage_9,Storage_10,
            Storage_11,Storage_12,Storage_13,Storage_14,Material_Storage
        };

        constexpr size_t BagMax = (size_t)Bag::Max;

        enum class AgentType {
            Living = 0xDB, Gadget = 0x200, Item = 0x400
        };

        enum class Allegiance : uint8_t {
            Ally_NonAttackable = 0x1, Neutral = 0x2, Enemy = 0x3, Spirit_Pet = 0x4, Minion = 0x5, Npc_Minipet = 0x6
        };

        enum class ItemType : uint8_t {
            Salvage, Axe = 2, Bag, Boots, Bow, Bundle, Chestpiece, Rune_Mod, Usable, Dye,
            Materials_Zcoins, Offhand, Gloves, Hammer = 15, Headpiece, CC_Shards,
            Key, Leggings, Gold_Coin, Quest_Item, Wand, Shield = 24, Staff = 26, Sword,
            Kit = 29, Trophy, Scroll, Daggers, Present, Minipet, Scythe, Spear, Storybook = 43, Costume, Costume_Headpiece, Unknown = 0xff
        };

        enum HeroID : uint32_t {
            NoHero, Norgu, Goren, Tahlkora, MasterOfWhispers, AcolyteJin, Koss,
            Dunkoro, AcolyteSousuke, Melonni, ZhedShadowhoof, GeneralMorgahn,
            MargridTheSly, Zenmai, Olias, Razah, MOX, KeiranThackeray, Jora,
            PyreFierceshot, Anton, Livia, Hayda, Kahmu, Gwen, Xandra, Vekk,
            Ogden, Merc1, Merc2, Merc3, Merc4, Merc5, Merc6, Merc7, Merc8,
            Miku, ZeiRi, AllHeroes
        };

        enum class MaterialSlot : uint32_t {
            Bone, IronIngot, TannedHideSquare, Scale, ChitinFragment,
            BoltofCloth, WoodPlank, GraniteSlab = 8,
            PileofGlitteringDust, PlantFiber, Feather,

            FurSquare, BoltofLinen, BoltofDamask, BoltofSilk,
            GlobofEctoplasm, SteelIngot, DeldrimorSteelIngot,
            MonstrousClaw, MonstrousEye, MonstrousFang, Ruby,
            Sapphire, Diamond, OnyxGemstone, LumpofCharcoal,
            ObsidianShard, TemperedGlassVial = 29, LeatherSquare,
            ElonianLeatherSquare, VialofInk, RollofParchment,
            RollofVellum, SpiritwoodPlank, AmberChunk, JadeiteShard,

            BronzeZCoin, SilverZCoin, GoldZCoin,

            Count
        };

        constexpr std::array HeroProfs = {
            Profession::None,
            Profession::Mesmer, // Norgu
            Profession::Warrior,// Goren
            Profession::Monk, // Tahlkora
            Profession::Necromancer, // Master Of Whispers
            Profession::Ranger, // Acolyte Jin
            Profession::Warrior, // Koss
            Profession::Monk, // Dunkoro
            Profession::Elementalist, // Acolyte Sousuke
            Profession::Dervish, // Melonni
            Profession::Elementalist, // Zhed Shadowhoof
            Profession::Paragon, // General Morgahn
            Profession::Ranger, // Magrid The Sly
            Profession::Assassin, // Zenmai
            Profession::Necromancer, // Olias
            Profession::None, // Razah
            Profession::Dervish, // MOX
            Profession::Paragon, // Keiran Thackeray
            Profession::Warrior, // Jora
            Profession::Ranger, // Pyre Fierceshot
            Profession::Assassin, // Anton
            Profession::Necromancer, // Livia
            Profession::Paragon, // Hayda
            Profession::Dervish, // Kahmu
            Profession::Mesmer, // Gwen
            Profession::Ritualist, // Xandra
            Profession::Elementalist, // Vekk
            Profession::Monk, // Ogden
            Profession::None, // Mercenary Hero 1
            Profession::None, // Mercenary Hero 2
            Profession::None, // Mercenary Hero 3
            Profession::None, // Mercenary Hero 4
            Profession::None, // Mercenary Hero 5
            Profession::None, // Mercenary Hero 6
            Profession::None, // Mercenary Hero 7
            Profession::None, // Mercenary Hero 8
            Profession::Assassin, // Miku
            Profession::Ritualist, // Zei Ri
        };

        enum class TitleID : uint32_t {
            Hero, TyrianCarto, CanthanCarto, Gladiator, Champion, Kurzick,
            Luxon, Drunkard,
            Deprecated_SkillHunter, // Pre hard mode update version
            Survivor, KoaBD,
            Deprecated_TreasureHunter, // Old title, non-account bound
            Deprecated_Wisdom, // Old title, non-account bound
            ProtectorTyria,
            ProtectorCantha, Lucky, Unlucky, Sunspear, ElonianCarto,
            ProtectorElona, Lightbringer, LDoA, Commander, Gamer,
            SkillHunterTyria, VanquisherTyria, SkillHunterCantha,
            VanquisherCantha, SkillHunterElona, VanquisherElona,
            LegendaryCarto, LegendaryGuardian, LegendarySkillHunter,
            LegendaryVanquisher, Sweets, GuardianTyria, GuardianCantha,
            GuardianElona, Asuran, Deldrimor, Vanguard, Norn, MasterOfTheNorth,
            Party, Zaishen, TreasureHunter, Wisdom, Codex,
            None = 0xff
        };

        enum class Tick { NOT_READY, READY };

        enum class InterfaceSize { SMALL = -1, NORMAL, LARGE, LARGER };
        namespace HealthbarHeight {
            constexpr size_t Small = 24;
            constexpr size_t Normal = 22;
            constexpr size_t Large = 26;
            constexpr size_t Larger = 30;
        }

        // travel, region, districts
        enum class ServerRegion {
            International = -2,
            America = 0,
            Korea,
            Europe,
            China,
            Japan,
            Unknown = 0xff
        };

        // in-game language for maps and in-game text
        enum class Language {
            English,
            Korean,
            French,
            German,
            Italian,
            Spanish,
            TraditionalChinese,
            Japanese = 8,
            Polish,
            Russian,
            BorkBorkBork = 17,
            Unknown = 0xff
        };

        enum class District { // arbitrary enum for game district
            Current,
            International,
            American,
            EuropeEnglish,
            EuropeFrench,
            EuropeGerman,
            EuropeItalian,
            EuropeSpanish,
            EuropePolish,
            EuropeRussian,
            AsiaKorean,
            AsiaChinese,
            AsiaJapanese,
            Unknown = 0xff
        };

        namespace Range {
            constexpr float Touch = 144.f;
            constexpr float Adjacent = 166.0f;
            constexpr float Nearby = 252.0f;
            constexpr float Area = 322.0f;
            constexpr float Earshot = 1012.0f;
            constexpr float Spellcast = 1248.0f;
            constexpr float Spirit = 2500.0f;
            constexpr float Compass = 5000.0f;
        };

        namespace SqrRange {
            constexpr float Adjacent = Range::Adjacent * Range::Adjacent;
            constexpr float Nearby = Range::Nearby * Range::Nearby;
            constexpr float Area = Range::Area * Range::Area;
            constexpr float Earshot = Range::Earshot * Range::Earshot;
            constexpr float Spellcast = Range::Spellcast * Range::Spellcast;
            constexpr float Spirit = Range::Spirit * Range::Spirit;
            constexpr float Compass = Range::Compass * Range::Compass;
        };

        namespace DialogID {
            constexpr int UwTeleEnquire = 127;      // "where can you teleport us to"
            constexpr int UwTelePlanes = 139;
            constexpr int UwTeleWastes = 140;
            constexpr int UwTeleLab = 141;
            constexpr int UwTeleMnt = 142;
            constexpr int UwTelePits = 143;
            constexpr int UwTelePools = 144;
            constexpr int UwTeleVale = 145;

            constexpr int FowCraftArmor = 127;

            constexpr int FerryKamadanToDocks = 133; // Assistant Hahnna
            constexpr int FerryDocksToKaineng = 136; // Mhenlo
            constexpr int FerryDocksToLA = 137;      // Mhenlo
            constexpr int FerryGateToLA = 133;       // Lionguard Neiro

            // Profession Changer Dialogs.
            constexpr int ProfChangeWarrior = 0x184;
            constexpr int ProfChangeRanger = 0x284;
            constexpr int ProfChangeMonk = 0x384;
            constexpr int ProfChangeNecro = 0x484;
            constexpr int ProfChangeMesmer = 0x584;
            constexpr int ProfChangeEle = 0x684;
            constexpr int ProfChangeAssassin = 0x784;
            constexpr int ProfChangeRitualist = 0x884;
            constexpr int ProfChangeParagon = 0x984;
            constexpr int ProfChangeDervish = 0xA84;

            constexpr int FactionMissionOutpost = 0x80000B;
            constexpr int NightfallMissionOutpost = 0x85;
        }

        enum class EffectID : uint32_t {
            black_cloud = 1,
            mesmer_symbol = 4,
            green_cloud = 7,
            green_sparks = 8,
            necro_symbol = 9,
            ele_symbol = 11,
            white_clouds = 13,
            monk_symbol = 18,
            bleeding = 23,
            blind = 24,
            burning = 25,
            disease = 26,
            poison = 27,
            dazed = 28,
            weakness = 29, //cracked_armor has same EffectID
            assasin_symbol = 34,
            ritualist_symbol = 35,
            dervish_symbol = 36,
        };

        namespace Camera {
            constexpr float FIRST_PERSON_DIST = 2.f;
            constexpr float DEFAULT_DIST = 750.f;
        }

        constexpr size_t SkillMax = 0xd69;
    }
}
