#pragma once

#include <ToolboxPlugin.h>

#include <IconsFontAwesome5.h>

class ChestOpener : public ToolboxPlugin {
public:
    ChestOpener() = default;
    ~ChestOpener() override = default;

    const char* Name() const override { return "ChestOpener"; }
    const char* Icon() const override { return ICON_FA_LOCK_OPEN; }

    void Initialize(ImGuiContext* ctx, ImGuiAllocFns allocator_fns, HMODULE toolbox_dll) override;
    void SignalTerminate() override;

    bool HasSettings() const override { return true; }
    void DrawSettings() override;
};
