
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

#define NO_AGENT 0

class ObserverPlayerWindow : public ToolboxWindow {
public:
    ObserverPlayerWindow() {};
    ~ObserverPlayerWindow() {
        //
    };

    static ObserverPlayerWindow& Instance() {
        static ObserverPlayerWindow instance;
        return instance;
    }



    virtual void ObserverPlayerWindow::Prepare(){};
    virtual uint32_t ObserverPlayerWindow::GetTracking();
    virtual uint32_t ObserverPlayerWindow::GetComparison();

    void DrawSkillHeaders();

    void DrawSkills(const std::unordered_map<uint32_t, ObserverModule::ObservedSkill*>& skills,
        const std::vector<uint32_t>& skill_ids);

    const char* Name() const override { return "Observer Player"; };
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
    uint32_t previously_tracked_agent_id = NO_AGENT;
    uint32_t previously_compared_agent_id = NO_AGENT;

    bool show_tracking = true;
    bool show_comparison = true;
    bool show_skills_used_on_self = true;
};
