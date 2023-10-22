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
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;

    void DrawHelp() override;
    void Update(float delta) override;
    void Draw(IDirect3DDevice9* pDevice) override;

    static constexpr size_t zb_cnt = 66;
    static constexpr size_t zm_cnt = 69;
    static constexpr size_t zc_cnt = 28;
    static constexpr size_t zv_cnt = 136;
    static constexpr size_t ws_cnt = 21;
    static constexpr size_t wbe_cnt = 9;
    static constexpr size_t wbp_cnt = 6;

private:
    bool subscribed_zaishen_bounties[zb_cnt] = {false};
    bool subscribed_zaishen_combats[zc_cnt] = {false};
    bool subscribed_zaishen_missions[zm_cnt] = {false};
    bool subscribed_zaishen_vanquishes[zv_cnt] = {false};
    bool subscribed_wanted_quests[ws_cnt] = {false};
    bool subscribed_weekly_bonus_pve[wbe_cnt] = {false};
    bool subscribed_weekly_bonus_pvp[wbp_cnt] = {false};

    bool show_zaishen_bounty_in_window = true;
    bool show_zaishen_combat_in_window = true;
    bool show_zaishen_missions_in_window = true;
    bool show_zaishen_vanquishes_in_window = true;
    bool show_wanted_quests_in_window = true;
    bool show_nicholas_in_window = true;
    bool show_weekly_bonus_pve_in_window = true;
    bool show_weekly_bonus_pvp_in_window = true;

    uint32_t subscriptions_lookahead_days = 7;

    float text_width = 200.0f;
    int daily_quest_window_count = 90;

    static void CmdWeeklyBonus(const wchar_t* message, int argc, const LPWSTR* argv);
    static void CmdWantedByShiningBlade(const wchar_t* message, int argc, const LPWSTR* argv);
    static void CmdZaishenBounty(const wchar_t* message, int argc, const LPWSTR* argv);
    static void CmdZaishenMission(const wchar_t* message, int argc, const LPWSTR* argv);
    static void CmdZaishenCombat(const wchar_t* message, int argc, const LPWSTR* argv);
    static void CmdZaishenVanquish(const wchar_t* message, int argc, const LPWSTR* argv);
    static void CmdVanguard(const wchar_t* message, int argc, const LPWSTR* argv);
    static void CmdNicholas(const wchar_t* message, int argc, const LPWSTR* argv);

    class ZaishenMission {
        ZaishenMission(GW::Constants::QuestID _quest_id, GW::Constants::MapID _map_id);
    };
};
