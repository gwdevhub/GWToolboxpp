#pragma once

#include <ToolboxPlugin.h>

class Clock : public ToolboxPlugin {
public:
    const char* Name() const override { return "Clock"; }

    void Draw(IDirect3DDevice9*) override;
    void Initialize(ImGuiContext*, ImGuiAllocFns, HMODULE) override;
    void Terminate() override;
};
