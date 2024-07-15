#pragma once

namespace GW {
    namespace EncStrings {

        // Text wrappers, 0x10a = inner text e.g. <c=@ItemBasic>
        static const wchar_t* ItemBasic = L"\xA3B";
        static const wchar_t* ItemCommon = L"\xA3D";
        static const wchar_t* ItemDull = L"\xA3E";
        static const wchar_t* ItemEnhance = L"\xA4F";
        static const wchar_t* ItemRare = L"\xA40";
        static const wchar_t* ItemUncommon = L"\xA42";
        static const wchar_t* ItemUnique = L"\xA43";

        static const wchar_t* MaterialTrader = L"\xCDA";
        static const wchar_t* RareMaterialTrader = L"\xCDB";

        static const wchar_t* NicholasTheTraveller = L"\x8102\x5362\xB438\x9C87\x4547";
        static const wchar_t* ZaishenBounty = L"\x8102\x545B\xD046\xBF3F\x615f";
        static const wchar_t* ZaishenVanquish = L"\x8102\x5459\x9068\xC6DD\x679";
        static const wchar_t* ZaishenMission = L"\x8102\x5457\xDEF1\xEACD\x5EDB";
        static const wchar_t* ZaishenCombat = L"\x8102\x545D\xBAED\xD4B5\x2C72";
        static const wchar_t* WantedByTheShiningBlade = L"\x8102\x63D6\xA268\xEDE8\x5D6D";

        enum class Profession : uint32_t {
            None, Warrior, Ranger, Monk, Necromancer, Mesmer,
            Elementalist, Assassin, Ritualist, Paragon, Dervish
        };

        // Profession enc string id, keyed by profession id
        constexpr uint32_t Profession[11] = {
            0x7f8, 0x7f9, 0x7fa, 0x7fb, 0x7fc, 0x7fd, 0x7fe, 0x7ff, 0x800, 0x7b3c, 0x7b3d
        };
        constexpr uint32_t ProfessionAcronym[11] = {
            0x801, 0x802, 0x803, 0x804, 0x805, 0x806, 0x807, 0x808, 0x809, 0x7b41, 0x7b42
        };

        namespace MapRegion {
            static const wchar_t* BattleIsles = L"\xE63";

            static const wchar_t* Kryta = L"\x3d62";
            static const wchar_t* CrystalDesert = L"\x3d63";
            static const wchar_t* MaguumaJungle = L"\x3d65";
            static const wchar_t* Underworld = L"\x3d66";
            static const wchar_t* Ascalon = L"\x3D67";
            static const wchar_t* NorthernShiverpeaks = L"\x3D68";
            static const wchar_t* SouthernShiverpeaks = L"\x3D69";
            static const wchar_t* SorrowsFurnace = L"\x3D6a";
            static const wchar_t* RingOfFire = L"\x3D6b";
            static const wchar_t* FissureOfWoe = L"\x3D6c";

            static const wchar_t* EchovaldForest = L"\x617c";
            static const wchar_t* ShingJeaIsland = L"\x617d";
            static const wchar_t* TheJadeSea = L"\x617e";
            static const wchar_t* KainengCity = L"\x617f";

            static const wchar_t* Istan = L"\x6185";
            static const wchar_t* RealmOfTorment = L"\x6186";
            static const wchar_t* Kourna = L"\x8101\x887";
            static const wchar_t* Vabbi = L"\x8101\x888";
            static const wchar_t* TheDesolation = L"\x8101\x889";

            static const wchar_t* TarnishedCoast = L"\x8101\x76Fa";
            static const wchar_t* FarShiverpeaks = L"\x8101\x76Fb";
            static const wchar_t* CharrHomelands = L"\x8101\x76Fc";
            static const wchar_t* DepthsOfTyria = L"\x8101\x76FD";
        }

