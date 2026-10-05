#pragma once

#include <ToolboxWindow.h>

class AgentAppearanceWindow : public ToolboxWindow {
    AgentAppearanceWindow() = default;
    ~AgentAppearanceWindow() override = default;

public:
    static AgentAppearanceWindow& Instance()
    {
        static AgentAppearanceWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Agent Appearance"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_PALETTE; }

    void Show();
    void Draw(IDirect3DDevice9* device) override;

private:
    bool pending_focus = false;
};
