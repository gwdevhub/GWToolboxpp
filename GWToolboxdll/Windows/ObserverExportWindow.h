
#pragma once

#include <GWCA\Constants\Constants.h>

#include <GWCA\GameContainers\Array.h>

#include <GWCA\GameEntities\Skill.h>
#include <GWCA\GameEntities\Agent.h>

#include <GWCA/Managers/AgentMgr.h>
#include "GWCA\Managers\UIMgr.h"

#include <GuiUtils.h>
#include <Timer.h>
#include <ToolboxWindow.h>

class ObserverExportWindow : public ToolboxWindow {
public:
    ObserverExportWindow() {};
    ~ObserverExportWindow() {
        //
    };

    static ObserverExportWindow& Instance() {
        static ObserverExportWindow instance;
        return instance;
    }

    nlohmann::json ToJSON();
    void ExportToJSON();

    nlohmann::json ObservableAgentToJSON(ObserverModule::ObservableAgent& agent);
    nlohmann::json SharedStatsToJSON(ObserverModule::SharedStats& stats);
    nlohmann::json ObservableAgentStatsToJSON(ObserverModule::ObservableAgentStats& stats);
    nlohmann::json ObservablePartyStatsToJSON(ObserverModule::ObservablePartyStats& stats);
    nlohmann::json ObservablePartyToJSON(ObserverModule::ObservableParty& party);
    nlohmann::json ObservableSkillToJSON(ObserverModule::ObservableSkill& skill);

    const char* Name() const override { return "Observer Export"; };
    const char* Icon() const override { return ICON_FA_EYE; }
    void Draw(IDirect3DDevice9* pDevice) override;
    void Initialize() override;

    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;
    void DrawSettingInternal() override;

protected:
    float text_long     = 0;
    float text_medium   = 0;
    float text_short    = 0;
    float text_tiny     = 0;
};
