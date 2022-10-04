#pragma once

#include <ToolboxPlugin.h>

class Armory : public ToolboxPlugin {
public:
    const char* Name() const override { return "Armory"; }

    void Draw(IDirect3DDevice9*) override;
    void Update(float) override;
    void DrawSettings() override;
    void Initialize(ImGuiContext*, ImGuiAllocFns, HMODULE) override;
    void Terminate() override;

private:
    bool visible = true;
};
