#pragma once

#include <ToolboxUIElement.h>

class ToolboxWidget : public ToolboxUIElement {
public:
    const bool IsWidget() const override { return true; }
    const char* TypeName() const override { return "widget"; }

    void LoadSettings(ToolboxIni* ini) override {
        ToolboxUIElement::LoadSettings(ini);
        lock_move = true;
        lock_size = true;
    }


    virtual ImGuiWindowFlags GetWinFlags(ImGuiWindowFlags flags = 0,
        bool noinput_if_frozen = true) const;
};
