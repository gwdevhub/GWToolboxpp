#pragma once

namespace GW {
    namespace Constants {

        namespace ModelID { // this is actually agent->PlayerNumber for agents
            constexpr int Rotscale = 2837;

            // New bosses 2020-04-22
            enum {
                AbaddonsCursed = 1338,
                BladeAncientSyuShai,
                YoannhTheRebuilber,
                FureystSharpsight
            };
            constexpr int EoE = 2876;
            constexpr int QZ = 2886;
            constexpr int Winnowing = 2875;
            constexpr int InfuriatingHeat = 5715;
            constexpr int Equinox = 4236;
            constexpr int Famine = 4238;
            constexpr int FrozenSoil = 2882;
            constexpr int Quicksand = 5718;
            constexpr int Lacerate = 4232;

            constexpr int ProphetVaresh = 5292;
            constexpr int CommanderVaresh = 5293;

            constexpr int Boo = 7449;

            constexpr int LockedChest = 8141; // this is actually ->ExtraType

            /* Birthday Minis */
            /* unknowns are commented as a guess as inventory name but fit a pattern of id schema */
            namespace Minipet {
                // First Year
                constexpr int Charr = 230;                  // Miniature Charr Shaman (purple)
                constexpr int Dragon = 231;                 // Miniature Bone Dragon (green)
                constexpr int Rurik = 232;                  // Miniature Prince Rurik (gold)
                constexpr int Shiro = 233;                  // Miniature Shiro (gold)
                constexpr int Titan = 234;                  // Miniature Burning Titan (purple)
                constexpr int Kirin = 235;                  // Miniature Kirin (purple)
                constexpr int NecridHorseman = 236;         // Miniature Necrid Horseman (white)
                constexpr int JadeArmor = 237;              // Miniature Jade Armor (white)
                constexpr int Hydra = 238;                  // Miniature Hydra (white)
                constexpr int FungalWallow = 239;           // Miniature Fungal Wallow (white)
                constexpr int SiegeTurtle = 240;            // Miniature Siege Turtle (white)
                constexpr int TempleGuardian = 241;         // Miniature Temple Guardian (white)
                constexpr int JungleTroll = 242;            // Miniature Jungle Troll (white)
                constexpr int WhiptailDevourer = 243;       // Miniature Whiptail Devourer (white)
                // Second Year
                constexpr int Gwen = 244;                   // Miniature Gwen (green)
                constexpr int GwenDoll = 245;               // Miniature Gwen Doll (??)
                constexpr int WaterDjinn = 246;             // Miniature Water Djinn (gold)
                constexpr int Lich = 247;                   // Miniature Lich (gold)
                constexpr int Elf = 248;                    // Miniature Elf (purple)
                constexpr int PalawaJoko = 249;             // Miniature Palawa Joko (purple)
                constexpr int Koss = 250;                   // Miniature Koss (purple)
                constexpr int MandragorImp = 251;           // Miniature Mandragor Imp (white)
                constexpr int HeketWarrior = 252;           // Miniature Heket Warrior (white)
                constexpr int HarpyRanger = 253;            // Miniature Harpy Ranger (white)
                constexpr int Juggernaut = 254;             // Miniature Juggernaut (white)
                constexpr int WindRider = 255;              // Miniature Wind Rider (white)
                constexpr int FireImp = 256;                // Miniature Fire Imp (white)
                constexpr int Aatxe = 257;                  // Miniature Aatxe (white)
                constexpr int ThornWolf = 258;              // Miniature Thorn Wolf (white)
                // Third Year
                constexpr int Abyssal = 259;                // Miniature Abyssal (white)
                constexpr int BlackBeast = 260;             // Miniature Black Beast of Aaaaarrrrrrgghh (gold)
                constexpr int Freezie = 261;                // Miniature Freezie (purple)
                constexpr int Irukandji = 262;              // Miniature Irukandji (white)
                constexpr int MadKingThorn = 263;           // Miniature Mad King Thorn (green)
                constexpr int ForestMinotaur = 264;         // Miniature Forest Minotaur (white)
                constexpr int Mursaat = 265;                // Miniature Mursaat (white)
                constexpr int Nornbear = 266;               // Miniature Nornbear (purple)
                constexpr int Ooze = 267;                   // Miniature Ooze (purple)
                constexpr int Raptor = 268;                 // Miniature Raptor (white)
                constexpr int RoaringEther = 269;           // Miniature Roaring Ether (white)
                constexpr int CloudtouchedSimian = 270;     // Miniature Cloudtouched Simian (white)
                constexpr int CaveSpider = 271;             // Miniature Cave Spider (white)
                constexpr int WhiteRabbit = 272;            // White Rabbit (gold)
                // Fourth Year
                constexpr int WordofMadness = 273;          // Miniature Word of Madness (white)
                constexpr int DredgeBrute = 274;            // Miniature Dredge Brute (white)
                constexpr int TerrorwebDryder = 275;        // Miniature Terrorweb Dryder (white)
                constexpr int Abomination = 276;            // Miniature Abomination (white)
                constexpr int KraitNeoss = 277;             // Miniature Krait Neoss (white)
                constexpr int DesertGriffon = 278;          // Miniature Desert Griffon (white)
                constexpr int Kveldulf = 279;               // Miniature Kveldulf (white)
                constexpr int QuetzalSly = 280;             // Miniature Quetzal Sly (white)
                constexpr int Jora = 281;                   // Miniature Jora (purple)
                constexpr int FlowstoneElemental = 282;     // Miniature Flowstone Elemental (purple)
                constexpr int Nian = 283;                   // Miniature Nian (purple)
                constexpr int DagnarStonepate = 284;        // Miniature Dagnar Stonepate (gold)
                constexpr int FlameDjinn = 285;             // Miniature Flame Djinn (gold)
                constexpr int EyeOfJanthir = 286;           // Miniature Eye of Janthir (green)
                // Fifth Year
                constexpr int Seer = 287;                   // Miniature Seer (white)
                constexpr int SiegeDevourer = 288;          // Miniature Siege Devourer (white)
                constexpr int ShardWolf = 289;              // Miniature Shard Wolf (white)
                constexpr int FireDrake = 290;              // Miniature Fire Drake (white)
                constexpr int SummitGiantHerder = 291;      // Miniature Summit Giant Herder (white)
                constexpr int OphilNahualli = 292;          // Miniature Ophil Nahualli (white)
                constexpr int CobaltScabara = 293;          // Miniature Cobalt Scabara (white)
                constexpr int ScourgeManta = 294;           // Miniature Scourge Manta (white)
                constexpr int Ventari = 295;                // Miniature Ventari (purple)
                constexpr int Oola = 296;                   // Miniature Oola (purple)
                constexpr int CandysmithMarley = 297;       // Miniature CandysmithMarley (purple)
                constexpr int ZhuHanuku = 298;              // Miniature Zhu Hanuku (gold)
                constexpr int KingAdelbern = 299;           // Miniature King Adelbern (gold)
                constexpr int MOX1 = 300;                   // Miniature M.O.X. (color?)
                constexpr int MOX2 = 301;                   // Miniature M.O.X. (color?)
                constexpr int MOX3 = 302;                   // Miniature M.O.X. (green)
                constexpr int MOX4 = 303;                   // Miniature M.O.X. (color?)
                constexpr int MOX5 = 304;                   // Miniature M.O.X. (color?)
                constexpr int MOX6 = 305;                   // Miniature M.O.X. (color?)