        static const wchar_t* Bone = L"\x22D0\xBEB5\xC462\x64B5";
        static const wchar_t* IronIngot = L"\x22EB\xC6B0\xBD46\x2DAD";
        static const wchar_t* TannedHideSquare = L"\x22E3\xD3A9\xC22C\x285A";
        static const wchar_t* Scale = L"\x22F0\x832C\xD6A3\x1382";
        static const wchar_t* ChitinFragment = L"\x22F1\x9156\x8692\x497D";
        static const wchar_t* BoltofCloth = L"\x22D4\x888E\x9089\x6EC8";
        static const wchar_t* WoodPlank = L"\x22E8\xE46D\x8FE4\x5F8B";
        static const wchar_t* GraniteSlab = L"\x22F2\xA623\xFAE8\xE5A";
        static const wchar_t* PileofGlitteringDust = L"\x22D8\xA4A4\xED0D\x4304";
        static const wchar_t* PlantFiber = L"\x22DD\xAC69\xDEA5\x73C5";
        static const wchar_t* Feather = L"\x22DC\x8209\xAD29\xBBD";
        static const wchar_t* FurSquare = L"\x22E4\xE5F4\xBBEF\x5A25";
        static const wchar_t* BoltofLinen = L"\x22D5\x8371\x8ED5\x56B4";
        static const wchar_t* BoltofDamask = L"\x22D6\xF04C\xF1E5\x5699";
        static const wchar_t* BoltofSilk = L"\x22D7\xFD2A\xC85B\x58B3";
        static const wchar_t* GlobofEctoplasm = L"\x22D9\xE7B8\xE9DD\x2322";
        static const wchar_t* SteelIngot = L"\x22EC\xF12D\x87A7\x6460";
        static const wchar_t* DeldrimorSteelIngot = L"\x22ED\xB873\x85A4\x74B";
        static const wchar_t* MonstrousClaw = L"\x22D2\xCDC6\xEFC8\x3C99";
        static const wchar_t* MonstrousEye = L"\x22DA\x9059\xD163\x2187";
        static const wchar_t* MonstrousFang = L"\x22DB\x8DCE\xC3FA\x4A26";
        static const wchar_t* Ruby = L"\x22E0\x93CC\x939C\x5286";
        static const wchar_t* Sapphire = L"\x22E1\xB785\x866C\x34F6";
        static const wchar_t* Diamond = L"\x22DE\xBB93\xABD4\x5439";
        static const wchar_t* OnyxGemstone = L"\x22DF\xD425\xC093\x1CF4";
        static const wchar_t* LumpofCharcoal = L"\x22D1\xDE2A\xED03\x2625";
        static const wchar_t* ObsidianShard = L"\x22EA\xFDA9\xDE53\x2D16";
        static const wchar_t* TemperedGlassVial = L"\x22E2\xCE9B\x8771\x7DC7";
        static const wchar_t* LeatherSquare = L"\x22E5\x9758\xC5DD\x727";
        static const wchar_t* ElonianLeatherSquare = L"\x22E6\xE8F4\xA898\x75CB";
        static const wchar_t* VialofInk = L"\x22E7\xC1DA\xF2C1\x452A";
        static const wchar_t* RollofParchment = L"\x22EE\xF65A\x86E6\x1C6C";
        static const wchar_t* RollofVellum = L"\x22EF\xC588\x861D\x5BD3";
        static const wchar_t* SpiritwoodPlank = L"\x22F3\xA11C\xC924\x5E15";
        static const wchar_t* AmberChunk = L"\x55D0\xF8B7\xB108\x6018";
        static const wchar_t* JadeiteShard = L"\x55D1\xD189\x845A\x7164";

