#pragma once
#include <GWCA/Constants/Constants.h>

#include <GWCA/Utilities/Hook.h>
#include <GWCA/GameEntities/Agent.h>

#include <ToolboxModule.h>
#include <ToolboxUIElement.h>

typedef uint32_t HomPoints;

enum class ResiliencePoints : HomPoints {
    AnyArmorStatue,
    ThreeArmorStatues,
    FiveArmorStatues,
    SevenArmorStatues,
    KurzickOrLuxonArmorStatue,
    VabbianArmorStatue,
    ObsidianArmorStatue,
    Count,
    TotalAvailable = 8
};
enum class ResilienceDetail {
    EliteCanthanArmor,
    EliteExoticArmor,
    EliteKurzickArmor,
    EliteLuxonArmor,
    ImperialAscendedArmor,
    AncientArmor,
    EliteSunspearArmor,
    VabbianArmor,
    PrimevalArmor,
    AsuranArmor,
    NornArmor,
    SilverEagleArmor,
    MonumentArmor,
    ObsidianArmor,
    GraniteCitadelEliteArmor,
    GraniteCitadelExclusiveArmor,
    GraniteCitadelAscendedArmor,
    MarhansGrottoEliteArmor,
    MarhansGrottoExclusiveArmor,
    MarhansGrottoAscendedArmor,
    Count
};
enum class FellowshipPoints : HomPoints {
    AnyHeroStatue,
    AnyPetStatue,
    AnyRarePetStatue,
    FiveCompanionStatues,
    TenCompanionStatues,
    TwentyCompanionStatues,
    ThirtyCompanionStatues,
    Count,
    TotalAvailable = 8
};
enum class FellowshipDetail {
    Zenmai,
    Norgu,
    Goren,
    ZhedShadowhoof,
    GeneralMorgahn,
    MargridTheSly,
    Tahlkora,
    Razah,
    MasterOfWhispers,
    Koss,
    Dunkoro,
    Melonni,
    AcolyteJin,
    AcolyteSousuke,
    Vekk,
    Livia,
    Hayda,
    OgdenStonehealer,
    PyreFierceshot,
    Jora,
    Kahmu,
    Xandra,
    Anton,
    Gwen,
    AnimalCompanion,
    BlackMoa,
    ImperialPhoenix,
    BlackWidowSpider,
    Olias,
    MOX,
    Count
};
enum class HonorPoints : HomPoints {
    AccountsLinked,
    AnyStatue,
    OnePvpStatue,
    FiveStatues,
    TenStatues,
    FifteenStatues,
    TwentyStatues,
    TwentyFiveStatues,
    ThirtyStatues,
    ThirtyFiveStatues,
    FourtyStatues,
    Count,
    TotalAvailable = 18
};
enum class HonorDetail {
    EternalChampion,
    EternalCommander,
    EternalSkillz,
    EternalGladiator,
    EternalHero,
    EternalLightbringer,
    EternalBookah,
    EternalDelver,
    EternalSlayer,
    EternalEbonVanguardAgent,
    EternalDefenderOfAscalon,
    EternalTyrianCartographer,
    EternalGuardianOfTyria,
    EternalProtectorOfTyria,
    EternalTyrianSkillHunter,
    EternalTyrianVanquisher,
    EternalCanthanCartographer,
    EternalGuardianOfCantha,
    EternalProtectorOfCantha,
    EternalCanthanSkillHunter,
    EternalCanthanVanquisher,
    EternalSaviorOfTheKurzicks,
    EternalSaviorOfTheLuxons,
    EternalElonionCartographer,
    EternalGuardianOfElona,
    EternalProtectorOfElona,
    EternalElonianSkillHunter,
    EternalElonianVanquisher,
    EternalAleHound,
    EternalPartyAnimal,
    EternalMasterOfTheNorth,
    EternalLegendaryCartographer,
    EternalLegendaryGuardian,
    EternalLegendarySkillHunter,
    EternalLegendaryVanquisher,
    EternalFortune,
    EternalSweetTooth,
    EternalSpearmarshal,
    EternalSurvivor,
    Unused,
    EternalTreasureHunter,
    EternalMisfortune,
    EternalSourceOfWisdom,
    EternalHeroOfTyria,
    EternalHeroOfCantha,
    EternalHeroOfElona,
    EternalConquerorOfSorrowsFurnace,
    EternalConquerorOfTheDeep,
    EternalConquerorOfUrgozsWarren,
    EternalConquerorOfTheFissureOfWoe,
    EternalConquerorOfTheUnderworld,
    EternalConquerorOfTheDomainOfAnguish,
    EternalZaishenSupporter,
    EternalCodexDisciple,
    Count
};
enum class ValorPoints : HomPoints {
    AnyWeaponStatue,
    DestroyerWeaponStatue,
    TormentedWeaponStatue,
    OppressorWeaponStatue,
    FiveWeaponStatues,
    ElevenWeaponStatues,
    FifteenWeaponStatues,
    Count,
    TotalAvailable = 8
};
enum class ValorDetail {
    DestroyerAxe,
    DestroyerBow,
    DestroyerDaggers,
    DestroyerFocus,
    DestroyerMaul,
    DestroyerScepter,
    DestroyerScythe,
    DestroyerShield,
    DestroyerSpear,
    DestroyerStaff,
    DestroyerSword,
    TormentedAxe,
    TormentedBow,
    TormentedDaggers,
    TormentedFocus,
    TormentedMaul,
    TormentedScepter,
    TormentedScythe,
    TormentedShield,
    TormentedSpear,
    TormentedStaff,
    TormentedSword,
    OppressorAxe,
    OppressorBow,
    OppressorDaggers,
    OppressorFocus,
    OppressorMaul,
    OppressorScepter,
    OppressorScythe,
    OppressorShield,
    OppressorSpear,
    OppressorStaff,
    OppressorSword,
    Count
};
enum class DevotionPoints : HomPoints {
    AnyMiniatureStatue,
    RareMiniatureStatue,
    UniqueMiniatureStatue,
    TwentyMiniatureStatues,
    ThirtyMiniatureStatues,
    FourtyMiniatureStatues,
    FiftyMiniatureStatues,
    Count,
    TotalAvailable = 8
};
enum class DevotionDetail {
    Common,
    Uncommon,
    Rare,
    Unique,
    Count
};
struct HallOfMonumentsAchievements {
    std::wstring character_name;
    enum class State {
        Pending,
        Loading,
        Error,
        Done
    } state = State::Pending;
    bool isReady() const { return state == State::Done;  }
    bool isPending() const { return state == State::Pending; }
    bool isLoading() const { return state == State::Loading; }
    char hom_code[128] = { 0 };
    void OpenInBrowser();
    // Details of which armors have or haven't been dedicated, indexed by ResilienceDetail
    bool resilience_detail[(size_t)ResilienceDetail::Count] = { 0 };
    // Details of which points have or haven't been earnt, indexed by ResiliencePoints
    uint32_t resilience_points[(size_t)ResiliencePoints::Count] = { 0 };
    // Total sum of armors dedicated
    uint32_t resilience_tally = 0;
    // Total sum of points achieved in resilience
    uint32_t resilience_points_total = 0;

