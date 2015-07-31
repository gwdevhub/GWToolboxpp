#ifndef __GWCONSTANTS_H
#define __GWCONSTANTS_H


namespace GwConstants {

	enum class Difficulty { Normal, Hard };

	enum class InstanceType { Outpost, Explorable, Loading };

	enum class Range {
		Adjacent = 156,
		Nearby = 240,
		Area = 312,
		Earshot = 1000,
		Spellcast = 1085,
		Spirit = 2500,
		Compass = 5000
	};

	enum class Profession { None, Warrior, Ranger, Monk, Necromancer, Mesmer, Elementalist, Assassin, Ritualist, Paragon, Dervish };

	enum class Attribute {													// probably not used but why not
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
	

	namespace SkillID {
		const int Recall = 925;
		const int UA = 268;
		const int EtherRenewal = 181;
		const int BurningSpeed = 823;
		const int SpiritBond = 1114;
		const int BalthazarSpirit = 242;
		const int LifeBond = 241;
		const int ProtectiveBond = 263;
		// Feel free to add more as needed
	}

	namespace MapID {
		const int ToA = 138;
		const int Kamadan = 449;
		const int DoA = 474;
		const int Embark = 857;
		const int Vlox = 624;
		const int Urgoz = 266;
		const int Deep = 307;
		const int Eotn = 642;
		const int UW = 72;
		const int FoW = 34;
	}

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
		const int Grog = 30855;
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

		// TODO alcohol
	}

	namespace Effect {
		const int Lightbringer = 1813;
		const int Hardmode = 1912;
		const int Corn = 2604;
		const int Apple = 2605;
		const int Redrock = 2973;
		const int Bluerock = 2971;
		const int Greenrock = 2972;
		const int Warsupplies = 3174;
		const int Kabobs = 1680;
		const int Cupcake = 1945;
		const int Egg = 1934;
		const int Grog = 2923;
		const int Pie = 2649;
		const int Lunars = 1926;
		const int ConsEssence = 2522;
		const int ConsArmor = 2520;
		const int ConsGrail = 2521;
		const int SkaleVigor = 1681;
		const int PahnaiSalad = 1682;
		const int WeakenedByDhuum = 3077;
		const int CremeBrulee = 1612;
		const int BlueDrink = 1916;
		const int ChocolateBunny = 1933;
		const int RedBeanCake = 1323;
	}

	namespace ModelID { // aka
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