        static const wchar_t* RedIrisFlowers = L"\x271E\xDBDF\xBBD8\x34CB";
        static const wchar_t* FeatheredAvicaraScalps = L"\x294F";
        static const wchar_t* MargoniteMasks = L"\x8101\x43E8\xBEBD\xF09A\x4455";
        static const wchar_t* QuetzalCrests = L"\x8102\x26DC\xADF9\x8D4B\x6575";
        static const wchar_t* PlagueIdols = L"\x56D3\xA490\xE607\x11B6";
        static const wchar_t* AzureRemains = L"\x294D";
        static const wchar_t* MandragorRootCake = L"\x8101\x52E9\xA18F\x8FE1\x3EE9";
        static const wchar_t* MahgoClaw = L"\x2960";
        static const wchar_t* MantidPincers = L"\x56EF\xD1D8\xC773\x2C26";
        static const wchar_t* SentientSeeds = L"\x8101\x43DE\xD124\xA4D9\x7D4A";
        static const wchar_t* StoneGrawlNecklaces = L"\x8102\x26EA\x8A6F\xD31C\x31DD";
        static const wchar_t* Herring = L"\x8102\x26D1";
        static const wchar_t* NagaSkins = L"\x5702\xA954\x959D\x51B8";
        static const wchar_t* GloomSeed = L"\x296A";
        static const wchar_t* CharrHide = L"\x2882";
        static const wchar_t* RubyDjinnEssence = L"\x8101\x43F3\xF5F8\xC245\x41F2";
        static const wchar_t* ThornyCarapaces = L"\x2930";
        static const wchar_t* BoneCharms = L"\x56DD\xC82C\xB7E0\x3EB9";
        static const wchar_t* ModniirManes = L"\x8102\x26E0\xA884\xE2D3\x7E01";
        static const wchar_t* SuperbCharrCarvings = L"\x8102\x26E9\x96D3\x8E81\x64D1";
        static const wchar_t* RollsofParchment = L"\x22EE\xF65A\x86E6\x1C6C";
        static const wchar_t* RoaringEtherClaws = L"\x8101\x5208\xA22C\xC074\x2373";
        static const wchar_t* BranchesofJuniBerries = L"\x8101\x5721";
        static const wchar_t* ShiverpeakManes = L"\x2945";
        static const wchar_t* FetidCarapaces = L"\x293C";
        static const wchar_t* MoonShells = L"\x6CCD\xC6FD\xA37B\x3529";
        static const wchar_t* MassiveJawbone = L"\x2921";
        static const wchar_t* ChromaticScale = L"\x8102\x26FA\x8E00\xEA86\x3A1D";
        static const wchar_t* MursaatTokens = L"\x292B";
        static const wchar_t* SentientLodestone = L"\x8101\x43FA\xA429\xC255\x23C4";
        static const wchar_t* JungleTrollTusks = L"\x2934";
        static const wchar_t* SapphireDjinnEssence = L"\x8101\x57DD\xF97D\xB5AD\x21FF";
        static const wchar_t* StoneCarving = L"\x56E6\xB928\x9FA2\x43E1";
        static const wchar_t* FeatheredCaromiScalps = L"\x2919";
        static const wchar_t* PillagedGoods = L"\x8101\x52EE\xBF76\xE319\x2B39";
        static const wchar_t* GoldCrimsonSkullCoin = L"\x56D5\x8B0F\xAB5B\x8A6";
        static const wchar_t* JadeBracelets = L"\x56D7\xDD87\x8A67\x167D";
        static const wchar_t* MinotaurHorns = L"\x2924";
        static const wchar_t* FrostedGriffonWings = L"\x294A";
        static const wchar_t* SilverBullionCoins = L"\x8101\x43E6\xBE4C\xE956\x780";
        static const wchar_t* Truffle = L"\x56DF\xFEE4\xCA2D\x27A";
        static const wchar_t* SkelkClaws = L"\x8102\x26DD\x85C5\xD98F\x5CCB";
        static const wchar_t* DessicatedHydraClaws = L"\x2923";
        static const wchar_t* FrigidHearts = L"\x294B";
        static const wchar_t* CelestialEssences = L"\x570A\x9453\x84A6\x64D4";
        static const wchar_t* PhantomResidue = L"\x2937";
        static const wchar_t* DrakeKabob = L"\x8101\x42D1\xFB15\xD39E\x5A26";
        static const wchar_t* AmberChunks = L"\x55D0\xF8B7\xB108\x6018";
        static const wchar_t* GlowingHearts = L"\x2914";
        static const wchar_t* SaurianBones = L"\x8102\x26D8\xB5B9\x9AF6\x42D6";
        static const wchar_t* BehemothHides = L"\x8101\x5207\xEBD7\xB733\x2E27";
        static const wchar_t* LuminousStone = L"\x8101\x4E35\xD63F\xCAB4\xDD1";
        static const wchar_t* IntricateGrawlNecklaces = L"\x2950";
        static const wchar_t* JadeiteShards = L"\x55D1\xD189\x845A\x7164";
        static const wchar_t* GoldDoubloon = L"\x8101\x43E5\xA891\xA83A\x426D";
        static const wchar_t* ShriveledEyes = L"\x291B";
        static const wchar_t* IcyLodestones = L"\x28EF";
        static const wchar_t* KeenOniTalon = L"\x5701\xD258\xC958\x506F";
        static const wchar_t* HardenedHumps = L"\x2910";
        static const wchar_t* PilesofElementalDust = L"\x8102\x26E7\xC330\xC111\x4058";
        static const wchar_t* NagaHides = L"\x56F2\x876E\xEACB\x730";
        static const wchar_t* SpiritwoodPlanks = L"\x22F3\xA11C\xC924\x5E15";
        static const wchar_t* StormyEye = L"\x293A";
        static const wchar_t* SkreeWings = L"\x8101\x43F0\xFF3B\x8E3E\x20B1";
        static const wchar_t* SoulStones = L"\x5706\xC61F\xF23D\x3C4";
        static const wchar_t* SpikedCrest = L"\x290F";
        static const wchar_t* DragonRoot = L"\x56E5\x922D\xCF17\x7258";
        static const wchar_t* BerserkerHorns = L"\x8102\x26E3\xB76F\xE56C\x1A2";
        static const wchar_t* BehemothJaw = L"\x292E";
        static const wchar_t* BowlofSkalefinSoup = L"\x8101\x42D2\xE08B\xB81A\x604";
        static const wchar_t* ForestMinotaurHorns = L"\x2915";
        static const wchar_t* PutridCysts = L"\x56ED\xE607\x9B27\x7299";
        static const wchar_t* JadeMandibles = L"\x2926";
        static const wchar_t* MaguumaManes = L"\x292F";
        static const wchar_t* SkullJuju = L"\x56E0\xFBEB\xA429\x7B5";
        static const wchar_t* MandragorSwamproots = L"\x8101\x5840\xB4F5\xB2A7\x5E0F";
        static const wchar_t* BottleofVabbianWine = L"\x8101\x52EA";
        static const wchar_t* WeaverLegs = L"\x8102\x26DA\x950E\x82F1\xA3D";
        static const wchar_t* TopazCrest = L"\x291F";
        static const wchar_t* RotWallowTusks = L"\x56FC\xD503\x9D77\x730C";
        static const wchar_t* FrostfireFangs = L"\x2946";
        static const wchar_t* DemonicRelic = L"\x8101\x43E7\xD854\xC981\x54DD";
        static const wchar_t* AbnormalSeeds = L"\x2917";
        static const wchar_t* DiamondDjinnEssence = L"\x8101\x43EA\xE72E\xAA23\x3C54";
        static const wchar_t* ForgottenSeals = L"\x2928";
        static const wchar_t* CopperCrimsonSkullCoins = L"\x56D4\x8663\xA244\x50F5";
        static const wchar_t* MossyMandibles = L"\x2932";
        static const wchar_t* EnslavementStones = L"\x2954";
        static const wchar_t* ElonianLeatherSquares = L"\x22E6\xE8F4\xA898\x75CB";
        static const wchar_t* CobaltTalons = L"\x8101\x43EC\x8335\xBAA8\x153C";
        static const wchar_t* MaguumaSpiderWeb = L"\x288C";
        static const wchar_t* ForgottenTrinketBoxes = L"\x56EB\xB8B7\xF734\x2985";
        static const wchar_t* IcyHumps = L"\x2947";
        static const wchar_t* SandblastedLodestone = L"\x8101\x43D2\x8CB3\xFC99\x602F";
        static const wchar_t* BlackPearls = L"\x56FB\xA16B\x9DAD\x62B6";
        static const wchar_t* InsectCarapaces = L"\x8101\x43F7\xFD85\x9D52\x6DFA";
        static const wchar_t* MergoyleSkulls = L"\x2911";
        static const wchar_t* DecayedOrrEmblems = L"\x2957";
        static const wchar_t* TemperedGlassVials = L"\x22E2\xCE9B\x8771\x7DC7";
        static const wchar_t* ScorchedLodestones = L"\x2943";
        static const wchar_t* WaterDjinnEssence = L"\x8101\x583C\xD7B3\xDD92\x598F";
        static const wchar_t* GuardianMoss = L"\x5703\xE1CC\xFE29\x4525";
        static const wchar_t* DwarvenAles = L"\x22C1";
        static const wchar_t* AmphibianTongues = L"\x8102\x26D9\xABE9\x9082\x4999";
        static const wchar_t* AlpineSeeds = L"\x294E";
        static const wchar_t* TangledSeeds = L"\x2931";
        static const wchar_t* StolenSupplies = L"\x56FA\xE3AB\xA19E\x5D6A";
        static const wchar_t* PahnaiSalad = L"\x8101\x42D3\xD9E7\xD4E3\x259E";
        static const wchar_t* VerminHides = L"\x5707\xF70E\xCAA2\x5CC5";
        static const wchar_t* RoaringEtherHeart = L"\x8101\x52ED\x86E9\xCEF3\x69D3";
        static const wchar_t* LeatheryClaws = L"\x2941";
        static const wchar_t* AzureCrest = L"\x56FE\xF2B0\x8B62\x116A";
        static const wchar_t* JotunPelt = L"\x8102\x26E2\xC8E7\x8B1F\x716A";
        static const wchar_t* HeketTongues = L"\x8101\x583E\xE3F5\x87A2\x194F";
        static const wchar_t* MountainTrollTusks = L"\x2951";
        static const wchar_t* VialsofInk = L"\x22E7\xC1DA\xF2C1\x452A";
        static const wchar_t* KournanPendants = L"\x8101\x43E9\xDBD0\xA0C6\x4AF1";
        static const wchar_t* SingedGargoyleSkulls = L"\x293D";
        static const wchar_t* DredgeIncisors = L"\x56E4\xDF8C\xAD76\x3958";
        static const wchar_t* StoneSummitBadges = L"\x2955";
        static const wchar_t* KraitSkins = L"\x8102\x26F4\xE764\xC908\x52E2";
        static const wchar_t* InscribedShards = L"\x8101\x43D0\x843D\x98D1\x775C";
        static const wchar_t* FeatheredScalps = L"\x56F6\xB464\x9A9E\x11EF";
        static const wchar_t* MummyWrappings = L"\x8101\x43EB\xF92D\xD469\x73A8";
        static const wchar_t* ShadowyRemnants = L"\x2916";
        static const wchar_t* AncientKappaShells = L"\x570B\xFE7B\xBD8A\x7CF4";
        static const wchar_t* Geode = L"\x8101\x5206\x8286\xFEFA\x191C";
        static const wchar_t* FibrousMandragorRoots = L"\x8102\x26E8\xC0DB\xD26E\x4711";
        static const wchar_t* GruesomeRibcages = L"\x293F";
        static const wchar_t* KrakenEyes = L"\x56FD\xC65F\xF6F1\x26B4";
        static const wchar_t* BogSkaleFins = L"\x2918";
        static const wchar_t* SentientSpores = L"\x8101\x583D\xB904\xF476\x59A7";
        static const wchar_t* AncientEyes = L"\x292D";
        static const wchar_t* CopperShillings = L"\x8101\x43E4\x8D6E\x83E5\x4C07";
        static const wchar_t* FrigidMandragorHusks = L"\x8102\x26DF\xF8E8\x8ACB\x58B4";
        static const wchar_t* BoltsofLinen = L"\x22D5\x8371\x8ED5\x56B4";
        static const wchar_t* CharrCarvings = L"\x28EE";

