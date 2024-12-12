#pragma once

namespace GW {
    namespace Constants {
        namespace ItemID { // aka item modelIDs
            constexpr int Dye = 146;

            // items that make agents
            constexpr int GhostInTheBox = 6368;
            constexpr int GhastlyStone = 32557;
            constexpr int LegionnaireStone = 37810;

            // materials
            constexpr int Bone = 921;
            constexpr int BoltofCloth = 925;
            constexpr int PileofGlitteringDust = 929;
            constexpr int Feather = 933;
            constexpr int PlantFiber = 934;
            constexpr int TannedHideSquare = 940;
            constexpr int WoodPlank = 946;
            constexpr int IronIngot = 948;
            constexpr int Scale = 953;
            constexpr int ChitinFragment = 954;
            constexpr int GraniteSlab = 955;

            constexpr int AmberChunk = 6532;
            constexpr int BoltofDamask = 927;
            constexpr int BoltofLinen = 926;
            constexpr int BoltofSilk = 928;
            constexpr int DeldrimorSteelIngot = 950;
            constexpr int Diamond = 935;
            constexpr int ElonianLeatherSquare = 943;
            constexpr int FurSquare = 941;
            constexpr int GlobofEctoplasm = 930;
            constexpr int JadeiteShard = 6533;
            constexpr int LeatherSquare = 942;
            constexpr int LumpofCharcoal = 922;
            constexpr int MonstrousClaw = 923;
            constexpr int MonstrousEye = 931;
            constexpr int MonstrousFang = 932;
            constexpr int ObsidianShard = 945;
            constexpr int OnyxGemstone = 936;
            constexpr int RollofParchment = 951;
            constexpr int RollofVellum = 952;
            constexpr int Ruby = 937;
            constexpr int Sapphire = 938;
            constexpr int SpiritwoodPlank = 956;
            constexpr int SteelIngot = 949;
            constexpr int TemperedGlassVial = 939;
            constexpr int VialofInk = 944;

            // XP scrolls
            constexpr int ScrollOfAdventurersInsight = 5853;
            constexpr int ScrollOfBerserkersInsight = 5595;
            constexpr int ScrollOfHerosInsight = 5594;
            constexpr int ScrollOfHuntersInsight = 5976;
            constexpr int ScrollOfRampagersInsight = 5975;
            constexpr int ScrollOfSlayersInsight = 5611;
            constexpr int ScrollOfTheLightbringer = 21233;

            // pcons
            constexpr int SkalefinSoup = 17061;
            constexpr int PahnaiSalad = 17062;
            constexpr int MandragorRootCake = 19170;
            constexpr int Pies = 28436;
            constexpr int Cupcakes = 22269;
            constexpr int Apples = 28431;
            constexpr int Corns = 28432;
            constexpr int Eggs = 22752;
            constexpr int Kabobs = 17060;
            constexpr int Warsupplies = 35121;
            constexpr int LunarPig = 29424;
            constexpr int LunarRat = 29425;
            constexpr int LunarOx = 29426;
            constexpr int LunarTiger = 29427;
            constexpr int LunarRabbit = 29428;
            constexpr int LunarDragon = 29429;
            constexpr int LunarSnake = 29430;
            constexpr int LunarHorse = 29431;
            constexpr int LunarSheep = 29432;
            constexpr int LunarMonkey = 29433;
            constexpr int LunarRooster = 29434;
            constexpr int LunarDog = 29435;
            constexpr int ConsEssence = 24859;
            constexpr int ConsArmor = 24860;
            constexpr int ConsGrail = 24861;
            constexpr int ResScrolls = 26501;
            constexpr int Mobstopper = 32558;
            constexpr int Powerstone = 24862;

