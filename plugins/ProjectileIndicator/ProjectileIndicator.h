#pragma once

#include <ToolboxUIPlugin.h>

#include <GWCA/Utilities/Hook.h>
#include <GWCA/Constants/Skills.h>
#include <GWCA/Constants/Constants.h>

#include <IconsFontAwesome5.h>

class ProjectileIndicator : public ToolboxUIPlugin {
public:
    ProjectileIndicator() = default;
    ~ProjectileIndicator() override = default;

    const char* Name() const override { return "ProjectileIndicator"; }
    const char* Icon() const override { return ICON_FA_CIRCLE; }

    void Initialize(ImGuiContext* ctx, ImGuiAllocFns allocator_fns, HMODULE toolbox_dll) override;
    void SignalTerminate() override;
    void Terminate() override;

    void DrawSettings() override;
    void LoadSettings(const wchar_t*) override;
    void SaveSettings(const wchar_t*) override;

    void Draw(IDirect3DDevice9*) override;

private:
    bool filled = true;
    ImVec4 color = {.557f, 0.f, .627f, .353f};
    int projectileTimer = 1000;
    std::vector<GW::Constants::SkillID> trackedSkills = {};
    std::vector<int> trackedEnemyModels = {};
    std::vector<int> suppressedProjecitileAnimationSources = {};
};
