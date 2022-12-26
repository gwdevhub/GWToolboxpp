#pragma once

#include <GWCA/Constants/Constants.h>

#include <ToolboxWindow.h>

#define NO_AGENT 0

class ObserverPlayerWindow : public ToolboxWindow {
protected:
    ObserverPlayerWindow() = default;
    ~ObserverPlayerWindow() = default;

public:
    static ObserverPlayerWindow& Instance() {
        static ObserverPlayerWindow instance;
        return instance;
    }

    virtual void Prepare() {}
    virtual uint32_t GetTracking();
    virtual uint32_t GetComparison();

    void DrawHeaders() const;
    void DrawAction(const std::string& name, const ObserverModule::ObservedAction* action);

    void DrawSkills(const std::unordered_map<GW::Constants::SkillID, ObserverModule::ObservedSkill*>& skills,
        const std::vector<GW::Constants::SkillID>& skill_ids);

    const char* Name() const override { return "Observer Player"; }
    const char* Icon() const override { return ICON_FA_EYE; }
    void Draw(IDirect3DDevice9* pDevice) override;
    void Initialize() override;

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
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

    bool show_attempts = false;
    bool show_cancels = true;
    bool show_interrupts = true;
    bool show_finishes = true;
    bool show_integrity = false;
};
