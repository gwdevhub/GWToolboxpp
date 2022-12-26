#pragma once

#include <map>
#include <queue>
#include <string>

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Packets/StoC.h>

#include <Timer.h>
#include <ToolboxWindow.h>

class PartyStatisticsWindow : public ToolboxWindow {
public:

    static PartyStatisticsWindow& Instance() {
        static PartyStatisticsWindow instance;
        return instance;
    }

    const char* Name() const override { return "Party Statistics"; }
    const char* Icon() const override { return ICON_FA_TABLE; }

    void Initialize() override;
    void Terminate() override;

    void Draw(IDirect3DDevice9* pDevice) override;
    void Update(float delta) override;

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingInternal() override;
};
