
#pragma once

#include <GWCA\Constants\Constants.h>

#include <GWCA\GameContainers\Array.h>

#include <GWCA\GameEntities\Skill.h>
#include "GWCA\Managers\UIMgr.h"

#include <Utils/GuiUtils.h>
#include <Timer.h>
#include <ToolboxWindow.h>

#include <Windows/ObserverPlayerWindow.h>

#define NO_AGENT 0

class ObserverTargetWindow : public ObserverPlayerWindow {
    ObserverTargetWindow() = default;
    ~ObserverTargetWindow() = default;

public:
    static ObserverTargetWindow& Instance() {
        static ObserverTargetWindow instance;
        return instance;
    }

    void Prepare() override;
    uint32_t GetTracking() override;
    uint32_t GetComparison() override;

    const char* Name() const override { return "Observer Target"; }
    const char8_t* Icon() const override { return ICON_FA_EYE; }

protected:
    uint32_t current_tracked_agent_id = NO_AGENT;
    uint32_t current_compared_agent_id = NO_AGENT;
};
