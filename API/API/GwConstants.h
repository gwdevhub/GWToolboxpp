#ifndef __GWCONSTANTS_H
#define __GWCONSTANTS_H

#pragma once

#include "GwConstantsSkills.h"
#include "GwConstantsMaps.h"

namespace GwConstants {

	enum class Difficulty { Normal, Hard };

	enum class InstanceType { Outpost, Explorable, Loading };

	enum class Profession { None, Warrior, Ranger, Monk, Necromancer, Mesmer, 
		Elementalist, Assassin, Ritualist, Paragon, Dervish };

	enum class Attribute {			
		FastCasting, IllusionMagic, DominationMagic, InspirationMagic,		// mesmer
		BloodMagic, DeathMagic, SoulReaping, Curses,						// necro
		AirMagic, EarthMagic, FireMagic, WaterMagic, EnergyStorage,			// ele
		HealingPrayers, SmitingPrayers, ProtectionPrayers, DivineFavor,		// monk
		Strength, AxeMastery, HammerMastery, Swordsmanship, Tactics,		// warrior
		BeastMastery, Expertise, WildernessSurvival, Marksmanship,			// ranger
		DaggerMastery = 29, DeadlyArts, ShadowArts,							// assassin (most)
		Communing, RestorationMagic, ChannelingMagic,						// ritualist (most)
		CriticalStrikes, SpawningPower,										// sin/rit primary (gw is weird)
		SpearMastery, Command, Motivation, Leadership,						// paragon
		ScytheMastery, WindPrayers, EarthPrayers, Mysticism,				// derv
		None = 0xff
	};

	enum class OnlineStatus{ Offline, Online, DND, Away };

	enum class Bag{	None,Backpack,Belt_Pouch,Bag_1,Bag_2,Equipment_Pack,
					Material_Storage,Unclaimed_Items,Storage_1,Storage_2,
					Storage_3,Storage_4,Storage_5,Storage_6,Storage_7,
					Storage_8,Storage_9,Equipped_Items};

	enum class AgentType{
		Living = 0xDB,Chest_Signpost = 0x200,Item = 0x400
	};

	enum class ItemType {
		Salvage,Axe=2,Bag,Boots,Bow,Chestpiece=7,Rune_Mod,Usable,Dye,
		Materials_Zcoins,Offhand,Gloves,Hammer=15,Headpiece,CC_Shards,
		Key,Leggings,Gold_Coin,Quest_Item,Wand,Shield=24,Staff=26,Sword,
		Kit=29,Trophy,Scroll,Daggers,Minipet,Scythe,Spear,Costume=45
	};

	enum class Tick { NOT_READY, READY };

	namespace Range {
		const int Adjacent = 156;
		const int Nearby = 240;
		const int Area = 312;
		const int Earshot = 1000;
		const int Spellcast = 1085;
		const int Spirit = 2500;
		const int Compass = 5000;
	};

	namespace SqrRange {
		const int Adjacent = Range::Adjacent * Range::Adjacent;
		const int Nearby = Range::Nearby * Range::Nearby;
		const int Area = Range::Area * Range::Area;
		const int Earshot = Range::Earshot * Range::Earshot;
		const int Spellcast = Range::Spellcast * Range::Spellcast;
		const int Spirit = Range::Spirit * Range::Spirit;
		const int Compass = Range::Compass * Range::Compass;
	};

	namespace ItemID { // aka item modelIDs
		// item that make agents
		const int GhostInTheBox = 6368;
		const int GhastlyStone = 32557;
		const int LegionnaireStone = 37810;

		// materials
		const int Bones = 921;
		const int Iron = 948;
		const int Granite = 955;
		const int Dust = 929;
		const int Fibers = 934;
		const int Feathers = 933;