                // In-game rewards and promotionals
                constexpr int BrownRabbit = 306;            // Miniature Brown Rabbit
                constexpr int Yakkington = 307;             // Miniature Yakkington
                // constexpr int Unknown308 = 308;
                constexpr int CollectorsEditionKuunavang = 309; // Miniature Kuunavang (green)
                constexpr int GrayGiant = 310;
                constexpr int Asura = 311;
                constexpr int DestroyerOfFlesh = 312;
                constexpr int PolarBear = 313;
                constexpr int CollectorsEditionVaresh = 314;
                constexpr int Mallyx = 315;
                constexpr int Ceratadon = 316;

                // Misc.
                constexpr int Kanaxai = 317;
                constexpr int Panda = 318;
                constexpr int IslandGuardian = 319;
                constexpr int NagaRaincaller = 320;
                constexpr int LonghairYeti = 321;
                constexpr int Oni = 322;
                constexpr int ShirokenAssassin = 323;
                constexpr int Vizu = 324;
                constexpr int ZhedShadowhoof = 325;
                constexpr int Grawl = 326;
                constexpr int GhostlyHero = 327;

                // Special events
                constexpr int Pig = 328;
                constexpr int GreasedLightning = 329;
                constexpr int WorldFamousRacingBeetle = 330;
                constexpr int CelestialPig = 331;
                constexpr int CelestialRat = 332;
                constexpr int CelestialOx = 333;
                constexpr int CelestialTiger = 334;
                constexpr int CelestialRabbit = 335;
                constexpr int CelestialDragon = 336;
                constexpr int CelestialSnake = 337;
                constexpr int CelestialHorse = 338;
                constexpr int CelestialSheep = 339;
                constexpr int CelestialMonkey = 340;
                constexpr int CelestialRooster = 341;
                constexpr int CelestialDog = 342;