            constexpr int Fruitcake = 21492;
            constexpr int RedBeanCake = 15479;
            constexpr int CremeBrulee = 15528;
            constexpr int SugaryBlueDrink = 21812;
            constexpr int ChocolateBunny = 22644;
            constexpr int JarOfHoney = 31150;
            constexpr int BRC = 31151;
            constexpr int GRC = 31152;
            constexpr int RRC = 31153;
            constexpr int KrytalLokum = 35125;
            constexpr int MinistreatOfPurity = 30208;

            // level-1 alcohol
            constexpr int Eggnog = 6375;
            constexpr int DwarvenAle = 5585;
            constexpr int HuntersAle = 910;
            constexpr int Absinthe = 6367;
            constexpr int WitchsBrew = 6049;
            constexpr int Ricewine = 15477;
            constexpr int ShamrockAle = 22190;
            constexpr int Cider = 28435;

            // level-5 alcohol
            constexpr int Grog = 30855;
            constexpr int SpikedEggnog = 6366;
            constexpr int AgedDwarvenAle = 24593;
            constexpr int AgedHuntersAle = 31145;
            constexpr int Keg = 31146;
            constexpr int FlaskOfFirewater = 2513;
            constexpr int KrytanBrandy = 35124;

            // DoA Gemstones
            constexpr int MargoniteGem = 21128;
            constexpr int StygianGem = 21129;
            constexpr int TitanGem = 21130;
            constexpr int TormentGem = 21131;

            // Holiday Drops
            constexpr int LunarToken = 21833;

            constexpr int Lockpick = 22751;

            constexpr int IdentificationKit = 2989;
            constexpr int IdentificationKit_Superior = 5899;
            constexpr int SalvageKit = 2992;
            constexpr int SalvageKit_Expert = 2991;
            constexpr int SalvageKit_Superior = 5900;

            // Alcohol
            constexpr int BattleIsleIcedTea = 36682;
            constexpr int BottleOfJuniberryGin = 19172;
            constexpr int BottleOfVabbianWine = 19173;
            constexpr int ZehtukasJug = 19171;

            // DP
            constexpr int FourLeafClover = 22191; // party-wide
            constexpr int OathOfPurity = 30206;   // party-wide
            constexpr int PeppermintCandyCane = 6370;
            constexpr int RefinedJelly = 19039;
            constexpr int ShiningBladeRations = 35127;
            constexpr int WintergreenCandyCane = 21488;

            // Morale
            constexpr int ElixirOfValor = 21227; // party-wide
            constexpr int Honeycomb = 26784;     // party-wide
            constexpr int PumpkinCookie = 28433;
            constexpr int RainbowCandyCane = 21489;      // party-wide
            constexpr int SealOfTheDragonEmpire = 30211; // party-wide

            // Summons
            constexpr int GakiSummon = 30960;
            constexpr int TurtleSummon = 30966;

            // Summons x3
            constexpr int TenguSummon = 30209;
            constexpr int ImperialGuardSummon = 30210;
            constexpr int WarhornSummon = 35126;

            // Tonics
            constexpr int ELGwen = 36442;
            constexpr int ELMiku = 36451;
            constexpr int ELMargo = 36456;
            constexpr int ELZenmai = 36493;

            // Other Consumables
            constexpr int ArmbraceOfTruth = 21127;
            constexpr int PhantomKey = 5882;
            constexpr int ResScroll = 26501;

            // Weapons
            constexpr int DSR = 32823;
            constexpr int EternalBlade = 1045;
            constexpr int ObsidianEdge = 1900;
            constexpr int VoltaicSpear = 2071;
            constexpr int CrystallineSword = 399;

            // Minis
            constexpr int MiniDhuum = 32822;

            // Bundles
            constexpr int UnholyText = 2619;
            constexpr int DungeonKey = 25410;
            constexpr int BossKey = 25416;
            constexpr int EnchantedTorch = 503;
            constexpr int LightOfSeborhin = 15531;
            constexpr int IstaniFireOil = 16339;

            // Money
            constexpr int GoldCoin = 2510;
            constexpr int GoldCoins = 2511;
        }
    }
}
