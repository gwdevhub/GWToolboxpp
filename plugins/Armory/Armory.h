#pragma once

#include <ToolboxPlugin.h>
#include <IconsFontAwesome5.h>

class Armory : public ToolboxPlugin {
public:
    [[nodiscard]] const char* Name() const override { return "Armory"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_VEST; }

    void Draw(IDirect3DDevice9*) override;
    void Update(float) override;
    void Initialize(ImGuiContext*, ImGuiAllocFns, HMODULE, bool*) override;
    void Terminate() override;
};