                // In-game
                constexpr int BlackMoaChick = 343;
                constexpr int Dhuum = 344;
                constexpr int MadKingsGuard = 345;
                constexpr int SmiteCrawler = 346;

                // Zaishen strongboxes, and targetable minipets
                constexpr int GuildLord = 347;
                constexpr int HighPriestZhang = 348;
                constexpr int GhostlyPriest = 349;
                constexpr int RiftWarden = 350;

                // More in-game drops and rewards
                constexpr int MiniatureLegionnaire = 7984;
                constexpr int MiniatureConfessorDorian = 8293;
                constexpr int MiniaturePrincessSalma = 8298;
                constexpr int MiniatureLivia = 8299;
                constexpr int MiniatureEvennia = 8300;
                constexpr int MiniatureConfessorIsaiah = 8301;
                // missing miniature 8298 ?
                constexpr int MiniaturePeacekeeperEnforcer = 8303;
                constexpr int MiniatureMinisterReiko = 8987;
                constexpr int MiniatureEcclesiateXunRao = 8988;
            }

            namespace DoA {
                // Friendly
                constexpr int FoundrySnakes = 5221;

                constexpr int BlackBeastOfArgh = 5150;
                constexpr int SmotheringTendril = 5214;
                constexpr int Fury = 5149;
                constexpr int LordJadoth = 5144;

                // stygian lords
                constexpr int StygianLordNecro = 5145;
                constexpr int StygianLordMesmer = 5146;
                constexpr int StygianLordEle = 5147;
                constexpr int StygianLordMonk = 5148;
                constexpr int StygianLordDerv = 5163;
                constexpr int StygianLordRanger = 5164;

                // margonites
                constexpr int MargoniteAnurKaya = 5166;
                constexpr int MargoniteAnurDabi = 5167;
                constexpr int MargoniteAnurSu = 5168;
                constexpr int MargoniteAnurKi = 5169;
                constexpr int MargoniteAnurVu = 5170;
                constexpr int MargoniteAnurTuk = 5171;
                constexpr int MargoniteAnurRuk = 5172;
                constexpr int MargoniteAnurRund = 5173;
                constexpr int MargoniteAnurMank = 5174;

                // stygians
                constexpr int StygianHunger = 5175;
                constexpr int StygianBrute = 5176;
                constexpr int StygianGolem = 5177;
                constexpr int StygianHorror = 5178;
                constexpr int StygianFiend = 5179;

