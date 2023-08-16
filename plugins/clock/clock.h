#pragma once

#include <ToolboxUIPlugin.h>

class Clock : public ToolboxUIPlugin {
public:
    const char* Name() const override { return "Clock"; }

    void Draw(IDirect3DDevice9*) override;
    void Initialize(ImGuiContext*, ImGuiAllocFns, HMODULE) override;
    void Terminate() override;
};
