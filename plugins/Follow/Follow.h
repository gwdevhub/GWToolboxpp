#pragma once

#include <ToolboxPlugin.h>

#include <GWCA/Utilities/Hook.h>

#include <IconsFontAwesome5.h>

class FollowPlugin : public ToolboxPlugin {
public:
    FollowPlugin() = default;
    ~FollowPlugin() override = default;

    const char* Name() const override { return "FollowPlugin"; }
    const char* Icon() const override { return ICON_FA_RUNNING; }

    void Update(float) override;

    void LoadSettings(const wchar_t*) override;
    void SaveSettings(const wchar_t*) override;
    void DrawSettings() override;
    bool HasSettings() const override { return true; }

    void Initialize(ImGuiContext* ctx, ImGuiAllocFns allocator_fns, HMODULE toolbox_dll) override;
    void SignalTerminate() override;

private:
    float followDistance = 200.f;
};