                // tormentors in veil
                constexpr int VeilMindTormentor = 5180;
                constexpr int VeilSoulTormentor = 5181;
                constexpr int VeilWaterTormentor = 5182;
                constexpr int VeilHeartTormentor = 5183;
                constexpr int VeilFleshTormentor = 5184;
                constexpr int VeilSpiritTormentor = 5185;
                constexpr int VeilEarthTormentor = 5186;
                constexpr int VeilSanityTormentor = 5187;
                // tormentors
                constexpr int MindTormentor = 5204;
                constexpr int SoulTormentor = 5205;
                constexpr int WaterTormentor = 5206;
                constexpr int HeartTormentor = 5207;
                constexpr int FleshTormentor = 5208;
                constexpr int SpiritTormentor = 5209;
                constexpr int EarthTormentor = 5210;
                constexpr int SanityTormentor = 5212;

                // titans
                constexpr int MiseryTitan = 5195;
                constexpr int RageTitan = 5196;
                constexpr int DementiaTitan = 5197;
                constexpr int AnguishTitan = 5198;
                constexpr int DespairTitan = 5199;
                constexpr int FuryTitan = 5200;
                constexpr int RageTitan2 = 5201;
                constexpr int DementiaTitan2 = 5202;
                constexpr int DespairTitan2 = 5203;

                constexpr int TorturewebDryder = 5215;
                constexpr int GreaterDreamRider = 5216;
                constexpr int GreaterDreamRider2 = 5217;
            }

            namespace UW {
                constexpr int ChainedSoul = 2317;
                constexpr int DyingNightmare = 2318;
                constexpr int ObsidianBehemoth = 2319;
                constexpr int ObsidianGuardian = 2320;
                constexpr int TerrorwebDryder = 2321;
                constexpr int TerrorwebDryderSilver = 2322;
                constexpr int KeeperOfSouls = 2323;
                constexpr int TerrorwebQueen = 2324; // boss-like
                constexpr int SmiteCrawler = 2325;
                constexpr int WailingLord = 2326;       // Note: same as FoW::Banshee
                constexpr int BanishedDreamRider = 2327;
                // 2324 ?
                constexpr int FourHorseman = 2329; // all four share the same id
                constexpr int MindbladeSpectre = 2330;

                constexpr int DeadCollector = 2332;
                constexpr int DeadThresher = 2333;
                constexpr int ColdfireNight = 2334;
                constexpr int StalkingNight = 2335;
                // 2332 ?
                constexpr int ChargedBlackness = 2337;
                constexpr int GraspingDarkness = 2338;
                constexpr int BladedAatxe = 2339;
                // 2336 ?
                constexpr int Slayer = 2341;
                constexpr int SkeletonOfDhuum1 = 2342;
                constexpr int SkeletonOfDhuum2 = 2343;
                constexpr int ChampionOfDhuum = 2344;
                constexpr int MinionOfDhuum = 2345;
                constexpr int Dhuum = 2346;

                constexpr int Reapers = 2348; // outside dhuum chamber
                constexpr int ReapersAtDhuum = 2349; // in dhuum chamber
                constexpr int IceElemental = 2350; // friendly, during waste quest near dhuum.
                constexpr int KingFrozenwind = 2352;
                constexpr int TorturedSpirit1 = 2353; // friendly, during quest
                constexpr int Escort1 = 2356; // souls npc spawned by escort quest
                constexpr int Escort2 = 2357;
                constexpr int Escort3 = 2358;
                constexpr int Escort4 = 2359;
                constexpr int Escort5 = 2360;
                constexpr int Escort6 = 2361;
                constexpr int PitsSoul1 = 2362;
                constexpr int PitsSoul2 = 2363;
                constexpr int PitsSoul3 = 2364;
                constexpr int PitsSoul4 = 2365;

                constexpr int TorturedSpirit = 2371;
                constexpr int MajorAlgheri = 2373;
            }

