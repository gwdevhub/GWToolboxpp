#pragma once

#include <ToolboxWindow.h>

class PartySearchWindow : public ToolboxWindow {
public:
    PartySearchWindow(const PartySearchWindow&) = delete;

private:
    PartySearchWindow() = default;

    ~PartySearchWindow() override;

public:
    static PartySearchWindow& Instance()
    {
        static PartySearchWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Party Search"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_PEOPLE_ARROWS; }

    struct Settings {
        bool print_game_chat = false;
        bool filter_alerts = false;
    };

    void Initialize() override;

    void Update(float delta) override;
    void Draw(IDirect3DDevice9* pDevice) override;
    void SignalTerminate() override;

    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void DrawSettingsInternal() override;

};