        static const wchar_t* GuildVersusGuild = L"\x8102\x58B6\xFE7C\xB94E\x2209";
        static const wchar_t* AllianceBattles = L"\x108\x107" L"Alliance Battles\x1"; // TODO: find this, subtitle when entering zone

        static const wchar_t* GuildVersusGuildBonus = L"\x8103\xAD9";
        static const wchar_t* GuildVersusGuildBonusDescription = L"\x8103\xADa";
        static const wchar_t* AllianceBattleBonus = L"\x8103\xADb";
        static const wchar_t* AllianceBattleBonusDescription = L"\x8103\xADc";
        static const wchar_t* RandomArenasBonus = L"\x8103\xADd";
        static const wchar_t* RandomArenasBonusDescription = L"\x8103\xADe";
        static const wchar_t* HeroesAscentBonus = L"\x8103\xADf";
        static const wchar_t* HeroesAscentBonusDescription = L"\x8103\xAe0";
        static const wchar_t* CompetitiveMissionBonus = L"\x8103\xAe1";
        static const wchar_t* CompetitiveMissionBonusDescription = L"\x8103\xAe2";
        static const wchar_t* CodexArenaBonus = L"\x8103\xAe3";
        static const wchar_t* CodexArenaBonusDescription = L"\x8103\xAe4";
        static const wchar_t* FactionSupportBonus = L"\x8103\xAe5";
        static const wchar_t* FactionSupportBonusDescription = L"\x8103\xAe6";
        static const wchar_t* ZaishenVanquishingBonus = L"\x8103\xAe7";
        static const wchar_t* ZaishenVanquishingBonusDescription = L"\x8103\xAe8";
        static const wchar_t* ExtraLuckBonus = L"\x8103\xAe9";
        static const wchar_t* ExtraLuckBonusDescription = L"\x8103\xAea";
        static const wchar_t* ElonianSupportBonus = L"\x8103\xAeb";
        static const wchar_t* ElonianSupportBonusDescription = L"\x8103\xAec";
        static const wchar_t* ZaishenBountyBonus = L"\x8103\xAed";
        static const wchar_t* ZaishenBountyBonusDescription = L"\x8103\xAee";
        static const wchar_t* FactionsEliteBonus = L"\x8103\xAef";
        static const wchar_t* FactionsEliteBonusDescription = L"\x8103\xAf0";
        static const wchar_t* NorthernSupportBonus = L"\x8103\xAf1";
        static const wchar_t* NorthernSupportBonusDescription = L"\x8103\xAf2";
        static const wchar_t* ZaishenMissionBonus = L"\x8103\xAf3";
        static const wchar_t* ZaishenMissionBonusDescription = L"\x8103\xAf4";
        static const wchar_t* PantheonBonus = L"\x8103\xAf5";
        static const wchar_t* PantheonBonusDescription = L"\x8103\xAf6";

        namespace Quest {
            static const wchar_t* ZaishenVanquish_Fahranur_the_First_City = L"\x8102\x5817\xE668\x9A08\x716F";

            static const wchar_t* ZaishenQuest_GOLEM = L"\x8102\x56CD\xB82D\x940D\x5815";

            static const wchar_t* ZaishenBounty_Urgoz = L"\x8102\x5491\x88BD\x9648\x42B3";
            static const wchar_t* ZaishenBounty_Rragar_Maneater = L"\x8102\x54F1\xA747\xA10C\x4514"; 
            static const wchar_t* ZaishenBounty_Zhim_Monns = L"\x8102\x5515\xFB97\xECDE\x6DE5";
            static const wchar_t* ZaishenBounty_Magmus = L"\x8102\x5545\xFCCD\x97B4\x1449";
        }
    }
}
