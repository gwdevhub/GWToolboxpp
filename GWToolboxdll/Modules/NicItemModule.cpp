#include <Modules/NicItemModule.h>
#include <Modules/ItemDescriptionHandler.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <Logger.h>
#include <Utils/GuiUtils.h>

namespace GW {
    namespace EncStrings {
        const wchar_t* RedIrisFlowers = L"\x271E\xDBDF\xBBD8\x34CB";
        const wchar_t* FeatheredAvicaraScalps = L"\x294F";
        const wchar_t* MargoniteMasks = L"\x8101\x43E8\xBEBD\xF09A\x4455";
        const wchar_t* QuetzalCrests = L"\x8102\x26DC\xADF9\x8D4B\x6575";
        const wchar_t* PlagueIdols = L"\x56D3\xA490\xE607\x11B6";
        const wchar_t* AzureRemains = L"\x294D";
        const wchar_t* MandragorRootCake = L"\x8101\x52E9\xA18F\x8FE1\x3EE9";
        const wchar_t* MahgoClaw = L"\x2960";
        const wchar_t* MantidPincers = L"\x56EF\xD1D8\xC773\x2C26";
        const wchar_t* SentientSeeds = L"\x8101\x43DE\xD124\xA4D9\x7D4A";
        const wchar_t* StoneGrawlNecklaces = L"\x8102\x26EA\x8A6F\xD31C\x31DD";
        const wchar_t* Herring = L"\x8102\x26D1";
        const wchar_t* NagaSkins = L"\x5702\xA954\x959D\x51B8";
        const wchar_t* GloomSeed = L"\x296A";
        const wchar_t* CharrHide = L"\x2882";
        const wchar_t* RubyDjinnEssence = L"\x8101\x43F3\xF5F8\xC245\x41F2";
        const wchar_t* ThornyCarapaces = L"\x2930";
        const wchar_t* BoneCharms = L"\x56DD\xC82C\xB7E0\x3EB9";
        const wchar_t* ModniirManes = L"\x8102\x26E0\xA884\xE2D3\x7E01";
        const wchar_t* SuperbCharrCarvings = L"\x8102\x26E9\x96D3\x8E81\x64D1";
        const wchar_t* RollsofParchment = L"\x22EE\xF65A\x86E6\x1C6C";
        const wchar_t* RoaringEtherClaws = L"\x8101\x5208\xA22C\xC074\x2373";
        const wchar_t* BranchesofJuniBerries = L"\x8101\x5721";
        const wchar_t* ShiverpeakManes = L"\x2945";
        const wchar_t* FetidCarapaces = L"\x293C";
        const wchar_t* MoonShells = L"\x6CCD\xC6FD\xA37B\x3529";
        const wchar_t* MassiveJawbone = L"\x2921";
        const wchar_t* ChromaticScale = L"\x8102\x26FA\x8E00\xEA86\x3A1D";
        const wchar_t* MursaatTokens = L"\x292B";
        const wchar_t* SentientLodestone = L"\x8101\x43FA\xA429\xC255\x23C4";
        const wchar_t* JungleTrollTusks = L"\x2934";
        const wchar_t* SapphireDjinnEssence = L"\x8101\x57DD\xF97D\xB5AD\x21FF";
        const wchar_t* StoneCarving = L"\x56E6\xB928\x9FA2\x43E1";
        const wchar_t* FeatheredCaromiScalps = L"\x2919";
        const wchar_t* PillagedGoods = L"\x8101\x52EE\xBF76\xE319\x2B39";
        const wchar_t* GoldCrimsonSkullCoin = L"\x56D5\x8B0F\xAB5B\x8A6";
        const wchar_t* JadeBracelets = L"\x56D7\xDD87\x8A67\x167D";
        const wchar_t* MinotaurHorns = L"\x2924";
        const wchar_t* FrostedGriffonWings = L"\x294A";
        const wchar_t* SilverBullionCoins = L"\x8101\x43E6\xBE4C\xE956\x780";
        const wchar_t* Truffle = L"\x56DF\xFEE4\xCA2D\x27A";
        const wchar_t* SkelkClaws = L"\x8102\x26DD\x85C5\xD98F\x5CCB";
        const wchar_t* DessicatedHydraClaws = L"\x2923";
        const wchar_t* FrigidHearts = L"\x294B";
        const wchar_t* CelestialEssences = L"\x570A\x9453\x84A6\x64D4";
        const wchar_t* PhantomResidue = L"\x2937";
        const wchar_t* DrakeKabob = L"\x8101\x42D1\xFB15\xD39E\x5A26";
        const wchar_t* AmberChunks = L"\x55D0\xF8B7\xB108\x6018";
        const wchar_t* GlowingHearts = L"\x2914";
        const wchar_t* SaurianBones = L"\x8102\x26D8\xB5B9\x9AF6\x42D6";
        const wchar_t* BehemothHides = L"\x8101\x5207\xEBD7\xB733\x2E27";
        const wchar_t* LuminousStone = L"\x8101\x4E35\xD63F\xCAB4\xDD1";
        const wchar_t* IntricateGrawlNecklaces = L"\x2950";
        const wchar_t* JadeiteShards = L"\x55D1\xD189\x845A\x7164";
        const wchar_t* GoldDoubloon = L"\x8101\x43E5\xA891\xA83A\x426D";
        const wchar_t* ShriveledEyes = L"\x291B";
        const wchar_t* IcyLodestones = L"\x28EF";
        const wchar_t* KeenOniTalon = L"\x5701\xD258\xC958\x506F";
        const wchar_t* HardenedHumps = L"\x2910";
        const wchar_t* PilesofElementalDust = L"\x8102\x26E7\xC330\xC111\x4058";
        const wchar_t* NagaHides = L"\x56F2\x876E\xEACB\x730";
        const wchar_t* SpiritwoodPlanks = L"\x22F3\xA11C\xC924\x5E15";
        const wchar_t* StormyEye = L"\x293A";
        const wchar_t* SkreeWings = L"\x8101\x43F0\xFF3B\x8E3E\x20B1";
        const wchar_t* SoulStones = L"\x5706\xC61F\xF23D\x3C4";
        const wchar_t* SpikedCrest = L"\x290F";
        const wchar_t* DragonRoot = L"\x56E5\x922D\xCF17\x7258";
        const wchar_t* BerserkerHorns = L"\x8102\x26E3\xB76F\xE56C\x1A2";
        const wchar_t* BehemothJaw = L"\x292E";
        const wchar_t* BowlofSkalefinSoup = L"\x8101\x42D2\xE08B\xB81A\x604";
        const wchar_t* ForestMinotaurHorns = L"\x2915";
        const wchar_t* PutridCysts = L"\x56ED\xE607\x9B27\x7299";
        const wchar_t* JadeMandibles = L"\x2926";
        const wchar_t* MaguumaManes = L"\x292F";
        const wchar_t* SkullJuju = L"\x56E0\xFBEB\xA429\x7B5";
        const wchar_t* MandragorSwamproots = L"\x8101\x5840\xB4F5\xB2A7\x5E0F";
        const wchar_t* BottleofVabbianWine = L"\x8101\x52EA";
        const wchar_t* WeaverLegs = L"\x8102\x26DA\x950E\x82F1\xA3D";
        const wchar_t* TopazCrest = L"\x291F";
        const wchar_t* RotWallowTusks = L"\x56FC\xD503\x9D77\x730C";
        const wchar_t* FrostfireFangs = L"\x2946";
        const wchar_t* DemonicRelic = L"\x8101\x43E7\xD854\xC981\x54DD";
        const wchar_t* AbnormalSeeds = L"\x2917";
        const wchar_t* DiamondDjinnEssence = L"\x8101\x43EA\xE72E\xAA23\x3C54";
        const wchar_t* ForgottenSeals = L"\x2928";
        const wchar_t* CopperCrimsonSkullCoins = L"\x56D4\x8663\xA244\x50F5";
        const wchar_t* MossyMandibles = L"\x2932";
        const wchar_t* EnslavementStones = L"\x2954";
        const wchar_t* ElonianLeatherSquares = L"\x22E6\xE8F4\xA898\x75CB";
        const wchar_t* CobaltTalons = L"\x8101\x43EC\x8335\xBAA8\x153C";
        const wchar_t* MaguumaSpiderWeb = L"\x288C";
        const wchar_t* ForgottenTrinketBoxes = L"\x56EB\xB8B7\xF734\x2985";
        const wchar_t* IcyHumps = L"\x2947";
        const wchar_t* SandblastedLodestone = L"\x8101\x43D2\x8CB3\xFC99\x602F";
        const wchar_t* BlackPearls = L"\x56FB\xA16B\x9DAD\x62B6";
        const wchar_t* InsectCarapaces = L"\x8101\x43F7\xFD85\x9D52\x6DFA";
        const wchar_t* MergoyleSkulls = L"\x2911";
        const wchar_t* DecayedOrrEmblems = L"\x2957";
        const wchar_t* TemperedGlassVials = L"\x22E2\xCE9B\x8771\x7DC7";
        const wchar_t* ScorchedLodestones = L"\x2943";
        const wchar_t* WaterDjinnEssence = L"\x8101\x583C\xD7B3\xDD92\x598F";
        const wchar_t* GuardianMoss = L"\x5703\xE1CC\xFE29\x4525";
        const wchar_t* DwarvenAles = L"\x22C1";
        const wchar_t* AmphibianTongues = L"\x8102\x26D9\xABE9\x9082\x4999";
        const wchar_t* AlpineSeeds = L"\x294E";
        const wchar_t* TangledSeeds = L"\x2931";
        const wchar_t* StolenSupplies = L"\x56FA\xE3AB\xA19E\x5D6A";
        const wchar_t* PahnaiSalad = L"\x8101\x42D3\xD9E7\xD4E3\x259E";
        const wchar_t* VerminHides = L"\x5707\xF70E\xCAA2\x5CC5";
        const wchar_t* RoaringEtherHeart = L"\x8101\x52ED\x86E9\xCEF3\x69D3";
        const wchar_t* LeatheryClaws = L"\x2941";
        const wchar_t* AzureCrest = L"\x56FE\xF2B0\x8B62\x116A";
        const wchar_t* JotunPelt = L"\x8102\x26E2\xC8E7\x8B1F\x716A";
        const wchar_t* HeketTongues = L"\x8101\x583E\xE3F5\x87A2\x194F";
        const wchar_t* MountainTrollTusks = L"\x2951";
        const wchar_t* VialsofInk = L"\x22E7\xC1DA\xF2C1\x452A";
        const wchar_t* KournanPendants = L"\x8101\x43E9\xDBD0\xA0C6\x4AF1";
        const wchar_t* SingedGargoyleSkulls = L"\x293D";
        const wchar_t* DredgeIncisors = L"\x56E4\xDF8C\xAD76\x3958";
        const wchar_t* StoneSummitBadges = L"\x2955";
        const wchar_t* KraitSkins = L"\x8102\x26F4\xE764\xC908\x52E2";
        const wchar_t* InscribedShards = L"\x8101\x43D0\x843D\x98D1\x775C";
        const wchar_t* FeatheredScalps = L"\x56F6\xB464\x9A9E\x11EF";
        const wchar_t* MummyWrappings = L"\x8101\x43EB\xF92D\xD469\x73A8";
        const wchar_t* ShadowyRemnants = L"\x2916";
        const wchar_t* AncientKappaShells = L"\x570B\xFE7B\xBD8A\x7CF4";
        const wchar_t* Geode = L"\x8101\x5206\x8286\xFEFA\x191C";
        const wchar_t* FibrousMandragorRoots = L"\x8102\x26E8\xC0DB\xD26E\x4711";
        const wchar_t* GruesomeRibcages = L"\x293F";
        const wchar_t* KrakenEyes = L"\x56FD\xC65F\xF6F1\x26B4";
        const wchar_t* BogSkaleFins = L"\x2918";
        const wchar_t* SentientSpores = L"\x8101\x583D\xB904\xF476\x59A7";
        const wchar_t* AncientEyes = L"\x292D";
        const wchar_t* CopperShillings = L"\x8101\x43E4\x8D6E\x83E5\x4C07";
        const wchar_t* FrigidMandragorHusks = L"\x8102\x26DF\xF8E8\x8ACB\x58B4";
        const wchar_t* BoltsofLinen = L"\x22D5\x8371\x8ED5\x56B4";
        const wchar_t* CharrCarvings = L"\x28EE";
    }
}

