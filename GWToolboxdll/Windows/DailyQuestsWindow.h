#pragma once
#include <GWCA/Constants/Constants.h>
#include <ToolboxWindow.h>
namespace GW::Constants {
    enum class QuestID : uint32_t;
}
class DailyQuests : public ToolboxWindow {
    ~DailyQuests() override = default;

public:
    static DailyQuests& Instance()
    {
        static DailyQuests instance;
        return instance;
    }
    DailyQuests() { show_menubutton = can_show_in_main_window; }
    [[nodiscard]] const char* Name() const override { return "Daily Quests"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_CALENDAR_ALT; }
    void Initialize() override;
    void Terminate() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;
    void DrawHelp() override;
    void Update(float delta) override;
    void Draw(IDirect3DDevice9* pDevice) override;

public:
    class QuestData {
    protected:
        GuiUtils::EncString* name_english = nullptr;
        GuiUtils::EncString* name_translated = nullptr;

    public:
        const GW::Constants::MapID map_id;
        std::wstring enc_name;
        QuestData(GW::Constants::MapID map_id = (GW::Constants::MapID)0, const wchar_t* enc_name = nullptr);
        // Assert that encoded strings are nulled. Because QuestData structs are static, this destructor is not guaranteed to run!!
        ~QuestData();
        const char* GetMapName();
        const std::string& GetRegionName();
        const char* GetQuestName();
        const wchar_t* GetQuestNameEnc();
        const std::wstring& GetWikiName();
        virtual const GW::Constants::MapID GetQuestGiverOutpost();
        void Travel();
        // Initialise encoded strings at runtime, allows us to code the daily quests arrays at compile time
        virtual void Decode(bool force = false);
        // Clear out any encoded strings to ensure any decoding errors are thrown within the lifecycle of the application
        void Terminate();
    };
    class NicholasCycleData : public QuestData {
    public:
        const uint32_t quantity;
        NicholasCycleData(const wchar_t* enc_name, uint32_t quantity, GW::Constants::MapID map_id);
        size_t GetCollectedQuantity();
        void Decode(bool force = false) override;
    };

    // Returned by all quest getters. Contains the quest data and the timestamp
    // at which this quest will be replaced by the next one in the rotation.
    struct DailyQuestResult {
        QuestData* quest;
        time_t next_rollover;
    };

    static DailyQuestResult GetZaishenBounty(time_t unix = 0);
    static DailyQuestResult GetZaishenVanquish(time_t unix = 0);
    static DailyQuestResult GetZaishenMission(time_t unix = 0);
    static DailyQuestResult GetZaishenCombat(time_t unix = 0);
    static DailyQuestResult GetNicholasTheTraveller(time_t unix = 0);
    static time_t GetTimestampFromNicholasTheTraveller(DailyQuests::NicholasCycleData* data);
    static DailyQuestResult GetNicholasSandford(time_t unix = 0);
    static DailyQuestResult GetVanguardQuest(time_t unix = 0);
    static DailyQuestResult GetWantedByShiningBlade(time_t unix = 0);
    static DailyQuestResult GetWeeklyPvEBonus(time_t unix = 0);
    static DailyQuestResult GetWeeklyPvPBonus(time_t unix = 0);
    // Returns info about the nicholas item if the given item name matches
    static NicholasCycleData* GetNicholasItemInfo(const wchar_t* item_name_encoded);
};
