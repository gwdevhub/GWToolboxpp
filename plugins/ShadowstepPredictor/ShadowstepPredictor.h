#pragma once

#include <ToolboxUIPlugin.h>

#include <GWCA/Constants/Skills.h>

#include <Pathing.h>

#include <IconsFontAwesome5.h>

#include <unordered_map>

class ShadowstepPredictor : public ToolboxUIPlugin {
public:
    ShadowstepPredictor()
    {
        show_closebutton = false;
        show_title = false;
        can_show_in_main_window = false;
        can_collapse = false;
        can_close = false;
        is_resizable = false;
        is_movable = false;
        lock_move = false;
        lock_size = false;
        show_menubutton = false;
    }
    ~ShadowstepPredictor() override = default;

    const char* Name() const override { return "Shadowstep Predictor"; }
    const char* Icon() const override { return ICON_FA_BEZIER_CURVE; }

    void Draw(IDirect3DDevice9* pDevice) override;
    void Update(float) override;

    void LoadSettings(const wchar_t*) override;
    void SaveSettings(const wchar_t*) override;
    void DrawSettings() override;

    void Initialize(ImGuiContext* ctx, ImGuiAllocFns allocator_fns, HMODULE toolbox_dll) override;
    void SignalTerminate() override;

private:
    std::unordered_map<GW::Constants::SkillID, OutcomeChances> chances;
    
    PathPoint shadowOfHasteLocation{};
    PathPoint shadowWalkLocation{};
    GW::AgentID recallTargetId{};

    ImVec4 successColor{0.f, 1.f, 0.f, 0.5f};
    ImVec4 failureColor{1.f, 0.f, 0.f, 0.67f};
    ImVec4 partialColor{1.f, 1.f, 0.f, 0.67f};

    bool showRecall = true;
    bool showSoH = true;
    bool showShadowWalk = true;
    bool showAllyShadowSteps = false;
    bool showEnemyShadowSteps = false;
};