            namespace FoW {
                constexpr int NimrosTheHunter = 1482;
                constexpr int MikoTheUnchained = 1965;

                constexpr int Banshee = 2326;           // Note: same as UW::WailingLord

                constexpr int MahgoHydra = 2796;
                constexpr int ArmoredCaveSpider = 2800;
                constexpr int SmokeWalker = 2801;
                constexpr int ObsidianFurnanceDrake = 2802;
                constexpr int DoubtersDryder = 2803;
                constexpr int ShadowMesmer = 2804;
                constexpr int ShadowElemental = 2805;
                constexpr int ShadowMonk = 2806;
                constexpr int ShadowWarrior = 2807;
                constexpr int ShadowRanger = 2808;
                constexpr int ShadowBeast = 2809;
                constexpr int Abyssal = 2810;           // Note: same as ShadowOverlord.
                constexpr int ShadowOverlord = 2810;    // Note: same as Abyssal.
                constexpr int SeedOfCorruption = 2811;
                constexpr int SpiritWood = 2812;
                constexpr int SpiritShepherd = 2813;
                constexpr int AncientSkale = 2814;
                constexpr int SnarlingDriftwood = 2815;
                constexpr int SkeletalEtherBreaker = 2816;
                constexpr int SkeletalIcehand = 2817;
                constexpr int SkeletalBond = 2818;
                constexpr int SkeletalBerserker = 2819;
                constexpr int SkeletalImpaler = 2820;
                constexpr int RockBorerWorm = 2821;
                constexpr int InfernalWurm = 2822;
                constexpr int DragonLich = 2823;
                constexpr int Menzies = 2824;
                // 2821 ?
                constexpr int Rastigan = 2826;          // Friendly NPC
                constexpr int Griffons = 2827;          // Friendly NPC
                constexpr int LordKhobay = 2828;        // Unfriendly NPC
                constexpr int Forgemaster = 2829;       // Friendly NPC
                // 2826 ?
                constexpr int TraitorousTempleGuard1 = 2831;
                constexpr int TraitorousTempleGuard2 = 2832;
                constexpr int TraitorousTempleGuard3 = 2833;
                // 2830 ?
                constexpr int ShardWolf = 2835;
            }

            namespace Urgoz {
                constexpr int HoppingVampire = 3745;
                constexpr int Urgoz = 3754;
            }

            namespace Deep {
                constexpr int Kanaxai = 4059;
                constexpr int KanaxaiAspect = 4060;
            }

            namespace EotnDungeons {
                constexpr int DiscOfChaos = 6071;
                constexpr int PlagueOfDestruction = 6083;
                constexpr int ZhimMonns = 6246;
                constexpr int Khabuus = 6248;
                constexpr int DuncanTheBlack = 6405;
                constexpr int JusticiarThommis = 6406;
                constexpr int RandStormweaver = 6407;
                constexpr int Selvetarm = 6408;
                constexpr int Forgewright = 6409;
                constexpr int HavokSoulwail = 6427;
                constexpr int RragarManeater3 = 6558; // lvl 3
                constexpr int RragarManeater12 = 6559; // lvl 1 and 2
                constexpr int Arachni = 6793;
                constexpr int Hidesplitter = 6801;
                constexpr int PrismaticOoze = 6806;
                constexpr int IlsundurLordofFire = 6814;
                constexpr int EldritchEttin = 6821;
                constexpr int TPSRegulartorGolem = 6832;
                constexpr int MalfunctioningEnduringGolem = 6834;
                constexpr int StormcloudIncubus = 6851;
                constexpr int CyndrTheMountainHeart = 6914;
                constexpr int InfernalSiegeWurm = 6927; // kath lvl1 boss
                constexpr int Frostmaw = 6932;
                constexpr int RemnantOfAntiquities = 6934;
                constexpr int MurakaiLadyOfTheNight = 7008;
                constexpr int ZoldarkTheUnholy = 7010;

