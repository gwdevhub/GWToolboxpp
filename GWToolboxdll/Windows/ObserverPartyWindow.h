#pragma once

#include <GWCA\Constants\Constants.h>

#include <GWCA\GameContainers\Array.h>

#include <GWCA\GameEntities\Skill.h>
#include "GWCA\Managers\UIMgr.h"

#include <GuiUtils.h>
#include <Timer.h>
#include <ToolboxWindow.h>

#define NO_AGENT 0

class ObserverPartyWindow : public ToolboxWindow {
public:
    //

private:
    ObserverPartyWindow() {};
    ~ObserverPartyWindow() {
        //
    };

public:
    static ObserverPartyWindow& Instance() {
        static ObserverPartyWindow instance;
        return instance;
    }

    const char* Name() const override { return "Parties"; }
    void Draw(IDirect3DDevice9* pDevice) override;
    void Initialize() override;

    void DrawBlankPartyMember(float& offset, const float _long, const float _tiny);
    void DrawPartyMember(float& offset, const float _long, const float _tiny, ObserverModule::ObservableAgent& agent, const bool odd);
    void DrawParty(float& offset, const float _long, const float _tiny, ObserverModule::ObservableParty& party);
    void DrawHeaders(const float _long, const float _tiny, const uint32_t party_count);

private:
    uint32_t last_living_target = NO_AGENT;
};
