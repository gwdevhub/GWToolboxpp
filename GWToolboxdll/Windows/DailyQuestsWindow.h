#pragma once

#include <GWCA/Constants/Constants.h>

#include <ToolboxWindow.h>

namespace GW::Constants {
    enum class QuestID : uint32_t;
}



class DailyQuests : public ToolboxWindow {
    DailyQuests() = default;
    ~DailyQuests() override = default;

public:
    static DailyQuests& Instance()
    {
        static DailyQuests instance;
        return instance;
    }

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
        // Initialise encoded strings at runtime, allows us to code the daily quests arrays at compile time
        virtual void Decode();
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

        // Clear out any encoded strings to ensure any decoding errors are thrown within the lifecycle of the application
        void Terminate();
    };

    class NicholasCycleData : public QuestData {
    protected:
        void Decode() override;
    public:
        const uint32_t quantity;
        NicholasCycleData(const wchar_t* enc_name, uint32_t quantity, GW::Constants::MapID map_id);
        size_t GetCollectedQuantity();
    };

    static QuestData* GetZaishenBounty(time_t unix = 0);
    static QuestData* GetZaishenVanquish(time_t unix = 0);
    static QuestData* GetZaishenMission(time_t unix = 0);
    static QuestData* GetZaishenCombat(time_t unix = 0);
    static NicholasCycleData* GetNicholasTheTraveller(time_t unix = 0);
    static time_t GetTimestampFromNicholasTheTraveller(DailyQuests::NicholasCycleData* data);

    static QuestData* GetNicholasSandford(time_t unix = 0);
    static QuestData* GetVanguardQuest(time_t unix = 0);
    static QuestData* GetWantedByShiningBlade(time_t unix = 0);

    static QuestData* GetWeeklyPvEBonus(time_t unix = 0);
    static QuestData* GetWeeklyPvPBonus(time_t unix = 0);

    // Returns info about the nicholas item if the given item name matches
    static NicholasCycleData* GetNicholasItemInfo(const wchar_t* item_name_encoded);
};
