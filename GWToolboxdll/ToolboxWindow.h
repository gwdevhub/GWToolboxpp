#pragma once

#include <Defines.h>
#include <ToolboxUIElement.h>

/*
A ToolboxWindow is a module which also has an interface
*/
class ToolboxWindow : public ToolboxUIElement {
public:
    [[nodiscard]] bool IsWindow() const override { return true; }
    [[nodiscard]] const char* TypeName() const override { return "window"; }

    void Initialize() override
    {
        ToolboxUIElement::Initialize();
        has_closebutton = true;
    }

    void LoadSettings(ToolboxIni* ini) override
    {
        ToolboxUIElement::LoadSettings(ini);
        LOAD_BOOL(lock_move);
        LOAD_BOOL(lock_size);
        LOAD_BOOL(show_closebutton);
    }

    void SaveSettings(ToolboxIni* ini) override
    {
        ToolboxUIElement::SaveSettings(ini);
        SAVE_BOOL(lock_move);
        SAVE_BOOL(lock_size);
        SAVE_BOOL(show_closebutton);
    }

    [[nodiscard]] virtual ImGuiWindowFlags GetWinFlags(ImGuiWindowFlags flags = 0) const;
};
