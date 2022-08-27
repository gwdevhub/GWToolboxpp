
#pragma once

#include <GWCA\Constants\Constants.h>

#include <GWCA\GameContainers\Array.h>

#include <GWCA\GameEntities\Skill.h>
#include <GWCA\GameEntities\Agent.h>

#include <GWCA/Managers/AgentMgr.h>
#include "GWCA\Managers\UIMgr.h"

#include <Utils/GuiUtils.h>
#include <Timer.h>
#include <ToolboxWindow.h>

class ObserverExportWindow : public ToolboxWindow {
public:
    ObserverExportWindow() = default;
    ~ObserverExportWindow() = default;

    static ObserverExportWindow& Instance() {
        static ObserverExportWindow instance;
        return instance;
    }

    enum class Version {
        V_0_1,
        V_1_0
    };

    std::string PadLeft(std::string input, uint8_t count, char c);
    nlohmann::json ToJSON_V_0_1();
    nlohmann::json ToJSON_V_1_0();
    void ExportToJSON(Version version);

    const char* Name() const override { return "Observer Export"; };
    const char8_t* Icon() const override { return ICON_FA_EYE; }
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
