#pragma once

#include <GWCA/Utilities/Hook.h>

#include <Utils/GuiUtils.h>
#include <ToolboxWindow.h>

class FactionLeaderboardWindow : public ToolboxWindow {
    FactionLeaderboardWindow() = default;
    ~FactionLeaderboardWindow() override = default;


public:
    static FactionLeaderboardWindow& Instance()
    {
        static FactionLeaderboardWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Faction Leaderboard"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_GLOBE; }

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

};
