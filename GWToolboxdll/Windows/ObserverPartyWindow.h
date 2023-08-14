#pragma once

#include <ToolboxWindow.h>

class ObserverPartyWindow : public ToolboxWindow {
    ObserverPartyWindow() = default;
    ~ObserverPartyWindow() override = default;

public:
    static ObserverPartyWindow& Instance()
    {
        static ObserverPartyWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Observer Parties"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_EYE; }
    void Draw(IDirect3DDevice9* pDevice) override;
    void Initialize() override;

    void DrawBlankPartyMember(float& offset) const;
    void DrawPartyMember(float& offset, ObserverModule::ObservableAgent& agent, const ObserverModule::ObservableGuild* guild,
                         bool odd, bool is_player, bool is_target) const;
    void DrawParty(float& offset, const ObserverModule::ObservableParty& party) const;
    void DrawHeaders(size_t party_count) const;

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;

protected:
    float text_long = 0;
    float text_medium = 0;
    float text_short = 0;
    float text_tiny = 0;


    bool show_player_number = true;
    bool show_profession = true;
    bool show_player_guild_tag = true;
    bool show_player_guild_rating = false;
    bool show_player_guild_rank = false;
    bool show_kills = true;
    bool show_deaths = true;
    bool show_kdr = true;
    bool show_cancels = true;
    bool show_interrupts = true;
    bool show_knockdowns = true;
    bool show_received_party_attacks = true;
    bool show_dealt_party_attacks = true;
    bool show_received_party_crits = true;
    bool show_dealt_party_crits = true;
    bool show_received_party_skills = true;
    bool show_dealt_party_skills = true;
    bool show_skills_used = true;

private:
    // ini
    ToolboxIni* inifile = nullptr;
};
