
#pragma once

#include <GWCA\Constants\Constants.h>

#include <GWCA\GameContainers\Array.h>

#include <GWCA\GameEntities\Skill.h>
#include "GWCA\Managers\UIMgr.h"

#include <GuiUtils.h>
#include <Timer.h>
#include <ToolboxWindow.h>

#define NO_AGENT 0

class ObserverPlayerWindow : public ToolboxWindow {
public:
    //

private:
    ObserverPlayerWindow() {};
    ~ObserverPlayerWindow() {
        //
    };

public:
    static ObserverPlayerWindow& Instance() {
        static ObserverPlayerWindow instance;
        return instance;
    }

    const char* Name() const override { return "Following"; }
    void Draw(IDirect3DDevice9* pDevice) override;
    void Initialize() override;

private:
    uint32_t last_player_id = NO_AGENT;
};
