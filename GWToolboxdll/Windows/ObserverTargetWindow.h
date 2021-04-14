
#pragma once

#include <GWCA\Constants\Constants.h>

#include <GWCA\GameContainers\Array.h>

#include <GWCA\GameEntities\Skill.h>
#include "GWCA\Managers\UIMgr.h"

#include <GuiUtils.h>
#include <Timer.h>
#include <ToolboxWindow.h>

#define NO_AGENT 0

class ObserverTargetWindow : public ToolboxWindow {
public:
    //

private:
    ObserverTargetWindow() {};
    ~ObserverTargetWindow() {
        //
    };

public:
    static ObserverTargetWindow& Instance() {
        static ObserverTargetWindow instance;
        return instance;
    }

    const char* Name() const override { return "Target"; }
    void Draw(IDirect3DDevice9* pDevice) override;
    void Initialize() override;

    void DrawSkillHeaders(float _long, float _mid, float _small, float _tiny);
    void DrawSkills(float _long, float _mid, float _small, float _tiny, const std::vector<uint32_t>& skill_ids,
        const std::unordered_map<uint32_t, ObserverModule::ObservedSkill*>& skils);

private:
    uint32_t last_target_id = NO_AGENT;
    uint32_t last_player_id = NO_AGENT;
};