namespace {
    std::vector<NicItemModule::NicItem*> nic_item_names;
}

const NicItemModule::NicItem* NicItemModule::GetNicItem(uint32_t item_id) {
    const auto item = GW::Items::GetItemById(item_id);
    if (!item) {
        return nullptr;
    }
    for (const auto& nic_item : nic_item_names) {
        if (wcsncmp(nic_item->enc_name, item->name_enc, wcsnlen(nic_item->enc_name, 256)) == 0) {
            return nic_item;
        }
    }
    return nullptr;
}

bool NicItemModule::IsNicItem(uint32_t item_id) {
    return GetNicItem(item_id);
}

void NicItemModule::Initialize()
{
    GW::GameThread::Enqueue([]() {
        nic_item_names.push_back(new NicItem(GW::EncStrings::RedIrisFlowers));
        nic_item_names.push_back(new NicItem(GW::EncStrings::FeatheredAvicaraScalps));
        nic_item_names.push_back(new NicItem(GW::EncStrings::MargoniteMasks));
        nic_item_names.push_back(new NicItem(GW::EncStrings::QuetzalCrests));
        nic_item_names.push_back(new NicItem(GW::EncStrings::PlagueIdols));
        nic_item_names.push_back(new NicItem(GW::EncStrings::AzureRemains));
        nic_item_names.push_back(new NicItem(GW::EncStrings::MandragorRootCake));
        nic_item_names.push_back(new NicItem(GW::EncStrings::MahgoClaw));
        nic_item_names.push_back(new NicItem(GW::EncStrings::MantidPincers));
        nic_item_names.push_back(new NicItem(GW::EncStrings::SentientSeeds));
        nic_item_names.push_back(new NicItem(GW::EncStrings::StoneGrawlNecklaces));
        nic_item_names.push_back(new NicItem(GW::EncStrings::Herring));
        nic_item_names.push_back(new NicItem(GW::EncStrings::NagaSkins));
        nic_item_names.push_back(new NicItem(GW::EncStrings::GloomSeed));
        nic_item_names.push_back(new NicItem(GW::EncStrings::CharrHide));
        nic_item_names.push_back(new NicItem(GW::EncStrings::RubyDjinnEssence));
        nic_item_names.push_back(new NicItem(GW::EncStrings::ThornyCarapaces));
        nic_item_names.push_back(new NicItem(GW::EncStrings::BoneCharms));
        nic_item_names.push_back(new NicItem(GW::EncStrings::ModniirManes));
        nic_item_names.push_back(new NicItem(GW::EncStrings::SuperbCharrCarvings));
        nic_item_names.push_back(new NicItem(GW::EncStrings::RollsofParchment));
        nic_item_names.push_back(new NicItem(GW::EncStrings::RoaringEtherClaws));
        nic_item_names.push_back(new NicItem(GW::EncStrings::BranchesofJuniBerries));
        nic_item_names.push_back(new NicItem(GW::EncStrings::ShiverpeakManes));
        nic_item_names.push_back(new NicItem(GW::EncStrings::FetidCarapaces));
        nic_item_names.push_back(new NicItem(GW::EncStrings::MoonShells));
        nic_item_names.push_back(new NicItem(GW::EncStrings::MassiveJawbone));
        nic_item_names.push_back(new NicItem(GW::EncStrings::ChromaticScale));
        nic_item_names.push_back(new NicItem(GW::EncStrings::MursaatTokens));
        nic_item_names.push_back(new NicItem(GW::EncStrings::SentientLodestone));
        nic_item_names.push_back(new NicItem(GW::EncStrings::JungleTrollTusks));
        nic_item_names.push_back(new NicItem(GW::EncStrings::SapphireDjinnEssence));
        nic_item_names.push_back(new NicItem(GW::EncStrings::StoneCarving));
        nic_item_names.push_back(new NicItem(GW::EncStrings::FeatheredCaromiScalps));
        nic_item_names.push_back(new NicItem(GW::EncStrings::PillagedGoods));
        nic_item_names.push_back(new NicItem(GW::EncStrings::GoldCrimsonSkullCoin));
        nic_item_names.push_back(new NicItem(GW::EncStrings::JadeBracelets));
        nic_item_names.push_back(new NicItem(GW::EncStrings::MinotaurHorns));
        nic_item_names.push_back(new NicItem(GW::EncStrings::FrostedGriffonWings));
        nic_item_names.push_back(new NicItem(GW::EncStrings::SilverBullionCoins));
        nic_item_names.push_back(new NicItem(GW::EncStrings::Truffle));
        nic_item_names.push_back(new NicItem(GW::EncStrings::SkelkClaws));
        nic_item_names.push_back(new NicItem(GW::EncStrings::DessicatedHydraClaws));
        nic_item_names.push_back(new NicItem(GW::EncStrings::FrigidHearts));
        nic_item_names.push_back(new NicItem(GW::EncStrings::CelestialEssences));
        nic_item_names.push_back(new NicItem(GW::EncStrings::PhantomResidue));
        nic_item_names.push_back(new NicItem(GW::EncStrings::DrakeKabob));
        nic_item_names.push_back(new NicItem(GW::EncStrings::AmberChunks));
        nic_item_names.push_back(new NicItem(GW::EncStrings::GlowingHearts));
        nic_item_names.push_back(new NicItem(GW::EncStrings::SaurianBones));
        nic_item_names.push_back(new NicItem(GW::EncStrings::BehemothHides));
        nic_item_names.push_back(new NicItem(GW::EncStrings::LuminousStone));
        nic_item_names.push_back(new NicItem(GW::EncStrings::IntricateGrawlNecklaces));
        nic_item_names.push_back(new NicItem(GW::EncStrings::JadeiteShards));
        nic_item_names.push_back(new NicItem(GW::EncStrings::GoldDoubloon));
        nic_item_names.push_back(new NicItem(GW::EncStrings::ShriveledEyes));
        nic_item_names.push_back(new NicItem(GW::EncStrings::IcyLodestones));
        nic_item_names.push_back(new NicItem(GW::EncStrings::KeenOniTalon));
        nic_item_names.push_back(new NicItem(GW::EncStrings::HardenedHumps));
        nic_item_names.push_back(new NicItem(GW::EncStrings::PilesofElementalDust));
        nic_item_names.push_back(new NicItem(GW::EncStrings::NagaHides));
        nic_item_names.push_back(new NicItem(GW::EncStrings::SpiritwoodPlanks));
        nic_item_names.push_back(new NicItem(GW::EncStrings::StormyEye));
        nic_item_names.push_back(new NicItem(GW::EncStrings::SkreeWings));
        nic_item_names.push_back(new NicItem(GW::EncStrings::SoulStones));
        nic_item_names.push_back(new NicItem(GW::EncStrings::SpikedCrest));
        nic_item_names.push_back(new NicItem(GW::EncStrings::DragonRoot));
        nic_item_names.push_back(new NicItem(GW::EncStrings::BerserkerHorns));
        nic_item_names.push_back(new NicItem(GW::EncStrings::BehemothJaw));
        nic_item_names.push_back(new NicItem(GW::EncStrings::BowlofSkalefinSoup));
        nic_item_names.push_back(new NicItem(GW::EncStrings::ForestMinotaurHorns));
        nic_item_names.push_back(new NicItem(GW::EncStrings::PutridCysts));
        nic_item_names.push_back(new NicItem(GW::EncStrings::JadeMandibles));
        nic_item_names.push_back(new NicItem(GW::EncStrings::MaguumaManes));
        nic_item_names.push_back(new NicItem(GW::EncStrings::SkullJuju));
        nic_item_names.push_back(new NicItem(GW::EncStrings::MandragorSwamproots));
        nic_item_names.push_back(new NicItem(GW::EncStrings::BottleofVabbianWine));
        nic_item_names.push_back(new NicItem(GW::EncStrings::WeaverLegs));
        nic_item_names.push_back(new NicItem(GW::EncStrings::TopazCrest));
        nic_item_names.push_back(new NicItem(GW::EncStrings::RotWallowTusks));
        nic_item_names.push_back(new NicItem(GW::EncStrings::FrostfireFangs));
        nic_item_names.push_back(new NicItem(GW::EncStrings::DemonicRelic));
        nic_item_names.push_back(new NicItem(GW::EncStrings::AbnormalSeeds));
        nic_item_names.push_back(new NicItem(GW::EncStrings::DiamondDjinnEssence));
        nic_item_names.push_back(new NicItem(GW::EncStrings::ForgottenSeals));
        nic_item_names.push_back(new NicItem(GW::EncStrings::CopperCrimsonSkullCoins));
        nic_item_names.push_back(new NicItem(GW::EncStrings::MossyMandibles));
        nic_item_names.push_back(new NicItem(GW::EncStrings::EnslavementStones));
        nic_item_names.push_back(new NicItem(GW::EncStrings::ElonianLeatherSquares));
        nic_item_names.push_back(new NicItem(GW::EncStrings::CobaltTalons));
        nic_item_names.push_back(new NicItem(GW::EncStrings::MaguumaSpiderWeb));
        nic_item_names.push_back(new NicItem(GW::EncStrings::ForgottenTrinketBoxes));
        nic_item_names.push_back(new NicItem(GW::EncStrings::IcyHumps));
        nic_item_names.push_back(new NicItem(GW::EncStrings::SandblastedLodestone));
        nic_item_names.push_back(new NicItem(GW::EncStrings::BlackPearls));
        nic_item_names.push_back(new NicItem(GW::EncStrings::InsectCarapaces));
        nic_item_names.push_back(new NicItem(GW::EncStrings::MergoyleSkulls));
        nic_item_names.push_back(new NicItem(GW::EncStrings::DecayedOrrEmblems));
        nic_item_names.push_back(new NicItem(GW::EncStrings::TemperedGlassVials));
        nic_item_names.push_back(new NicItem(GW::EncStrings::ScorchedLodestones));
        nic_item_names.push_back(new NicItem(GW::EncStrings::WaterDjinnEssence));
        nic_item_names.push_back(new NicItem(GW::EncStrings::GuardianMoss));
        nic_item_names.push_back(new NicItem(GW::EncStrings::DwarvenAles));
        nic_item_names.push_back(new NicItem(GW::EncStrings::AmphibianTongues));
        nic_item_names.push_back(new NicItem(GW::EncStrings::AlpineSeeds));
        nic_item_names.push_back(new NicItem(GW::EncStrings::TangledSeeds));
        nic_item_names.push_back(new NicItem(GW::EncStrings::StolenSupplies));
        nic_item_names.push_back(new NicItem(GW::EncStrings::PahnaiSalad));
        nic_item_names.push_back(new NicItem(GW::EncStrings::VerminHides));
        nic_item_names.push_back(new NicItem(GW::EncStrings::RoaringEtherHeart));
        nic_item_names.push_back(new NicItem(GW::EncStrings::LeatheryClaws));
        nic_item_names.push_back(new NicItem(GW::EncStrings::AzureCrest));
        nic_item_names.push_back(new NicItem(GW::EncStrings::JotunPelt));
        nic_item_names.push_back(new NicItem(GW::EncStrings::HeketTongues));
        nic_item_names.push_back(new NicItem(GW::EncStrings::MountainTrollTusks));
        nic_item_names.push_back(new NicItem(GW::EncStrings::VialsofInk));
        nic_item_names.push_back(new NicItem(GW::EncStrings::KournanPendants));
        nic_item_names.push_back(new NicItem(GW::EncStrings::SingedGargoyleSkulls));
        nic_item_names.push_back(new NicItem(GW::EncStrings::DredgeIncisors));
        nic_item_names.push_back(new NicItem(GW::EncStrings::StoneSummitBadges));
        nic_item_names.push_back(new NicItem(GW::EncStrings::KraitSkins));
        nic_item_names.push_back(new NicItem(GW::EncStrings::InscribedShards));
        nic_item_names.push_back(new NicItem(GW::EncStrings::FeatheredScalps));
        nic_item_names.push_back(new NicItem(GW::EncStrings::MummyWrappings));
        nic_item_names.push_back(new NicItem(GW::EncStrings::ShadowyRemnants));
        nic_item_names.push_back(new NicItem(GW::EncStrings::AncientKappaShells));
        nic_item_names.push_back(new NicItem(GW::EncStrings::Geode));
        nic_item_names.push_back(new NicItem(GW::EncStrings::FibrousMandragorRoots));
        nic_item_names.push_back(new NicItem(GW::EncStrings::GruesomeRibcages));
        nic_item_names.push_back(new NicItem(GW::EncStrings::KrakenEyes));
        nic_item_names.push_back(new NicItem(GW::EncStrings::BogSkaleFins));
        nic_item_names.push_back(new NicItem(GW::EncStrings::SentientSpores));
        nic_item_names.push_back(new NicItem(GW::EncStrings::AncientEyes));
        nic_item_names.push_back(new NicItem(GW::EncStrings::CopperShillings));
        nic_item_names.push_back(new NicItem(GW::EncStrings::FrigidMandragorHusks));
        nic_item_names.push_back(new NicItem(GW::EncStrings::BoltsofLinen));
        nic_item_names.push_back(new NicItem(GW::EncStrings::CharrCarvings));
    });
}

void NicItemModule::Terminate()
{
    for (auto m : nic_item_names) {
        delete m;
    }
    nic_item_names.clear();
    ToolboxModule::Terminate();
}