		// pcons
		const int BRC = 31151;
		const int GRC = 31152;
		const int RRC = 31153;
		const int Pies = 28436;
		const int Cupcakes = 22269;
		const int Apples = 28431;
		const int Corns = 28432;
		const int Eggs = 22752;
		const int Kabobs = 17060;
		const int Warsupplies = 35121;
		const int LunarRabbit = 29428;
		const int LunarDragon = 29429;
		const int LunarSnake = 29430;
		const int LunarHorse = 29431;
		const int LunarSheep = 29432;
		const int ConsEssence = 24859;
		const int ConsArmor = 24860;
		const int ConsGrail = 24861;
		const int ResScrolls = 26501;
		const int SkalefinSoup = 17061;
		const int PahnaiSalad = 17062;
		const int Mobstopper = 32558;
		const int Powerstone = 24862;

		const int CremeBrulee = 15528;
		const int Fruitcake = 21492;
		const int SugaryBlueDrink = 21812;
		const int RedBeanCake = 15479;
		const int JarOfHoney = 31150;
		const int ChocolateBunny = 22644;

		// level-1 alcohol
		const int Eggnog = 6375;
		const int DwarvenAle = 5585;
		const int HuntersAle = 910;
		const int Absinthe = 6367;
		const int WitchsBrew = 6049;
		const int Ricewine = 15477;
		const int ShamrockAle = 22190;
		const int Cider = 28435;

		// level-5 alcohol
		const int Grog = 30855;
		const int SpikedEggnog = 6366;
		const int AgedDwarvenAle = 24593;
		const int AgedHungersAle = 31145;
		const int Keg = 31146;
		const int FlaskOfFirewater = 2513;
		const int KrytanBrandy = 35124;
	}

	namespace ModelID { // probably this name is wrong
		const int SkeletonOfDhuum = 2338;
		const int Boo = 7445;
	}

	namespace DialogID {
		const int UwTeleVale = 138;
		const int UwTelePlanes = 139;
		const int UwTeleWastes = 140;
		const int UwTeleLab = 141;
		const int UwTeleMnt = 142;
		const int UwTelePits = 143;
		const int UwTelePools = 144;

		const int FowCraftArmor = 127;

		const int FerryKamadanToDocks = 133; // Assistant Hahnna
		const int FerryDocksToKaineng = 136; // Mhenlo
		const int FerryDocksToLA = 137;		 // Mhenlo
		const int FerryGateToLA = 133;		 // Lionguard Neiro

		// Profession Changer Dialogs.
		const int ProfChangeWarrior = 0x184;
		const int ProfChangeRanger = 0x284;
		const int ProfChangeMonk = 0x384;
		const int ProfChangeNecro = 0x484;
		const int ProfChangeMesmer = 0x584;
		const int ProfChangeEle = 0x684;
		const int ProfChangeAssassin = 0x784;
		const int ProfChangeRitualist = 0x884;
		const int ProfChangeParagon = 0x984;
		const int ProfChangeDervish = 0xA84;
	}

	namespace QuestID {
		namespace UW {
			const int Chamber = 101;
			const int Wastes = 102;
			const int UWG = 103;
			const int Mnt = 104;
			const int Pits = 105;
			const int Planes = 106;
			const int Pools = 107;
			const int Escort = 108;
			const int Restore = 109;
			const int Vale = 110;
		}

		namespace Fow {
			const int Defend = 202;
			const int ArmyOfDarknesses = 203;
			const int WailingLord = 204;
			const int Griffons = 205;
			const int Slaves = 206;
			const int Restore = 207;
			const int Hunt = 208;
			const int Forgemaster = 209;
			const int Tos = 211;
			const int Toc = 212;
			const int Khobay = 224;
		}

		namespace Doa {
			// gloom
			const int DeathbringerCompany = 749;
			const int RiftBetweenUs = 752;
			const int ToTheRescue = 753;

			// city
			const int City = 751;

			// Veil
			const int BreachingStygianVeil = 742;
			const int BroodWars = 755;

			// Foundry
			const int FoundryBreakout = 743;
			const int FoundryOfFailedCreations = 744;
		}
	}
}




#endif