    // Details of which companions have or haven't been dedicated, indexed by FellowshipDetail
    bool fellowship_detail[(size_t)FellowshipDetail::Count] = { 0 };
    // Details of which points have or haven't been earnt, indexed by FellowshipPoints
    uint32_t fellowship_points[(size_t)FellowshipPoints::Count] = { 0 };
    // Total sum of companions dedicated
    uint32_t fellowship_tally = 0;
    // Total sum of points achieved in fellowship
    uint32_t fellowship_points_total = 0;

    // Details of which titles have or haven't been dedicated, indexed by HonorDetail
    bool honor_detail[(size_t)HonorDetail::Count] = { 0 };
    // Details of which points have or haven't been earnt, indexed by HonorPoints
    uint32_t honor_points[(size_t)HonorPoints::Count] = { 0 };
    // Total sum of titles dedicated
    uint32_t honor_tally = 0;
    // Total sum of points achieved in honor
    uint32_t honor_points_total = 0;

    // Details of which weapons have or haven't been dedicated, indexed by ValorDetail
    bool valor_detail[(size_t)ValorDetail::Count] = { 0 };
    // Details of which points have or haven't been earnt, indexed by ValorPoints
    uint32_t valor_points[(size_t)ValorPoints::Count] = { 0 };
    // Total sum of weapons dedicated
    uint32_t valor_tally = 0;
    // Total sum of points achieved in valor
    uint32_t valor_points_total = 0;

    // Details of how many different types of minipet have or haven't been dedicated, indexed by DevotionDetail
    uint32_t devotion_detail[(size_t)DevotionDetail::Count] = { 0 };
    // Details of which points have or haven't been earnt, indexed by DevotionPoints
    uint32_t devotion_points[(size_t)DevotionPoints::Count] = { 0 };
    // Total sum of minipets dedicated
    uint32_t devotion_tally = 0;
    // Total sum of points achieved in devotion
    uint32_t devotion_points_total = 0;
};

typedef void(OnAchievementsLoadedCallback)(HallOfMonumentsAchievements* result);

class HallOfMonumentsModule : public ToolboxModule {
    HallOfMonumentsModule() = default;
public:
    static HallOfMonumentsModule& Instance() {
        static HallOfMonumentsModule instance;
        return instance;
    }

    const char* Name() const override { return "Hall of Monuments"; }

    bool HasSettings() override { return false; }

    const char* GetDevotionPointsDescription(DevotionPoints id) {
        switch (id) {
        case DevotionPoints::AnyMiniatureStatue: return "Any Miniature Statue";
        case DevotionPoints::RareMiniatureStatue: return "Rare Miniature Statue";
        case DevotionPoints::TwentyMiniatureStatues: return "20 Miniature Statues";
        case DevotionPoints::ThirtyMiniatureStatues: return "30 Miniature Statues";
        case DevotionPoints::FourtyMiniatureStatues: return "40 Miniature Statues";
        }
        return "";
    }

    // Decode a zero terminated base64 encoded hom code
    bool DecodeHomCode(const char* in, HallOfMonumentsAchievements* out);
    // Decode a zero terminated base64 encoded hom code
    bool DecodeHomCode(HallOfMonumentsAchievements* out);
    // Get the account achiemenets for the current player
    static void AsyncGetAccountAchievements(const std::wstring& character_name, HallOfMonumentsAchievements* out, OnAchievementsLoadedCallback = nullptr);
};