                constexpr int Brigand = 7009; // soo
                constexpr int FendiNin = 7013;
                constexpr int SoulOfFendiNin = 7014;

                constexpr int KeymasterOfMurakai = 7018;
                constexpr int AngrySnowman = 7411;
            }

            namespace BonusMissionPack {
                constexpr int WarAshenskull = 7079;
                constexpr int RoxAshreign = 7080;
                constexpr int AnrakTindershot = 7081;
                constexpr int DettMortash = 7082;
                constexpr int AkinCinderspire = 7083;
                constexpr int TwangSootpaws = 7084;
                constexpr int MagisEmberglow = 7085;
                constexpr int MerciaTheSmug = 7114;
                constexpr int OptimusCaliph = 7115;
                constexpr int LazarusTheDire = 7116;
                constexpr int AdmiralJakman = 7179;
                constexpr int PalawaJoko = 7198;
                constexpr int YuriTheHand = 7146;
                constexpr int MasterRiyo = 7147;
                constexpr int CaptainSunpu = 7149;
                constexpr int MinisterWona = 7150;
            }
            namespace PolymockSummon {
                enum {
                    // Polymocks
                    MursaatElementalist = 5847,
                    FlameDjinn,
                    IceImp,
                    NagaShaman
                };
            }
            namespace SummoningStone {
                constexpr int MercantileMerchant = 467;
                constexpr int ZaishenArcher = 468;
                constexpr int ZaishenAvatarOfBalthazar = 469;
                constexpr int ZaishenChampionOfBalthazar = 470;
                constexpr int ZaishenPriestOfBalthazar = 471;
                constexpr int ZaishenFootman = 472;
                constexpr int ZaishenGuildLord = 473;
                constexpr int MysteriousCrystalSpider = 490;
                constexpr int MysteriousSaltsprayDragon = 491;
                constexpr int MysteriousRestlessCorpse = 492;
                constexpr int MysteriousSmokePhantom = 493;
                constexpr int MysteriousSwarmOfBees = 494;
                constexpr int AutomatonGolem = 512;
                constexpr int IgneousFireImp = 513;
                constexpr int JadeiteSiegeTurtle = 514;
                constexpr int DemonicOni = 515;
                constexpr int AmberJuggernaut  = 516;
                constexpr int MysticalGaki = 517;
                constexpr int GelatinousOoze = 518;
                constexpr int ChitinousDevourer = 519;
                constexpr int ArcticKveldulf  = 520;
                constexpr int FossilizedRaptor = 521;
                constexpr int MischievousGrentch = 522;
                constexpr int FrostySnowman = 523;
                constexpr int GhastlyDreamRider = 524;
                constexpr int ImperialCripplingSlash = 8990;
                constexpr int ImperialTripleChop = 8991;
                constexpr int ImperialBarrage = 8992;
                constexpr int ImperialQuiveringBlade = 8993;
                constexpr int TenguHundredBlades = 8994;
                constexpr int TenguBroadHeadArrow = 8995;
                constexpr int TenguPalmStrike = 8996;
                constexpr int TenguLifeSheath = 8997;
                constexpr int TenguAngchuElementalist = 8998;
                constexpr int TenguFeveredDreams = 8999;
                constexpr int TenguSpitefulSpirit = 9000;
                constexpr int TenguPreservation = 9001;
                constexpr int TenguPrimalRage = 9002;
                constexpr int TenguGlassArrows = 9003;
                constexpr int TenguWayOftheAssassin = 9004;
                constexpr int TenguPeaceandHarmony = 9005;
                constexpr int TenguSandstorm = 9006;
                constexpr int TenguPanic = 9007;
                constexpr int TenguAuraOftheLich = 9008;
                constexpr int TenguDefiantWasXinrae = 9009;
            }

            constexpr int EbonVanguardAssassin = PolymockSummon::NagaShaman + 2;
        }
    }
}
