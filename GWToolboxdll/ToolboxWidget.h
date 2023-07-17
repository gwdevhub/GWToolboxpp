#pragma once

#include <ToolboxUIElement.h>

class ToolboxWidget : public ToolboxUIElement {
public:
    [[nodiscard]] bool IsWidget() const override { return true; }
    [[nodiscard]] const char* TypeName() const override { return "widget"; }

    void LoadSettings(ToolboxIni* ini) override
    {
        ToolboxUIElement::LoadSettings(ini);
        lock_move = true;
        lock_size = true;
    }


    [[nodiscard]] virtual ImGuiWindowFlags GetWinFlags(ImGuiWindowFlags flags = 0,
                                                       bool noinput_if_frozen = true) const;
};
